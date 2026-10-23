#include <windows.h>
#include <setupapi.h>
#include <tbs.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
static int checks, libraries, sets;
static bool load_fail, export_fail, create_fail, destroy_fail, enum_fail;
static int device_count = 2, property_mode;
static TBS_RESULT tpm_status;
static UINT32 tpm_version = TPM_VERSION_20;
static void expect(bool value, const char *name) {
    checks++;
    if (!value) { printf("check failed: %s\n", name); ExitProcess(1); }
}
static HMODULE WINAPI fixture_load(LPCWSTR, HANDLE, DWORD flags) {
    expect(flags == LOAD_LIBRARY_SEARCH_SYSTEM32, "system library path");
    if (load_fail) { SetLastError(ERROR_MOD_NOT_FOUND); return NULL; }
    libraries++;
    return (HMODULE) 1;
}
static BOOL WINAPI fixture_free(HMODULE) { libraries--; return TRUE; }
static TBS_RESULT WINAPI fixture_tpm(UINT32 size, PVOID output) {
    TPM_DEVICE_INFO *information = (TPM_DEVICE_INFO *) output;
    expect(size == sizeof(*information) && information->structVersion == TPM_VERSION_20, "tpm query layout");
    information->tpmVersion = tpm_version;
    return tpm_status;
}
static HDEVINFO WINAPI fixture_create(const GUID *guid, PCWSTR enumerator, HWND, DWORD flags) {
    expect((guid == NULL && enumerator != NULL && wcscmp(enumerator, L"PCI") == 0
                && flags == (DIGCF_PRESENT | DIGCF_ALLCLASSES))
            || (guid != NULL && guid->Data1 == 0x4d36e967 && enumerator == NULL && flags == DIGCF_PRESENT), "device filter");
    if (create_fail) { SetLastError(ERROR_ACCESS_DENIED); return INVALID_HANDLE_VALUE; }
    sets++;
    return (HDEVINFO) 1;
}
static BOOL WINAPI fixture_enum(HDEVINFO, DWORD index, PSP_DEVINFO_DATA information) {
    expect(information->cbSize == sizeof(*information), "device query layout");
    if (enum_fail) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    if (index >= (DWORD) device_count) { SetLastError(ERROR_NO_MORE_ITEMS); return FALSE; }
    return TRUE;
}
static BOOL WINAPI fixture_property(HDEVINFO, PSP_DEVINFO_DATA, DWORD property, PDWORD type, PBYTE output, DWORD capacity, PDWORD required) {
    const wchar_t multiple[] = L"PCI\\VEN_1234\0PCI\\CC_0300\0";
    const wchar_t single[] = L"device";
    bool ids = property == SPDRP_HARDWAREID;
    *type = ids ? REG_MULTI_SZ : REG_SZ;
    *required = ids ? sizeof(multiple) : sizeof(single);
    if (property_mode == 1) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    if (property_mode == 2) { *required = capacity + 2; SetLastError(ERROR_INSUFFICIENT_BUFFER); return FALSE; }
    memcpy(output, ids ? multiple : single, *required);
    if (property_mode == 3) *type = REG_DWORD;
    if (property_mode == 4) (*required)--;
    if (property_mode == 5) ((wchar_t *) output)[*required / 2 - 1] = L'x';
    if (property_mode == 6 && ids) ((wchar_t *) output)[*required / 2 - 2] = L'x';
    return TRUE;
}
static BOOL WINAPI fixture_destroy(HDEVINFO) {
    sets--;
    if (destroy_fail) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    return TRUE;
}
static int driver_mode;
static BOOL WINAPI fixture_driver(HDEVINFO, PSP_DEVINFO_DATA, const DEVPROPKEY *key, DEVPROPTYPE *type,
        PBYTE output, DWORD capacity, PDWORD required, DWORD flags) {
    expect(flags == 0 && key->fmtid.Data1 == 0xa8b865dd && key->fmtid.Data4[7] == 0xd6, "driver property key");
    if (driver_mode == 1) { SetLastError(ERROR_NOT_FOUND); return FALSE; }
    if (driver_mode == 2) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    if (driver_mode == 3) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return FALSE; }
    if (driver_mode == 7) return FALSE;
    if (key->pid == 2) {
        *type = DEVPROP_TYPE_FILETIME; *required = 8;
        *(uint64_t *) output = 123456789;
    } else {
        const wchar_t *text = key->pid == 9 ? L"provider" : key->pid == 3 ? L"1.2.3.4" : L"oem42.inf";
        *type = DEVPROP_TYPE_STRING;
        *required = (DWORD) ((wcslen(text) + 1) * sizeof(wchar_t));
        expect(capacity >= *required, "driver output capacity");
        memcpy(output, text, *required);
    }
    if (driver_mode == 4) *type = DEVPROP_TYPE_UINT32;
    if (driver_mode == 5) (*required)--;
    if (driver_mode == 6 && key->pid != 2) ((wchar_t *) output)[*required / 2 - 1] = L'x';
    return TRUE;
}
static FARPROC WINAPI fixture_export(HMODULE, LPCSTR name) {
    if (strcmp(name, "SetupDiGetDevicePropertyW") == 0 && !export_fail) return (FARPROC) fixture_driver;
    if (export_fail) { SetLastError(ERROR_PROC_NOT_FOUND); return NULL; }
    if (strcmp(name, "Tbsi_GetDeviceInfo") == 0) return (FARPROC) fixture_tpm;
    if (strcmp(name, "SetupDiGetClassDevsW") == 0) return (FARPROC) fixture_create;
    if (strcmp(name, "SetupDiEnumDeviceInfo") == 0) return (FARPROC) fixture_enum;
    if (strcmp(name, "SetupDiGetDeviceRegistryPropertyW") == 0) return (FARPROC) fixture_property;
    if (strcmp(name, "SetupDiDestroyDeviceInfoList") == 0) return (FARPROC) fixture_destroy;
    return NULL;
}
#define LoadLibraryExW fixture_load
#define FreeLibrary fixture_free
#define GetProcAddress fixture_export
#include "physical/tpm.hpp"
#include "physical/devices.hpp"
int main() {
    vm_tpm_report tpm = vm_tpm_query();
    expect(tpm.state == vm_tpm_present && tpm.version == TPM_VERSION_20 && tpm.reason == vm_reason_none, "tpm 2");
    tpm_version = TPM_VERSION_12;
    expect(vm_tpm_query().version == TPM_VERSION_12, "tpm 1.2");
    tpm_version = 99;
    expect(vm_tpm_query().state == vm_tpm_unknown, "unrecognized tpm response");
    tpm_status = TBS_E_TPM_NOT_FOUND;
    tpm = vm_tpm_query();
    expect(tpm.state == vm_tpm_absent && tpm.native_error == TBS_E_TPM_NOT_FOUND && tpm.version == 0, "tpm absent");
    tpm_status = TBS_E_BAD_PARAMETER;
    tpm = vm_tpm_query();
    expect(tpm.state == vm_tpm_unknown && tpm.reason == vm_reason_tpm_query && tpm.native_error == TBS_E_BAD_PARAMETER, "tpm query failure");
    export_fail = true;
    expect(vm_tpm_query().reason == vm_reason_tpm_api, "missing tpm export");
    export_fail = false; load_fail = true;
    expect(vm_tpm_query().native_error == ERROR_MOD_NOT_FOUND, "missing tpm library");
    load_fail = false;
    struct { vm_device_info values[2]; unsigned sentinel; } buffer = {};
    buffer.sentinel = 0x12345678;
    vm_device_report devices = vm_devices_query(vm_device_pci, buffer.values, 2);
    expect(devices.count == 2 && devices.reason == vm_reason_none && devices.property_failures == 0, "complete pci list");
    expect(buffer.values[0].hardware_ids[13] == L'P' && buffer.sentinel == 0x12345678, "multi string and bounds");
    expect(vm_devices_query(vm_device_disk, buffer.values, 2).reason == vm_reason_none, "disk filter");
    devices = vm_devices_query(vm_device_pci, buffer.values, 1);
    expect(devices.count == 1 && devices.truncated && devices.reason == vm_reason_capacity, "capacity limit");
    devices = vm_devices_query(vm_device_pci, NULL, 0);
    expect(devices.count == 0 && devices.truncated, "zero capacity with devices");
    expect(vm_devices_query(vm_device_pci, NULL, 1).reason == vm_reason_argument, "null buffer");
    expect(vm_devices_query((vm_device_kind) 99, NULL, 0).reason == vm_reason_argument, "invalid kind");
    expect(wcscmp(buffer.values[0].driver_provider, L"provider") == 0
            && wcscmp(buffer.values[0].driver_version, L"1.2.3.4") == 0
            && wcscmp(buffer.values[0].driver_inf, L"oem42.inf") == 0
            && buffer.values[0].driver_date == 123456789, "driver values collected");
    for (int mode = 1; mode <= 7; mode++) {
        driver_mode = mode;
        devices = vm_devices_query(vm_device_pci, buffer.values, 2);
        expect(devices.property_failures > 0 && buffer.values[0].provider_error != 0
                && buffer.values[0].driver_provider[0] == 0 && buffer.sentinel == 0x12345678,
                "invalid driver property cleared");
        vm_property_state expected = mode == 1 ? vm_property_missing : mode == 2 ? vm_property_denied
                : mode == 3 ? vm_property_truncated : mode == 7 ? vm_property_failed : vm_property_invalid;
        expect(vm_property_status(buffer.values[0].provider_error) == expected, "driver property status");
    }
    driver_mode = 0;
    expect(vm_property_status(ERROR_PROC_NOT_FOUND) == vm_property_unsupported, "unsupported property status");
    expect(vm_property_status(0) == vm_property_available, "available property status");
    device_count = 0;
    expect(vm_devices_query(vm_device_pci, NULL, 0).reason == vm_reason_none, "empty device list");
    device_count = 2;
    for (int mode = 1; mode <= 6; mode++) {
        property_mode = mode;
        devices = vm_devices_query(vm_device_pci, buffer.values, 2);
        expect(devices.property_failures > 0 && buffer.values[0].hardware_error != 0
                && buffer.values[0].hardware_ids[0] == 0 && buffer.sentinel == 0x12345678, "invalid property retained and cleared");
    }
    property_mode = 0; enum_fail = true;
    devices = vm_devices_query(vm_device_pci, buffer.values, 2);
    expect(devices.reason == vm_reason_device_enumeration && devices.native_error == ERROR_ACCESS_DENIED, "enumeration failure");
    enum_fail = false; create_fail = true;
    expect(vm_devices_query(vm_device_pci, buffer.values, 2).native_error == ERROR_ACCESS_DENIED, "device set failure");
    create_fail = false; destroy_fail = true;
    expect(vm_devices_query(vm_device_pci, buffer.values, 2).reason == vm_reason_device_enumeration, "cleanup failure");
    destroy_fail = false; export_fail = true;
    expect(vm_devices_query(vm_device_pci, buffer.values, 2).reason == vm_reason_device_api, "missing setup export");
    export_fail = false; load_fail = true;
    expect(vm_devices_query(vm_device_pci, buffer.values, 2).reason == vm_reason_device_api, "missing setup library");
    expect(libraries == 0 && sets == 0, "resources released");
    printf("hardware fixture checks -> ok (%d)\n", checks);
}
