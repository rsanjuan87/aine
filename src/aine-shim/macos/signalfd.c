// AINE: signalfd → kqueue EVFILT_SIGNAL
// Android usa signalfd en algunos casos; ART en M1 no lo requiere críticamente
// Implementación mínima: suficiente para no crashear
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define SFD_NONBLOCK  04000
#define SFD_CLOEXEC   02000000

// Estructura que se lee de un signalfd en Linux
typedef struct {
    uint32_t ssi_signo;
    int32_t  ssi_errno;
    int32_t  ssi_code;
    uint32_t ssi_pid;
    uint32_t ssi_uid;
    int32_t  ssi_fd;
    uint32_t ssi_tid;
    uint32_t ssi_band;
    uint32_t ssi_overrun;
    uint32_t ssi_trapno;
    int32_t  ssi_status;
    int32_t  ssi_int;
    uint64_t ssi_ptr;
    uint64_t ssi_utime;
    uint64_t ssi_stime;
    uint64_t ssi_addr;
    uint8_t  pad[48];
} signalfd_siginfo_t;

int aine_signalfd(int fd, const sigset_t *mask, int flags) {
    int kq;
    if (fd == -1) {
        kq = kqueue();
        if (kq < 0) return -1;
    } else {
        kq = fd; // reuse existing signalfd
    }

    if (flags & SFD_NONBLOCK) fcntl(kq, F_SETFL, O_NONBLOCK);
    if (flags & SFD_CLOEXEC)  fcntl(kq, F_SETFD, FD_CLOEXEC);

    // Registrar cada señal del mask en el kqueue
    for (int sig = 1; sig < NSIG; sig++) {
        if (sigismember(mask, sig)) {
            struct kevent kev;
            EV_SET(&kev, sig, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
            kevent(kq, &kev, 1, NULL, 0, NULL); // ignorar error por señal no válida
        }
    }
    return kq;
}
