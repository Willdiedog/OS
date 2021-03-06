/*++ BUILD Version: 0009    // Increment this if a change has global effects

Copyright (c) Microsoft Corporation. All rights reserved. 

You may only use this code if you agree to the terms of the Windows Research Kernel Source Code License agreement (see License.txt).
If you do not agree to the terms, do not use the code.


Module Name:

    ps.h

Abstract:

    This module contains the process structure public data structures and
    procedure prototypes to be used within the NT system.

--*/

#ifndef _PS_
#define _PS_


//
// Process Object
//

//
// Process object body.  A pointer to this structure is returned when a handle
// to a process object is referenced.  This structure contains a process control
// block (PCB) which is the kernel's representation of a process.
//

#define MEMORY_PRIORITY_BACKGROUND 0
#define MEMORY_PRIORITY_WASFOREGROUND 1
#define MEMORY_PRIORITY_FOREGROUND 2

typedef struct _MMSUPPORT_FLAGS {

    //
    // The next 8 bits are protected by the expansion lock.
    //

    UCHAR SessionSpace : 1;
    UCHAR BeingTrimmed : 1;
    UCHAR SessionLeader : 1;
    UCHAR TrimHard : 1;
    UCHAR MaximumWorkingSetHard : 1;
    UCHAR ForceTrim : 1;
    UCHAR MinimumWorkingSetHard : 1;
    UCHAR Available0 : 1;

    UCHAR MemoryPriority : 8;

    //
    // The next 16 bits are protected by the working set mutex.
    //

    USHORT GrowWsleHash : 1;
    USHORT AcquiredUnsafe : 1;
    USHORT Available : 14;
} MMSUPPORT_FLAGS;

typedef ULONG WSLE_NUMBER, *PWSLE_NUMBER;

typedef struct _MMSUPPORT {
    LIST_ENTRY WorkingSetExpansionLinks;   // 表头MmWorkingSetExpansionHead
    LARGE_INTEGER LastTrimTime;

    MMSUPPORT_FLAGS Flags;
    ULONG PageFaultCount;
    WSLE_NUMBER PeakWorkingSetSize;
    WSLE_NUMBER GrowthSinceLastEstimate;

    WSLE_NUMBER MinimumWorkingSetSize;
    WSLE_NUMBER MaximumWorkingSetSize;
    struct _MMWSL *VmWorkingSetList;
    WSLE_NUMBER Claim;

    WSLE_NUMBER NextEstimationSlot;
    WSLE_NUMBER NextAgingSlot;
    WSLE_NUMBER EstimatedAvailable;
    WSLE_NUMBER WorkingSetSize;

    EX_PUSH_LOCK WorkingSetMutex;

} MMSUPPORT, *PMMSUPPORT;

typedef struct _MMADDRESS_NODE {
    union {
        LONG_PTR Balance : 2;
        struct _MMADDRESS_NODE *Parent;
    } u1;
    struct _MMADDRESS_NODE *LeftChild;
    struct _MMADDRESS_NODE *RightChild;
    ULONG_PTR StartingVpn;
    ULONG_PTR EndingVpn;
} MMADDRESS_NODE, *PMMADDRESS_NODE;  // MMVAD

//
// A pair of macros to deal with the packing of parent & balance in the
// MMADDRESS_NODE.
//

#define SANITIZE_PARENT_NODE(Parent) ((PMMADDRESS_NODE)(((ULONG_PTR)(Parent)) & ~0x3))

//
// Macro to carefully preserve the balance while updating the parent.
//

#define MI_MAKE_PARENT(ParentNode,ExistingBalance) \
                (PMMADDRESS_NODE)((ULONG_PTR)(ParentNode) | ((ExistingBalance) & 0x3))

typedef struct _MM_AVL_TABLE {
    MMADDRESS_NODE  BalancedRoot;  // 平衡二叉树根节点
    ULONG_PTR DepthOfTree: 5;
    ULONG_PTR Unused: 3;
#if defined (_WIN64)
    ULONG_PTR NumberGenericTableElements: 56;
#else
    ULONG_PTR NumberGenericTableElements: 24;
#endif
    PVOID NodeHint;
    PVOID NodeFreeHint;
} MM_AVL_TABLE, *PMM_AVL_TABLE;

//
// Client impersonation information.
//

typedef struct _PS_IMPERSONATION_INFORMATION {
    PACCESS_TOKEN Token;
    BOOLEAN CopyOnOpen;
    BOOLEAN EffectiveOnly;
    SECURITY_IMPERSONATION_LEVEL ImpersonationLevel;
} PS_IMPERSONATION_INFORMATION, *PPS_IMPERSONATION_INFORMATION;

//
// Audit Information structure: this is a member of the EPROCESS structure
// and currently contains only the name of the exec'ed image file.
//

typedef struct _SE_AUDIT_PROCESS_CREATION_INFO {
    POBJECT_NAME_INFORMATION ImageFileName;
} SE_AUDIT_PROCESS_CREATION_INFO, *PSE_AUDIT_PROCESS_CREATION_INFO;

typedef enum _PS_QUOTA_TYPE {
    PsNonPagedPool = 0,  // 非换页内存池
    PsPagedPool    = 1,  // 换页内存池
    PsPageFile     = 2,  // 交换文件
    PsQuotaTypes   = 3
} PS_QUOTA_TYPE, *PPS_QUOTA_TYPE;

typedef struct _EPROCESS_QUOTA_ENTRY {
    SIZE_T Usage;  // Current usage count
    SIZE_T Limit;  // Unhindered progress may be made to this point
    SIZE_T Peak;   // Peak quota usage
    SIZE_T Return; // Quota value to return to the pool once its big enough
} EPROCESS_QUOTA_ENTRY, *PEPROCESS_QUOTA_ENTRY;

//#define PS_TRACK_QUOTA 1

#define EPROCESS_QUOTA_TRACK_MAX 10000

typedef struct _EPROCESS_QUOTA_TRACK {
    SIZE_T Charge;
    PVOID Caller;
    PVOID FreeCaller;
    PVOID Process;
} EPROCESS_QUOTA_TRACK, *PEPROCESS_QUOTA_TRACK;

typedef struct _EPROCESS_QUOTA_BLOCK {
    EPROCESS_QUOTA_ENTRY QuotaEntry[PsQuotaTypes];
    LIST_ENTRY QuotaList; // All additional quota blocks are chained through here
    ULONG ReferenceCount;  // 多少进程在使用此内存分配额度标准
    ULONG ProcessCount; // Total number of processes still referencing this block
#if defined (PS_TRACK_QUOTA)
    EPROCESS_QUOTA_TRACK Tracker[2][EPROCESS_QUOTA_TRACK_MAX];
#endif
} EPROCESS_QUOTA_BLOCK, *PEPROCESS_QUOTA_BLOCK;

//
// Pagefault monitoring.
//

typedef struct _PAGEFAULT_HISTORY {
    ULONG CurrentIndex;
    ULONG MaxIndex;
    KSPIN_LOCK SpinLock;
    PVOID Reserved;
    PROCESS_WS_WATCH_INFORMATION WatchInfo[1];  // 进程的页面错误信息
} PAGEFAULT_HISTORY, *PPAGEFAULT_HISTORY;

#define PS_WS_TRIM_FROM_EXE_HEADER        1
#define PS_WS_TRIM_BACKGROUND_ONLY_APP    2

//
// Wow64 process structure.
//



typedef struct _WOW64_PROCESS {
    PVOID Wow64;
} WOW64_PROCESS, *PWOW64_PROCESS;

#if defined (_WIN64)
#define PS_GET_WOW64_PROCESS(Process) ((Process)->Wow64Process)
#else
#define PS_GET_WOW64_PROCESS(Process) ((Process), ((PWOW64_PROCESS)NULL))
#endif

#define PS_SET_BITS(Flags, Flag) \
    RtlInterlockedSetBitsDiscardReturn (Flags, Flag)

#define PS_TEST_SET_BITS(Flags, Flag) \
    RtlInterlockedSetBits (Flags, Flag)

#define PS_CLEAR_BITS(Flags, Flag) \
    RtlInterlockedClearBitsDiscardReturn (Flags, Flag)

#define PS_TEST_CLEAR_BITS(Flags, Flag) \
    RtlInterlockedClearBits (Flags, Flag)

#define PS_SET_CLEAR_BITS(Flags, sFlag, cFlag) \
    RtlInterlockedSetClearBits (Flags, sFlag, cFlag)

#define PS_TEST_ALL_BITS_SET(Flags, Bits) \
    ((Flags&(Bits)) == (Bits))

// Process structure.
// 执行体层
// If you remove a field from this structure, please also
// remove the reference to it from within the kernel debugger
// (nt\private\sdktools\ntsd\ntkext.c)
//

typedef struct _EPROCESS {
    KPROCESS Pcb;

    //
    // Lock used to protect:
    // The list of threads in the process.
    // Process token.
    // Win32 process field.
    // Process and thread affinity setting.
    //

    EX_PUSH_LOCK ProcessLock;  // push lock

    LARGE_INTEGER CreateTime;  // 进程的创建时间
    LARGE_INTEGER ExitTime;  // 进程的退出时间

    //
    // Structure to allow lock free cross process access to the process
    // handle table, process section and address space. Acquire rundown
    // protection with this if you do cross process handle table, process
    // section or address space references.
    //

    EX_RUNDOWN_REF RundownProtect;  // 进程停止保护锁  

    HANDLE UniqueProcessId; // 进程ID  PHANDLE_TABLE_ENTRY

    //
    // Global list of all processes in the system. Processes are removed
    // from this list in the object deletion routine.  References to
    // processes in this list must be done with ObReferenceObjectSafe
    // because of this.
    //

    LIST_ENTRY ActiveProcessLinks; // 活动进程双链表  PsActiveProcessHead

    //
    // Quota Fields.
    //

    SIZE_T QuotaUsage[PsQuotaTypes];  // 一进程的内存使用量
    SIZE_T QuotaPeak[PsQuotaTypes];  // 一进程的尖峰使用量
    SIZE_T CommitCharge;  // 进程虚拟内存已提交的页面数

    //
    // VmCounters.
    //

    SIZE_T PeakVirtualSize;  // 虚拟内存大小de尖峰值
    SIZE_T VirtualSize;  // 虚拟内存大小

    LIST_ENTRY SessionProcessLinks;  // 系统会话进程链表

    PVOID DebugPort;  // 调试端口
    PVOID ExceptionPort; // 异常端口
    PHANDLE_TABLE ObjectTable;  // 进程句柄表 已打开的对象的句柄表  HANDLE_TABLE

    //
    // Security.
    //

    EX_FAST_REF Token;  // 进程的快速引用，指向该进程的访问令牌，用于该进程的安全访问检查

    PFN_NUMBER WorkingSetPage;  // 进程工作集的页面
    KGUARDED_MUTEX AddressCreationLock;  // guarded mutex 守护互斥体锁  用于保护对地址空间的操作
    KSPIN_LOCK HyperSpaceLock; // spin lock 用于保护进程的超空间

    struct _ETHREAD *ForkInProgress;  // 正在复制地址空间的线程
    ULONG_PTR HardwareTrigger;  // 记录硬件错误性能分析次数

    PMM_AVL_TABLE PhysicalVadRoot; // 指向进程的物理VAD树的根 VAD: virtual address descriptor
    PVOID CloneRoot; // 平衡树  在进程地址空间复制时，被创建   用于支持fork语义(进程创建子进程方面的优化方法)
    PFN_NUMBER NumberOfPrivatePages;    // 进程私有页面的数量
    PFN_NUMBER NumberOfLockedPages;	    // 进程被锁页面的数量
    PVOID Win32Process;	// Windows子系统进程(GUI进程)
    struct _EJOB *Job; 
    PVOID SectionObject;  // 进程可执行映像文件的内存区对象 Image

    PVOID SectionBaseAddress;  // SectionObject基地址

    PEPROCESS_QUOTA_BLOCK QuotaBlock;  // 进程内存块分配额度  表头：PspQuotaBlockList    系统默认值PspDefaultQuotaBlock

    PPAGEFAULT_HISTORY WorkingSetWatch;  // 监视进程的页面错误
    HANDLE Win32WindowStation;  // 进程所属窗口的句柄   句柄作用域：进程内部
    HANDLE InheritedFromUniqueProcessId;  // 父进程标识符

    PVOID LdtInformation;  // 进程LDT(局部描述符表)
    PVOID VadFreeHint;  // VAD(虚拟地址描述符)节点   用于加速VAD树的查找
    PVOID VdmObjects;	// 当前进程VDM数据区 virual directory memory??
    PVOID DeviceMap;    // 进程使用到的设备表

    PVOID Spare0[3];
    union {
        HARDWARE_PTE PageDirectoryPte;  // 顶级页目录页面的页表项
        ULONGLONG Filler;
    };
    PVOID Session;  // 进程所在的系统会话 
    UCHAR ImageFileName[ 16 ];  // 进程的映像文件名

    LIST_ENTRY JobLinks;  // 链接1个Job中的所有进程   
    PVOID LockedPagesList;  // 是个LOCK_HEADER指针    记录哪些页面被锁  

    LIST_ENTRY ThreadListHead;  // 包含一个进程的所有线程 

    //
    // Used by rdr/security for authentication.
    //

    PVOID SecurityPort;  // 与lsass进程间的跨进程通信端口

#ifdef _WIN64
    PWOW64_PROCESS Wow64Process;
#else
    PVOID PaeTop;  // PAE内存访问 PAE: Physical Address Extension 支持4GB 以上物理内存
#endif

    ULONG ActiveThreads; // 当前活动线程数   0：所有线程都已退出

    ACCESS_MASK GrantedAccess;  // 进程的访问权限  如：PROCESS_VM_READ

    ULONG DefaultHardErrorProcessing;  // 默认的硬件错误处理

    NTSTATUS LastThreadExitStatus;  // 记录刚才最后一个线程的退出状态

    //
    // Peb  Process Env Block
    //

    PPEB Peb;  // 进程环境块 

    //
    // Pointer to the prefetches trace block.
    // 进程预取痕迹
    EX_FAST_REF PrefetchTrace;  

    LARGE_INTEGER ReadOperationCount;  // NtReadFile 调用次数
    LARGE_INTEGER WriteOperationCount; // NtWriteFile 调用次数
    LARGE_INTEGER OtherOperationCount;
    LARGE_INTEGER ReadTransferCount;  // IO读操作完成次数
    LARGE_INTEGER WriteTransferCount; // IO写操作完成次数
    LARGE_INTEGER OtherTransferCount;

    SIZE_T CommitChargeLimit; // 已提交页面数量的限制值 0-没有限制
    SIZE_T CommitChargePeak;  // 尖峰时刻已提交的页面数量

    PVOID AweInfo;  // AWEINFO  AWE: Address Windowing Extension  地址窗口扩展

    //
    // This is used for SeAuditProcessCreation.
    // It contains the full path to the image file.
    //

    SE_AUDIT_PROCESS_CREATION_INFO SeAuditProcessCreationInfo; //  包含创建进程时，指定的进程映像全路径名

    MMSUPPORT Vm;  // 虚拟内存支持  包含工作集管理器在判断是否需要修剪一个进程的信息

#if !defined(_WIN64)
    LIST_ENTRY MmProcessLinks;  // 有自己地址空间的进程链表  表头：MmProcessList   用于全局内存管理
#else
    ULONG Spares[2];
#endif

    ULONG ModifiedPageCount; // 当前进程已修改页面数

    #define PS_JOB_STATUS_NOT_REALLY_ACTIVE      0x00000001UL
    #define PS_JOB_STATUS_ACCOUNTING_FOLDED      0x00000002UL
    #define PS_JOB_STATUS_NEW_PROCESS_REPORTED   0x00000004UL
    #define PS_JOB_STATUS_EXIT_PROCESS_REPORTED  0x00000008UL
    #define PS_JOB_STATUS_REPORT_COMMIT_CHANGES  0x00000010UL
    #define PS_JOB_STATUS_LAST_REPORT_MEMORY     0x00000020UL
    #define PS_JOB_STATUS_REPORT_PHYSICAL_PAGE_CHANGES  0x00000040UL

    ULONG JobStatus;


    //
    // Process flags. Use interlocked operations with PS_SET_BITS, etc
    // to modify these.
    //

    #define PS_PROCESS_FLAGS_CREATE_REPORTED        0x00000001UL // Create process debug call has occurred
    #define PS_PROCESS_FLAGS_NO_DEBUG_INHERIT       0x00000002UL // Don't inherit debug port
    #define PS_PROCESS_FLAGS_PROCESS_EXITING        0x00000004UL // PspExitProcess entered
    #define PS_PROCESS_FLAGS_PROCESS_DELETE         0x00000008UL // Delete process has been issued
    #define PS_PROCESS_FLAGS_WOW64_SPLIT_PAGES      0x00000010UL // Wow64 split pages
    #define PS_PROCESS_FLAGS_VM_DELETED             0x00000020UL // VM is deleted
    #define PS_PROCESS_FLAGS_OUTSWAP_ENABLED        0x00000040UL // Outswap enabled
    #define PS_PROCESS_FLAGS_OUTSWAPPED             0x00000080UL // Outswapped
    #define PS_PROCESS_FLAGS_FORK_FAILED            0x00000100UL // Fork status
    #define PS_PROCESS_FLAGS_WOW64_4GB_VA_SPACE     0x00000200UL // Wow64 process with 4gb virtual address space
    #define PS_PROCESS_FLAGS_ADDRESS_SPACE1         0x00000400UL // Addr space state1
    #define PS_PROCESS_FLAGS_ADDRESS_SPACE2         0x00000800UL // Addr space state2
    #define PS_PROCESS_FLAGS_SET_TIMER_RESOLUTION   0x00001000UL // SetTimerResolution has been called
    #define PS_PROCESS_FLAGS_BREAK_ON_TERMINATION   0x00002000UL // Break on process termination
    #define PS_PROCESS_FLAGS_CREATING_SESSION       0x00004000UL // Process is creating a session
    #define PS_PROCESS_FLAGS_USING_WRITE_WATCH      0x00008000UL // Process is using the write watch APIs
    #define PS_PROCESS_FLAGS_IN_SESSION             0x00010000UL // Process is in a session
    #define PS_PROCESS_FLAGS_OVERRIDE_ADDRESS_SPACE 0x00020000UL // Process must use native address space (Win64 only)
    #define PS_PROCESS_FLAGS_HAS_ADDRESS_SPACE      0x00040000UL // This process has an address space
    #define PS_PROCESS_FLAGS_LAUNCH_PREFETCHED      0x00080000UL // Process launch was prefetched
    #define PS_PROCESS_INJECT_INPAGE_ERRORS         0x00100000UL // Process should be given inpage errors - hardcoded in trap.asm too
    #define PS_PROCESS_FLAGS_VM_TOP_DOWN            0x00200000UL // Process memory allocations default to top-down
    #define PS_PROCESS_FLAGS_IMAGE_NOTIFY_DONE      0x00400000UL // We have sent a message for this image
    #define PS_PROCESS_FLAGS_PDE_UPDATE_NEEDED      0x00800000UL // The system PDEs need updating for this process (NT32 only)
    #define PS_PROCESS_FLAGS_VDM_ALLOWED            0x01000000UL // Process allowed to invoke NTVDM support
    #define PS_PROCESS_FLAGS_SMAP_ALLOWED           0x02000000UL // Process allowed to invoke SMAP support
    #define PS_PROCESS_FLAGS_CREATE_FAILED          0x04000000UL // Process create failed

    #define PS_PROCESS_FLAGS_DEFAULT_IO_PRIORITY    0x38000000UL // The default I/O priority for created threads. (3 bits)

    #define PS_PROCESS_FLAGS_PRIORITY_SHIFT         27
    
    #define PS_PROCESS_FLAGS_EXECUTE_SPARE1         0x40000000UL //
    #define PS_PROCESS_FLAGS_EXECUTE_SPARE2         0x80000000UL //


    union {

        ULONG Flags;  // 进程Flags  PS_PROCESS_FLAGS_XXX

        //
        // Fields can only be set by the PS_SET_BITS and other interlocked
        // macros.  Reading fields is best done via the bit definitions so
        // references are easy to locate.
        //

        struct {
            ULONG CreateReported            : 1;
            ULONG NoDebugInherit            : 1;
            ULONG ProcessExiting            : 1;
            ULONG ProcessDelete             : 1;
            ULONG Wow64SplitPages           : 1;
            ULONG VmDeleted                 : 1;
            ULONG OutswapEnabled            : 1;
            ULONG Outswapped                : 1;
            ULONG ForkFailed                : 1;
            ULONG Wow64VaSpace4Gb           : 1;
            ULONG AddressSpaceInitialized   : 2;
            ULONG SetTimerResolution        : 1;
            ULONG BreakOnTermination        : 1;
            ULONG SessionCreationUnderway   : 1;
            ULONG WriteWatch                : 1;
            ULONG ProcessInSession          : 1;
            ULONG OverrideAddressSpace      : 1;
            ULONG HasAddressSpace           : 1;
            ULONG LaunchPrefetched          : 1;
            ULONG InjectInpageErrors        : 1;
            ULONG VmTopDown                 : 1;
            ULONG ImageNotifyDone           : 1;
            ULONG PdeUpdateNeeded           : 1;    // NT32 only
            ULONG VdmAllowed                : 1;
            ULONG SmapAllowed               : 1;
            ULONG CreateFailed              : 1;
            ULONG DefaultIoPriority         : 3;
            ULONG Spare1                    : 1;
            ULONG Spare2                    : 1;
        };
    };

    NTSTATUS ExitStatus;  // 进程退出状态

    USHORT NextPageColor;  // 物理页面分配算法
    union {
        struct {
            UCHAR SubSystemMinorVersion;  // 一进程的子系统主版本号
            UCHAR SubSystemMajorVersion;  // 一进程的子系统次版本号
        };
        USHORT SubSystemVersion; 
    };
    UCHAR PriorityClass; // 进程的优先级    PROCESS_PRIORITY_CLASS_XXX

    MM_AVL_TABLE VadRoot; // 平衡二叉树的根节点  管理该进程的虚拟地址空间

    ULONG Cookie;  // 一代表该进程的随机值

} EPROCESS, *PEPROCESS; 

C_ASSERT( FIELD_OFFSET(EPROCESS, Pcb) == 0 );
           
//
// Thread termination port
//

typedef struct _TERMINATION_PORT {
    struct _TERMINATION_PORT *Next;
    PVOID Port;
} TERMINATION_PORT, *PTERMINATION_PORT;


// Thread Object
//
// Thread object body.  A pointer to this structure is returned when a handle
// to a thread object is referenced.  This structure contains a thread control
// block (TCB) which is the kernel's representation of a thread.
//

#define PS_GET_THREAD_CREATE_TIME(Thread) ((Thread)->CreateTime.QuadPart)

#define PS_SET_THREAD_CREATE_TIME(Thread, InputCreateTime) \
            ((Thread)->CreateTime.QuadPart = (InputCreateTime.QuadPart))

//
// Macro to return TRUE if the specified thread is impersonating.
//

#define PS_IS_THREAD_IMPERSONATING(Thread) (((Thread)->CrossThreadFlags&PS_CROSS_THREAD_FLAGS_IMPERSONATING) != 0)

typedef struct _ETHREAD {
    KTHREAD Tcb;

    LARGE_INTEGER CreateTime; // 线程创建时间

    union {
        LARGE_INTEGER ExitTime;  // 线程退出时间
        LIST_ENTRY LpcReplyChain;   // 用于跨进程通信 LPC  Load Procedure Call
        LIST_ENTRY KeyedWaitChain;  // 带键事件的等待链表
    };
    union {
        NTSTATUS ExitStatus;  // 退出状态
        PVOID OfsChain; 
    };

    //
    // Registry
    //

    LIST_ENTRY PostBlockList;  // PCM_POST_BLOCK类型节点  用于一线程向 配置管理器 登记 注册表键的变化通知

    //
    // Single linked list of termination blocks
    //

    union {
        //
        // List of termination ports
        //

        PTERMINATION_PORT TerminationPort;  // 线程退出时，系统会通知所有已经登记过要接收其终止事件的那些端口

        //
        // List of threads to be reaped. Only used at thread exit
        //

        struct _ETHREAD *ReaperLink;  // 在线程退出时，该节点被挂在PsReaperListHead链表上   在线程回收期(reaper)的工作项目(WorkItem)中回收该线程的内核栈

        //
        // Keyvalue being waited for
        //
        PVOID KeyedWaitValue;  // 带键事件的键值

    };

    KSPIN_LOCK ActiveTimerListLock;  // 操作ActiveTimerListHead链表的自旋锁
    LIST_ENTRY ActiveTimerListHead;  // 当前线程的所有定时器

    CLIENT_ID Cid;  // 线程的唯一标识符

    //
    // Lpc Load Procedure Call
    // 信号量对象

    union {
        KSEMAPHORE LpcReplySemaphore;   // LPC应答通知
        KSEMAPHORE KeyedWaitSemaphore;  // 用于处理带键的事件
    };

    union {
        PVOID LpcReplyMessage;          // -> Message that contains the reply   只想LPCP_MESSAGE的指针  包含LPC应答消息
        PVOID LpcWaitingOnPort;         // 在哪个端口对象上等待 
    };

    //
    // Security
    //
    //
    //    Client - If non null, indicates the thread is impersonating
    //        a client.
    //

    PPS_IMPERSONATION_INFORMATION ImpersonationInfo;  // 指向线程的模仿信息  

    //
    // Io
    //

    LIST_ENTRY IrpList;    // 当前线程所有正在处理 但未完成的I/O请求(IRP对象)

    //
    //  File Systems  文件系统
    //

    ULONG_PTR TopLevelIrp;  // either NULL, an Irp or a flag defined in FsRtl.h  指向线程的顶级IRP或指向NULL或一个IRP   只有线程I/O调用层次中最顶级的组件是文件系统时，其才指向当前IRP
    struct _DEVICE_OBJECT *DeviceToVerify;   // 待检验的设备对象   设备发生变化

    PEPROCESS ThreadsProcess;  // 当前线程所属进程
    PVOID StartAddress;  // 系统DLL中的 线程启动地址
    union {
        PVOID Win32StartAddress;  // Windows子系统接收到的 的线程启动地址  CreateThread返回的地址
        ULONG LpcReceivedMessageId;  // 接受到的LPC消息的ID  
    };
    //
    // Ps
    //

    LIST_ENTRY ThreadListEntry;  // ThreadListHead 

    //
    // Rundown protection structure. Acquire this to do cross thread
    // TEB, TEB32 or stack references.
    //

    EX_RUNDOWN_REF RundownProtect;  // 线程的停止保护锁  

    //
    // Lock to protect thread impersonation information
    //
    EX_PUSH_LOCK ThreadLock;  // 推锁，用于保护线程数据  PspLockThreadSecurityExclusive 

    ULONG LpcReplyMessageId;    // MessageId this thread is waiting for reply to  指明当前线程正在等待一个消息的应答

    ULONG ReadClusterSize;  // 指明一次I/O操作中读取多少个页面   用于页面交换文件和内存映射文件的读操作

    //
    // Client/server
    //

    ACCESS_MASK GrantedAccess;  // 线程的访问权限  THREAD_XXX  THREAD_TERMINATE-线程终止权限

    //
    // Flags for cross thread access. Use interlocked operations
    // via PS_SET_BITS etc.
    //

    //
    // Used to signify that the delete APC has been queued or the
    // thread has called PspExitThread itself.
    //

    #define PS_CROSS_THREAD_FLAGS_TERMINATED           0x00000001UL

    //
    // Thread create failed
    //

    #define PS_CROSS_THREAD_FLAGS_DEADTHREAD           0x00000002UL

    //
    // Debugger isn't shown this thread
    //

    #define PS_CROSS_THREAD_FLAGS_HIDEFROMDBG          0x00000004UL

    //
    // Thread is impersonating
    //

    #define PS_CROSS_THREAD_FLAGS_IMPERSONATING        0x00000008UL

    //
    // This is a system thread
    //

    #define PS_CROSS_THREAD_FLAGS_SYSTEM               0x00000010UL

    //
    // Hard errors are disabled for this thread
    //

    #define PS_CROSS_THREAD_FLAGS_HARD_ERRORS_DISABLED 0x00000020UL

    //
    // We should break in when this thread is terminated
    //

    #define PS_CROSS_THREAD_FLAGS_BREAK_ON_TERMINATION 0x00000040UL

    //
    // This thread should skip sending its create thread message
    //
    #define PS_CROSS_THREAD_FLAGS_SKIP_CREATION_MSG    0x00000080UL

    //
    // This thread should skip sending its final thread termination message
    //
    #define PS_CROSS_THREAD_FLAGS_SKIP_TERMINATION_MSG 0x00000100UL

    union {

        ULONG CrossThreadFlags;  // 跨线程访问的标志符  

        //
        // The following fields are for the debugger only. Do not use.
        // Use the bit definitions instead.
        //

        struct {
            ULONG Terminated              : 1;  // 线程已终止
            ULONG DeadThread              : 1;  // 线程创建失败
            ULONG HideFromDebugger        : 1;  // 线程对于调试器不可见
            ULONG ActiveImpersonationInfo : 1;  // 线程正在模仿？？
            ULONG SystemThread            : 1;  // 是个系统线程
            ULONG HardErrorsAreDisabled   : 1;  // 对于该线程，硬件错误无效
            ULONG BreakOnTermination      : 1;  // 调试器在线程终止时，停下该线程
            ULONG SkipCreationMsg         : 1;  // 不向调试器发送创建消息
            ULONG SkipTerminationMsg      : 1;  // 不向调试器发送终止消息
        };
    };

    //
    // Flags to be accessed in this thread's context only at PASSIVE
    // level -- no need to use interlocked operations.
    //

    union {
        ULONG SameThreadPassiveFlags;  // 被线程自身访问的标志位(APC中断级别上)

        struct {

            //
            // This thread is an active Ex worker thread; it should
            // not terminate.
            //

            ULONG ActiveExWorker : 1;
            ULONG ExWorkerCanWaitUser : 1;
            ULONG MemoryMaker : 1;

            //
            // Thread is active in the keyed event code. LPC should not run above this in an APC.
            //
            ULONG KeyedEventInUse : 1;
        };
    };

    //
    // Flags to be accessed in this thread's context only at APC_LEVEL.
    // No need to use interlocked operations.
    //

    union {
        ULONG SameThreadApcFlags;
        struct {

            //
            // The stored thread's MSGID is valid. This is only accessed
            // while the LPC mutex is held so it's an APC_LEVEL flag.
            //

            BOOLEAN LpcReceivedMsgIdValid : 1;
            BOOLEAN LpcExitThreadCalled   : 1;
            BOOLEAN AddressSpaceOwner     : 1;
            BOOLEAN OwnsProcessWorkingSetExclusive  : 1;
            BOOLEAN OwnsProcessWorkingSetShared     : 1;
            BOOLEAN OwnsSystemWorkingSetExclusive   : 1;
            BOOLEAN OwnsSystemWorkingSetShared      : 1;
            BOOLEAN OwnsSessionWorkingSetExclusive  : 1;
            BOOLEAN OwnsSessionWorkingSetShared     : 1;

            #define PS_SAME_THREAD_FLAGS_OWNS_A_WORKING_SET    0x000001F8UL

            BOOLEAN ApcNeeded                       : 1;
        };
    };

    BOOLEAN ForwardClusterOnly;  // 是否仅仅前向聚集
    BOOLEAN DisablePageFaultClustering;  // 控制页面交换的聚集与否
    UCHAR ActiveFaultCount;  // 正在进行中的页面错误数量

#if defined (PERF_DATA)
    ULONG PerformanceCountLow;
    LONG PerformanceCountHigh;
#endif

} ETHREAD, *PETHREAD;

C_ASSERT( FIELD_OFFSET(ETHREAD, Tcb) == 0 );

//
// The following two inline functions allow a thread or process object to
// be converted into a kernel thread or process, respectively, without
// having to expose the ETHREAD and EPROCESS definitions to the world.
//
// These functions take advantage of the fact that the kernel structures
// appear as the first element in the respective object structures.
//
// begin_ntosp

PKTHREAD
FORCEINLINE
PsGetKernelThread(
    IN PETHREAD ThreadObject
    )
{
    return (PKTHREAD)ThreadObject;
}

PKPROCESS
FORCEINLINE
PsGetKernelProcess(
    IN PEPROCESS ProcessObject
    )
{
    return (PKPROCESS)ProcessObject;
}

NTKERNELAPI
NTSTATUS
PsGetContextThread(
    __in PETHREAD Thread,
    __inout PCONTEXT ThreadContext,
    __in KPROCESSOR_MODE Mode
    );

NTKERNELAPI
NTSTATUS
PsSetContextThread(
    __in PETHREAD Thread,
    __in PCONTEXT ThreadContext,
    __in KPROCESSOR_MODE Mode
    );

// end_ntosp

//
// Initial PEB
//

typedef struct _INITIAL_PEB {
    BOOLEAN InheritedAddressSpace;      // These four fields cannot change unless the
    BOOLEAN ReadImageFileExecOptions;   //
    BOOLEAN BeingDebugged;              //
    union {
        BOOLEAN BitField;                  //
        struct {
            BOOLEAN ImageUsesLargePages : 1;
            BOOLEAN SpareBits : 7;
         };
    };
    HANDLE Mutant;                      // PEB structure is also updated.
} INITIAL_PEB, *PINITIAL_PEB;

#if defined(_WIN64)
typedef struct _INITIAL_PEB32 {
    BOOLEAN InheritedAddressSpace;      // These four fields cannot change unless the
    BOOLEAN ReadImageFileExecOptions;   //
    BOOLEAN BeingDebugged;              //
    union {
        BOOLEAN BitField;                  //
        struct {
            BOOLEAN ImageUsesLargePages : 1;
            BOOLEAN SpareBits : 7;
         };
    };
    LONG Mutant;                        // PEB structure is also updated.
} INITIAL_PEB32, *PINITIAL_PEB32;
#endif

typedef struct _PS_JOB_TOKEN_FILTER {
    ULONG CapturedSidCount ;
    PSID_AND_ATTRIBUTES CapturedSids ;
    ULONG CapturedSidsLength ;

    ULONG CapturedGroupCount ;
    PSID_AND_ATTRIBUTES CapturedGroups ;
    ULONG CapturedGroupsLength ;

    ULONG CapturedPrivilegeCount ;
    PLUID_AND_ATTRIBUTES CapturedPrivileges ;
    ULONG CapturedPrivilegesLength ;
} PS_JOB_TOKEN_FILTER, * PPS_JOB_TOKEN_FILTER ;

//
// Job Object
//

typedef struct _EJOB {
    KEVENT Event;

    //
    // All jobs are chained together via this list.
    // Protected by the global lock PspJobListLock
    //

    LIST_ENTRY JobLinks;

    //
    // All processes within this job. Processes are removed from this
    // list at last dereference. Safe object referencing needs to be done.
    // Protected by the joblock.
    //

    LIST_ENTRY ProcessListHead;
    ERESOURCE JobLock;

    //
    // Accounting Info
    //

    LARGE_INTEGER TotalUserTime;
    LARGE_INTEGER TotalKernelTime;
    LARGE_INTEGER ThisPeriodTotalUserTime;
    LARGE_INTEGER ThisPeriodTotalKernelTime;
    ULONG TotalPageFaultCount;
    ULONG TotalProcesses;
    ULONG ActiveProcesses;
    ULONG TotalTerminatedProcesses;

    //
    // Limitable Attributes
    //

    LARGE_INTEGER PerProcessUserTimeLimit;
    LARGE_INTEGER PerJobUserTimeLimit;
    ULONG LimitFlags;
    SIZE_T MinimumWorkingSetSize;
    SIZE_T MaximumWorkingSetSize;
    ULONG ActiveProcessLimit;
    KAFFINITY Affinity;
    UCHAR PriorityClass;

    //
    // UI restrictions
    //

    ULONG UIRestrictionsClass;

    //
    // Security Limitations:  write once, read always
    //

    ULONG SecurityLimitFlags;
    PACCESS_TOKEN Token;
    PPS_JOB_TOKEN_FILTER Filter;

    //
    // End Of Job Time Limit
    //

    ULONG EndOfJobTimeAction;
    PVOID CompletionPort;
    PVOID CompletionKey;

    ULONG SessionId;

    ULONG SchedulingClass;

    ULONGLONG ReadOperationCount;
    ULONGLONG WriteOperationCount;
    ULONGLONG OtherOperationCount;
    ULONGLONG ReadTransferCount;
    ULONGLONG WriteTransferCount;
    ULONGLONG OtherTransferCount;

    //
    // Extended Limits
    //

    IO_COUNTERS IoInfo;         // not used yet
    SIZE_T ProcessMemoryLimit;
    SIZE_T JobMemoryLimit;
    SIZE_T PeakProcessMemoryUsed;
    SIZE_T PeakJobMemoryUsed;
    SIZE_T CurrentJobMemoryUsed;

    KGUARDED_MUTEX MemoryLimitsLock;

    //
    // List of jobs in a job set. Processes within a job in a job set
    // can create processes in the same or higher members of the jobset.
    // Protected by the global lock PspJobListLock
    //

    LIST_ENTRY JobSetLinks;

    //
    // Member level for this job in the jobset.
    //

    ULONG MemberLevel;

    //
    // This job has had its last handle closed.
    //

#define PS_JOB_FLAGS_CLOSE_DONE 0x1UL

    ULONG JobFlags;
} EJOB;
typedef EJOB *PEJOB;


//
// Global Variables
//

extern ULONG PsPrioritySeparation;
extern ULONG PsRawPrioritySeparation;
extern LIST_ENTRY PsActiveProcessHead;
extern const UNICODE_STRING PsNtDllPathName;
extern PVOID PsSystemDllBase;
extern PEPROCESS PsInitialSystemProcess;  // 所有系统进程的父进程
extern PVOID PsNtosImageBase;
extern PVOID PsHalImageBase;

#if defined(_AMD64_)

extern INVERTED_FUNCTION_TABLE PsInvertedFunctionTable;

#endif

extern LIST_ENTRY PsLoadedModuleList;
extern ERESOURCE PsLoadedModuleResource;
extern ALIGNED_SPINLOCK PsLoadedModuleSpinLock;
extern LCID PsDefaultSystemLocaleId;
extern LCID PsDefaultThreadLocaleId;
extern LANGID PsDefaultUILanguageId;
extern LANGID PsInstallUILanguageId;
extern PEPROCESS PsIdleProcess;  // 空闲进程  ID=0
extern SINGLE_LIST_ENTRY PsReaperListHead;
extern WORK_QUEUE_ITEM PsReaperWorkItem;

#define PS_EMBEDDED_NO_USERMODE 1 // no user mode code will run on the system

extern ULONG PsEmbeddedNTMask;

BOOLEAN
PsChangeJobMemoryUsage(
    IN ULONG Flags,
    IN SSIZE_T Amount
    );

VOID
PsReportProcessMemoryLimitViolation(
    VOID
    );

#define THREAD_HIT_SLOTS 750

extern ULONG PsThreadHits[THREAD_HIT_SLOTS];

VOID
PsThreadHit(
    IN PETHREAD Thread
    );

VOID
PsEnforceExecutionTimeLimits(
    VOID
    );

BOOLEAN
PsInitSystem (
    IN ULONG Phase,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

NTSTATUS
PsMapSystemDll (
    IN PEPROCESS Process,
    OUT PVOID *DllBase OPTIONAL,
    IN LOGICAL UseLargePages
    );

VOID
PsInitializeQuotaSystem (
    VOID
    );

LOGICAL
PsShutdownSystem (
    VOID
    );

BOOLEAN
PsWaitForAllProcesses (
    VOID);

NTSTATUS
PsLocateSystemDll (
    BOOLEAN ReplaceExisting
    );

VOID
PsChangeQuantumTable(
    BOOLEAN ModifyActiveProcesses,
    ULONG PrioritySeparation
    );

//
// Get Current Prototypes
//
#define THREAD_TO_PROCESS(Thread) ((Thread)->ThreadsProcess)
#define IS_SYSTEM_THREAD(Thread)  (((Thread)->CrossThreadFlags&PS_CROSS_THREAD_FLAGS_SYSTEM) != 0)

#define _PsGetCurrentProcess() (CONTAINING_RECORD(((KeGetCurrentThread())->ApcState.Process),EPROCESS,Pcb))
#define PsGetCurrentProcessByThread(xCurrentThread) (ASSERT((xCurrentThread) == PsGetCurrentThread ()),CONTAINING_RECORD(((xCurrentThread)->Tcb.ApcState.Process),EPROCESS,Pcb))

#define _PsGetCurrentThread() ((PETHREAD)KeGetCurrentThread())

//
// N.B. The kernel thread object is architecturally defined as being at offset
//      zero of the executive thread object. This assumption has been exported
//      from ntddk.h for some time.
//

C_ASSERT(FIELD_OFFSET(ETHREAD, Tcb) == 0);

#if defined(_NTOSP_)

// begin_ntosp

NTKERNELAPI
PEPROCESS
PsGetCurrentProcess(
    VOID
    );

#define PsGetCurrentThread() ((PETHREAD)KeGetCurrentThread())

// end_ntosp

#else

#define PsGetCurrentProcess() _PsGetCurrentProcess()

#define PsGetCurrentThread() _PsGetCurrentThread()

#endif

//
// Exit kernel mode APC routine.
//

VOID
PsExitSpecialApc(
    IN PKAPC Apc,
    IN PKNORMAL_ROUTINE *NormalRoutine,
    IN PVOID *NormalContext,
    IN PVOID *SystemArgument1,
    IN PVOID *SystemArgument2
    );

// begin_ntddk begin_wdm begin_nthal begin_ntifs begin_ntosp
//
// System Thread and Process Creation and Termination
//

NTKERNELAPI
NTSTATUS
PsCreateSystemThread(
    __out PHANDLE ThreadHandle,
    __in ULONG DesiredAccess,
    __in_opt POBJECT_ATTRIBUTES ObjectAttributes,
    __in_opt  HANDLE ProcessHandle,
    __out_opt PCLIENT_ID ClientId,
    __in PKSTART_ROUTINE StartRoutine,
    __in_opt PVOID StartContext
    );

NTKERNELAPI
NTSTATUS
PsTerminateSystemThread(
    __in NTSTATUS ExitStatus
    );

NTKERNELAPI
NTSTATUS
PsWrapApcWow64Thread (
    __inout PVOID *ApcContext,
    __inout PVOID *ApcRoutine);


// end_ntddk end_wdm end_nthal end_ntifs end_ntosp

NTKERNELAPI
NTSTATUS
PsCreateSystemProcess(
    __out PHANDLE ProcessHandle,
    __in ULONG DesiredAccess,
    __in_opt POBJECT_ATTRIBUTES ObjectAttributes
    );

typedef
VOID (*PBBT_NOTIFY_ROUTINE)(
    PKTHREAD Thread
    );

NTKERNELAPI
ULONG
PsSetBBTNotifyRoutine(
    __in PBBT_NOTIFY_ROUTINE BBTNotifyRoutine
    );

// begin_ntifs begin_ntddk

typedef
VOID
(*PCREATE_PROCESS_NOTIFY_ROUTINE)(
    IN HANDLE ParentId,
    IN HANDLE ProcessId,
    IN BOOLEAN Create
    );

NTKERNELAPI
NTSTATUS
PsSetCreateProcessNotifyRoutine(
    __in PCREATE_PROCESS_NOTIFY_ROUTINE NotifyRoutine,
    __in BOOLEAN Remove
    );

typedef
VOID
(*PCREATE_THREAD_NOTIFY_ROUTINE)(
    IN HANDLE ProcessId,
    IN HANDLE ThreadId,
    IN BOOLEAN Create
    );

NTKERNELAPI
NTSTATUS
PsSetCreateThreadNotifyRoutine(
    __in PCREATE_THREAD_NOTIFY_ROUTINE NotifyRoutine
    );

NTKERNELAPI
NTSTATUS
PsRemoveCreateThreadNotifyRoutine (
    __in PCREATE_THREAD_NOTIFY_ROUTINE NotifyRoutine
    );

//
// Structures for Load Image Notify
//

typedef struct _IMAGE_INFO {
    union {
        ULONG Properties;
        struct {
            ULONG ImageAddressingMode  : 8;  // code addressing mode
            ULONG SystemModeImage      : 1;  // system mode image
            ULONG ImageMappedToAllPids : 1;  // image mapped into all processes
            ULONG Reserved             : 22;
        };
    };
    PVOID       ImageBase;
    ULONG       ImageSelector;
    SIZE_T      ImageSize;
    ULONG       ImageSectionNumber;
} IMAGE_INFO, *PIMAGE_INFO;

#define IMAGE_ADDRESSING_MODE_32BIT     3

typedef
VOID
(*PLOAD_IMAGE_NOTIFY_ROUTINE)(
    IN PUNICODE_STRING FullImageName,
    IN HANDLE ProcessId,                // pid into which image is being mapped
    IN PIMAGE_INFO ImageInfo
    );

NTKERNELAPI
NTSTATUS
PsSetLoadImageNotifyRoutine(
    __in PLOAD_IMAGE_NOTIFY_ROUTINE NotifyRoutine
    );

NTKERNELAPI
NTSTATUS
PsRemoveLoadImageNotifyRoutine(
    __in PLOAD_IMAGE_NOTIFY_ROUTINE NotifyRoutine
    );

// end_ntddk

//
// Security Support
//

NTKERNELAPI
NTSTATUS
PsAssignImpersonationToken(
    __in PETHREAD Thread,
    __in HANDLE Token
    );

// begin_ntosp

NTKERNELAPI
PACCESS_TOKEN
PsReferencePrimaryToken(
    __inout PEPROCESS Process
    );

NTKERNELAPI
VOID
PsDereferencePrimaryToken(
    __in PACCESS_TOKEN PrimaryToken
    );

NTKERNELAPI
VOID
PsDereferenceImpersonationToken(
    __in PACCESS_TOKEN ImpersonationToken
    );

// end_ntifs
// end_ntosp


#define PsDereferencePrimaryTokenEx(P,T) (ObFastDereferenceObject (&P->Token,(T)))

#define PsDereferencePrimaryToken(T) (ObDereferenceObject((T)))

#define PsDereferenceImpersonationToken(T)                                    \
            {if (ARGUMENT_PRESENT((T))) {                                     \
                (ObDereferenceObject((T)));                                   \
             } else {                                                         \
                ;                                                             \
             }                                                                \
            }


#define PsProcessAuditId(Process)    ((Process)->UniqueProcessId)

// begin_ntosp
// begin_ntifs

NTKERNELAPI
PACCESS_TOKEN
PsReferenceImpersonationToken(
    __inout PETHREAD Thread,
    __out PBOOLEAN CopyOnOpen,
    __out PBOOLEAN EffectiveOnly,
    __out PSECURITY_IMPERSONATION_LEVEL ImpersonationLevel
    );

// end_ntifs

PACCESS_TOKEN
PsReferenceEffectiveToken(
    IN PETHREAD Thread,
    OUT PTOKEN_TYPE TokenType,
    OUT PBOOLEAN EffectiveOnly,
    OUT PSECURITY_IMPERSONATION_LEVEL ImpersonationLevel
    );

// begin_ntifs

NTKERNELAPI
LARGE_INTEGER
PsGetProcessExitTime(
    VOID
    );

// end_ntifs
// end_ntosp

#if defined(_NTDDK_) || defined(_NTIFS_)

// begin_ntifs begin_ntosp

NTKERNELAPI
BOOLEAN
PsIsThreadTerminating(
    __in PETHREAD Thread
    );

// end_ntifs end_ntosp

#else

//
// BOOLEAN
// PsIsThreadTerminating(
//   IN PETHREAD Thread
//   )
//
//  Returns TRUE if thread is in the process of terminating.
//

#define PsIsThreadTerminating(T)                                            \
    (((T)->CrossThreadFlags&PS_CROSS_THREAD_FLAGS_TERMINATED) != 0)

#endif

extern BOOLEAN PsImageNotifyEnabled;

VOID
PsCallImageNotifyRoutines(
    IN PUNICODE_STRING FullImageName,
    IN HANDLE ProcessId,                // pid into which image is being mapped
    IN PIMAGE_INFO ImageInfo
    );

// begin_ntifs
// begin_ntosp

NTKERNELAPI
NTSTATUS
PsImpersonateClient(
    __inout PETHREAD Thread,
    __in PACCESS_TOKEN Token,
    __in BOOLEAN CopyOnOpen,
    __in BOOLEAN EffectiveOnly,
    __in SECURITY_IMPERSONATION_LEVEL ImpersonationLevel
    );

// end_ntosp

NTKERNELAPI
BOOLEAN
PsDisableImpersonation(
    __inout PETHREAD Thread,
    __inout PSE_IMPERSONATION_STATE ImpersonationState
    );

NTKERNELAPI
VOID
PsRestoreImpersonation(
    __inout PETHREAD Thread,
    __in PSE_IMPERSONATION_STATE ImpersonationState
    );

// end_ntifs

// begin_ntosp begin_ntifs

NTKERNELAPI
VOID
PsRevertToSelf(
    VOID
    );

// end_ntifs

NTKERNELAPI
VOID
PsRevertThreadToSelf(
    __inout PETHREAD Thread
    );

// end_ntosp

NTSTATUS
PsOpenTokenOfThread(
    IN HANDLE ThreadHandle,
    IN BOOLEAN OpenAsSelf,
    OUT PACCESS_TOKEN *Token,
    OUT PBOOLEAN CopyOnOpen,
    OUT PBOOLEAN EffectiveOnly,
    OUT PSECURITY_IMPERSONATION_LEVEL ImpersonationLevel
    );

NTSTATUS
PsOpenTokenOfProcess(
    IN HANDLE ProcessHandle,
    OUT PACCESS_TOKEN *Token
    );

NTSTATUS
PsOpenTokenOfJob(
    IN HANDLE JobHandle,
    OUT PACCESS_TOKEN * Token
    );

//
// Cid
//

NTKERNELAPI
NTSTATUS
PsLookupProcessThreadByCid(
    __in PCLIENT_ID Cid,
    __deref_opt_out PEPROCESS *Process,
    __deref_out PETHREAD *Thread
    );

// begin_ntosp

NTKERNELAPI
NTSTATUS
PsLookupProcessByProcessId(
    __in HANDLE ProcessId,
    __deref_out PEPROCESS *Process
    );

NTKERNELAPI
NTSTATUS
PsLookupThreadByThreadId(
    __in HANDLE ThreadId,
    __deref_out PETHREAD *Thread
    );

// begin_ntifs
//
// Quota Operations
//

NTKERNELAPI
VOID
PsChargePoolQuota(
    __in PEPROCESS Process,
    __in POOL_TYPE PoolType,
    __in ULONG_PTR Amount
    );

NTKERNELAPI
NTSTATUS
PsChargeProcessPoolQuota(
    __in PEPROCESS Process,
    __in POOL_TYPE PoolType,
    __in ULONG_PTR Amount
    );

NTKERNELAPI
VOID
PsReturnPoolQuota(
    __in PEPROCESS Process,
    __in POOL_TYPE PoolType,
    __in ULONG_PTR Amount
    );

// end_ntifs
// end_ntosp

NTSTATUS
PsChargeProcessQuota (
    IN PEPROCESS Process,
    IN PS_QUOTA_TYPE QuotaType,
    IN SIZE_T Amount
    );

VOID
PsReturnProcessQuota (
    IN PEPROCESS Process,
    IN PS_QUOTA_TYPE QuotaType,
    IN SIZE_T Amount
    );

NTKERNELAPI
NTSTATUS
PsChargeProcessNonPagedPoolQuota(
    __in PEPROCESS Process,
    __in SIZE_T Amount
    );

NTKERNELAPI
VOID
PsReturnProcessNonPagedPoolQuota(
    __in PEPROCESS Process,
    __in SIZE_T Amount
    );

NTKERNELAPI
NTSTATUS
PsChargeProcessPagedPoolQuota(
    __in PEPROCESS Process,
    __in SIZE_T Amount
    );

NTKERNELAPI
VOID
PsReturnProcessPagedPoolQuota(
    __in PEPROCESS Process,
    __in SIZE_T Amount
    );

NTSTATUS
PsChargeProcessPageFileQuota(
    IN PEPROCESS Process,
    IN SIZE_T Amount
    );

VOID
PsReturnProcessPageFileQuota(
    IN PEPROCESS Process,
    IN SIZE_T Amount
    );

//
// Context Management
//

VOID
PspContextToKframes(
    OUT PKTRAP_FRAME TrapFrame,
    OUT PKEXCEPTION_FRAME ExceptionFrame,
    IN PCONTEXT Context
    );

VOID
PspContextFromKframes(
    OUT PKTRAP_FRAME TrapFrame,
    OUT PKEXCEPTION_FRAME ExceptionFrame,
    IN PCONTEXT Context
    );

VOID
PsReturnSharedPoolQuota(
    IN PEPROCESS_QUOTA_BLOCK QuotaBlock,
    IN ULONG_PTR PagedAmount,
    IN ULONG_PTR NonPagedAmount
    );

PEPROCESS_QUOTA_BLOCK
PsChargeSharedPoolQuota(
    IN PEPROCESS Process,
    IN ULONG_PTR PagedAmount,
    IN ULONG_PTR NonPagedAmount
    );

//
// Exception Handling
//

BOOLEAN
PsForwardException (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN BOOLEAN DebugException,
    IN BOOLEAN SecondChance
    );

// begin_ntosp

typedef
NTSTATUS
(*PKWIN32_PROCESS_CALLOUT) (
    IN PEPROCESS Process,
    IN BOOLEAN Initialize
    );

typedef enum _PSW32JOBCALLOUTTYPE {
    PsW32JobCalloutSetInformation,
    PsW32JobCalloutAddProcess,
    PsW32JobCalloutTerminate
} PSW32JOBCALLOUTTYPE;

typedef struct _WIN32_JOBCALLOUT_PARAMETERS {
    PVOID Job;
    PSW32JOBCALLOUTTYPE CalloutType;
    IN PVOID Data;
} WIN32_JOBCALLOUT_PARAMETERS, *PKWIN32_JOBCALLOUT_PARAMETERS;

typedef
NTSTATUS
(*PKWIN32_JOB_CALLOUT) (
    IN PKWIN32_JOBCALLOUT_PARAMETERS Parm
     );

typedef enum _PSW32THREADCALLOUTTYPE {
    PsW32ThreadCalloutInitialize,
    PsW32ThreadCalloutExit
} PSW32THREADCALLOUTTYPE;

typedef
NTSTATUS
(*PKWIN32_THREAD_CALLOUT) (
    IN PETHREAD Thread,
    IN PSW32THREADCALLOUTTYPE CalloutType
    );

typedef enum _PSPOWEREVENTTYPE {
    PsW32FullWake,
    PsW32EventCode,
    PsW32PowerPolicyChanged,
    PsW32SystemPowerState,
    PsW32SystemTime,
    PsW32DisplayState,
    PsW32CapabilitiesChanged,
    PsW32SetStateFailed,
    PsW32GdiOff,
    PsW32GdiOn
} PSPOWEREVENTTYPE;

typedef struct _WIN32_POWEREVENT_PARAMETERS {
    PSPOWEREVENTTYPE EventNumber;
    ULONG_PTR Code;
} WIN32_POWEREVENT_PARAMETERS, *PKWIN32_POWEREVENT_PARAMETERS;

typedef enum _POWERSTATETASK {
    PowerState_BlockSessionSwitch,
    PowerState_Init,
    PowerState_QueryApps,
    PowerState_QueryFailed,
    PowerState_SuspendApps,
    PowerState_ShowUI,
    PowerState_NotifyWL,
    PowerState_ResumeApps,
    PowerState_UnBlockSessionSwitch

} POWERSTATETASK;

typedef struct _WIN32_POWERSTATE_PARAMETERS {
    BOOLEAN Promotion;
    POWER_ACTION SystemAction;
    SYSTEM_POWER_STATE MinSystemState;
    ULONG Flags;
    BOOLEAN fQueryDenied;
    POWERSTATETASK PowerStateTask;
} WIN32_POWERSTATE_PARAMETERS, *PKWIN32_POWERSTATE_PARAMETERS;

typedef
NTSTATUS
(*PKWIN32_POWEREVENT_CALLOUT) (
    IN PKWIN32_POWEREVENT_PARAMETERS Parm
    );

typedef
NTSTATUS
(*PKWIN32_POWERSTATE_CALLOUT) (
    IN PKWIN32_POWERSTATE_PARAMETERS Parm
    );

typedef
NTSTATUS
(*PKWIN32_OBJECT_CALLOUT) (
    IN PVOID Parm
    );

typedef struct _WIN32_CALLOUTS_FPNS {
    PKWIN32_PROCESS_CALLOUT ProcessCallout;
    PKWIN32_THREAD_CALLOUT ThreadCallout;
    PKWIN32_GLOBALATOMTABLE_CALLOUT GlobalAtomTableCallout;
    PKWIN32_POWEREVENT_CALLOUT PowerEventCallout;
    PKWIN32_POWERSTATE_CALLOUT PowerStateCallout;
    PKWIN32_JOB_CALLOUT JobCallout;
    PVOID BatchFlushRoutine;
    PKWIN32_OBJECT_CALLOUT DesktopOpenProcedure;
    PKWIN32_OBJECT_CALLOUT DesktopOkToCloseProcedure;
    PKWIN32_OBJECT_CALLOUT DesktopCloseProcedure;
    PKWIN32_OBJECT_CALLOUT DesktopDeleteProcedure;
    PKWIN32_OBJECT_CALLOUT WindowStationOkToCloseProcedure;
    PKWIN32_OBJECT_CALLOUT WindowStationCloseProcedure;
    PKWIN32_OBJECT_CALLOUT WindowStationDeleteProcedure;
    PKWIN32_OBJECT_CALLOUT WindowStationParseProcedure;
    PKWIN32_OBJECT_CALLOUT WindowStationOpenProcedure;
} WIN32_CALLOUTS_FPNS, *PKWIN32_CALLOUTS_FPNS;

NTKERNELAPI
VOID
PsEstablishWin32Callouts(
    __in PKWIN32_CALLOUTS_FPNS pWin32Callouts
    );

typedef enum _PSPROCESSPRIORITYMODE {
    PsProcessPriorityBackground,
    PsProcessPriorityForeground,
    PsProcessPrioritySpinning
} PSPROCESSPRIORITYMODE;

NTKERNELAPI
VOID
PsSetProcessPriorityByClass(
    __inout PEPROCESS Process,
    __in PSPROCESSPRIORITYMODE PriorityMode
    );

// end_ntosp

VOID
PsWatchWorkingSet(
    IN NTSTATUS Status,
    IN PVOID PcValue,
    IN PVOID Va
    );

// begin_ntddk begin_nthal begin_ntifs begin_ntosp

NTKERNELAPI
HANDLE
PsGetCurrentProcessId(
    VOID
    );

NTKERNELAPI
HANDLE
PsGetCurrentThreadId(
    VOID
    );

// end_ntosp

NTKERNELAPI
BOOLEAN
PsGetVersion(
    __out_opt PULONG MajorVersion,
    __out_opt PULONG MinorVersion,
    __out_opt PULONG BuildNumber,
    __out_opt PUNICODE_STRING CSDVersion
    );

// end_ntddk end_nthal end_ntifs

// begin_ntosp

NTKERNELAPI
ULONG
PsGetCurrentProcessSessionId(
    VOID
    );

NTKERNELAPI
PVOID
PsGetCurrentThreadStackLimit(
    VOID
    );

NTKERNELAPI
PVOID
PsGetCurrentThreadStackBase(
    VOID
    );

NTKERNELAPI
CCHAR
PsGetCurrentThreadPreviousMode(
    VOID
    );

NTKERNELAPI
PERESOURCE
PsGetJobLock(
    __in PEJOB Job
    );

NTKERNELAPI
ULONG
PsGetJobSessionId(
    __in PEJOB Job
    );

NTKERNELAPI
ULONG
PsGetJobUIRestrictionsClass(
    __in PEJOB Job
    );

NTKERNELAPI
LONGLONG
PsGetProcessCreateTimeQuadPart(
    __in PEPROCESS Process
    );

NTKERNELAPI
PVOID
PsGetProcessDebugPort(
    __in PEPROCESS Process
    );

NTKERNELAPI
BOOLEAN
PsIsProcessBeingDebugged(
    __in PEPROCESS Process
    );

NTKERNELAPI
BOOLEAN
PsGetProcessExitProcessCalled(
    __in PEPROCESS Process
    );

NTKERNELAPI
NTSTATUS
PsGetProcessExitStatus(
    __in PEPROCESS Process
    );

NTKERNELAPI
HANDLE
PsGetProcessId(
    __in PEPROCESS Process
    );

NTKERNELAPI
UCHAR *
PsGetProcessImageFileName(
    __in PEPROCESS Process
    );

#define PsGetCurrentProcessImageFileName() PsGetProcessImageFileName(PsGetCurrentProcess())

NTKERNELAPI
HANDLE
PsGetProcessInheritedFromUniqueProcessId(
    __in PEPROCESS Process
    );

NTKERNELAPI
PEJOB
PsGetProcessJob(
    __in PEPROCESS Process
    );

NTKERNELAPI
ULONG
PsGetProcessSessionId(
    __in PEPROCESS Process
    );

NTKERNELAPI
ULONG
PsGetProcessSessionIdEx(
    __in PEPROCESS Process
    );

NTKERNELAPI
PVOID
PsGetProcessSectionBaseAddress(
    __in PEPROCESS Process
    );

#define PsGetProcessPcb(Process) ((PKPROCESS)(Process))

NTKERNELAPI
PPEB
PsGetProcessPeb(
    __in PEPROCESS Process
    );

NTKERNELAPI
UCHAR
PsGetProcessPriorityClass(
    __in PEPROCESS Process
    );

NTKERNELAPI
HANDLE
PsGetProcessWin32WindowStation(
    __in PEPROCESS Process
    );

#define PsGetCurrentProcessWin32WindowStation() PsGetProcessWin32WindowStation(PsGetCurrentProcess())

NTKERNELAPI
PVOID
PsGetProcessWin32Process(
    __in PEPROCESS Process
    );

NTKERNELAPI
PVOID
PsGetCurrentProcessWin32Process(
    VOID
    );

#if defined(_WIN64)

NTKERNELAPI
PVOID
PsGetProcessWow64Process(
    __in PEPROCESS Process
    );

NTKERNELAPI
PVOID
PsGetCurrentProcessWow64Process(
    VOID
    );

#endif

NTKERNELAPI
HANDLE
PsGetThreadId(
    __in PETHREAD Thread
     );

NTKERNELAPI
CCHAR
PsGetThreadFreezeCount(
    __in PETHREAD Thread
    );

NTKERNELAPI
BOOLEAN
PsGetThreadHardErrorsAreDisabled(
    __in PETHREAD Thread
    );

NTKERNELAPI
PEPROCESS
PsGetThreadProcess(
    __in PETHREAD Thread
    );

NTKERNELAPI
PEPROCESS
PsGetCurrentThreadProcess(
    VOID
    );

NTKERNELAPI
HANDLE
PsGetThreadProcessId(
    __in PETHREAD Thread
    );

NTKERNELAPI
HANDLE
PsGetCurrentThreadProcessId(
    VOID
    );

NTKERNELAPI
ULONG
PsGetThreadSessionId(
    __in PETHREAD Thread
    );

#define  PsGetThreadTcb(Thread) ((PKTHREAD)(Thread))

NTKERNELAPI
PVOID
PsGetThreadTeb(
    __in PETHREAD Thread
    );

NTKERNELAPI
PVOID
PsGetCurrentThreadTeb(
    VOID
    );

NTKERNELAPI
PVOID
PsGetThreadWin32Thread(
    __in PETHREAD Thread
    );

NTKERNELAPI
PVOID
PsGetCurrentThreadWin32Thread(
    VOID
    );

NTKERNELAPI
PVOID
PsGetCurrentThreadWin32ThreadAndEnterCriticalRegion(
    __out PHANDLE ProcessId
    );

NTKERNELAPI                         //ntifs
BOOLEAN                             //ntifs
PsIsSystemThread(                   //ntifs
    __in PETHREAD Thread                 //ntifs
    );                              //ntifs

NTKERNELAPI
BOOLEAN
PsIsSystemProcess(
    __in PEPROCESS Process
     );   

NTKERNELAPI
BOOLEAN
PsIsThreadImpersonating (
    __in PETHREAD Thread
    );

NTSTATUS
PsReferenceProcessFilePointer (
    IN PEPROCESS Process,
    OUT PVOID *pFilePointer
    );

NTKERNELAPI
VOID
PsSetJobUIRestrictionsClass(
    __out PEJOB Job,
    __in ULONG UIRestrictionsClass
    );

NTKERNELAPI
VOID
PsSetProcessPriorityClass(
    __out PEPROCESS Process,
    __in UCHAR PriorityClass
    );

NTKERNELAPI
NTSTATUS
PsSetProcessWin32Process(
    __in PEPROCESS Process,
    __in PVOID Win32Process,
    __in PVOID PrevWin32Process
    );

NTKERNELAPI
VOID
PsSetProcessWindowStation(
    __out PEPROCESS Process,
    __in HANDLE Win32WindowStation
    );

NTKERNELAPI
VOID
PsSetThreadHardErrorsAreDisabled(
    __in PETHREAD Thread,
    __in BOOLEAN HardErrorsAreDisabled
    );

NTKERNELAPI
VOID
PsSetThreadWin32Thread(
    __inout PETHREAD Thread,
    __in PVOID Win32Thread,
    __in PVOID PrevWin32Thread
    );

NTKERNELAPI
PVOID
PsGetProcessSecurityPort(
    __in PEPROCESS Process
    );

NTKERNELAPI
NTSTATUS
PsSetProcessSecurityPort(
    __out PEPROCESS Process,
    __in PVOID Port
    );

typedef
NTSTATUS
(*PROCESS_ENUM_ROUTINE)(
    IN PEPROCESS Process,
    IN PVOID Context
    );

typedef
NTSTATUS
(*THREAD_ENUM_ROUTINE)(
    IN PEPROCESS Process,
    IN PETHREAD Thread,
    IN PVOID Context
    );

NTSTATUS
PsEnumProcesses (
    IN PROCESS_ENUM_ROUTINE CallBack,
    IN PVOID Context
    );

NTSTATUS
PsEnumProcessThreads (
    IN PEPROCESS Process,
    IN THREAD_ENUM_ROUTINE CallBack,
    IN PVOID Context
    );

PEPROCESS
PsGetNextProcess (
    IN PEPROCESS Process
    );

PETHREAD
PsGetNextProcessThread (
    IN PEPROCESS Process,
    IN PETHREAD Thread
    );

VOID
PsQuitNextProcess (
    IN PEPROCESS Process
    );

VOID
PsQuitNextProcessThread (
    IN PETHREAD Thread
    );

PEJOB
PsGetNextJob (
    IN PEJOB Job
    );

PEPROCESS
PsGetNextJobProcess (
    IN PEJOB Job,
    IN PEPROCESS Process
    );

VOID
PsQuitNextJob (
    IN PEJOB Job
    );

VOID
PsQuitNextJobProcess (
    IN PEPROCESS Process
    );

NTSTATUS
PsSuspendProcess (
    IN PEPROCESS Process
    );

NTSTATUS
PsResumeProcess (
    IN PEPROCESS Process
    );

NTSTATUS
PsTerminateProcess(
    IN PEPROCESS Process,
    IN NTSTATUS Status
    );

NTSTATUS
PsSuspendThread (
    IN PETHREAD Thread,
    OUT PULONG PreviousSuspendCount OPTIONAL
    );

NTSTATUS
PsResumeThread (
    IN PETHREAD Thread,
    OUT PULONG PreviousSuspendCount OPTIONAL
    );

#ifndef _WIN64

NTSTATUS
PsSetLdtEntries (
    IN ULONG Selector0,
    IN ULONG Entry0Low,
    IN ULONG Entry0Hi,
    IN ULONG Selector1,
    IN ULONG Entry1Low,
    IN ULONG Entry1Hi
    );

NTSTATUS
PsSetProcessLdtInfo (
    IN PPROCESS_LDT_INFORMATION LdtInformation,
    IN ULONG LdtInformationLength
    );

#endif

// end_ntosp

#endif // _PS_P
