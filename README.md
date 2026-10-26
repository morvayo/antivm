# antivm

small header only c++ library for checking vms and debuggers on windows

checks cpu and firmware data including smbios and acpi
also reports tpm and boot state along with pci devices disks and driver details

## compiling

use a c++17 compiler and add src to your include path
include the [vm header](src/vm.hpp) from your own program
msvc and clang cl builds return unknown for the assembly trap checks
zig windows gnu builds run those checks on x64 and x86

## limits

these checks report observable evidence rather than prove a machine is physical
a hypervisor can change the cpu and firmware data visible to its guest
the hyper-v root partition exception requires the documented Hv#1 interface
before interpreting its partition privileges

## json reports

`vm_report_json` writes one `vm_report` to a caller-owned buffer without allocating
it returns the required character count excluding the terminator
pass `nullptr, 0` to measure first and reserve one extra byte for the terminator
`SIZE_MAX` indicates an invalid argument or formatting error
when the returned count is at least the capacity the output is truncated and must not be used as JSON
unknown record locations are emitted as `null`

```cpp
#include "json.hpp"
#include <cstdio>
#include <vector>

int main() {
    vm_report report = {vm_unknown, vm_reason_platform};
    size_t length = vm_report_json(report, nullptr, 0);
    if (length == SIZE_MAX) return 1;
    std::vector<char> output(length + 1);
    if (vm_report_json(report, output.data(), output.size()) != length) return 1;
    std::puts(output.data());
}
```

pass a report from `vm_detect_report()` to export a live detection result

## tests

```sh
cmake -S . -B build -DANTIVM_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## throughput

![vm detection output](hahaha.png)

## license

original contributions use the [mit license](LICENSE)
