#ifndef VM_JSON_HPP
#define VM_JSON_HPP

#include "unknown/state.hpp"
#include <stddef.h>
#include <stdio.h>

static const char *vm_json_location(uint64_t value, uint64_t missing, char (&buffer)[32]) {
    if (value == missing) return "null";
    snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long) value);
    return buffer;
}

static size_t vm_report_json(const vm_report &report, char *output, size_t capacity) {
    if (output == NULL && capacity != 0) return SIZE_MAX;
    const char *state = report.state == vm_physical ? "physical"
            : report.state == vm_virtual ? "virtual" : "unknown";
    char type[32], handle[32], offset[32], field[32];
    int length = snprintf(output, capacity,
            "{\"state\":\"%s\",\"reason\":\"%s\",\"native_error\":%u,"
            "\"provider\":%u,\"table\":%u,\"applicable\":%s,"
            "\"record_type\":%s,\"record_handle\":%s,\"record_offset\":%s,\"field_offset\":%s}",
            state, vm_reason_name(report.reason), (unsigned) report.native_error,
            (unsigned) report.provider, (unsigned) report.table, report.applicable ? "true" : "false",
            vm_json_location(report.record_type, UINT32_MAX, type),
            vm_json_location(report.record_handle, UINT32_MAX, handle),
            vm_json_location(report.record_offset, UINT64_MAX, offset),
            vm_json_location(report.field_offset, UINT32_MAX, field));
    return length < 0 ? SIZE_MAX : (size_t) length;
}

#endif
