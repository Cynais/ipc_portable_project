#include "ipc_transport.h"
#include "ipc_platform_internal.h"

#include <stdlib.h>
#include <string.h>

#define IPC_SHARED_MAGIC   UINT32_C(0x49504331) /* "IPC1" */
#define IPC_SHARED_VERSION UINT32_C(1)

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t capacity;
    uint32_t message_size;
    uint8_t data[];
} ipc_shared_area_t;

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(ipc_shared_area_t) == 16, "En-tete partage inattendu");
#endif

struct ipc_transport {
    ipc_role_t role;
    uint32_t capacity;
    ipc_shared_area_t *shared;
    ipc_platform_t platform;
};

static ipc_result_t initialize_server(
    ipc_transport_t *transport,
    const char *name,
    uint32_t capacity)
{
    size_t mapping_size = sizeof(ipc_shared_area_t) + (size_t)capacity;
    ipc_result_t result = ipc_platform_server_create(&transport->platform, name, mapping_size);

    if (result != IPC_OK) {
        return result;
    }

    transport->shared = (ipc_shared_area_t *)transport->platform.base;
    transport->capacity = capacity;

    memset(transport->shared, 0, sizeof(ipc_shared_area_t));
    transport->shared->magic = IPC_SHARED_MAGIC;
    transport->shared->version = IPC_SHARED_VERSION;
    transport->shared->capacity = capacity;
    transport->shared->message_size = 0;
    return IPC_OK;
}

static ipc_result_t initialize_client(
    ipc_transport_t *transport,
    const char *name,
    uint32_t requested_capacity)
{
    ipc_result_t result = ipc_platform_client_open(&transport->platform, name);
    size_t minimum_size;

    if (result != IPC_OK) {
        return result;
    }

    transport->shared = (ipc_shared_area_t *)transport->platform.base;

    if (transport->platform.mapping_size < sizeof(ipc_shared_area_t)) {
        return IPC_ERR_PROTOCOL;
    }
    if (transport->shared->magic != IPC_SHARED_MAGIC ||
        transport->shared->version != IPC_SHARED_VERSION) {
        return IPC_ERR_PROTOCOL;
    }

    transport->capacity = transport->shared->capacity;
    minimum_size = sizeof(ipc_shared_area_t) + (size_t)transport->capacity;
    if (transport->platform.mapping_size < minimum_size) {
        return IPC_ERR_PROTOCOL;
    }
    if (requested_capacity != 0 && requested_capacity != transport->capacity) {
        return IPC_ERR_PROTOCOL;
    }

    return IPC_OK;
}

ipc_result_t ipc_transport_open(
    ipc_transport_t **out_transport,
    const char *name,
    ipc_role_t role,
    uint32_t capacity)
{
    ipc_transport_t *transport;
    ipc_result_t result;

    if (out_transport == NULL || name == NULL || name[0] == '\0') {
        return IPC_ERR_ARGUMENT;
    }
    if (role != IPC_ROLE_CLIENT && role != IPC_ROLE_SERVER) {
        return IPC_ERR_ARGUMENT;
    }
    if (role == IPC_ROLE_SERVER && capacity == 0) {
        return IPC_ERR_ARGUMENT;
    }

    transport = (ipc_transport_t *)calloc(1, sizeof(*transport));
    if (transport == NULL) {
        return IPC_ERR_SYSTEM;
    }
    transport->role = role;

    result = (role == IPC_ROLE_SERVER)
        ? initialize_server(transport, name, capacity)
        : initialize_client(transport, name, capacity);

    if (result != IPC_OK) {
        ipc_platform_close(&transport->platform);
        free(transport);
        return result;
    }

    *out_transport = transport;
    return IPC_OK;
}

ipc_result_t ipc_transport_send(
    ipc_transport_t *transport,
    const void *buffer,
    uint32_t size)
{
    if (transport == NULL || (size > 0 && buffer == NULL)) {
        return IPC_ERR_ARGUMENT;
    }
    if (size > transport->capacity) {
        return IPC_ERR_TOO_LARGE;
    }

    if (size > 0) {
        memcpy(transport->shared->data, buffer, size);
    }
    transport->shared->message_size = size;

    return (transport->role == IPC_ROLE_CLIENT)
        ? ipc_platform_signal_request(&transport->platform)
        : ipc_platform_signal_response(&transport->platform);
}

ipc_result_t ipc_transport_receive(
    ipc_transport_t *transport,
    void *buffer,
    uint32_t buffer_size,
    uint32_t *received_size,
    uint32_t timeout_ms)
{
    ipc_result_t result;
    uint32_t message_size;

    if (transport == NULL || buffer == NULL || received_size == NULL) {
        return IPC_ERR_ARGUMENT;
    }

    result = (transport->role == IPC_ROLE_CLIENT)
        ? ipc_platform_wait_response(&transport->platform, timeout_ms)
        : ipc_platform_wait_request(&transport->platform, timeout_ms);

    if (result != IPC_OK) {
        return result;
    }

    message_size = transport->shared->message_size;
    if (message_size > transport->capacity) {
        return IPC_ERR_PROTOCOL;
    }

    *received_size = message_size;
    if (message_size > buffer_size) {
        return IPC_ERR_BUFFER_TOO_SMALL;
    }

    if (message_size > 0) {
        memcpy(buffer, transport->shared->data, message_size);
    }
    return IPC_OK;
}

uint32_t ipc_transport_capacity(const ipc_transport_t *transport)
{
    return transport ? transport->capacity : 0;
}

void ipc_transport_close(ipc_transport_t *transport)
{
    if (transport == NULL) {
        return;
    }
    ipc_platform_close(&transport->platform);
    free(transport);
}

const char *ipc_result_string(ipc_result_t result)
{
    switch (result) {
        case IPC_OK: return "OK";
        case IPC_ERR_ARGUMENT: return "argument invalide";
        case IPC_ERR_SYSTEM: return "erreur systeme";
        case IPC_ERR_TIMEOUT: return "timeout";
        case IPC_ERR_TOO_LARGE: return "message trop grand";
        case IPC_ERR_BUFFER_TOO_SMALL: return "buffer de reception trop petit";
        case IPC_ERR_PROTOCOL: return "protocole incompatible ou corrompu";
        case IPC_ERR_NOT_FOUND: return "canal introuvable";
        default: return "erreur inconnue";
    }
}
