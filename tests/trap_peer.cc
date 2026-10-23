#include "trap_runtime.hpp"

vm_state peer_probe(bool cpuid) {
#if defined(__x86_64__) || defined(__i386__)
    return vm_trap_probe(cpuid);
#else
    (void) cpuid;
    return vm_trap_check();
#endif
}
