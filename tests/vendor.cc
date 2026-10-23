#include <stdio.h>
#include <stdlib.h>
#include "virtual/vendor.hpp"

static unsigned checks;
static void expect(bool result, const char *name) {
    checks++;
    if (!result) {
        fprintf(stderr, "check failed: %s\n", name);
        exit(1);
    }
}

int main() {
    const char *names[] = {"VMware", "VirtualBox", "QEMU", "innotek", "Parallels"};
    for (const char *name : names) {
        size_t size = strlen(name);
        expect(vm_firmware_vendor((const uint8_t *) name, size), "exact vendor");
        expect(!vm_firmware_vendor((const uint8_t *) name, size - 1), "truncated vendor");
        char text[128];
        snprintf(text, sizeof(text), "prefix%s", name);
        expect(!vm_firmware_vendor((const uint8_t *) text, strlen(text)), "embedded vendor prefix");
        snprintf(text, sizeof(text), "%sReady", name);
        expect(!vm_firmware_vendor((const uint8_t *) text, strlen(text)), "embedded vendor suffix");
        snprintf(text, sizeof(text), "(%s), Inc", name);
        expect(vm_firmware_vendor((const uint8_t *) text, strlen(text)), "delimited vendor");
    }
    const char *positive[] = {"qemu", "vMwArE", "Microsoft Virtual Machine", "microsoft\0virtual machine"};
    for (size_t i = 0; i < 4; i++) {
        size_t size = i == 3 ? 25 : strlen(positive[i]);
        expect(vm_firmware_vendor((const uint8_t *) positive[i], size), "case and binary separators");
    }
    const char *negative[] = {"QEMURocks", "xQEMU", "QEMU2", "Microsoft Surface", "Virtual Machine",
            "Microsoftish Virtual Machine", "Microsoft Virtual MachineX", "Dell"};
    for (const char *text : negative) {
        expect(!vm_firmware_vendor((const uint8_t *) text, strlen(text)), "physical or embedded string");
    }
    const uint8_t binary[] = {0, 'q', 'e', 'm', 'u', 0};
    expect(vm_firmware_vendor(binary, sizeof(binary)), "bounded binary token");
    expect(!vm_firmware_vendor(NULL, 100), "null buffer");
    expect(!vm_text_token(binary, sizeof(binary), NULL), "null token");
    expect(!vm_text_token(binary, sizeof(binary), ""), "empty token");
    printf("vendor checks -> ok (%u)\n", checks);
}
