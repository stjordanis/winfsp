/**
 * @file sys/driver.h
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#ifndef WINFSP_SYS_DRIVER_H_INCLUDED
#define WINFSP_SYS_DRIVER_H_INCLUDED

#define WINFSP_SYS_INTERNAL
#include <ntifs.h>
#include <ntstrsafe.h>
#include <wdmsec.h>
#include <winfsp/fsctl.h>

#define DRIVER_NAME                     "WinFsp"

/* IoCreateDeviceSecure default SDDL's */
#define FSP_FSCTL_DEVICE_SDDL           "D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;WD)"
    /* System:GENERIC_ALL, Administrators:GENERIC_ALL, World:GENERIC_READ */
#define FSP_FSVRT_DEVICE_SDDL           "D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;WD)"
    /* System:GENERIC_ALL, Administrators:GENERIC_ALL, World:GENERIC_READ */

/* DEBUGLOG */
#if DBG
#define DEBUGLOG(fmt, ...)              \
    DbgPrint("[%d] " DRIVER_NAME "!" __FUNCTION__ ": " fmt "\n", KeGetCurrentIrql(), __VA_ARGS__)
#else
#define DEBUGLOG(fmt, ...)              ((void)0)
#endif

/* FSP_ENTER/FSP_LEAVE */
#if DBG
#define FSP_DEBUGLOG_(fmt, rfmt, ...)   \
    DbgPrint(AbnormalTermination() ?    \
        "[%d] " DRIVER_NAME "!" __FUNCTION__ "(" fmt ") = *AbnormalTermination*\n" :\
        "[%d] " DRIVER_NAME "!" __FUNCTION__ "(" fmt ")" rfmt "\n",\
        KeGetCurrentIrql(), __VA_ARGS__)
#define FSP_DEBUGLOG_NOCRIT_(fmt, rfmt, ...)\
    DbgPrint(                           \
        "[%d] " DRIVER_NAME "!" __FUNCTION__ "(" fmt ")" rfmt "\n",\
        KeGetCurrentIrql(), __VA_ARGS__)
#define FSP_DEBUGBRK_()                 \
    do                                  \
    {                                   \
        static int fsp_bp_enabled = 1;  \
        if (fsp_bp_enabled && HasDbgBreakPoint(__FUNCTION__))\
            try { DbgBreakPoint(); } except (EXCEPTION_EXECUTE_HANDLER) {}\
    } while (0,0)
#else
#define FSP_DEBUGLOG_(fmt, rfmt, ...)   ((void)0)
#define FSP_DEBUGLOG_NOCRIT_(fmt, rfmt, ...)((void)0)
#define FSP_DEBUGBRK_()                 ((void)0)
#endif
#define FSP_ENTER_(...)                 \
    FSP_DEBUGBRK_();                    \
    FsRtlEnterFileSystem();             \
    try                                 \
    {                                   \
        __VA_ARGS__
#define FSP_LEAVE_(...)                 \
    goto fsp_leave_label;               \
    fsp_leave_label:;                   \
    }                                   \
    finally                             \
    {                                   \
        __VA_ARGS__;                    \
        FsRtlExitFileSystem();          \
    }
#define FSP_ENTER_NOCRIT_(...)          \
    FSP_DEBUGBRK_();                    \
    {                                   \
        __VA_ARGS__
#define FSP_LEAVE_NOCRIT_(...)          \
    goto fsp_leave_label;               \
    fsp_leave_label:;                   \
        __VA_ARGS__;                    \
    }
#define FSP_ENTER(...)                  \
    NTSTATUS Result = STATUS_SUCCESS; FSP_ENTER_(__VA_ARGS__)
#define FSP_LEAVE(fmt, ...)             \
    FSP_LEAVE_(FSP_DEBUGLOG_(fmt, " = %s", __VA_ARGS__, NtStatusSym(Result))); return Result
#define FSP_ENTER_MJ(...)               \
    NTSTATUS Result = STATUS_SUCCESS;   \
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp);\
    BOOLEAN fsp_device_release = FALSE; \
    FSP_ENTER_(__VA_ARGS__);            \
    do                                  \
    {                                   \
        if (!FspDeviceRetain(IrpSp->DeviceObject))\
        {                               \
            Result = STATUS_CANCELLED;  \
            goto fsp_leave_label;       \
        }                               \
        fsp_device_release = TRUE;      \
    } while (0,0)
#define FSP_LEAVE_MJ(fmt, ...)          \
    FSP_LEAVE_(                         \
        FSP_DEBUGLOG_("%p, %s%c, %s%s, " fmt, " = %s[%lld]",\
            Irp,                        \
            (const char *)&FspDeviceExtension(IrpSp->DeviceObject)->Kind,\
            Irp->RequestorMode == KernelMode ? 'K' : 'U',\
            IrpMajorFunctionSym(IrpSp->MajorFunction),\
            IrpMinorFunctionSym(IrpSp->MajorFunction, IrpSp->MinorFunction),\
            __VA_ARGS__,                \
            NtStatusSym(Result),        \
            (LONGLONG)Irp->IoStatus.Information);\
        if (STATUS_PENDING == Result)   \
        {                               \
            if (0 == (IrpSp->Control & SL_PENDING_RETURNED))\
            {                           \
                /* if the IRP has not been marked pending already */\
                ASSERT(FspFsvolDeviceExtensionKind == FspDeviceExtension(DeviceObject)->Kind);\
                FSP_FSVOL_DEVICE_EXTENSION *FsvolDeviceExtension =\
                    FspFsvolDeviceExtension(DeviceObject);\
                FSP_FSVRT_DEVICE_EXTENSION *FsvrtDeviceExtension =\
                    FspFsvrtDeviceExtension(FsvolDeviceExtension->FsvrtDeviceObject);\
                if (!FspIoqPostIrp(&FsvrtDeviceExtension->Ioq, Irp))\
                {                       \
                    /* this can only happen if the Ioq was stopped */\
                    ASSERT(FspIoqStopped(&FsvrtDeviceExtension->Ioq));\
                    FspIopCompleteRequest(Irp, Result = STATUS_CANCELLED);\
                }                       \
            }                           \
        }                               \
        else                            \
            FspIopCompleteRequestEx(Irp, Result, fsp_device_release);\
    );                                  \
    return Result
#define FSP_ENTER_IOP(...)              \
    NTSTATUS Result = STATUS_SUCCESS;   \
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp); (VOID)IrpSp;\
    FSP_ENTER_NOCRIT_(__VA_ARGS__)
#define FSP_LEAVE_IOP()                 \
    FSP_LEAVE_NOCRIT_(); return Result
#define FSP_ENTER_IOC(...)              \
    NTSTATUS Result = STATUS_SUCCESS;   \
    PIO_STACK_LOCATION IrpSp = IoGetCurrentIrpStackLocation(Irp); (VOID)IrpSp;\
    FSP_ENTER_NOCRIT_(__VA_ARGS__)
#define FSP_LEAVE_IOC(fmt, ...)         \
    FSP_LEAVE_NOCRIT_(                  \
        FSP_DEBUGLOG_NOCRIT_("%p, %s%c, %s%s, " fmt, " = %s[%lld]",\
            Irp,                        \
            (const char *)&FspDeviceExtension(IrpSp->DeviceObject)->Kind,\
            Irp->RequestorMode == KernelMode ? 'K' : 'U',\
            IrpMajorFunctionSym(IrpSp->MajorFunction),\
            IrpMinorFunctionSym(IrpSp->MajorFunction, IrpSp->MinorFunction),\
            __VA_ARGS__,                \
            NtStatusSym(Result),        \
            (LONGLONG)Irp->IoStatus.Information);\
        FspIopCompleteRequest(Irp, Result);\
    )
#define FSP_ENTER_BOOL(...)             \
    BOOLEAN Result = TRUE; FSP_ENTER_(__VA_ARGS__)
#define FSP_LEAVE_BOOL(fmt, ...)        \
    FSP_LEAVE_(FSP_DEBUGLOG_(fmt, " = %s", __VA_ARGS__, Result ? "TRUE" : "FALSE")); return Result
#define FSP_ENTER_VOID(...)             \
    FSP_ENTER_(__VA_ARGS__)
#define FSP_LEAVE_VOID(fmt, ...)        \
    FSP_LEAVE_(FSP_DEBUGLOG_(fmt, "", __VA_ARGS__))
#define FSP_RETURN(...)                 \
    do                                  \
    {                                   \
        __VA_ARGS__;                    \
        goto fsp_leave_label;           \
    } while (0,0)

/* misc macros */
#define FSP_TAG                         ' psF'
#define FSP_IO_INCREMENT                IO_NETWORK_INCREMENT

/* disable warnings */
#pragma warning(disable:4100)           /* unreferenced formal parameter */
#pragma warning(disable:4200)           /* zero-sized array in struct/union */

/* driver major functions */
_Function_class_(DRIVER_DISPATCH)
_IRQL_requires_max_(APC_LEVEL)
    /* see https://msdn.microsoft.com/en-us/library/windows/hardware/ff540124(v=vs.85).aspx */
_IRQL_requires_same_
typedef NTSTATUS FSP_DRIVER_DISPATCH(
    _In_ struct _DEVICE_OBJECT *DeviceObject, _Inout_ struct _IRP *Irp);
_Dispatch_type_(IRP_MJ_CLEANUP)         FSP_DRIVER_DISPATCH FspCleanup;
_Dispatch_type_(IRP_MJ_CLOSE)           FSP_DRIVER_DISPATCH FspClose;
_Dispatch_type_(IRP_MJ_CREATE)          FSP_DRIVER_DISPATCH FspCreate;
_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)  FSP_DRIVER_DISPATCH FspDeviceControl;
_Dispatch_type_(IRP_MJ_DIRECTORY_CONTROL) FSP_DRIVER_DISPATCH FspDirectoryControl;
_Dispatch_type_(IRP_MJ_FILE_SYSTEM_CONTROL) FSP_DRIVER_DISPATCH FspFileSystemControl;
_Dispatch_type_(IRP_MJ_FLUSH_BUFFERS)   FSP_DRIVER_DISPATCH FspFlushBuffers;
_Dispatch_type_(IRP_MJ_LOCK_CONTROL)    FSP_DRIVER_DISPATCH FspLockControl;
_Dispatch_type_(IRP_MJ_QUERY_EA)        FSP_DRIVER_DISPATCH FspQueryEa;
_Dispatch_type_(IRP_MJ_QUERY_INFORMATION) FSP_DRIVER_DISPATCH FspQueryInformation;
_Dispatch_type_(IRP_MJ_QUERY_SECURITY)  FSP_DRIVER_DISPATCH FspQuerySecurity;
_Dispatch_type_(IRP_MJ_QUERY_VOLUME_INFORMATION) FSP_DRIVER_DISPATCH FspQueryVolumeInformation;
_Dispatch_type_(IRP_MJ_READ)            FSP_DRIVER_DISPATCH FspRead;
_Dispatch_type_(IRP_MJ_SET_EA)          FSP_DRIVER_DISPATCH FspSetEa;
_Dispatch_type_(IRP_MJ_SET_INFORMATION) FSP_DRIVER_DISPATCH FspSetInformation;
_Dispatch_type_(IRP_MJ_SET_SECURITY)    FSP_DRIVER_DISPATCH FspSetSecurity;
_Dispatch_type_(IRP_MJ_SET_VOLUME_INFORMATION) FSP_DRIVER_DISPATCH FspSetVolumeInformation;
_Dispatch_type_(IRP_MJ_SHUTDOWN)        FSP_DRIVER_DISPATCH FspShutdown;
_Dispatch_type_(IRP_MJ_WRITE)           FSP_DRIVER_DISPATCH FspWrite;

/* I/O processing functions */
_IRQL_requires_max_(APC_LEVEL)
_IRQL_requires_same_
typedef NTSTATUS FSP_IOPREP_DISPATCH(
    _Inout_ PIRP Irp, _Inout_ FSP_FSCTL_TRANSACT_REQ *Request);
_IRQL_requires_max_(APC_LEVEL)
_IRQL_requires_same_
typedef VOID FSP_IOCMPL_DISPATCH(
    _Inout_ PIRP Irp, _In_ const FSP_FSCTL_TRANSACT_RSP *Response);
FSP_IOPREP_DISPATCH FspFsvolCreatePrepare;
FSP_IOCMPL_DISPATCH FspCleanupComplete;
FSP_IOCMPL_DISPATCH FspCloseComplete;
FSP_IOCMPL_DISPATCH FspFsvolCreateComplete;
FSP_IOCMPL_DISPATCH FspDeviceControlComplete;
FSP_IOCMPL_DISPATCH FspDirectoryControlComplete;
FSP_IOCMPL_DISPATCH FspFileSystemControlComplete;
FSP_IOCMPL_DISPATCH FspFlushBuffersComplete;
FSP_IOCMPL_DISPATCH FspLockControlComplete;
FSP_IOCMPL_DISPATCH FspQueryEaComplete;
FSP_IOCMPL_DISPATCH FspQueryInformationComplete;
FSP_IOCMPL_DISPATCH FspQuerySecurityComplete;
FSP_IOCMPL_DISPATCH FspQueryVolumeInformationComplete;
FSP_IOCMPL_DISPATCH FspReadComplete;
FSP_IOCMPL_DISPATCH FspSetEaComplete;
FSP_IOCMPL_DISPATCH FspSetInformationComplete;
FSP_IOCMPL_DISPATCH FspSetSecurityComplete;
FSP_IOCMPL_DISPATCH FspSetVolumeInformationComplete;
FSP_IOCMPL_DISPATCH FspShutdownComplete;
FSP_IOCMPL_DISPATCH FspWriteComplete;

/* fast I/O */
FAST_IO_CHECK_IF_POSSIBLE FspFastIoCheckIfPossible;

/* resource acquisition */
FAST_IO_ACQUIRE_FILE FspAcquireFileForNtCreateSection;
FAST_IO_RELEASE_FILE FspReleaseFileForNtCreateSection;
FAST_IO_ACQUIRE_FOR_MOD_WRITE FspAcquireForModWrite;
FAST_IO_RELEASE_FOR_MOD_WRITE FspReleaseForModWrite;
FAST_IO_ACQUIRE_FOR_CCFLUSH FspAcquireForCcFlush;
FAST_IO_RELEASE_FOR_CCFLUSH FspReleaseForCcFlush;

/* I/O queue */
typedef struct
{
    KSPIN_LOCK SpinLock;
    BOOLEAN Stopped;
    KEVENT PendingIrpEvent;
    LIST_ENTRY PendingIrpList, ProcessIrpList;
    IO_CSQ PendingIoCsq, ProcessIoCsq;
} FSP_IOQ;
VOID FspIoqInitialize(FSP_IOQ *Ioq);
VOID FspIoqStop(FSP_IOQ *Ioq);
BOOLEAN FspIoqStopped(FSP_IOQ *Ioq);
BOOLEAN FspIoqPostIrp(FSP_IOQ *Ioq, PIRP Irp);
PIRP FspIoqNextPendingIrp(FSP_IOQ *Ioq, ULONG millis);
BOOLEAN FspIoqStartProcessingIrp(FSP_IOQ *Ioq, PIRP Irp);
PIRP FspIoqEndProcessingIrp(FSP_IOQ *Ioq, UINT_PTR IrpHint);

/* I/O processing */
NTSTATUS FspIopCreateRequest(
    PIRP Irp, PUNICODE_STRING FileName, ULONG ExtraSize, FSP_FSCTL_TRANSACT_REQ **PRequest);
VOID FspIopCompleteRequestEx(PIRP Irp, NTSTATUS Result, BOOLEAN DeviceRelease);
static inline VOID FspIopCompleteRequest(PIRP Irp, NTSTATUS Result)
{
    FspIopCompleteRequestEx(Irp, Result, TRUE);
}
NTSTATUS FspIopDispatchPrepare(PIRP Irp, FSP_FSCTL_TRANSACT_REQ *Request);
VOID FspIopDispatchComplete(PIRP Irp, const FSP_FSCTL_TRANSACT_RSP *Response);

/* device management */
enum
{
    FspFsctlDeviceExtensionKind = '\0ltC',  /* file system control device (e.g. \Device\WinFsp.Disk) */
    FspFsvrtDeviceExtensionKind = '\0trV',  /* virtual volume device (e.g. \Device\Volume{GUID}) */
    FspFsvolDeviceExtensionKind = '\0loV',  /* file system volume device (unnamed) */
};
typedef struct
{
    KSPIN_LOCK SpinLock;
    LONG RefCount;
    ERESOURCE Resource;
    UINT32 Kind;
} FSP_DEVICE_EXTENSION;
typedef struct
{
    FSP_DEVICE_EXTENSION Base;
    ULONG FsvrtDeviceObjectCount;
} FSP_FSCTL_DEVICE_EXTENSION;
typedef struct
{
    FSP_DEVICE_EXTENSION Base;
    PDEVICE_OBJECT FsctlDeviceObject;
    PDEVICE_OBJECT FsvolDeviceObject;
    FSP_FSCTL_VOLUME_PARAMS VolumeParams;
    FSP_IOQ Ioq;
    PVPB SwapVpb;
    BOOLEAN Deleted;
    FSP_FSCTL_DECLSPEC_ALIGN UINT8 SecurityDescriptorBuf[];
} FSP_FSVRT_DEVICE_EXTENSION;
typedef struct
{
    FSP_DEVICE_EXTENSION Base;
    PDEVICE_OBJECT FsvrtDeviceObject;
    FAST_MUTEX GenericTableFastMutex;
    RTL_AVL_TABLE GenericTable;
} FSP_FSVOL_DEVICE_EXTENSION;
static inline
FSP_DEVICE_EXTENSION *FspDeviceExtension(PDEVICE_OBJECT DeviceObject)
{
    return DeviceObject->DeviceExtension;
}
static inline
FSP_FSCTL_DEVICE_EXTENSION *FspFsctlDeviceExtension(PDEVICE_OBJECT DeviceObject)
{
    ASSERT(FspFsctlDeviceExtensionKind == ((FSP_DEVICE_EXTENSION *)DeviceObject->DeviceExtension)->Kind);
    return DeviceObject->DeviceExtension;
}
static inline
FSP_FSVRT_DEVICE_EXTENSION *FspFsvrtDeviceExtension(PDEVICE_OBJECT DeviceObject)
{
    ASSERT(FspFsvrtDeviceExtensionKind == ((FSP_DEVICE_EXTENSION *)DeviceObject->DeviceExtension)->Kind);
    return DeviceObject->DeviceExtension;
}
static inline
FSP_FSVOL_DEVICE_EXTENSION *FspFsvolDeviceExtension(PDEVICE_OBJECT DeviceObject)
{
    ASSERT(FspFsvolDeviceExtensionKind == ((FSP_DEVICE_EXTENSION *)DeviceObject->DeviceExtension)->Kind);
    return DeviceObject->DeviceExtension;
}
NTSTATUS FspDeviceCreateSecure(UINT32 Kind, ULONG ExtraSize,
    PUNICODE_STRING DeviceName, DEVICE_TYPE DeviceType,
    PUNICODE_STRING DeviceSddl, LPCGUID DeviceClassGuid,
    PDEVICE_OBJECT *PDeviceObject);
NTSTATUS FspDeviceCreate(UINT32 Kind, ULONG ExtraSize,
    DEVICE_TYPE DeviceType,
    PDEVICE_OBJECT *PDeviceObject);
VOID FspDeviceDelete(PDEVICE_OBJECT DeviceObject);
BOOLEAN FspDeviceRetain(PDEVICE_OBJECT DeviceObject);
VOID FspDeviceRelease(PDEVICE_OBJECT DeviceObject);
VOID FspFsctlDeviceVolumeCreated(PDEVICE_OBJECT DeviceObject);
VOID FspFsctlDeviceVolumeDeleted(PDEVICE_OBJECT DeviceObject);
PVOID FspFsvolDeviceLookupContext(PDEVICE_OBJECT DeviceObject, UINT64 Identifier);
PVOID FspFsvolDeviceInsertContext(PDEVICE_OBJECT DeviceObject, UINT64 Identifier, PVOID Context,
    PBOOLEAN PInserted);
VOID FspFsvolDeviceDeleteContext(PDEVICE_OBJECT DeviceObject, UINT64 Identifier,
    PBOOLEAN PDeleted);
NTSTATUS FspDeviceCopyList(
    PDEVICE_OBJECT **PDeviceObjects, PULONG PDeviceObjectCount);
VOID FspDeviceDeleteList(
    PDEVICE_OBJECT *DeviceObjects, ULONG DeviceObjectCount);
VOID FspDeviceDeleteAll(VOID);

/* file objects */
typedef struct
{
    ERESOURCE Resource;
    ERESOURCE PagingIoResource;
    FAST_MUTEX HeaderFastMutex;
} FSP_FILE_CONTEXT_NONPAGED;
typedef struct
{
    FSRTL_ADVANCED_FCB_HEADER Header;
    FSP_FILE_CONTEXT_NONPAGED *NonPaged;
    UNICODE_STRING FileName;
    WCHAR FileNameBuf[];
} FSP_FILE_CONTEXT;
NTSTATUS FspFileContextCreate(ULONG ExtraSize, FSP_FILE_CONTEXT **PContext);
VOID FspFileContextDelete(FSP_FILE_CONTEXT *Context);

/* misc */
NTSTATUS FspCreateGuid(GUID *Guid);
BOOLEAN FspValidRelativeSecurityDescriptor(
    PSECURITY_DESCRIPTOR SecurityDescriptor, ULONG SecurityDescriptorLength,
    SECURITY_INFORMATION RequiredInformation);
NTSTATUS FspSecuritySubjectContextAccessCheck(
    PSECURITY_DESCRIPTOR SecurityDescriptor, ACCESS_MASK DesiredAccess, KPROCESSOR_MODE AccessMode);

/* debug */
#if DBG
BOOLEAN HasDbgBreakPoint(const char *Function);
const char *NtStatusSym(NTSTATUS Status);
const char *IrpMajorFunctionSym(UCHAR MajorFunction);
const char *IrpMinorFunctionSym(UCHAR MajorFunction, UCHAR MinorFunction);
const char *IoctlCodeSym(ULONG ControlCode);
#endif

/* extern */
extern PDRIVER_OBJECT FspDriverObject;
extern PDEVICE_OBJECT FspFsctlDiskDeviceObject;
extern PDEVICE_OBJECT FspFsctlNetDeviceObject;
extern FSP_IOPREP_DISPATCH *FspIopPrepareFunction[];
extern FSP_IOCMPL_DISPATCH *FspIopCompleteFunction[];

#endif
