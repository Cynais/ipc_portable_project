#ifndef IPC_PLATFORM_INTERNAL_H
#define IPC_PLATFORM_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include "ipc_transport.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef struct {
    HANDLE mapping;
    HANDLE request_event;
    HANDLE response_event;
    void *base;
    size_t mapping_size;
} ipc_platform_t;
#else
#include <semaphore.h>
typedef struct {
    int shm_fd;
    sem_t *request_sem;
    sem_t *response_sem;
    void *base;
    size_t mapping_size;
    char shm_name[128];
    char request_name[128];
    char response_name[128];
    int owner;
} ipc_platform_t;
#endif

ipc_result_t ipc_platform_server_create(
    ipc_platform_t *platform,
    const char *name,
    size_t mapping_size);

ipc_result_t ipc_platform_client_open(
    ipc_platform_t *platform,
    const char *name);

ipc_result_t ipc_platform_wait_request(ipc_platform_t *platform, uint32_t timeout_ms);
ipc_result_t ipc_platform_signal_request(ipc_platform_t *platform);
ipc_result_t ipc_platform_wait_response(ipc_platform_t *platform, uint32_t timeout_ms);
ipc_result_t ipc_platform_signal_response(ipc_platform_t *platform);

void ipc_platform_close(ipc_platform_t *platform);

#endif
