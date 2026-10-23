# antivm

small header only c++ library for checking vms and debuggers on windows

checks cpu and firmware data including smbios and acpi
also reports tpm and boot state along with pci devices disks and driver details

results are physical virtual or unknown
failed checks stay unknown because broken shit proves nothing
tpm and secure boot do not decide whether a machine is virtual
some vms will get through so test it on your own setup

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
see the [third party notice](THIRD_PARTY.md) for excluded material

## credits

firmware parsing follows the [smbios specification](https://www.dmtf.org/standards/smbios) and [acpi specification](https://uefi.org/specifications)
