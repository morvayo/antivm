#ifndef TEST_TRAP_RUNTIME_HPP
#define TEST_TRAP_RUNTIME_HPP

#include <windows.h>

inline volatile LONG active_handlers;
inline volatile LONG peak_handlers;
inline volatile LONG installed_handlers;
inline volatile LONG removed_handlers;
inline volatile LONG registration_failed;
inline bool reject_registration;
inline bool reject_removal;

inline PVOID WINAPI tracked_add(ULONG first, PVECTORED_EXCEPTION_HANDLER callback) {
    if (reject_registration) return NULL;
    PVOID handle = AddVectoredExceptionHandler(first, callback);
    if (handle != NULL) {
        LONG count = InterlockedIncrement(&active_handlers);
        InterlockedIncrement(&installed_handlers);
        if (count > peak_handlers) InterlockedExchange(&peak_handlers, count);
    } else {
        InterlockedIncrement(&registration_failed);
    }
    return handle;
}

inline ULONG WINAPI tracked_remove(PVOID handle) {
    ULONG result = RemoveVectoredExceptionHandler(handle);
    if (result != 0) {
        InterlockedDecrement(&active_handlers);
        InterlockedIncrement(&removed_handlers);
    }
    return reject_removal ? 0 : result;
}

#define AddVectoredExceptionHandler tracked_add
#define RemoveVectoredExceptionHandler tracked_remove
#include "virtual/trap.hpp"
#undef AddVectoredExceptionHandler
#undef RemoveVectoredExceptionHandler

#endif
