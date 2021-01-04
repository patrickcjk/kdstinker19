#pragma once
#include "Types.h"
#include "DriverUtil.h"
#include <Ntstrsafe.h>

namespace Hooks
{
    inline void* orig_device_control = NULL;

    NTSTATUS gh_IoCreateDevice(
        PDRIVER_OBJECT  DriverObject,
        ULONG           DeviceExtensionSize,
        PUNICODE_STRING DeviceName,
        DEVICE_TYPE     DeviceType,
        ULONG           DeviceCharacteristics,
        BOOLEAN         Exclusive,
        PDEVICE_OBJECT* DeviceObject
    );

    PHYSICAL_ADDRESS gh_MmGetPhysicalAddress(
        PVOID BaseAddress
    );

    BOOLEAN gh_MmIsAddressValid(
        PVOID VirtualAddress
    );

    NTSTATUS gh_ZwDeviceIoControlFile(
        HANDLE           FileHandle,
        HANDLE           Event,
        PIO_APC_ROUTINE  ApcRoutine,
        PVOID            ApcContext,
        PIO_STATUS_BLOCK IoStatusBlock,
        ULONG            IoControlCode,
        PVOID            InputBuffer,
        ULONG            InputBufferLength,
        PVOID            OutputBuffer,
        ULONG            OutputBufferLength
    );

    VOID gh_RtlInitAnsiString(
        PANSI_STRING          DestinationString,
        PCSZ SourceString
    );

    VOID gh_RtlInitUnicodeString(
        PUNICODE_STRING         DestinationString,
        PCWSTR SourceString
    );

    VOID LoadImageNotifyRoutine(
        PUNICODE_STRING FullImageName,
        HANDLE ProcessId,
        PIMAGE_INFO ImageInfo
    );
}