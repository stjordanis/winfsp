/**
 * @file sys/cleanup.c
 *
 * @copyright 2015 Bill Zissimopoulos
 */

#include <sys/driver.h>

static NTSTATUS FspFsctlCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvrtCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
static NTSTATUS FspFsvolCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp);
FSP_DRIVER_DISPATCH FspCleanup;
FSP_IOCOMPLETION_DISPATCH FspCleanupComplete;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, FspFsctlCleanup)
#pragma alloc_text(PAGE, FspFsvrtCleanup)
#pragma alloc_text(PAGE, FspFsvolCleanup)
#pragma alloc_text(PAGE, FspCleanup)
#pragma alloc_text(PAGE, FspCleanupComplete)
#endif

static NTSTATUS FspFsctlCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    return Result;
}

static NTSTATUS FspFsvrtCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    NTSTATUS Result = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    return Result;
}

static NTSTATUS FspFsvolCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp, PIO_STACK_LOCATION IrpSp)
{
    PAGED_CODE();

    return STATUS_INVALID_DEVICE_REQUEST;
}

NTSTATUS FspCleanup(
    PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    FSP_ENTER_MJ(PAGED_CODE());

    ASSERT(IRP_MJ_CLEANUP == IrpSp->MajorFunction);

    switch (FspDeviceExtension(DeviceObject)->Kind)
    {
    case FspFsvolDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvolCleanup(DeviceObject, Irp, IrpSp));
    case FspFsvrtDeviceExtensionKind:
        FSP_RETURN(Result = FspFsvrtCleanup(DeviceObject, Irp, IrpSp));
    case FspFsctlDeviceExtensionKind:
        FSP_RETURN(Result = FspFsctlCleanup(DeviceObject, Irp, IrpSp));
    default:
        FSP_RETURN(Result = STATUS_INVALID_DEVICE_REQUEST);
    }

    FSP_LEAVE_MJ("FileObject=%p", IrpSp->FileObject);
}

VOID FspCleanupComplete(
    PIRP Irp, FSP_FSCTL_TRANSACT_RSP *Response)
{
    FSP_ENTER_IOC(PAGED_CODE());

    FSP_LEAVE_IOC("FileObject=%p", IrpSp->FileObject);
}
