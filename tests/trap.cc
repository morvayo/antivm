#include <stdio.h>
#include "trap_runtime.hpp"

vm_state peer_probe(bool cpuid);

#if defined(__x86_64__) || defined(__i386__)
static HANDLE start_event;

static DWORD WINAPI worker(void *argument) {
    if (WaitForSingleObject(start_event, 10000) != WAIT_OBJECT_0) return 1;
    for (unsigned i = 0; i < 100; i++) {
        vm_state state = argument == NULL ? vm_trap_probe((i & 1) != 0) : peer_probe((i & 1) != 0);
        if (state != vm_unknown && state != vm_physical && state != vm_virtual) return 1;
        if (vm_current_trap != NULL) return 1;
    }
    return 0;
}
#endif

int main() {
#if defined(__x86_64__) || defined(__i386__)
    reject_registration = true;
    if (vm_trap_probe(false) != vm_unknown || vm_current_trap != NULL) return 1;
    reject_registration = false;
    EXCEPTION_RECORD record = {};
    CONTEXT context = {};
    EXCEPTION_POINTERS exception = {&record, &context};
    if (vm_trap_handler(&exception) != EXCEPTION_CONTINUE_SEARCH) return 1;
    vm_trap_record nested = {};
    vm_current_trap = &nested;
    if (vm_trap_probe(false) != vm_unknown || vm_current_trap != &nested) return 1;
    record.ExceptionAddress = (void *) 1;
    context.EFlags = 0x100;
    if (vm_trap_handler(&exception) != EXCEPTION_CONTINUE_SEARCH || context.EFlags != 0x100) return 1;
    vm_current_trap = NULL;
    HANDLE threads[8] = {};
    start_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (start_event == NULL) return 1;
    for (unsigned i = 0; i < 8; i++) {
        threads[i] = CreateThread(NULL, 0, worker, (void *) (uintptr_t) (i & 1), 0, NULL);
        if (threads[i] == NULL) ExitProcess(1);
    }
    if (!SetEvent(start_event)) ExitProcess(1);
    if (WaitForMultipleObjects(8, threads, TRUE, 60000) != WAIT_OBJECT_0) ExitProcess(1);
    bool ok = true;
    for (HANDLE thread : threads) {
        DWORD code = 1;
        if (!GetExitCodeThread(thread, &code) || code != 0) ok = false;
        CloseHandle(thread);
    }
    CloseHandle(start_event);
    if (!ok || active_handlers != 0 || peak_handlers != 1 || installed_handlers != 800
            || removed_handlers != 800 || registration_failed != 0) return 1;
    reject_removal = true;
    if (vm_trap_probe(false) != vm_unknown) return 1;
    reject_removal = false;
    LONG installed = installed_handlers;
    if (peer_probe(false) != vm_unknown || installed_handlers != installed
            || active_handlers != 0 || vm_current_trap != NULL) return 1;
    puts("trap checks -> ok (800 probes across 8 threads and 2 translation units)");
#else
    if (vm_trap_check() != vm_unknown || peer_probe(false) != vm_unknown) return 1;
    puts("trap checks -> unsupported compiler returns unknown");
#endif
}
