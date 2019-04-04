/*
    Copyright (c) 2005-2019 Intel Corporation

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.




*/

/*
    This microbenchmark measures prioritization of the tasks that were spawned to execute async_node
    successor's body. The idea of the prioritization is to have TBB worker threads react eagerly on
    the work, which was returned back to the graph through async_node's gateway interface while
    being occupied by CPU work generated by function_node with nested parallelism. Worker threads
    should prefer async_node task instead of next parallel_for task. The result of correct work
    prioritization is the interleaving of CPU and ASYNC work:

        ASYNC task=1 started, thread_idx=0
        CPU task=1 started, thread_idx=0
            CPU task=1, nested pfor task=0, thread_idx=0
            CPU task=1, nested pfor task=4, thread_idx=1
        ASYNC task=1 finished, thread_idx=0
        ASYNC task=2 started, thread_idx=0
            CPU task=1, nested pfor task=1, thread_idx=0
            CPU task=1, nested pfor task=5, thread_idx=1
        ASYNC task=2 finished, thread_idx=0
        ASYNC task=3 started, thread_idx=0
            CPU task=1, nested pfor task=2, thread_idx=0
            CPU task=1, nested pfor task=6, thread_idx=1
        ASYNC task=3 finished, thread_idx=0
        ASYNC task=4 started, thread_idx=0
            CPU task=1, nested pfor task=3, thread_idx=0
            CPU task=1, nested pfor task=7, thread_idx=1
        ASYNC task=4 finished, thread_idx=0
        ASYNC task=5 started, thread_idx=0
        CPU task=1 finished, thread_idx=0
        CPU task=2 started, thread_idx=0
            CPU task=2, nested pfor task=0, thread_idx=0
            CPU task=2, nested pfor task=4, thread_idx=1
            CPU task=2, nested pfor task=5, thread_idx=1
        ASYNC task=5 finished, thread_idx=0
        ASYNC task=6 started, thread_idx=0
            CPU task=2, nested pfor task=1, thread_idx=0
        ASYNC task=6 finished, thread_idx=1
            CPU task=2, nested pfor task=2, thread_idx=0
        ASYNC task=7 started, thread_idx=1
            CPU task=2, nested pfor task=6, thread_idx=1
        ASYNC task=7 finished, thread_idx=0
        ASYNC task=8 started, thread_idx=0
            CPU task=2, nested pfor task=7, thread_idx=1
            CPU task=2, nested pfor task=3, thread_idx=0
        CPU task=2 finished, thread_idx=0
        ASYNC task=8 finished, thread_idx=1
        Elapsed time: 8.002

    The parameters are chosen so that CPU and ASYNC work take approximately the same time.
*/

#include "tbb/task_scheduler_init.h"
#include "tbb/parallel_for.h"
#include "tbb/concurrent_queue.h"
#include "tbb/tick_count.h"
#include "tbb/tbb_thread.h"
#include "tbb/flow_graph.h"
#include "tbb/task_arena.h"
#include <vector>
#include <cstdio>

const int NUM_THREADS = 2;                          // number of threads TBB is initialized with
const int CPU_LIMIT = 2;                            // number of repetitions of sub-graph with function_node
const double CPU_SPIN = 1.;                         // execution time of every parallel_for task
const int NESTED_CPU_TASKS_COUNT = 4 * NUM_THREADS; // number of parallel_for tasks
const int ASYNC_LIMIT = 8;                          // number of repetitions of sub-graph with async_node
const double ASYNC_SPIN = 0.5;                      // execution time of every async_node work

void spin(double s) {
    tbb::tick_count start = tbb::tick_count::now();
    while ((tbb::tick_count::now() - start).seconds() < s);
}

typedef int data_type;
typedef tbb::flow::async_node<data_type, data_type> async_node_type;
typedef tbb::flow::multifunction_node<data_type,
    tbb::flow::tuple<data_type, data_type> > decider_node_type;

struct AsyncActivity {
    typedef async_node_type::gateway_type gateway_type;

    struct work_type {
        data_type input;
        gateway_type* gateway;
    };
    bool done;
    bool end_of_work() { return done; }
    tbb::concurrent_queue<work_type> my_queue;
    tbb::tbb_thread my_service_thread;

    struct ServiceThreadFunc {
        void operator()(AsyncActivity* activity) {
            while (!activity->end_of_work()) {
                work_type work;
                while (activity->my_queue.try_pop(work)) {
                    spin(ASYNC_SPIN); // do work
                    work.gateway->try_put(work.input);
                    work.gateway->release_wait();
                }
            }
        }
    };

    void stop_and_wait() {
        done = true;
        my_service_thread.join();
    }

    void submit(data_type input, gateway_type* gateway) {
        work_type work = { input, gateway };
        gateway->reserve_wait();
        my_queue.push(work);
    }

    AsyncActivity() : done(false), my_service_thread(ServiceThreadFunc(), this) {}
};

struct StartBody {
    bool has_run;
    bool operator()(data_type& input) {
        if (has_run) return false;
        else {
            input = 1;
            has_run = true;
            return true;
        }
    }
    StartBody() : has_run(false) {}
};

struct ParallelForBody {
    const data_type& my_input;
    ParallelForBody(const data_type& input) : my_input(input) {}
    void operator()(const data_type& p) const {
        std::printf("    CPU task=%d, nested pfor task=%d, thread_idx=%d\n", my_input, p,
                    tbb::this_task_arena::current_thread_index());
        spin(CPU_SPIN);
    }
};

struct CpuWorkBody {
    const int parallel_for_tasks_count;
    data_type operator()(const data_type& input) {
        std::printf("CPU task=%d started, thread_idx=%d\n", input,
                    tbb::this_task_arena::current_thread_index());
        tbb::parallel_for(0, parallel_for_tasks_count, ParallelForBody(input));
        return input;
    }
    CpuWorkBody() : parallel_for_tasks_count(NESTED_CPU_TASKS_COUNT) {}
};

struct DeciderBody {
    const int& my_limit;
    DeciderBody( const int& limit ) : my_limit( limit ) {}
    void operator()(data_type input, decider_node_type::output_ports_type& ports) {
        const char* work_type = my_limit == ASYNC_LIMIT ? "ASYNC" : "CPU";
        std::printf("%s task=%d finished, thread_idx=%d\n", work_type, input,
                    tbb::this_task_arena::current_thread_index());
        if (input < my_limit)
            tbb::flow::get<0>(ports).try_put(input + 1);
        else
            tbb::flow::get<1>(ports).try_put(input + 1);
    }
};

struct AsyncSubmissionBody {
    AsyncActivity* my_activity;
    void operator()(data_type input, async_node_type::gateway_type& gateway) {
        my_activity->submit(input, &gateway);
        std::printf("ASYNC task=%d started, thread_idx=%d\n", input,
                    tbb::this_task_arena::current_thread_index());
    }
    AsyncSubmissionBody(AsyncActivity* activity) : my_activity(activity) {}
};

int main() {
    tbb::task_scheduler_init init(NUM_THREADS);
    AsyncActivity activity;
    tbb::flow::graph g;

    tbb::flow::source_node<data_type> starter_node(g, StartBody(), false);
    tbb::flow::function_node<data_type, data_type> cpu_work_node(g, tbb::flow::unlimited, CpuWorkBody());
    decider_node_type cpu_restarter_node(g, tbb::flow::unlimited, DeciderBody(CPU_LIMIT));
    async_node_type async_node(g, tbb::flow::unlimited, AsyncSubmissionBody(&activity));
    decider_node_type async_restarter_node(g, tbb::flow::unlimited, DeciderBody(ASYNC_LIMIT)
#if __TBB_PREVIEW_FLOW_GRAPH_PRIORITIES
                                           , /*priority=*/1
#endif
    );

    tbb::flow::make_edge(starter_node, cpu_work_node);
    tbb::flow::make_edge(cpu_work_node, cpu_restarter_node);
    tbb::flow::make_edge(tbb::flow::output_port<0>(cpu_restarter_node), cpu_work_node);

    tbb::flow::make_edge(starter_node, async_node);
    tbb::flow::make_edge(async_node, async_restarter_node);
    tbb::flow::make_edge(tbb::flow::output_port<0>(async_restarter_node), async_node);

    tbb::tick_count start_time = tbb::tick_count::now();
    starter_node.activate();
    g.wait_for_all();
    activity.stop_and_wait();
    std::printf("Elapsed time: %lf seconds\n", (tbb::tick_count::now() - start_time).seconds());

    return 0;
}
