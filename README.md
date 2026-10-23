# antivm

small header only c++ library for checking vms and debuggers on windows

checks cpu and firmware data including smbios and acpi
also reports tpm and boot state along with pci devices disks and driver details

## compiling

use a c++17 compiler and add src to your include path
include the [vm header](src/vm.hpp) from your own program
msvc and clang cl builds return unknown for the assembly trap checks
zig windows gnu builds run those checks on x64 and x86

## tests

configure with cmake then build and run ctest
the workflow tests msvc and zig on both architectures plus clang cl on x64
tests cover firmware parsing and bad inputs as well as driver query failures
the zig trap test runs eight threads across two source files and checks handler cleanup

## license

original contributions use the [mit license](LICENSE)
