#ifndef IPC_TRANSPORT_H
#define IPC_TRANSPORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ipc_transport ipc_transport_t;

typedef enum {
    IPC_ROLE_CLIENT = 1,
    IPC_ROLE_SERVER = 2
} ipc_role_t;

typedef enum {
    IPC_OK = 0,
    IPC_ERR_ARGUMENT = -1,
    IPC_ERR_SYSTEM = -2,
    IPC_ERR_TIMEOUT = -3,
    IPC_ERR_TOO_LARGE = -4,
    IPC_ERR_BUFFER_TOO_SMALL = -5,
    IPC_ERR_PROTOCOL = -6,
    IPC_ERR_NOT_FOUND = -7
} ipc_result_t;

#define IPC_WAIT_INFINITE UINT32_MAX

/*
 * Le serveur cree le canal. Le client l'ouvre ensuite.
 * capacity = taille maximale d'un message, en octets.
 */
ipc_result_t ipc_transport_open(
    ipc_transport_t **out_transport,
    const char *name,
    ipc_role_t role,
    uint32_t capacity);

/*
 * Envoie exactement size octets.
 * Client : publie une requete.
 * Serveur: publie une reponse.
 */
ipc_result_t ipc_transport_send(
    ipc_transport_t *transport,
    const void *buffer,
    uint32_t size);

/*
 * Recoit un message complet.
 * received_size recoit la taille du message effectivement copie.
 */
ipc_result_t ipc_transport_receive(
    ipc_transport_t *transport,
    void *buffer,
    uint32_t buffer_size,
    uint32_t *received_size,
    uint32_t timeout_ms);

uint32_t ipc_transport_capacity(const ipc_transport_t *transport);

void ipc_transport_close(ipc_transport_t *transport);

const char *ipc_result_string(ipc_result_t result);

#ifdef __cplusplus
}
#endif

#endif
