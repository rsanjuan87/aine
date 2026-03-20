// AINE: src/aine-binder/macos/binder-daemon.cpp
// Daemon Binder para macOS: Unix socket server que actúa como servicemanager
// Escucha en /tmp/aine-binder.sock para evitar restricciones Mach bootstrap.

#include "../include/aine-binder.h"
#include "mach-transport.h"
#include "../common/parcel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <errno.h>

// =============================================================================
// Registro de servicios
// =============================================================================

#define MAX_SERVICES 128
#define MAX_SVC_NAME 256

typedef struct {
    char name[MAX_SVC_NAME];
    int  handle;   // placeholder handle (Mach port not needed in M2)
    int  in_use;
} service_entry_t;

static service_entry_t g_services[MAX_SERVICES];
static pthread_mutex_t g_svc_mutex = PTHREAD_MUTEX_INITIALIZER;

static service_entry_t *find_service(const char *name) {
    for (int i = 0; i < MAX_SERVICES; i++) {
        if (g_services[i].in_use &&
            strcmp(g_services[i].name, name) == 0) {
            return &g_services[i];
        }
    }
    return NULL;
}

static int add_service(const char *name, int handle) {
    pthread_mutex_lock(&g_svc_mutex);
    if (find_service(name)) {
        pthread_mutex_unlock(&g_svc_mutex);
        return -1;  // already registered
    }
    for (int i = 0; i < MAX_SERVICES; i++) {
        if (!g_services[i].in_use) {
            strncpy(g_services[i].name, name, MAX_SVC_NAME - 1);
            g_services[i].name[MAX_SVC_NAME - 1] = '\0';
            g_services[i].handle  = i + 1;   // simple integer handle M2
            g_services[i].in_use  = 1;
            pthread_mutex_unlock(&g_svc_mutex);
            return 0;
        }
    }
    pthread_mutex_unlock(&g_svc_mutex);
    return -1;  // full
}

// =============================================================================
// Protocolo Service Manager
// =============================================================================

// android.os.IServiceManager interface descriptor
#define ISVC_MGR "android.os.IServiceManager"

// Construir respuesta BR_REPLY con status + parcel data
static void build_br_reply(uint8_t *out, uint32_t *out_size,
                             const void *parcel_data, uint32_t parcel_size,
                             int32_t status) {
    uint32_t pos = 0;

    // BR_REPLY command
    uint32_t cmd = BR_REPLY;
    memcpy(out + pos, &cmd, 4); pos += 4;

    // binder_transaction_data (skeleton — we only fill data pointer)
    struct binder_transaction_data txn;
    memset(&txn, 0, sizeof(txn));
    // Inline data — use buf[0..3] for size, rest for data
    // For simplicity we embed the parcel right after txn using data.buf trick
    // but since we can't use pointers in messages, we encode inline:
    // [BR_REPLY(4)][txn(56)][parcel_data(N)]
    // We'll set data_size and put data after the txn struct
    txn.data_size    = parcel_size;
    txn.offsets_size = 0;
    // data.ptr.buffer will point to data after txn — client must read inline
    // We use a sentinel to signal inline data: cookie = 0xAINE
    txn.cookie = 0xA1E0A1E0;
    memcpy(out + pos, &txn, sizeof(txn)); pos += sizeof(txn);

    if (parcel_data && parcel_size > 0) {
        memcpy(out + pos, parcel_data, parcel_size);
        pos += parcel_size;
    }
    *out_size = pos;
}

static void build_br_error(uint8_t *out, uint32_t *out_size, int32_t err) {
    uint32_t cmd = BR_ERROR;
    memcpy(out, &cmd, 4);
    memcpy(out + 4, &err, 4);
    *out_size = 8;
}

// Manejar SVC_MGR_ADD_SERVICE
static void handle_add_service(Parcel *in_parcel,
                                 uint8_t *reply, uint32_t *reply_size) {
    char name[MAX_SVC_NAME];
    if (parcel_read_string16(in_parcel, name, sizeof(name))) {
        build_br_error(reply, reply_size, -22); // EINVAL
        return;
    }
    // Registrar el servicio (port = MACH_PORT_NULL como placeholder M2)
    add_service(name, 0);
    fprintf(stderr, "[aine-binder] addService: '%s'\n", name);

    Parcel resp;
    parcel_init(&resp);
    parcel_write_int32(&resp, 0); // OK
    build_br_reply(reply, reply_size,
        parcel_data(&resp), (uint32_t)parcel_size(&resp), 0);
    parcel_free(&resp);
}

// Manejar SVC_MGR_GET_SERVICE / CHECK_SERVICE
static void handle_get_service(Parcel *in_parcel,
                                 uint8_t *reply, uint32_t *reply_size) {
    char name[MAX_SVC_NAME];
    if (parcel_read_string16(in_parcel, name, sizeof(name))) {
        build_br_error(reply, reply_size, -22);
        return;
    }
    service_entry_t *svc = find_service(name);
    fprintf(stderr, "[aine-binder] getService: '%s' → %s\n",
        name, svc ? "found" : "not found");

    Parcel resp;
    parcel_init(&resp);
    if (svc) {
        // flat_binder_object con handle
        struct flat_binder_object fbo;
        memset(&fbo, 0, sizeof(fbo));
        fbo.type   = BINDER_TYPE_HANDLE;
        fbo.handle = 1; // placeholder handle value M2
        parcel_write_bytes(&resp, &fbo, sizeof(fbo));
        parcel_write_int32(&resp, 0); // status OK
    } else {
        parcel_write_int32(&resp, -2); // NAME_NOT_FOUND
    }
    build_br_reply(reply, reply_size,
        parcel_data(&resp), (uint32_t)parcel_size(&resp), 0);
    parcel_free(&resp);
}

// Manejar SVC_MGR_LIST_SERVICES
static void handle_list_services(Parcel *in_parcel,
                                   uint8_t *reply, uint32_t *reply_size) {
    int32_t index;
    parcel_read_int32(in_parcel, &index);

    Parcel resp;
    parcel_init(&resp);

    pthread_mutex_lock(&g_svc_mutex);
    int count = 0;
    for (int i = 0; i < MAX_SERVICES; i++) {
        if (g_services[i].in_use) count++;
    }
    if (index < 0 || index >= count) {
        pthread_mutex_unlock(&g_svc_mutex);
        parcel_write_int32(&resp, -1); // end of list
    } else {
        int j = 0;
        for (int i = 0; i < MAX_SERVICES && j <= index; i++) {
            if (!g_services[i].in_use) continue;
            if (j == index) {
                parcel_write_int32(&resp, 0); // status OK — before name
                parcel_write_string16(&resp, g_services[i].name);
                break;
            }
            j++;
        }
        pthread_mutex_unlock(&g_svc_mutex);
    }

    build_br_reply(reply, reply_size,
        parcel_data(&resp), (uint32_t)parcel_size(&resp), 0);
    parcel_free(&resp);
}

// =============================================================================
// Dispatch de una transacción BC_TRANSACTION
// =============================================================================

static void dispatch_transaction(const struct binder_transaction_data *txn,
                                  const uint8_t *inline_data,
                                  uint8_t *reply, uint32_t *reply_size) {
    Parcel in;
    parcel_init_from(&in, inline_data, txn->data_size);
    parcel_skip_interface_token(&in);

    fprintf(stderr, "[aine-binder] txn: target.handle=%u code=%u\n",
        txn->target.handle, txn->code);

    switch (txn->code) {
        case SVC_MGR_ADD_SERVICE:
            handle_add_service(&in, reply, reply_size);
            break;
        case SVC_MGR_GET_SERVICE:
        case SVC_MGR_CHECK_SERVICE:
            handle_get_service(&in, reply, reply_size);
            break;
        case SVC_MGR_LIST_SERVICES:
            handle_list_services(&in, reply, reply_size);
            break;
        default:
            fprintf(stderr, "[aine-binder] unknown code %u\n", txn->code);
            build_br_error(reply, reply_size, -38); // ENOSYS
            break;
    }
    parcel_free(&in);
}

// =============================================================================
// Loop principal del daemon
// =============================================================================

static volatile int g_running = 1;

static void sig_handler(int sig) {
    (void)sig;
    g_running = 0;
}

int aine_binder_daemon_run(void) {
    signal(SIGTERM, sig_handler);
    signal(SIGINT,  sig_handler);
    signal(SIGPIPE, SIG_IGN);    // ignore broken pipe from disconnected clients

    int listen_fd = aine_transport_daemon_listen();
    if (listen_fd < 0) {
        fprintf(stderr, "[aine-binder] failed to listen on %s: %s\n",
            AINE_BINDER_SOCKET_PATH, strerror(errno));
        return -1;
    }
    fprintf(stderr, "[aine-binder] daemon listening on %s\n",
        AINE_BINDER_SOCKET_PATH);

    // Pre-registro del service manager (servicio IServiceManager)
    add_service("android.os.IServiceManager", 0);

    static uint8_t recv_buf[AINE_BINDER_MSG_MAX];
    static uint8_t reply_buf[AINE_BINDER_MSG_MAX];

    while (g_running) {
        // Use select to handle interruption by signal
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(listen_fd, &fds);
        struct timeval tv = {1, 0};
        int sel = select(listen_fd + 1, &fds, NULL, NULL, &tv);
        if (sel <= 0) continue;  // timeout or signal

        int client_fd = aine_transport_daemon_accept(listen_fd);
        if (client_fd < 0) continue;

        // Read one request
        uint32_t recv_size = sizeof(recv_buf);
        if (aine_transport_recv(client_fd, recv_buf, &recv_size) < 0) {
            close(client_fd);
            continue;
        }

        // Parse BC_* commands
        uint32_t pos = 0;
        uint32_t reply_size = 0;

        while (pos + 4 <= recv_size) {
            uint32_t cmd;
            memcpy(&cmd, recv_buf + pos, 4); pos += 4;

            if (cmd == BC_TRANSACTION || cmd == BC_REPLY) {
                if (pos + (uint32_t)sizeof(struct binder_transaction_data) > recv_size)
                    break;
                struct binder_transaction_data txn;
                memcpy(&txn, recv_buf + pos, sizeof(txn));
                pos += (uint32_t)sizeof(txn);
                const uint8_t *inline_data = recv_buf + pos;
                if (cmd == BC_TRANSACTION) {
                    dispatch_transaction(&txn, inline_data,
                                         reply_buf, &reply_size);
                }
                pos += (uint32_t)txn.data_size;
            } else if (cmd == BC_ENTER_LOOPER || cmd == BC_REGISTER_LOOPER) {
                uint32_t noop = BR_NOOP;
                memcpy(reply_buf, &noop, 4);
                reply_size = 4;
                break;
            } else {
                break;
            }
        }

        if (reply_size > 0)
            aine_transport_send(client_fd, reply_buf, reply_size);

        close(client_fd);
    }

    fprintf(stderr, "[aine-binder] daemon stopped\n");
    close(listen_fd);
    unlink(AINE_BINDER_SOCKET_PATH);
    return 0;
}

int aine_binder_daemon_is_running(void) {
    // Check if socket file exists and daemon accepts connections
    int fd = aine_transport_connect();
    if (fd >= 0) { close(fd); return 1; }
    return 0;
}

int aine_binder_daemon_start(void) {
    if (aine_binder_daemon_is_running()) return 0;
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        // Proceso hijo — se convierte en daemon
        setsid();
        aine_binder_daemon_run();
        _exit(0);
    }
    // Esperar a que el daemon registre su puerto
    for (int i = 0; i < 20; i++) {
        usleep(50000); // 50ms
        if (aine_binder_daemon_is_running()) return 0;
    }
    return -1;
}

// =============================================================================
// main() — solo cuando compilando el ejecutable daemon
// =============================================================================

#ifdef AINE_BINDER_MAIN
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;
    fprintf(stderr, "[aine-binder-daemon] starting...\n");
    return aine_binder_daemon_run();
}
#endif
