#include <assert.h>
#include <stdio.h>
#include "vm.hpp"

static unsigned checks;
static uint8_t smbios[1024], acpi[128];
static size_t smbios_size;
static UINT acpi_size;
static int enumeration_mode, query_mode;

static void expect(vm_report report, vm_state state, vm_reason reason) {
    if (report.state != state || report.reason != reason) {
        fprintf(stderr, "expected %d/%s; got %d/%s\n", (int) state, vm_reason_name(reason),
                (int) report.state, vm_reason_name(report.reason));
        assert(false);
    }
    checks++;
}

static void reset_smbios() {
    memset(smbios, 0, sizeof(smbios));
    smbios[1] = 3; smbios[2] = 6;
    smbios_size = 8;
}

static void append(unsigned type, unsigned formatted, const char *a, const char *b,
        const char *c, const char *d) {
    size_t start = smbios_size;
    memset(smbios + start, 0, formatted);
    smbios[start] = (uint8_t) type;
    smbios[start + 1] = (uint8_t) formatted;
    if (formatted >= 8) {
        smbios[start + 4] = 1; smbios[start + 5] = 2;
        smbios[start + 6] = 3; smbios[start + 7] = 4;
    }
    smbios_size += formatted;
    const char *strings[] = {a, b, c, d};
    for (const char *text : strings) {
        size_t length = strlen(text) + 1;
        memcpy(smbios + smbios_size, text, length);
        smbios_size += length;
    }
    smbios[smbios_size++] = 0;
}

static void finish_smbios() {
    const uint8_t end[] = {127, 4, 0xff, 0xff, 0, 0};
    memcpy(smbios + smbios_size, end, sizeof(end));
    smbios_size += sizeof(end);
    uint32_t length = (uint32_t) smbios_size - 8;
    memcpy(smbios + 4, &length, 4);
}

static vm_report system_record(const char *manufacturer, const char *product, const char *version, const char *serial) {
    reset_smbios();
    append(1, 8, manufacturer, product, version, serial);
    finish_smbios();
    return vm_smbios_report(smbios, smbios_size);
}

static void checksum_acpi() {
    acpi[9] = 0;
    uint8_t sum = 0;
    for (UINT i = 0; i < acpi_size; i++) sum += acpi[i];
    acpi[9] = (uint8_t) -sum;
}

static void make_acpi(const char *oem, const char *table, UINT size = 36) {
    memset(acpi, 0, sizeof(acpi));
    acpi_size = size;
    memcpy(acpi, "DSDT", 4);
    memcpy(acpi + 4, &size, 4);
    acpi[8] = 2;
    memset(acpi + 10, ' ', 14);
    memcpy(acpi + 10, oem, strlen(oem));
    memcpy(acpi + 16, table, strlen(table));
    checksum_acpi();
}

static UINT WINAPI table_query(DWORD, DWORD, PVOID data, DWORD capacity) {
    if (query_mode == 1) { SetLastError(ERROR_ACCESS_DENIED); return 0; }
    if (query_mode == 3) return 32u * 1024u * 1024u;
    if (data == NULL) return acpi_size;
    if (query_mode == 2) return acpi_size - 1;
    if (capacity >= acpi_size) memcpy(data, acpi, acpi_size);
    return acpi_size;
}

static UINT WINAPI table_enum(DWORD, PVOID data, DWORD capacity) {
    if (enumeration_mode == 1 || enumeration_mode == 2) {
        SetLastError(enumeration_mode == 1 ? ERROR_ACCESS_DENIED : ERROR_NOT_SUPPORTED);
        return 0;
    }
    if (enumeration_mode == 4) return 3;
    if (data == NULL) return 4;
    if (enumeration_mode == 3) return 0;
    if (capacity >= 4) memcpy(data, "DSDT", 4);
    return 4;
}

int main() {
    vm_report origin = system_record("VMware, Inc.", "Desktop", "1.0", "serial");
    assert(origin.record_type == 1 && origin.record_handle == 0 && origin.record_offset == 8 && origin.field_offset == 4);
    checks++;
    reset_smbios(); append(0, 20, "Dell", "1.0", "date", "serial");
    append(1, 8, "FreeBSD", "BHYVE", "1.0", "serial"); finish_smbios();
    origin = vm_smbios_report(smbios, smbios_size);
    assert(origin.record_type == 1 && origin.record_offset > 8 && origin.field_offset == 5);
    checks++;
    smbios[origin.record_offset + 2] = 0x34; smbios[origin.record_offset + 3] = 0x12;
    origin = vm_smbios_report(smbios, smbios_size);
    assert(origin.record_handle == 0x1234); checks++;
    smbios[origin.record_offset + 4] = 99;
    origin = vm_smbios_report(smbios, smbios_size);
    assert(origin.state == vm_unknown && origin.record_handle == 0x1234 && origin.field_offset == 4); checks++;
    reset_smbios(); append(0, 20, "Dell", "1.0", "date", "serial"); finish_smbios(); smbios[8 + 0x13] = 0x10;
    origin = vm_smbios_report(smbios, smbios_size);
    assert(origin.record_type == 0 && origin.field_offset == 0x13); checks++;
    make_acpi("DELL", "QEMU");
    origin = vm_acpi_report(acpi, acpi_size);
    assert(origin.state == vm_virtual && origin.field_offset == 16 && origin.table == 0x54445344u && origin.provider == 0x41435049u); checks++;
    make_acpi("QEMU", "DESKTOP");
    assert(vm_acpi_report(acpi, acpi_size).field_offset == 10); checks++;
    acpi[9]++;
    assert(vm_acpi_report(acpi, acpi_size).field_offset == 9); checks++;
    vm_report empty;
    assert(empty.record_offset == UINT64_MAX && empty.record_type == UINT32_MAX && empty.field_offset == UINT32_MAX); checks++;
    expect(system_record("Xen", "HVM domU", "1.0", "serial"), vm_virtual, vm_reason_system_product);
    expect(system_record("xen ", "hvm DOMU ", "1.0", "serial"), vm_virtual, vm_reason_system_product);
    expect(system_record("Xenon", "HVM domU", "1.0", "serial"), vm_physical, vm_reason_none);
    expect(system_record("Xen", "Desktop", "1.0", "serial"), vm_physical, vm_reason_none);
    expect(system_record("Dell", "HVM domU", "1.0", "serial"), vm_physical, vm_reason_none);
    expect(system_record("FreeBSD", "BHYVE", "1.0", "serial"), vm_virtual, vm_reason_system_product);
    expect(system_record("FreeBSD", "Desktop", "1.0", "serial"), vm_physical, vm_reason_none);
    expect(system_record("Dell", "BHYVE", "1.0", "serial"), vm_physical, vm_reason_none);
    expect(system_record("FreeBSD Community", "BHYVE", "1.0", "serial"), vm_physical, vm_reason_none);
    reset_smbios(); append(2, 8, "FreeBSD", "BHYVE", "1.0", "serial"); finish_smbios();
    expect(vm_smbios_report(smbios, smbios_size), vm_virtual, vm_reason_board_product);
    reset_smbios(); append(1, 8, "Xen", "Desktop", "1.0", "serial");
    append(1, 8, "Dell", "HVM domU", "1.0", "serial"); finish_smbios();
    expect(vm_smbios_report(smbios, smbios_size), vm_physical, vm_reason_none);
    reset_smbios(); append(0, 20, "Dell", "1.0", "date", "serial"); finish_smbios();
    smbios[8 + 0x13] = 0x10;
    expect(vm_smbios_report(smbios, smbios_size), vm_virtual, vm_reason_smbios_virtual);
    smbios[8 + 0x13] = 0;
    expect(vm_smbios_report(smbios, smbios_size), vm_physical, vm_reason_none);
    smbios[8 + 0x13] = 0x08;
    expect(vm_smbios_report(smbios, smbios_size), vm_physical, vm_reason_none);
    smbios[8 + 0x13] = 0x60;
    expect(vm_smbios_report(smbios, smbios_size), vm_physical, vm_reason_none);
    smbios[8 + 0x13] = 0x10; smbios[1] = 2; smbios[2] = 2;
    expect(vm_smbios_report(smbios, smbios_size), vm_physical, vm_reason_none);
    smbios[2] = 3;
    expect(vm_smbios_report(smbios, smbios_size), vm_virtual, vm_reason_smbios_virtual);
    smbios[1] = 3; smbios[2] = 0;
    expect(vm_smbios_report(smbios, smbios_size), vm_virtual, vm_reason_smbios_virtual);
    reset_smbios(); append(0, 19, "Physical", "1.0", "date", "serial"); finish_smbios();
    expect(vm_smbios_report(smbios, smbios_size), vm_physical, vm_reason_none);
    reset_smbios();
    memset(smbios + 8, 0, 22); smbios[9] = 20; smbios[8 + 0x13] = 0x10; smbios_size = 30;
    finish_smbios();
    expect(vm_smbios_report(smbios, smbios_size), vm_virtual, vm_reason_smbios_virtual);
    smbios[8 + 0x13] = 0;
    expect(vm_smbios_report(smbios, smbios_size), vm_unknown, vm_reason_smbios_structure);
    expect(system_record("Dell Inc.", "Desktop", "1.0", "VMware"), vm_physical, vm_reason_none);
    expect(system_record("Microsoft Corporation", "Surface Pro", "1.0", "serial"), vm_physical, vm_reason_none);
    expect(system_record("Microsoft Corporation", "Virtual Machine", "1.0", "serial"), vm_virtual, vm_reason_system_product);
    expect(system_record("Dell", "Virtual Machine", "1.0", "serial"), vm_physical, vm_reason_none);
    expect(system_record("VMwareReady Industries", "Desktop", "1.0", "serial"), vm_physical, vm_reason_none);
    expect(system_record("VMware, Inc.", "Desktop", "1.0", "serial"), vm_virtual, vm_reason_system_manufacturer);
    expect(system_record("Dell", "VirtualBox", "1.0", "serial"), vm_virtual, vm_reason_system_product);
    expect(system_record("Dell", "Desktop", "QEMU", "serial"), vm_virtual, vm_reason_system_version);
    reset_smbios(); append(0, 18, "VMware", "1.0", "01/01/2020", "serial"); finish_smbios();
    expect(vm_smbios_report(smbios, smbios_size), vm_virtual, vm_reason_bios_vendor);
    reset_smbios(); append(0, 18, "Dell", "Parallels", "01/01/2020", "serial"); finish_smbios();
    expect(vm_smbios_report(smbios, smbios_size), vm_virtual, vm_reason_bios_version);
    reset_smbios(); append(2, 8, "innotek GmbH", "Board", "1.0", "serial"); finish_smbios();
    expect(vm_smbios_report(smbios, smbios_size), vm_virtual, vm_reason_board_manufacturer);
    reset_smbios(); append(2, 8, "Dell", "VirtualBox", "1.0", "serial"); finish_smbios();
    expect(vm_smbios_report(smbios, smbios_size), vm_virtual, vm_reason_board_product);
    reset_smbios(); append(2, 8, "Dell", "Board", "QEMU", "serial"); finish_smbios();
    expect(vm_smbios_report(smbios, smbios_size), vm_virtual, vm_reason_board_version);
    reset_smbios(); append(3, 9, "Parallels", "Desktop", "1.0", "serial"); finish_smbios();
    expect(vm_smbios_report(smbios, smbios_size), vm_virtual, vm_reason_chassis_manufacturer);
    reset_smbios(); append(1, 8, "Dell", "Desktop", "1.0", "serial");
    append(11, 4, "VMware", "QEMU", "VirtualBox", "Parallels"); finish_smbios();
    expect(vm_smbios_report(smbios, smbios_size), vm_physical, vm_reason_none);
    expect(vm_smbios_report(NULL, 0), vm_unknown, vm_reason_smbios_header);
    expect(vm_smbios_report(smbios, 8), vm_unknown, vm_reason_smbios_header);
    system_record("Dell", "Desktop", "1.0", "serial"); smbios[1] = 0;
    expect(vm_smbios_report(smbios, smbios_size), vm_unknown, vm_reason_smbios_header);
    system_record("Dell", "Desktop", "1.0", "serial"); smbios[4]++;
    expect(vm_smbios_report(smbios, smbios_size), vm_unknown, vm_reason_smbios_header);
    system_record("Dell", "Desktop", "1.0", "serial"); smbios[9] = 3;
    expect(vm_smbios_report(smbios, smbios_size), vm_unknown, vm_reason_smbios_structure);
    system_record("Dell", "Desktop", "1.0", "serial"); smbios[12] = 9;
    expect(vm_smbios_report(smbios, smbios_size), vm_unknown, vm_reason_smbios_string);
    system_record("Dell", "Desktop", "1.0", "serial"); smbios[12] = 0;
    expect(vm_smbios_report(smbios, smbios_size), vm_unknown, vm_reason_smbios_string);
    reset_smbios(); finish_smbios();
    expect(vm_smbios_report(smbios, smbios_size), vm_unknown, vm_reason_smbios_structure);
    make_acpi("DELL", "DESKTOP");
    expect(vm_acpi_report(acpi, acpi_size), vm_physical, vm_reason_none);
    make_acpi("VMWARE", "VMWARE");
    expect(vm_acpi_report(acpi, acpi_size), vm_virtual, vm_reason_acpi_oem);
    make_acpi("VBOX", "VBOXFACP");
    expect(vm_acpi_report(acpi, acpi_size), vm_virtual, vm_reason_acpi_oem);
    make_acpi("BOCHS", "BXPCDSDT");
    expect(vm_acpi_report(acpi, acpi_size), vm_virtual, vm_reason_acpi_oem);
    make_acpi("BOCHS", "DESKTOP");
    expect(vm_acpi_report(acpi, acpi_size), vm_physical, vm_reason_none);
    make_acpi("INTEL", "BXPCDSDT");
    expect(vm_acpi_report(acpi, acpi_size), vm_physical, vm_reason_none);
    make_acpi("DELL", "DESKTOP", 64);
    memcpy(acpi + 36, "VMware VirtualBox", 17); checksum_acpi();
    expect(vm_acpi_report(acpi, acpi_size), vm_physical, vm_reason_none);
    acpi[9]++;
    expect(vm_acpi_report(acpi, acpi_size), vm_unknown, vm_reason_acpi_checksum);
    make_acpi("DELL", "DESKTOP"); acpi[4]++;
    expect(vm_acpi_report(acpi, acpi_size), vm_unknown, vm_reason_acpi_header);
    make_acpi("DELL", "DESKTOP"); acpi[0] = 0;
    expect(vm_acpi_report(acpi, acpi_size), vm_unknown, vm_reason_acpi_header);
    make_acpi("DELL", "DESKTOP");
    expect(vm_acpi_report(acpi, acpi_size, 1), vm_unknown, vm_reason_acpi_header);
    expect(vm_acpi_report(NULL, 0), vm_unknown, vm_reason_acpi_header);
    make_acpi("DELL", "DESKTOP", 64); memcpy(acpi, "FACS", 4);
    expect(vm_acpi_report(acpi, acpi_size), vm_physical, vm_reason_none);
    make_acpi("DELL", "DESKTOP"); memcpy(acpi, "FACS", 4);
    expect(vm_acpi_report(acpi, acpi_size), vm_unknown, vm_reason_acpi_header);
    make_acpi("VBOX", "VBOXFACP");
    vm_report report = vm_firmware_tables_report(table_enum, table_query, 0x41435049u);
    expect(report, vm_virtual, vm_reason_acpi_oem);
    assert(report.provider == 0x41435049u && report.table == 0x54445344u); checks++;
    acpi[9]++;
    expect(vm_firmware_tables_report(table_enum, table_query, 0x41435049u), vm_unknown, vm_reason_acpi_checksum);
    query_mode = 1;
    report = vm_firmware_table_report(table_query, 0x41435049u, 0x54445344u);
    expect(report, vm_unknown, vm_reason_firmware_read);
    assert(report.native_error == ERROR_ACCESS_DENIED && report.provider == 0x41435049u); checks++;
    query_mode = 2;
    expect(vm_firmware_table_report(table_query, 0x41435049u, 0x54445344u), vm_unknown, vm_reason_firmware_read);
    query_mode = 3;
    expect(vm_firmware_table_report(table_query, 0x41435049u, 0x54445344u), vm_unknown, vm_reason_firmware_size);
    query_mode = 0; enumeration_mode = 1;
    report = vm_firmware_tables_report(table_enum, table_query, 0x41435049u);
    expect(report, vm_unknown, vm_reason_firmware_enumeration);
    assert(report.native_error == ERROR_ACCESS_DENIED); checks++;
    enumeration_mode = 2;
    report = vm_firmware_tables_report(table_enum, table_query, 0x41435049u);
    expect(report, vm_unknown, vm_reason_firmware_unsupported);
    assert(!report.applicable); checks++;
    enumeration_mode = 3;
    expect(vm_firmware_tables_report(table_enum, table_query, 0x41435049u), vm_unknown, vm_reason_firmware_enumeration);
    enumeration_mode = 4;
    expect(vm_firmware_tables_report(table_enum, table_query, 0x41435049u), vm_unknown, vm_reason_firmware_enumeration);
    expect(vm_firmware_table_report(NULL, 0, 0), vm_unknown, vm_reason_firmware_api);
    vm_result result;
    expect(result.report(), vm_unknown, vm_reason_incomplete);
    vm_report unsupported = {vm_unknown, vm_reason_firmware_unsupported}; unsupported.applicable = false;
    result.observe(unsupported);
    expect(result.report(), vm_unknown, vm_reason_incomplete);
    result.observe(vm_physical);
    expect(result.report(), vm_physical, vm_reason_none);
    result.observe(vm_report{vm_unknown, vm_reason_firmware_read, 5, 0x41435049u});
    expect(result.report(), vm_unknown, vm_reason_firmware_read);
    result.observe(vm_physical);
    assert(result.report().native_error == 5 && result.report().provider == 0x41435049u); checks++;
    result.observe(vm_report{vm_virtual, vm_reason_hypervisor});
    expect(result.report(), vm_virtual, vm_reason_hypervisor);
    result.observe(vm_report{vm_unknown, vm_reason_smbios_header});
    expect(result.report(), vm_virtual, vm_reason_hypervisor);
    system_record("Dell", "Desktop", "1.0", "serial");
    smbios[smbios_size - 5] = 5;
    smbios[smbios_size++] = 0;
    uint32_t extra_length = (uint32_t) smbios_size - 8;
    memcpy(smbios + 4, &extra_length, 4);
    expect(vm_smbios_report(smbios, smbios_size), vm_unknown, vm_reason_smbios_structure);
    system_record("Dell", "Desktop", "1.0", "serial");
    size_t end_position = smbios_size - 6;
    memcpy(smbios + end_position + 4, "junk\0", 6);
    smbios_size += 4;
    extra_length = (uint32_t) smbios_size - 8;
    memcpy(smbios + 4, &extra_length, 4);
    expect(vm_smbios_report(smbios, smbios_size), vm_unknown, vm_reason_smbios_structure);
    unsigned seed = 0x6a09e667;
    for (unsigned i = 0; i < 10000; i++) {
        system_record("Dell", "Desktop", "1.0", "serial");
        make_acpi("DELL", "DESKTOP", 64);
        for (unsigned j = 0; j < 8; j++) {
            seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
            smbios[seed % smbios_size] ^= (uint8_t) (seed >> 24);
            acpi[seed % acpi_size] ^= (uint8_t) (seed >> 16);
        }
        (void) vm_smbios_report(smbios, smbios_size);
        (void) vm_acpi_report(acpi, acpi_size);
    }
    checks++;
    printf("format and report checks -> passed (%u); mutated buffers -> 20000\n", checks);
    return 0;
}
