#include "unknown/state.hpp"
#include <stdio.h>
#include <string.h>

int main() {
    int failures = 0;
    auto check = [&](bool condition, const char *name) {
        if (condition) return;
        fprintf(stderr, "failed: %s\n", name);
        failures++;
    };

    check(vm_error(vm_physical) == 0, "physical error code");
    check(vm_error(vm_virtual) == 20, "virtual error code");
    check(vm_error(vm_unknown) == 100, "unknown error code");
    check(strcmp(vm_message(vm_error(vm_physical)), "physical") == 0, "physical message");
    check(strcmp(vm_message(vm_error(vm_virtual)), "virtual") == 0, "virtual message");
    check(strcmp(vm_message(vm_error(vm_unknown)), "unknown") == 0, "unknown message");
    check(strcmp(vm_message(10), "debugger detected") == 0, "debugger message");
    check(strcmp(vm_message(-1), "unknown") == 0, "unrecognized message");

    vm_result empty;
    check(empty.report().state == vm_unknown, "empty observation state");
    check(empty.report().reason == vm_reason_incomplete, "empty observation reason");

    vm_report ignored = {vm_virtual, vm_reason_hypervisor};
    ignored.applicable = false;
    empty.observe(ignored);
    check(!empty.observed && empty.report().state == vm_unknown, "inapplicable evidence");

    vm_result incomplete;
    vm_report failure = {vm_unknown, vm_reason_firmware_read, 42};
    failure.record_offset = 64;
    incomplete.observe(failure);
    incomplete.observe(vm_physical);
    check(incomplete.state() == vm_unknown, "physical evidence does not erase failure");
    check(incomplete.report().native_error == 42 && incomplete.report().record_offset == 64,
            "failure metadata");

    vm_report detected = {vm_virtual, vm_reason_system_product};
    detected.record_handle = 7;
    incomplete.observe(detected);
    check(incomplete.state() == vm_virtual, "detection takes precedence over failure");
    check(incomplete.report().record_handle == 7, "detection metadata");
    incomplete.observe({vm_virtual, vm_reason_hypervisor});
    check(incomplete.report().reason == vm_reason_system_product, "first detection is retained");

    vm_result physical;
    physical.observe(vm_physical);
    check(physical.report().state == vm_physical && physical.report().reason == vm_reason_none,
            "physical observation");
    physical.observe(failure);
    check(physical.state() == vm_unknown, "later failure is retained");

    return failures == 0 ? 0 : 1;
}
