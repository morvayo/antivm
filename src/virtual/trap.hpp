#ifndef VM_TRAP_HPP
#define VM_TRAP_HPP

#include "../unknown/state.hpp"
#include <stdint.h>

#if defined(_WIN32) && (defined(__x86_64__) || defined(__i386__))
#include <windows.h>

struct vm_trap_record {
    uintptr_t first;
    uintptr_t last;
    vm_state result;
    uintptr_t debug_registers = 0;
};

inline thread_local vm_trap_record *vm_current_trap;
inline SRWLOCK vm_trap_lock = SRWLOCK_INIT;
inline bool vm_trap_cleanup_failed;

static LONG CALLBACK vm_trap_handler(EXCEPTION_POINTERS *exception) {
    vm_trap_record *trap = vm_current_trap;
    if (trap == NULL) return EXCEPTION_CONTINUE_SEARCH;
    uintptr_t address = (uintptr_t) exception->ExceptionRecord->ExceptionAddress;
    // bızım aralıkta patlamadıysa başkasının cenazesıne atlama
    if (address < trap->first || address >= trap->last) return EXCEPTION_CONTINUE_SEARCH;
    DWORD code = exception->ExceptionRecord->ExceptionCode;
    if ((code == EXCEPTION_PRIV_INSTRUCTION || code == EXCEPTION_ILLEGAL_INSTRUCTION)
            && address == trap->first) {
        trap->result = vm_unknown;
#if defined(__x86_64__)
        exception->ContextRecord->Rip = trap->last;
#else
        exception->ContextRecord->Eip = trap->last;
#endif
        exception->ContextRecord->EFlags &= ~0x100u;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (code != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
    if ((exception->ContextRecord->ContextFlags & CONTEXT_DEBUG_REGISTERS) == CONTEXT_DEBUG_REGISTERS) {
        trap->debug_registers = exception->ContextRecord->Dr0 | exception->ContextRecord->Dr1
                | exception->ContextRecord->Dr2 | exception->ContextRecord->Dr3;
    }
    trap->result = *(const unsigned char *) address == 0x90 ? vm_physical : vm_virtual;
    exception->ContextRecord->EFlags &= ~0x100u;
    return EXCEPTION_CONTINUE_EXECUTION;
}

static vm_state vm_trap_probe(bool cpuid, uintptr_t *debug_registers = NULL) {
    if (debug_registers != NULL) *debug_registers = 0;
    if (vm_current_trap != NULL) return vm_unknown;
    AcquireSRWLockExclusive(&vm_trap_lock);
    if (vm_trap_cleanup_failed) {
        ReleaseSRWLockExclusive(&vm_trap_lock);
        return vm_unknown;
    }
    vm_trap_record trap = {0, 0, vm_unknown};
    PVOID handler = AddVectoredExceptionHandler(1, vm_trap_handler);
    if (handler == NULL) {
        ReleaseSRWLockExclusive(&vm_trap_lock);
        return vm_unknown;
    }
    // başka thread ortalığı dağıttı dıye hesabı bıze kıtleme amk
    vm_current_trap = &trap;
#if defined(__x86_64__)
    if (cpuid) {
        __asm__ volatile (
            "leaq 1f(%%rip), %%r11\n\t"
            "movq %%r11, %0\n\t"
            "leaq 2f(%%rip), %%r11\n\t"
            "movq %%r11, %1\n\t"
            "movl $1, %%eax\n\t"
            "pushfq\n\t"
            "orq $0x100, (%%rsp)\n\t"
            "popfq\n\t"
            "1: cpuid\n\t"
            "nop\n\t"
            "pushfq\n\t"
            "andq $-257, (%%rsp)\n\t"
            "popfq\n\t"
            "2:\n\t"
            : "=m"(trap.first), "=m"(trap.last)
            : : "rax", "rbx", "rcx", "rdx", "r11", "memory", "cc");
    } else {
        __asm__ volatile (
            "leaq 1f(%%rip), %%r11\n\t"
            "movq %%r11, %0\n\t"
            "leaq 2f(%%rip), %%r11\n\t"
            "movq %%r11, %1\n\t"
            "pushfq\n\t"
            "orq $0x100, (%%rsp)\n\t"
            "popfq\n\t"
            "1: rdtsc\n\t"
            "nop\n\t"
            "pushfq\n\t"
            "andq $-257, (%%rsp)\n\t"
            "popfq\n\t"
            "2:\n\t"
            : "=m"(trap.first), "=m"(trap.last)
            : : "rax", "rdx", "r11", "memory", "cc");
    }
#else
    if (cpuid) {
        __asm__ volatile (
            "movl $1f, %0\n\t"
            "movl $2f, %1\n\t"
            "movl $1, %%eax\n\t"
            "pushfl\n\t"
            "orl $0x100, (%%esp)\n\t"
            "popfl\n\t"
            "1: cpuid\n\t"
            "nop\n\t"
            "pushfl\n\t"
            "andl $-257, (%%esp)\n\t"
            "popfl\n\t"
            "2:\n\t"
            : "=m"(trap.first), "=m"(trap.last)
            : : "eax", "ebx", "ecx", "edx", "memory", "cc");
    } else {
        __asm__ volatile (
            "movl $1f, %0\n\t"
            "movl $2f, %1\n\t"
            "pushfl\n\t"
            "orl $0x100, (%%esp)\n\t"
            "popfl\n\t"
            "1: rdtsc\n\t"
            "nop\n\t"
            "pushfl\n\t"
            "andl $-257, (%%esp)\n\t"
            "popfl\n\t"
            "2:\n\t"
            : "=m"(trap.first), "=m"(trap.last)
            : : "eax", "edx", "memory", "cc");
    }
#endif
    vm_current_trap = NULL;
    bool removed = RemoveVectoredExceptionHandler(handler) != 0;
    if (!removed) vm_trap_cleanup_failed = true;
    ReleaseSRWLockExclusive(&vm_trap_lock);
    if (!removed) return vm_unknown;
    if (debug_registers != NULL) *debug_registers = trap.debug_registers;
    return trap.result;
}

static vm_state vm_trap_check(void) {
    vm_state rdtsc = vm_trap_probe(false);
    vm_state cpuid = vm_trap_probe(true);
    if (rdtsc == vm_virtual || cpuid == vm_virtual) return vm_virtual;
    if (rdtsc == vm_unknown || cpuid == vm_unknown) return vm_unknown;
    return vm_physical;
}
#else
static vm_state vm_trap_check(void) {
    return vm_unknown;
}
#endif

#endif
