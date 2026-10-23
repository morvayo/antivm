#include <windows.h>
#include <stdio.h>
#include <string.h>
static unsigned checks, loads, frees, reads;
static bool firmware_missing, firmware_failed, library_failed, registry_missing;
static FIRMWARE_TYPE firmware_value = FirmwareTypeUefi;
static DWORD registry_type = REG_DWORD, registry_size = 4, registry_value = 1;
static LSTATUS registry_status;
static void expect(bool value, const char *name) {
    checks++;
    if (!value) { printf("check failed: %s\n", name); ExitProcess(1); }
}
static BOOL WINAPI fixture_firmware(PFIRMWARE_TYPE output) {
    if (firmware_failed) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    *output = firmware_value;
    return TRUE;
}
static LSTATUS WINAPI fixture_read(HKEY root, LPCWSTR path, LPCWSTR name, DWORD flags, LPDWORD type, PVOID output, LPDWORD size) {
    reads++;
    expect(root == HKEY_LOCAL_MACHINE && wcscmp(path, L"SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State") == 0
            && wcscmp(name, L"UEFISecureBootEnabled") == 0
            && flags == (RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6464KEY), "secure boot query target");
    expect(*size == sizeof(DWORD), "registry buffer size");
    *type = registry_type; *size = registry_size;
    *(DWORD *) output = registry_value;
    return registry_status;
}
static HMODULE WINAPI fixture_module(LPCWSTR) { return (HMODULE) 1; }
static HMODULE WINAPI fixture_load(LPCWSTR name, HANDLE, DWORD flags) {
    expect(wcscmp(name, L"advapi32.dll") == 0 && flags == LOAD_LIBRARY_SEARCH_SYSTEM32, "registry system library");
    if (library_failed) { SetLastError(ERROR_MOD_NOT_FOUND); return NULL; }
    loads++;
    return (HMODULE) 2;
}
static BOOL WINAPI fixture_free(HMODULE) { frees++; return TRUE; }
static FARPROC WINAPI fixture_export(HMODULE, LPCSTR name) {
    if (strcmp(name, "GetFirmwareType") == 0 && !firmware_missing) return (FARPROC) fixture_firmware;
    if (strcmp(name, "RegGetValueW") == 0 && !registry_missing) return (FARPROC) fixture_read;
    SetLastError(ERROR_PROC_NOT_FOUND);
    return NULL;
}
#define GetModuleHandleW fixture_module
#define GetProcAddress fixture_export
#define LoadLibraryExW fixture_load
#define FreeLibrary fixture_free
#include "physical/boot.hpp"
int main() {
    vm_boot_report report = vm_boot_query();
    expect(report.mode == vm_boot_uefi && report.firmware_reason == vm_reason_none
            && report.secure_boot == vm_secure_boot_enabled && report.secure_boot_reason == vm_reason_none, "uefi enabled");
    registry_value = 0;
    expect(vm_boot_query().secure_boot == vm_secure_boot_disabled, "secure boot disabled");
    registry_value = 2;
    report = vm_boot_query();
    expect(report.secure_boot == vm_secure_boot_unknown && report.secure_boot_error == ERROR_INVALID_DATA, "invalid secure boot value");
    registry_value = 1; registry_type = REG_SZ;
    expect(vm_boot_query().secure_boot == vm_secure_boot_unknown, "invalid registry type");
    registry_type = REG_DWORD; registry_size = 3;
    expect(vm_boot_query().secure_boot == vm_secure_boot_unknown, "invalid registry size");
    registry_size = 4; registry_status = ERROR_ACCESS_DENIED;
    report = vm_boot_query();
    expect(report.secure_boot == vm_secure_boot_unknown && report.secure_boot_error == ERROR_ACCESS_DENIED
            && report.mode == vm_boot_uefi, "query failure preserves firmware report");
    registry_status = ERROR_FILE_NOT_FOUND;
    expect(vm_boot_query().secure_boot == vm_secure_boot_unknown, "missing value stays unknown");
    registry_status = 0; firmware_value = FirmwareTypeBios;
    unsigned before = reads;
    report = vm_boot_query();
    expect(report.mode == vm_boot_bios && report.secure_boot == vm_secure_boot_not_applicable
            && reads == before, "bios skips secure boot query");
    firmware_value = FirmwareTypeUnknown;
    report = vm_boot_query();
    expect(report.mode == vm_boot_unknown && report.firmware_error == ERROR_INVALID_DATA
            && report.secure_boot == vm_secure_boot_enabled, "independent unknown firmware");
    firmware_failed = true;
    report = vm_boot_query();
    expect(report.mode == vm_boot_unknown && report.firmware_error == ERROR_ACCESS_DENIED, "firmware failure preserved");
    firmware_failed = false; firmware_missing = true;
    expect(vm_boot_query().firmware_reason == vm_reason_boot_api, "firmware export missing");
    firmware_missing = false; firmware_value = FirmwareTypeUefi; library_failed = true;
    expect(vm_boot_query().secure_boot_error == ERROR_MOD_NOT_FOUND, "registry library missing");
    library_failed = false; registry_missing = true;
    report = vm_boot_query();
    expect(report.secure_boot == vm_secure_boot_unknown && report.secure_boot_error == ERROR_PROC_NOT_FOUND, "registry export missing");
    expect(loads == frees, "loaded libraries released");
    printf("boot fixture checks -> ok (%u)\n", checks);
}
