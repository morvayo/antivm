#define VM_REGISTERS_HPP
#define VM_KERNEL_HPP
#define VM_SYSCALL_HPP
static int vm_debug_registers_error(void) { return 0; }
static int vm_kernel_modules_error(void) { return 0; }
#include <windows.h>
#include <intrin.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <winternl.h>
#include "state.hpp"

#define VM_TRAP_HPP
static vm_state fixture_trap = vm_physical;
static vm_state vm_trap_check(void) { return fixture_trap; }

static int hypervisor;
static bool root_partition;
static bool short_cpu_range;
static bool sandbox_module;
static int enumeration_mode;
static int firmware_mode;
static int smbios_mode = -1;
static int acpi_mode;
static int firmware_calls;
static bool debugger;
static bool remote_debugger;
static bool remote_query_failed;
static bool missing_query;
static int process_query_mode;
static uint8_t peb_data[3];
static bool missing_peb;
static bool missing_system_query;
static bool kernel_query_failed;
static bool kernel_enabled;
static bool kernel_not_present;
static int failures;
static int checks;
static bool missing_firmware_api;
static int memory_mode;
static int memory_closes;
static int memory_unmaps;
static uint8_t memory_data[0x10000];

static void fixture_cpuid(int output[4], int leaf) {
    memset(output, 0, sizeof(int) * 4);
    if (leaf == 1 && hypervisor != 0) output[2] = (int) 0x80000000u;
    if (leaf == 0x40000000) {
        output[0] = short_cpu_range ? 0x40000000 : 0x40000003;
        if (hypervisor == 1) {
            output[1] = 0x7263694d;
            output[2] = 0x666f736f;
            output[3] = 0x76482074;
        }
    }
    if (leaf == 0x40000003) output[1] = root_partition ? 1 : 0;
}

static UINT WINAPI fixture_enum(DWORD provider, PVOID buffer, DWORD capacity) {
    if (provider == 0x41435049u && acpi_mode == 4) { SetLastError(ERROR_ACCESS_DENIED); return 0; }
    if (enumeration_mode == 1 || enumeration_mode == 2) {
        SetLastError(enumeration_mode == 1 ? ERROR_ACCESS_DENIED : ERROR_INVALID_FUNCTION);
        return 0;
    }
    if (enumeration_mode == 4) return 3;
    if (buffer == NULL) return sizeof(DWORD);
    if (enumeration_mode == 3) return 0;
    if (capacity >= sizeof(DWORD)) *(DWORD *) buffer = 0;
    return sizeof(DWORD);
}

static UINT WINAPI fixture_firmware(DWORD provider, DWORD, PVOID output, DWORD capacity) {
    int mode = provider == 0x52534d42u && smbios_mode >= 0 ? smbios_mode : firmware_mode;
    if (provider == 0x41435049u) mode = acpi_mode == 3 ? 2 : 0;
    firmware_calls++;
    if (mode == 3) return 0;
    if (mode == 4) return 32u * 1024u * 1024u;
    uint8_t payload[256] = {};
    UINT size = 0;
    if (provider == 0x52534d42u) {
        payload[1] = 3;
        payload[2] = 6;
        payload[8] = 1;
        payload[9] = 8;
        payload[12] = 1;
        payload[13] = 2;
        payload[14] = 3;
        payload[15] = 4;
        size = 16;
        const char *strings[] = {mode == 1 ? "VMware, Inc." : "Dell Inc", "Desktop", "1.0", "serial"};
        for (const char *text : strings) {
            UINT length = (UINT) strlen(text) + 1;
            memcpy(payload + size, text, length);
            size += length;
        }
        payload[size++] = 0;
        payload[size++] = 127;
        payload[size++] = 4;
        payload[size++] = 0;
        payload[size++] = 0;
        payload[size++] = 0;
        payload[size++] = 0;
        UINT length = mode == 5 ? size : size - 8;
        memcpy(payload + 4, &length, 4);
    } else if (provider == 0x41435049u) {
        size = 36;
        memcpy(payload, "DSDT", 4);
        memcpy(payload + 4, &size, 4);
        payload[8] = 2;
        memcpy(payload + 10, "DELL  ", 6);
        memcpy(payload + 16, "DESKTOP ", 8);
        if (acpi_mode == 1) { memcpy(payload + 10, "VBOX  ", 6); memcpy(payload + 16, "VBOXFACP", 8); }
        uint8_t sum = 0;
        for (UINT i = 0; i < size; i++) sum += payload[i];
        payload[9] = (uint8_t) -sum;
        if (acpi_mode == 2) payload[9]++;
    } else {
        const char *text = mode == 1 ? "VMware" : "Dell Inc";
        size = (UINT) strlen(text);
        memcpy(payload, text, size);
    }
    if (output == NULL) return size;
    if (mode == 2) return size - 1;
    if (capacity >= size) memcpy(output, payload, size);
    return size;
}
static HMODULE WINAPI fixture_module(LPCSTR name) {
    if (strcmp(name, "sbiedll.dll") == 0) return sandbox_module ? (HMODULE) 1 : NULL;
    return (HMODULE) 1;
}

static BOOL WINAPI fixture_debugger(void) { return debugger; }
static BOOL WINAPI fixture_remote(HANDLE, PBOOL result) {
    *result = remote_debugger;
    return !remote_query_failed;
}
static LONG WINAPI fixture_query(HANDLE, ULONG kind, PVOID result, ULONG, PULONG) {
    if (process_query_mode == 1) return (LONG) 0xc0000022u;
    *(HANDLE *) result = kind == 7 && process_query_mode == 2 ? (HANDLE) -1 : NULL;
    if (kind == 7) return 0;
    if (process_query_mode == 3) return 0;
    if (process_query_mode == 4) return (LONG) 0xc0000022u;
    return (LONG) 0xc0000353u;
}
static LONG WINAPI fixture_system(ULONG kind, PVOID result, ULONG size, PULONG) {
    if (kernel_query_failed || kind != 35 || size != 2) return (LONG) 0xc0000022u;
    ((BOOLEAN *) result)[0] = kernel_enabled;
    ((BOOLEAN *) result)[1] = kernel_not_present;
    return 0;
}
static uintptr_t fixture_peb(unsigned long) {
    return missing_peb ? 0 : (uintptr_t) peb_data;
}
static LONG NTAPI fixture_open(PHANDLE result, ACCESS_MASK, POBJECT_ATTRIBUTES) {
    if (memory_mode == 2) return (LONG) 0xc0000022u;
    *result = (HANDLE) 1;
    return 0;
}
static LONG NTAPI fixture_map(HANDLE, HANDLE, PVOID *result, ULONG_PTR, SIZE_T,
        PLARGE_INTEGER, PSIZE_T size, ULONG, ULONG, ULONG) {
    if (memory_mode == 3) return (LONG) 0xc0000022u;
    if (memory_mode == 1) memcpy(memory_data, "VMware", 7);
    *result = memory_mode == 6 ? NULL : memory_data;
    *size = memory_mode == 7 ? 0x20000 : sizeof(memory_data);
    return 0;
}
static LONG NTAPI fixture_unmap(HANDLE, PVOID) {
    memory_unmaps++;
    return memory_mode == 4 ? (LONG) 0xc0000022u : 0;
}
static LONG NTAPI fixture_close(HANDLE) {
    memory_closes++;
    return memory_mode == 5 ? (LONG) 0xc0000022u : 0;
}
static FARPROC WINAPI fixture_proc(HMODULE, LPCSTR name) {
    if (strcmp(name, "EnumSystemFirmwareTables") == 0) {
        return missing_firmware_api ? NULL : (FARPROC) fixture_enum;
    }
    if (strcmp(name, "GetSystemFirmwareTable") == 0) {
        return missing_firmware_api ? NULL : (FARPROC) fixture_firmware;
    }
    if (strcmp(name, "NtOpenSection") == 0) return memory_mode == 8 ? NULL : (FARPROC) fixture_open;
    if (strcmp(name, "NtMapViewOfSection") == 0) return memory_mode == 8 ? NULL : (FARPROC) fixture_map;
    if (strcmp(name, "NtUnmapViewOfSection") == 0) return memory_mode == 8 ? NULL : (FARPROC) fixture_unmap;
    if (strcmp(name, "NtClose") == 0) return memory_mode == 8 ? NULL : (FARPROC) fixture_close;
    if (missing_query) return NULL;
    if (strcmp(name, "NtQuerySystemInformation") == 0) {
        return missing_system_query ? NULL : (FARPROC) fixture_system;
    }
    return (FARPROC) fixture_query;
}

#define __cpuid fixture_cpuid
#define EnumSystemFirmwareTables fixture_enum
#define GetSystemFirmwareTable fixture_firmware
#define GetModuleHandleA fixture_module
#define IsDebuggerPresent fixture_debugger
#define CheckRemoteDebuggerPresent fixture_remote
#define GetProcAddress fixture_proc
#define __readgsqword fixture_peb
#define __readfsdword fixture_peb
static LONG vm_native_query_process(HANDLE process, ULONG kind, PVOID buffer, ULONG size, PULONG length) {
    auto query = (decltype(&fixture_query)) fixture_proc((HMODULE) 1, "NtQueryInformationProcess");
    return query != NULL ? query(process, kind, buffer, size, length) : (LONG) 0xc00000bbu;
}
static LONG vm_native_query_system(ULONG kind, PVOID buffer, ULONG size, PULONG length) {
    auto query = (decltype(&fixture_system)) fixture_proc((HMODULE) 1, "NtQuerySystemInformation");
    return query != NULL ? query(kind, buffer, size, length) : (LONG) 0xc00000bbu;
}
#include "vm.hpp"

static void reset(void) {
    hypervisor = 0;
    smbios_mode = -1;
    acpi_mode = 0;
    root_partition = short_cpu_range = sandbox_module = false;
    enumeration_mode = firmware_mode = firmware_calls = 0;
    debugger = remote_debugger = remote_query_failed = missing_query = false;
    process_query_mode = 0;
    memset(peb_data, 0, sizeof(peb_data));
    missing_peb = missing_system_query = kernel_query_failed = kernel_enabled = false;
    kernel_not_present = true;
    fixture_trap = vm_physical;
    missing_firmware_api = false;
    memory_mode = memory_closes = memory_unmaps = 0;
    memset(memory_data, 0, sizeof(memory_data));
    memcpy(memory_data, "Dell Inc", 8);
}

static void expect(bool condition, const char *name) {
    checks++;
    if (condition) return;
    fprintf(stderr, "check failed: %s\n", name);
    failures++;
}

int main(void) {
    reset(); hypervisor = 2; smbios_mode = 1; acpi_mode = 1;
    vm_detection_details collected = vm_detect_details();
    expect(collected.result.state == vm_virtual && collected.cpu.state == vm_virtual
            && collected.firmware.smbios.state == vm_virtual && collected.firmware.acpi.state == vm_virtual,
            "collection keeps independent positive reports");
    expect(!collected.trap.applicable && collected.sandbox.applicable, "skipped trap is explicit");
    reset(); fixture_trap = vm_unknown;
    collected = vm_detect_details();
    expect(collected.result.state == vm_unknown && collected.trap.state == vm_unknown
            && collected.firmware.smbios.state == vm_physical, "collection preserves failure and continues");
    reset(); smbios_mode = 1; acpi_mode = 2;
    collected = vm_detect_details();
    expect(collected.result.state == vm_virtual && collected.firmware.smbios.state == vm_virtual
            && collected.firmware.acpi.reason == vm_reason_acpi_checksum, "positive report retains other failures");
    reset(); missing_firmware_api = true;
    collected = vm_detect_details();
    expect(collected.firmware.legacy.applicable && !collected.firmware.smbios.applicable, "legacy collection path");
    reset();
    expect(vm_debugger_report().code == 0 && vm_debugger_report().reason == vm_reason_none, "clean debugger report");
    debugger = true;
    expect(vm_debugger_report().reason == vm_reason_debugger_api, "debugger api reason");
    reset(); peb_data[2] = 1;
    expect(vm_debugger_report().reason == vm_reason_debugger_peb, "peb reason");
    reset(); missing_peb = true;
    expect(vm_debugger_report().code == 100 && vm_debugger_report().reason == vm_reason_debugger_peb, "missing peb reason");
    reset(); remote_debugger = true;
    expect(vm_debugger_report().reason == vm_reason_debugger_remote, "remote debugger reason");
    reset(); remote_query_failed = true; SetLastError(ERROR_ACCESS_DENIED);
    vm_debug_report debug_details = vm_debugger_report();
    expect(debug_details.code == 100 && debug_details.reason == vm_reason_debugger_remote && debug_details.native_error == ERROR_ACCESS_DENIED, "remote query error preserved");
    reset(); process_query_mode = 1;
    debug_details = vm_debugger_report();
    expect(debug_details.reason == vm_reason_debugger_port && debug_details.native_error == 0xc0000022u, "debug port status preserved");
    process_query_mode = 2;
    expect(vm_debugger_report().reason == vm_reason_debugger_port, "debug port reason");
    process_query_mode = 3;
    expect(vm_debugger_report().reason == vm_reason_debugger_object, "debug object reason");
    process_query_mode = 4;
    debug_details = vm_debugger_report();
    expect(debug_details.reason == vm_reason_debugger_object && debug_details.native_error == 0xc0000022u, "debug object status preserved");
    reset(); kernel_query_failed = true;
    debug_details = vm_debugger_report();
    expect(debug_details.reason == vm_reason_debugger_kernel && debug_details.native_error == 0xc0000022u, "kernel query status preserved");
    reset(); kernel_enabled = true; kernel_not_present = false;
    expect(vm_debugger_report().reason == vm_reason_debugger_kernel, "kernel debugger reason");
    expect(strcmp(vm_reason_name(vm_reason_debugger_registers), "debug registers") == 0, "debugger reason label");
    const char *vendors[] = {"VirtualBox", "VMware", "Parallels", "QEMU", "innotek", "Microsoft Virtual Machine"};
    for (const char *vendor : vendors) {
        size_t size = strlen(vendor);
        expect(vm_firmware_vendor((const uint8_t *) vendor, size), "vendor at buffer boundary");
        expect(!vm_firmware_vendor((const uint8_t *) vendor, size - 1), "truncated vendor");
    }
    expect(!vm_firmware_vendor(NULL, 0), "empty firmware");
    const char *physical_microsoft = "Microsoft Corporation Surface Laptop";
    expect(!vm_firmware_vendor((const uint8_t *) physical_microsoft, strlen(physical_microsoft)),
            "physical Microsoft OEM is not a VM marker");
    expect(vm_error(vm_unknown) == 100, "unknown blocks execution");
    expect(vm_error((vm_state) 123) == 100, "invalid state blocks execution");
    expect(vm_error(vm_virtual) == 20, "virtual blocks execution");
    expect(vm_error(vm_physical) == 0, "physical permits execution");
    reset();
    expect(vm_detect() == vm_physical, "completed physical fixture");
    firmware_mode = 1;
    expect(vm_detect() == vm_virtual, "firmware VM detection");
    reset(); hypervisor = 2;
    expect(vm_detect() == vm_virtual && firmware_calls == 0, "CPU VM detection");
    reset(); hypervisor = 1;
    expect(vm_detect() == vm_virtual, "Hyper-V guest");
    root_partition = true;
    expect(vm_detect() == vm_physical, "Hyper-V root continues checks");
    firmware_mode = 1;
    expect(vm_detect() == vm_virtual, "Hyper-V root does not bypass firmware");
    short_cpu_range = true; firmware_mode = 0;
    expect(vm_detect() == vm_virtual, "invalid Hyper-V leaf range");
    reset(); sandbox_module = true;
    expect(vm_detect() == vm_virtual, "Sandboxie module");
    reset(); enumeration_mode = 2;
    expect(vm_detect() == vm_physical, "unsupported FIRM falls back to SMBIOS");
    firmware_mode = 1;
    expect(vm_detect() == vm_virtual, "SMBIOS VM detection");
    firmware_mode = 5;
    expect(vm_detect() == vm_unknown, "malformed SMBIOS stays unknown");
    const int enumeration_modes[] = {1, 3, 4};
    for (int mode : enumeration_modes) {
        reset(); enumeration_mode = mode;
        expect(vm_detect() == vm_unknown, "enumeration failure stays unknown");
    }
    const int firmware_modes[] = {2, 3, 4};
    for (int mode : firmware_modes) {
        reset(); firmware_mode = mode;
        expect(vm_detect() == vm_unknown, "firmware failure stays unknown");
    }
    reset(); expect(vm_debugger_error() == 0, "completed debugger checks");
    debugger = true; expect(vm_debugger_error() == 10, "local debugger");
    reset(); remote_debugger = true; expect(vm_debugger_error() == 10, "remote debugger");
    reset(); remote_query_failed = true; expect(vm_debugger_error() == 100, "remote query failure");
    reset(); missing_query = true; expect(vm_debugger_error() == 100, "missing native query");
    reset(); process_query_mode = 1; expect(vm_debugger_error() == 100, "debug port query failure");
    process_query_mode = 2; expect(vm_debugger_error() == 10, "debug port");
    process_query_mode = 3; expect(vm_debugger_error() == 10, "debug object");
    process_query_mode = 4; expect(vm_debugger_error() == 100, "debug object query failure");
    reset(); peb_data[2] = 1; expect(vm_debugger_error() == 10, "PEB debugger flag");
    reset(); missing_peb = true; expect(vm_debugger_error() == 100, "missing PEB");
    reset(); missing_system_query = true; expect(vm_debugger_error() == 100, "missing kernel query");
    reset(); kernel_query_failed = true; expect(vm_debugger_error() == 100, "kernel query failure");
    reset(); kernel_enabled = true; kernel_not_present = false; kernel_not_present = false;
    expect(vm_debugger_error() == 10, "active kernel debugger");
    kernel_not_present = true;
    expect(vm_debugger_error() == 0, "enabled kernel debugging without a debugger");
    reset(); fixture_trap = vm_virtual;
    expect(vm_detect() == vm_virtual, "trap mismatch rejects virtualization");
    reset(); fixture_trap = vm_unknown;
    expect(vm_detect() == vm_unknown, "incomplete trap check stays unknown");
    reset(); hypervisor = 1; root_partition = true; fixture_trap = vm_virtual;
    expect(vm_detect() == vm_physical, "Hyper-V root follows upstream trap exemption");
    reset(); missing_firmware_api = true;
    expect(vm_detect() == vm_physical, "legacy memory fallback completes without a signal");
    expect(memory_closes == 1 && memory_unmaps == 1, "legacy mapping is released");
    reset(); missing_firmware_api = true; memory_mode = 1;
    expect(vm_detect() == vm_virtual, "legacy memory fallback finds vendor");
    const int memory_modes[] = {2, 3, 4, 5, 6, 7, 8};
    for (int mode : memory_modes) {
        reset(); missing_firmware_api = true; memory_mode = mode;
        expect(vm_detect() == vm_unknown, "failed legacy check stays unknown");
    }
    reset(); missing_firmware_api = true; memory_mode = 3;
    expect(vm_detect() == vm_unknown && memory_closes == 1 && memory_unmaps == 0,
            "failed mapping still closes the section");
    if (failures != 0) return 1;
    reset(); firmware_mode = 0; smbios_mode = 1;
    expect(vm_detect() == vm_virtual, "SMBIOS-only VM is detected with supported FIRM");
    reset(); firmware_mode = 1; smbios_mode = 0;
    expect(vm_detect() == vm_virtual, "FIRM-only VM is detected with clean SMBIOS");
    reset(); firmware_mode = 0; smbios_mode = 5;
    expect(vm_detect() == vm_unknown, "malformed SMBIOS is not hidden by clean FIRM");
    reset(); firmware_mode = 2; smbios_mode = 1;
    expect(vm_detect() == vm_virtual, "positive SMBIOS is retained when FIRM cannot complete");
    reset(); enumeration_mode = 1; smbios_mode = 1;
    expect(vm_detect() == vm_virtual, "positive SMBIOS is retained when FIRM access is denied");
    reset(); firmware_mode = 2; smbios_mode = 0;
    expect(vm_detect() == vm_unknown, "clean SMBIOS does not hide a failed FIRM read");
    reset(); firmware_mode = 1; smbios_mode = 5;
    expect(vm_detect() == vm_virtual, "positive FIRM is retained with malformed SMBIOS");
    reset(); firmware_mode = 0; smbios_mode = 0;
    expect(vm_detect() == vm_physical, "both completed firmware providers are clean");
    reset(); smbios_mode = 1;
    vm_report details = vm_detect_report();
    expect(details.state == vm_virtual && details.reason == vm_reason_system_manufacturer
            && details.provider == 0x52534d42u, "SMBIOS reason reaches the public report");
    reset(); enumeration_mode = 1;
    details = vm_detect_report();
    expect(details.state == vm_unknown && details.reason == vm_reason_firmware_enumeration
            && details.native_error == ERROR_ACCESS_DENIED && details.provider == 0x4649524du,
            "enumeration error reaches the public report");
    reset(); hypervisor = 2;
    details = vm_detect_report();
    expect(details.state == vm_virtual && details.reason == vm_reason_hypervisor, "CPU reason reaches the public report");
    reset(); fixture_trap = vm_unknown;
    details = vm_detect_report();
    expect(details.state == vm_unknown && details.reason == vm_reason_trap_failed, "trap failure reaches the public report");
    reset(); acpi_mode = 1;
    details = vm_detect_report();
    expect(details.state == vm_virtual && details.reason == vm_reason_acpi_oem
            && details.provider == 0x41435049u, "ACPI-only VM reaches the public report");
    reset(); acpi_mode = 2;
    details = vm_detect_report();
    expect(details.state == vm_unknown && details.reason == vm_reason_acpi_checksum
            && details.provider == 0x41435049u, "ACPI checksum error reaches the public report");
    reset(); acpi_mode = 3;
    details = vm_detect_report();
    expect(details.state == vm_unknown && details.reason == vm_reason_firmware_read
            && details.provider == 0x41435049u, "ACPI short read reaches the public report");
    reset(); acpi_mode = 4;
    details = vm_detect_report();
    expect(details.state == vm_unknown && details.reason == vm_reason_firmware_enumeration
            && details.native_error == ERROR_ACCESS_DENIED && details.provider == 0x41435049u,
            "ACPI access denial reaches the public report");
    reset(); smbios_mode = 5; acpi_mode = 1;
    details = vm_detect_report();
    expect(details.state == vm_virtual && details.reason == vm_reason_acpi_oem,
            "positive ACPI evidence survives another provider failure");
    reset(); smbios_mode = 1; acpi_mode = 4;
    details = vm_detect_report();
    expect(details.state == vm_virtual && details.reason == vm_reason_system_manufacturer,
            "positive SMBIOS does not depend on ACPI access");
    reset(); missing_firmware_api = true; memory_mode = 4;
    details = vm_detect_report();
    expect(details.state == vm_unknown && details.reason == vm_reason_memory_release
            && details.native_error == 0xc0000022u, "legacy release error preserves NTSTATUS");
    reset(); missing_firmware_api = true; memory_mode = 2;
    details = vm_detect_report();
    expect(details.state == vm_unknown && details.reason == vm_reason_memory_access
            && details.native_error == 0xc0000022u, "legacy access error preserves NTSTATUS");
    printf("checks -> ok (%d)\n", checks);
    return 0;
}
