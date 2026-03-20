// AINE: /proc → Mach/Darwin translation
// Android y ART leen /proc/self/maps, /proc/self/status, /proc/cpuinfo, etc.
// En macOS usamos mach_vm_region_recurse + sysctl para generar contenido equivalente
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/sysctl.h>
#include <sys/types.h>

// AINE: genera /proc/self/maps como tmpfile con mach_vm_region_recurse
static int generate_proc_self_maps(void) {
    FILE *f = tmpfile();
    if (!f) return -1;

    mach_port_t task = mach_task_self();
    mach_vm_address_t addr = 0;
    mach_vm_size_t    size = 0;
    uint32_t          depth = 1;

    while (1) {
        struct vm_region_submap_info_64 info;
        mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
        kern_return_t kr = mach_vm_region_recurse(
            task, &addr, &size, &depth,
            (vm_region_recurse_info_t)&info, &count);
        if (kr != KERN_SUCCESS) break;

        char perms[5] = "----";
        if (info.protection & VM_PROT_READ)    perms[0] = 'r';
        if (info.protection & VM_PROT_WRITE)   perms[1] = 'w';
        if (info.protection & VM_PROT_EXECUTE) perms[2] = 'x';
        perms[3] = (info.share_mode == SM_SHARED) ? 's' : 'p';
        perms[4] = '\0';

        fprintf(f, "%llx-%llx %s 00000000 00:00 0\n",
                (unsigned long long)addr,
                (unsigned long long)(addr + size),
                perms);
        addr += size;
        size = 0;
    }

    fflush(f);
    rewind(f);
    return fileno(f);
}

// AINE: genera /proc/cpuinfo simulado para ARM64
static int generate_proc_cpuinfo(void) {
    FILE *f = tmpfile();
    if (!f) return -1;

    int ncpu = 0;
    size_t len = sizeof(ncpu);
    sysctlbyname("hw.logicalcpu", &ncpu, &len, NULL, 0);
    if (ncpu <= 0) ncpu = 4;

    for (int i = 0; i < ncpu; i++) {
        fprintf(f,
            "processor\t: %d\n"
            "BogoMIPS\t: 100.00\n"
            "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32\n"
            "CPU implementer\t: 0x61\n"
            "CPU architecture: 8\n"
            "CPU variant\t: 0x1\n"
            "CPU part\t: 0x022\n"
            "CPU revision\t: 0\n\n",
            i);
    }

    fflush(f);
    rewind(f);
    return fileno(f);
}

// AINE: genera /proc/self/status mínimo que ART necesita
static int generate_proc_self_status(void) {
    FILE *f = tmpfile();
    if (!f) return -1;

    pid_t pid = getpid();
    fprintf(f,
        "Name:\taine\n"
        "State:\tR (running)\n"
        "Pid:\t%d\n"
        "PPid:\t%d\n"
        "TracerPid:\t0\n"
        "Threads:\t1\n",
        pid, getppid());

    fflush(f);
    rewind(f);
    return fileno(f);
}

// Forward declaration del binder open (implementado en binder-dev.c, mismo dylib)
extern int aine_binder_shim_open(void);

// AINE: interposición de open() para interceptar /proc paths y /dev/binder
int aine_open(const char *path, int flags, ...) {
    if (path) {
        // /dev/binder → Mach IPC via aine-binder daemon
        if (strcmp(path, "/dev/binder") == 0 ||
            strcmp(path, "/dev/hwbinder") == 0 ||
            strcmp(path, "/dev/vndbinder") == 0) {
            return aine_binder_shim_open();
        }
        if (strcmp(path, "/proc/self/maps") == 0)
            return generate_proc_self_maps();
        if (strcmp(path, "/proc/cpuinfo") == 0)
            return generate_proc_cpuinfo();
        if (strcmp(path, "/proc/self/status") == 0)
            return generate_proc_self_status();
        if (strncmp(path, "/proc/", 6) == 0) {
            // AINE: otros /proc paths — devolver fd vacío, no crash
            return open("/dev/null", O_RDONLY);
        }
    }
    // Llamada original vía va_list
    va_list ap;
    va_start(ap, flags);
    int mode = va_arg(ap, int);
    va_end(ap);
    return open(path, flags, mode);
}
