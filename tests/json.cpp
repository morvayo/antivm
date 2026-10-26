#include "json.hpp"
#include <string.h>
#include <vector>

int main() {
    vm_report report = {vm_virtual, vm_reason_system_product, 42};
    report.provider = 1;
    report.table = 2;
    report.record_type = 1;
    report.record_handle = 65535;
    report.record_offset = 18446744073709551614ull;
    report.field_offset = 5;
    const char *expected = "{\"state\":\"virtual\",\"reason\":\"smbios system product\","
            "\"native_error\":42,\"provider\":1,\"table\":2,\"applicable\":true,"
            "\"record_type\":1,\"record_handle\":65535,"
            "\"record_offset\":18446744073709551614,\"field_offset\":5}";
    size_t needed = vm_report_json(report, NULL, 0);
    if (needed != strlen(expected)) return 1;
    std::vector<char> output(needed + 1);
    if (vm_report_json(report, output.data(), output.size()) != needed) return 2;
    if (strcmp(output.data(), expected) != 0) return 3;

    for (size_t capacity = 1; capacity <= needed; capacity++) {
        std::vector<char> short_output(capacity + 1, '!');
        if (vm_report_json(report, short_output.data(), capacity) != needed) return 4;
        if (short_output[capacity - 1] != 0 || short_output[capacity] != '!') return 5;
        if (strncmp(short_output.data(), expected, capacity - 1) != 0) return 6;
    }
    char untouched = '!';
    if (vm_report_json(report, &untouched, 0) != needed || untouched != '!') return 7;
    if (vm_report_json(report, NULL, 1) != SIZE_MAX) return 8;

    vm_report unknown;
    unknown.applicable = false;
    char buffer[512];
    if (vm_report_json(unknown, buffer, sizeof(buffer)) >= sizeof(buffer)) return 9;
    if (strstr(buffer, "\"state\":\"unknown\"") == NULL
            || strstr(buffer, "\"applicable\":false") == NULL
            || strstr(buffer, "\"record_type\":null") == NULL
            || strstr(buffer, "\"record_handle\":null") == NULL
            || strstr(buffer, "\"record_offset\":null") == NULL
            || strstr(buffer, "\"field_offset\":null") == NULL) return 10;
    vm_report physical = {vm_physical, vm_reason_none};
    vm_report_json(physical, buffer, sizeof(buffer));
    if (strstr(buffer, "\"state\":\"physical\"") == NULL) return 11;
    puts(output.data());
    return 0;
}
