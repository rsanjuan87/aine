// AINE: src/aine-binder/macos/binder-transport.h
// Transporte Unix socket para Binder IPC en macOS
// Reemplaza Mach bootstrap (que requiere privilegios en macOS moderno)
#pragma once

#include "../include/aine-binder.h"
#include <stdint.h>
#include <stddef.h>

// Socket path del daemon
#define AINE_BINDER_SOCKET_PATH "/tmp/aine-binder.sock"

// Conectar al daemon (crea socket y conecta). Devuelve fd del socket o -1.
int aine_transport_connect(void);

// Enviar datos por socket (síncrono, escribe size + data)
int aine_transport_send(int sock_fd, const void *data, uint32_t size);

// Recibir datos por socket (síncrono, lee size + data)
int aine_transport_recv(int sock_fd, void *buf, uint32_t *size);

// Daemon: crear socket de escucha. Devuelve fd del listening socket o -1.
int aine_transport_daemon_listen(void);

// Daemon: aceptar una conexión. Devuelve fd del cliente o -1.
int aine_transport_daemon_accept(int listen_fd);
