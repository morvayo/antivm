#include "virtual/vendor.hpp"

int main() {
    if (!vm_hyperv_root_partition(0x40000005u, 0x31237648u, 1)) return 1;
    if (!vm_hyperv_root_partition(0x40000003u, 0x31237648u, 3)) return 2;
    if (vm_hyperv_root_partition(0x40000002u, 0x31237648u, 1)) return 3;
    if (vm_hyperv_root_partition(0x40000005u, 0, 1)) return 4;
    if (vm_hyperv_root_partition(0x40000005u, 0x31237649u, 1)) return 5;
    if (vm_hyperv_root_partition(0x40000005u, 0x31237648u, 0)) return 6;
    if (vm_hyperv_root_partition(0x40000005u, 0x31237648u, 2)) return 7;
    return 0;
}
