#ifndef VM_VENDOR_HPP
#define VM_VENDOR_HPP

#include <stdint.h>
#include <stddef.h>
#include <string.h>

static bool vm_hyperv_root_partition(uint32_t maximum_leaf, uint32_t interface_id,
        uint32_t partition_privileges) {
    return maximum_leaf >= 0x40000003u && interface_id == 0x31237648u
            && (partition_privileges & 1u) != 0;
}

static unsigned char vm_ascii_lower(unsigned char value) {
    return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
}

static bool vm_text_equal(const uint8_t *data, size_t size, const char *text) {
    if (data == NULL || text == NULL) return false;
    while (size != 0 && (data[size - 1] == ' ' || data[size - 1] == 0)) size--;
    if (size != strlen(text)) return false;
    for (size_t i = 0; i < size; i++) {
        if (vm_ascii_lower(data[i]) != vm_ascii_lower((unsigned char) text[i])) return false;
    }
    return true;
}

static bool vm_text_token(const uint8_t *data, size_t size, const char *text) {
    if (data == NULL || text == NULL) return false;
    size_t length = strlen(text);
    if (length == 0 || length > size) return false;
    for (size_t i = 0; i <= size - length; i++) {
        bool equal = true;
        for (size_t j = 0; j < length; j++) {
            if (vm_ascii_lower(data[i + j]) != vm_ascii_lower((unsigned char) text[j])) {
                equal = false;
                break;
            }
        }
        if (!equal) continue;
        unsigned char before = i == 0 ? ' ' : vm_ascii_lower(data[i - 1]);
        unsigned char after = i + length == size ? ' ' : vm_ascii_lower(data[i + length]);
        bool left = (before >= 'a' && before <= 'z') || (before >= '0' && before <= '9');
        bool right = (after >= 'a' && after <= 'z') || (after >= '0' && after <= '9');
        if (!left && !right) return true;
    }
    return false;
}

static bool vm_vendor_field(const uint8_t *data, size_t size) {
    const char *names[] = {"vmware", "virtualbox", "qemu", "innotek", "parallels"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (vm_text_token(data, size, names[i])) return true;
    }
    return false;
}

static bool vm_firmware_vendor(const uint8_t *data, size_t data_size) {
    // mıcrosoft görünce vm dıye atlama lan surface de aynı soyadını taşıyor
    return vm_vendor_field(data, data_size)
            || (vm_text_token(data, data_size, "microsoft")
                && vm_text_token(data, data_size, "virtual machine"));
}

#endif
