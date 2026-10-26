#ifndef VM_STATE_HPP
#define VM_STATE_HPP

#include <stdint.h>

enum vm_state {
    vm_unknown,
    vm_physical,
    vm_virtual
};

enum vm_reason {
    vm_reason_none,
    vm_reason_incomplete,
    vm_reason_platform,
    vm_reason_hypervisor,
    vm_reason_sandbox,
    vm_reason_trap,
    vm_reason_trap_failed,
    vm_reason_firmware_api,
    vm_reason_firmware_unsupported,
    vm_reason_firmware_size,
    vm_reason_firmware_read,
    vm_reason_firmware_enumeration,
    vm_reason_firmware_vendor,
    vm_reason_allocation,
    vm_reason_memory_access,
    vm_reason_memory_release,
    vm_reason_smbios_header,
    vm_reason_smbios_structure,
    vm_reason_smbios_string,
    vm_reason_bios_vendor,
    vm_reason_bios_version,
    vm_reason_system_manufacturer,
    vm_reason_system_product,
    vm_reason_system_version,
    vm_reason_board_manufacturer,
    vm_reason_board_product,
    vm_reason_board_version,
    vm_reason_chassis_manufacturer,
    vm_reason_acpi_header,
    vm_reason_acpi_checksum,
    vm_reason_acpi_oem,
    vm_reason_debugger_api,
    vm_reason_debugger_peb,
    vm_reason_debugger_remote,
    vm_reason_debugger_port,
    vm_reason_debugger_object,
    vm_reason_debugger_handles,
    vm_reason_debugger_registers,
    vm_reason_debugger_kernel,
    vm_reason_debugger_modules,
    vm_reason_memory_query,
    vm_reason_tpm_api,
    vm_reason_tpm_query,
    vm_reason_device_api,
    vm_reason_device_enumeration,
    vm_reason_capacity,
    vm_reason_argument,
    vm_reason_smbios_virtual,
    vm_reason_boot_api,
    vm_reason_boot_query,
    vm_reason_secure_boot
};

struct vm_report {
    vm_state state = vm_unknown;
    vm_reason reason = vm_reason_incomplete;
    uint32_t native_error = 0;
    uint32_t provider = 0;
    uint32_t table = 0;
    bool applicable = true;
    uint32_t record_type = UINT32_MAX;
    uint32_t record_handle = UINT32_MAX;
    uint64_t record_offset = UINT64_MAX;
    uint32_t field_offset = UINT32_MAX;
};

static const char *vm_reason_name(vm_reason reason) {
    switch (reason) {
        case vm_reason_none: return "none";
        case vm_reason_platform: return "unsupported platform";
        case vm_reason_hypervisor: return "cpu hypervisor";
        case vm_reason_sandbox: return "sandbox module";
        case vm_reason_trap: return "cpu trap";
        case vm_reason_trap_failed: return "cpu trap incomplete";
        case vm_reason_firmware_api: return "firmware api unavailable";
        case vm_reason_firmware_unsupported: return "firmware provider unsupported";
        case vm_reason_firmware_size: return "invalid firmware size";
        case vm_reason_firmware_read: return "firmware read failed";
        case vm_reason_firmware_enumeration: return "firmware enumeration failed";
        case vm_reason_firmware_vendor: return "firmware vendor";
        case vm_reason_allocation: return "allocation failed";
        case vm_reason_memory_access: return "physical memory access failed";
        case vm_reason_memory_release: return "physical memory release failed";
        case vm_reason_smbios_header: return "invalid smbios header";
        case vm_reason_smbios_structure: return "invalid smbios structure";
        case vm_reason_smbios_string: return "invalid smbios string index";
        case vm_reason_bios_vendor: return "smbios bios vendor";
        case vm_reason_bios_version: return "smbios bios version";
        case vm_reason_system_manufacturer: return "smbios system manufacturer";
        case vm_reason_system_product: return "smbios system product";
        case vm_reason_system_version: return "smbios system version";
        case vm_reason_board_manufacturer: return "smbios board manufacturer";
        case vm_reason_board_product: return "smbios board product";
        case vm_reason_board_version: return "smbios board version";
        case vm_reason_chassis_manufacturer: return "smbios chassis manufacturer";
        case vm_reason_acpi_header: return "invalid acpi header";
        case vm_reason_acpi_checksum: return "invalid acpi checksum";
        case vm_reason_acpi_oem: return "acpi oem identifiers";
        case vm_reason_debugger_api: return "debugger api";
        case vm_reason_debugger_peb: return "debugger peb";
        case vm_reason_debugger_remote: return "remote debugger query";
        case vm_reason_debugger_port: return "process debug port";
        case vm_reason_debugger_object: return "process debug object";
        case vm_reason_debugger_handles: return "debugger handle check";
        case vm_reason_debugger_registers: return "debug registers";
        case vm_reason_debugger_kernel: return "kernel debugger query";
        case vm_reason_debugger_modules: return "kernel debugger modules";
        case vm_reason_memory_query: return "memory query failed";
        case vm_reason_tpm_api: return "tpm api unavailable";
        case vm_reason_tpm_query: return "tpm query failed";
        case vm_reason_device_api: return "device api unavailable";
        case vm_reason_device_enumeration: return "device enumeration failed";
        case vm_reason_capacity: return "output capacity reached";
        case vm_reason_argument: return "invalid argument";
        case vm_reason_smbios_virtual: return "smbios virtual machine flag";
        case vm_reason_boot_api: return "firmware type api unavailable";
        case vm_reason_boot_query: return "firmware type query failed";
        case vm_reason_secure_boot: return "secure boot state unavailable";
        default: return "incomplete";
    }
}

struct vm_result {
    bool observed = false;
    bool incomplete = false;
    bool detected = false;
    vm_report detection = {vm_virtual, vm_reason_incomplete};
    vm_report failure;

    void observe(vm_state value) {
        observe(vm_report{value, value == vm_physical ? vm_reason_none : vm_reason_incomplete});
    }

    void observe(vm_report value) {
        if (!value.applicable) return;
        observed = true;
        if (value.state == vm_virtual) {
            if (!detected) detection = value;
            detected = true;
        } else if (value.state != vm_physical) {
            // sonradan temız geldı dıye öncekı bokluğu halı altına süpürme
            if (!incomplete) failure = value;
            incomplete = true;
        }
    }

    vm_state state() const {
        if (detected) return vm_virtual;
        // okuyamadık dıye fızıksel yazma amk burada fal bakmıyoruz
        return observed && !incomplete ? vm_physical : vm_unknown;
    }

    vm_report report() const {
        if (detected) return detection;
        if (incomplete) {
            vm_report value = failure;
            value.state = vm_unknown;
            return value;
        }
        return {state(), observed ? vm_reason_none : vm_reason_incomplete};
    }
};

static const char *vm_message(int code) {
    if (code == 0) return "physical";
    if (code == 10) return "debugger detected";
    if (code == 20) return "virtual";
    return "unknown";
}

static int vm_error(vm_state state) {
    if (state == vm_physical) return 0;
    return state == vm_virtual ? 20 : 100;
}

#endif
