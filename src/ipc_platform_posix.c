#ifndef _WIN32

#define _POSIX_C_SOURCE 200809L

#include "ipc_platform_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int make_names(ipc_platform_t *platform, const char *name)
{
    int a = snprintf(platform->shm_name, sizeof(platform->shm_name), "/%s_shm", name);
    int b = snprintf(platform->request_name, sizeof(platform->request_name), "/%s_req", name);
    int c = snprintf(platform->response_name, sizeof(platform->response_name), "/%s_rsp", name);

    return a > 0 && (size_t)a < sizeof(platform->shm_name) &&
           b > 0 && (size_t)b < sizeof(platform->request_name) &&
           c > 0 && (size_t)c < sizeof(platform->response_name);
}

static ipc_result_t map_server(ipc_platform_t *platform, size_t mapping_size)
{
    platform->shm_fd = shm_open(platform->shm_name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (platform->shm_fd < 0) {
        return IPC_ERR_SYSTEM;
    }

    if (ftruncate(platform->shm_fd, (off_t)mapping_size) != 0) {
        return IPC_ERR_SYSTEM;
    }

    platform->base = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE, MAP_SHARED, platform->shm_fd, 0);
    if (platform->base == MAP_FAILED) {
        platform->base = NULL;
        return IPC_ERR_SYSTEM;
    }

    platform->mapping_size = mapping_size;
    return IPC_OK;
}

static ipc_result_t create_semaphores(ipc_platform_t *platform)
{
    platform->request_sem = sem_open(platform->request_name, O_CREAT | O_EXCL, 0600, 0);
    if (platform->request_sem == SEM_FAILED) {
        platform->request_sem = NULL;
        return IPC_ERR_SYSTEM;
    }

    platform->response_sem = sem_open(platform->response_name, O_CREAT | O_EXCL, 0600, 0);
    if (platform->response_sem == SEM_FAILED) {
        platform->response_sem = NULL;
        return IPC_ERR_SYSTEM;
    }
    return IPC_OK;
}

ipc_result_t ipc_platform_server_create(
    ipc_platform_t *platform,
    const char *name,
    size_t mapping_size)
{
    ipc_result_t result;

    if (!make_names(platform, name)) {
        return IPC_ERR_ARGUMENT;
    }

    platform->shm_fd = -1;
    platform->owner = 1;

    result = map_server(platform, mapping_size);
    if (result != IPC_OK) {
        ipc_platform_close(platform);
        return result;
    }

    result = create_semaphores(platform);
    if (result != IPC_OK) {
        ipc_platform_close(platform);
        return result;
    }

    return IPC_OK;
}

ipc_result_t ipc_platform_client_open(ipc_platform_t *platform, const char *name)
{
    struct stat st;

    if (!make_names(platform, name)) {
        return IPC_ERR_ARGUMENT;
    }

    platform->shm_fd = shm_open(platform->shm_name, O_RDWR, 0600);
    if (platform->shm_fd < 0) {
        return (errno == ENOENT) ? IPC_ERR_NOT_FOUND : IPC_ERR_SYSTEM;
    }

    if (fstat(platform->shm_fd, &st) != 0 || st.st_size <= 0) {
        ipc_platform_close(platform);
        return IPC_ERR_SYSTEM;
    }

    platform->mapping_size = (size_t)st.st_size;
    platform->base = mmap(NULL, platform->mapping_size, PROT_READ | PROT_WRITE, MAP_SHARED, platform->shm_fd, 0);
    if (platform->base == MAP_FAILED) {
        platform->base = NULL;
        ipc_platform_close(platform);
        return IPC_ERR_SYSTEM;
    }

    platform->request_sem = sem_open(platform->request_name, 0);
    if (platform->request_sem == SEM_FAILED) {
        platform->request_sem = NULL;
        ipc_platform_close(platform);
        return IPC_ERR_NOT_FOUND;
    }

    platform->response_sem = sem_open(platform->response_name, 0);
    if (platform->response_sem == SEM_FAILED) {
        platform->response_sem = NULL;
        ipc_platform_close(platform);
        return IPC_ERR_NOT_FOUND;
    }

    return IPC_OK;
}

static ipc_result_t wait_sem(sem_t *sem, uint32_t timeout_ms)
{
    int rc;

    if (timeout_ms == IPC_WAIT_INFINITE) {
        do {
            rc = sem_wait(sem);
        } while (rc != 0 && errno == EINTR);
        return rc == 0 ? IPC_OK : IPC_ERR_SYSTEM;
    }

    {
        struct timespec deadline;
        uint64_t nanoseconds;

        if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
            return IPC_ERR_SYSTEM;
        }

        deadline.tv_sec += (time_t)(timeout_ms / 1000u);
        nanoseconds = (uint64_t)deadline.tv_nsec + (uint64_t)(timeout_ms % 1000u) * UINT64_C(1000000);
        deadline.tv_sec += (time_t)(nanoseconds / UINT64_C(1000000000));
        deadline.tv_nsec = (long)(nanoseconds % UINT64_C(1000000000));

        do {
            rc = sem_timedwait(sem, &deadline);
        } while (rc != 0 && errno == EINTR);
    }

    if (rc == 0) {
        return IPC_OK;
    }
    return errno == ETIMEDOUT ? IPC_ERR_TIMEOUT : IPC_ERR_SYSTEM;
}

static ipc_result_t signal_sem(sem_t *sem)
{
    return sem_post(sem) == 0 ? IPC_OK : IPC_ERR_SYSTEM;
}

ipc_result_t ipc_platform_wait_request(ipc_platform_t *platform, uint32_t timeout_ms)
{
    return wait_sem(platform->request_sem, timeout_ms);
}

ipc_result_t ipc_platform_signal_request(ipc_platform_t *platform)
{
    return signal_sem(platform->request_sem);
}

ipc_result_t ipc_platform_wait_response(ipc_platform_t *platform, uint32_t timeout_ms)
{
    return wait_sem(platform->response_sem, timeout_ms);
}

ipc_result_t ipc_platform_signal_response(ipc_platform_t *platform)
{
    return signal_sem(platform->response_sem);
}

void ipc_platform_close(ipc_platform_t *platform)
{
    if (platform == NULL) {
        return;
    }

    if (platform->base != NULL && platform->mapping_size > 0) {
        munmap(platform->base, platform->mapping_size);
    }
    if (platform->request_sem != NULL) {
        sem_close(platform->request_sem);
    }
    if (platform->response_sem != NULL) {
        sem_close(platform->response_sem);
    }
    if (platform->shm_fd >= 0) {
        close(platform->shm_fd);
    }

    if (platform->owner) {
        if (platform->request_name[0] != '\0') sem_unlink(platform->request_name);
        if (platform->response_name[0] != '\0') sem_unlink(platform->response_name);
        if (platform->shm_name[0] != '\0') shm_unlink(platform->shm_name);
    }

    platform->base = NULL;
    platform->mapping_size = 0;
    platform->request_sem = NULL;
    platform->response_sem = NULL;
    platform->shm_fd = -1;
}

#endif
