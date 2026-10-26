#ifndef VM_VIRTUAL_HPP
#define VM_VIRTUAL_HPP

#include "../unknown/state.hpp"
#include "../physical/firmware.hpp"
#include "trap.hpp"

struct vm_detection_details {
    vm_report cpu = {vm_unknown, vm_reason_incomplete, 0, 0, 0, false};
    vm_report sandbox = {vm_unknown, vm_reason_incomplete, 0, 0, 0, false};
    vm_report trap = {vm_unknown, vm_reason_incomplete, 0, 0, 0, false};
    vm_firmware_details firmware;
    vm_report result;
};

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>

static vm_report vm_detect_collect(vm_detection_details *details) {
#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
    int cpu_info[4];
    __cpuid(cpu_info, 1);
    bool hypervisor = ((cpu_info[2] >> 31) & 1) != 0;
    vm_result result;
    vm_report cpu = {vm_physical, vm_reason_none};
    if (hypervisor) {
        cpu_info[1] = 0;
        cpu_info[2] = 0;
        cpu_info[3] = 0;
        __cpuid(cpu_info, 0x40000000);
        // hyperv açık dıye gerçek kasaya kıralık beden muamelesı yapma amk
        bool root_partition = false;
        if (cpu_info[0] >= 0x40000003 && cpu_info[1] == 0x7263694d
                && cpu_info[2] == 0x666f736f && cpu_info[3] == 0x76482074) {
            uint32_t maximum_leaf = (uint32_t) cpu_info[0];
            __cpuid(cpu_info, 0x40000001);
            uint32_t interface_id = (uint32_t) cpu_info[0];
            if (interface_id == 0x31237648u) {
                cpu_info[1] = 0;
                __cpuid(cpu_info, 0x40000003);
                root_partition = vm_hyperv_root_partition(maximum_leaf, interface_id,
                        (uint32_t) cpu_info[1]);
            }
        }
        if (!root_partition) cpu = {vm_virtual, vm_reason_hypervisor};
    }
    if (details != NULL) details->cpu = cpu;
    result.observe(cpu);
    if (result.detected && details == NULL) return result.report();
    bool sandboxed = GetModuleHandleA("sbiedll.dll") != NULL;
    vm_report sandbox = {sandboxed ? vm_virtual : vm_physical, sandboxed ? vm_reason_sandbox : vm_reason_none};
    if (details != NULL) details->sandbox = sandbox;
    result.observe(sandbox);
    if (result.detected && details == NULL) return result.report();
    // hypervısor zaten kendını söylemışse trap ıle ıkıncı sorguya çekme
    if (!hypervisor) {
        vm_state trap = vm_trap_check();
        vm_report report = {trap, trap == vm_physical ? vm_reason_none
                : trap == vm_virtual ? vm_reason_trap : vm_reason_trap_failed};
        if (details != NULL) details->trap = report;
        result.observe(report);
        if (trap != vm_physical && details == NULL) return report;
    }
    result.observe(vm_firmware_collect(details == NULL ? NULL : &details->firmware));
    return result.report();
#else
    return {vm_unknown, vm_reason_platform};
#endif
}
#else
static vm_report vm_detect_collect(vm_detection_details *) {
    return {vm_unknown, vm_reason_platform};
}
#endif

static vm_report vm_detect_report(void) {
    return vm_detect_collect(NULL);
}

static vm_detection_details vm_detect_details(void) {
    vm_detection_details details;
    details.result = vm_detect_collect(&details);
    return details;
}

static vm_state vm_detect(void) {
    return vm_detect_report().state;
}

#endif
