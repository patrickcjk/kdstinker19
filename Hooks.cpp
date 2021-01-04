#include "Hooks.h"
#include <fltKernel.h>

#define INTEL_LAN_DRIVER_IOCTL      0x80862007
#define INTEL_LAN_COPY_CASE_NUMBER  0x33

namespace Hooks
{
    typedef struct _COPY_MEMORY_BUFFER_INFO
    {
        ULONG64 case_number;
        ULONG64 reserved;
        void* source;
        void* destination;
        ULONG64 length;
    }COPY_MEMORY_BUFFER_INFO, * PCOPY_MEMORY_BUFFER_INFO;

    typedef union _virt_addr_t
    {
        void* value;
        struct
        {
            ULONG64 offset : 12;
            ULONG64 pt_index : 9;
            ULONG64 pd_index : 9;
            ULONG64 pdpt_index : 9;
            ULONG64 pml4_index : 9;
            ULONG64 reserved : 16;
        };
    } virt_addr_t, * pvirt_addr_t;

    void mem_dump(void* base_addr, unsigned len)
    {
        static int dump_index = 0;

        if (!base_addr || !len)
            return;

        HANDLE             h_file;
        UNICODE_STRING     name;
        OBJECT_ATTRIBUTES  attr;
        IO_STATUS_BLOCK    status_block;
        LARGE_INTEGER      offset{ NULL };

        const wchar_t* format = L"\\DosDevices\\C:\\dump_%d.bin";

        wchar_t raidBuffer[0xFF];
        RtlStringCbPrintfW(raidBuffer, 0xFF * sizeof(wchar_t), format, dump_index);

        RtlInitUnicodeString(&name, raidBuffer);
        InitializeObjectAttributes(&attr, &name, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

        auto status = ZwCreateFile(
            &h_file,
            GENERIC_WRITE,
            &attr,
            &status_block,
            NULL,
            FILE_ATTRIBUTE_NORMAL,
            NULL,
            FILE_OVERWRITE_IF,
            FILE_SYNCHRONOUS_IO_NONALERT,
            NULL,
            NULL
        );

        status = ZwWriteFile(
            h_file,
            NULL,
            NULL,
            NULL,
            &status_block,
            base_addr,
            len,
            &offset,
            NULL
        );

        ZwClose(h_file);
        dump_index++;
    }

    NTSTATUS device_control(PDEVICE_OBJECT device_obj, PIRP irp)
    {
        UNREFERENCED_PARAMETER(device_obj);
        PIO_STACK_LOCATION stack_location = IoGetCurrentIrpStackLocation(irp);

        //DBG_PRINT("device io control called with code 0x%x", stack_location->Parameters.DeviceIoControl.IoControlCode);

        if (stack_location->Parameters.DeviceIoControl.IoControlCode == INTEL_LAN_DRIVER_IOCTL && stack_location->Parameters.DeviceIoControl.InputBufferLength)
        {
            PCOPY_MEMORY_BUFFER_INFO copy_memory_buffer = reinterpret_cast<PCOPY_MEMORY_BUFFER_INFO>(stack_location->Parameters.SetFile.DeleteHandle);
            
            // if case is memmove and the destination is in the kernel (pml4 index is > 255)
            if (copy_memory_buffer->case_number == INTEL_LAN_COPY_CASE_NUMBER)
            {
                if (virt_addr_t{ copy_memory_buffer->destination }.pml4_index > 255)
                {
                    // there are a few writes of size 0xC (inline jump code) we can skip those.
                    if (copy_memory_buffer->length > 0x100)
                    {
                        DBG_PRINT("=============== Dumping Memory ==============");
                       
                        DBG_PRINT("Copying memory from 0x%p to 0x%p of size 0x%x",
                                copy_memory_buffer->source,
                                copy_memory_buffer->destination,
                                copy_memory_buffer->length
                        );
                        
                        // dump memory from inside of the calling process to disk.
                        mem_dump(copy_memory_buffer->source, copy_memory_buffer->length);
                    }
                }
            }
        }


        return reinterpret_cast<decltype(&device_control)>(orig_device_control)(device_obj, irp);
    }

    using ptIoCreateDeviceSecure = NTSTATUS(__fastcall*)(PDRIVER_OBJECT, ULONG, PUNICODE_STRING , DEVICE_TYPE, ULONG, BOOLEAN, PCUNICODE_STRING, LPCGUID, PDEVICE_OBJECT*);

    NTSTATUS hkIoCreateDeviceSecure(
        PDRIVER_OBJECT   DriverObject,
        ULONG            DeviceExtensionSize,
        PUNICODE_STRING  DeviceName,
        DEVICE_TYPE      DeviceType,
        ULONG            DeviceCharacteristics,
        BOOLEAN          Exclusive,
        PCUNICODE_STRING DefaultSDDLString,
        LPCGUID          DeviceClassGuid,
        PDEVICE_OBJECT*  DeviceObject
    )
    {

        DBG_PRINT("> hkIoCreateDeviceSecure called! (DriverObject: 0x%p)", DriverObject);
        DBG_PRINT(">     - IRP_MJ_DEVICE_CONTROL: 0x%p", DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]);
        DBG_PRINT(">     - IRP_MJ_READ: 0x%p", DriverObject->MajorFunction[IRP_MJ_READ]);
        DBG_PRINT(">     - IRP_MJ_WRITE: 0x%p", DriverObject->MajorFunction[IRP_MJ_WRITE]);

        // swap ioctl pointer
        orig_device_control = DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL];
        DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = &device_control;

        DBG_PRINT("     - swapped ioctl function from 0x%p to 0x%p", orig_device_control, &device_control);




        //call original function

        UNICODE_STRING SystemRoutineName;
        RtlInitUnicodeString(&SystemRoutineName, L"IoCreateDeviceSecure");

        auto original_function = reinterpret_cast<ptIoCreateDeviceSecure>(MmGetSystemRoutineAddress(&SystemRoutineName));
        if (!original_function)
        { 
            DBG_PRINT("IoCreateDeviceSecure function not found!");
            return STATUS_UNSUCCESSFUL;
        }

        auto result = original_function(DriverObject, DeviceExtensionSize, DeviceName, DeviceType, DeviceCharacteristics, Exclusive, DefaultSDDLString, DeviceClassGuid, DeviceObject);
        DBG_PRINT("IoCreateDeviceSecure returned 0x%x", result);
        return result;
    }

    PIMAGE_FILE_HEADER get_file_header(void* base_addr)
    {
        if (!base_addr || *(short*)base_addr != 0x5A4D)
            return NULL;

        PIMAGE_DOS_HEADER dos_headers = reinterpret_cast<PIMAGE_DOS_HEADER>(base_addr);
        PIMAGE_NT_HEADERS nt_headers = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<DWORD_PTR>(base_addr) + dos_headers->e_lfanew);
        return &nt_headers->FileHeader;
    }

    PVOID gh_MmGetSystemRoutineAddress(PUNICODE_STRING SystemRoutineName)
    {
        DBG_PRINT("MmGetSystemRoutineAddress: %ws", SystemRoutineName->Buffer);

        if (wcsstr(SystemRoutineName->Buffer, L"IoCreateDeviceSecure"))
        {
            DBG_PRINT("Hooked %ws", SystemRoutineName->Buffer);
            return &hkIoCreateDeviceSecure;
        }

        return MmGetSystemRoutineAddress(SystemRoutineName);
    }

    VOID LoadImageNotifyRoutine(PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO image_info)
    {
        if (get_file_header(image_info->ImageBase)->TimeDateStamp == 0x5CC04DA0)
        {
            DBG_PRINT("> intel driver was loaded from %ws", FullImageName->Buffer);

            DriverUtil::IATHook(image_info->ImageBase, "MmGetSystemRoutineAddress", &gh_MmGetSystemRoutineAddress);

        }
    }
}