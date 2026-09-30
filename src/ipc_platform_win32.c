#ifdef _WIN32

#include "ipc_platform_internal.h"

#include <stdio.h>
#include <string.h>

static int make_name(char *dst, size_t dst_size, const char *base, const char *suffix)
{
    int n = snprintf(dst, dst_size, "Local\\%s_%s", base, suffix);
    return n > 0 && (size_t)n < dst_size;
}

static ipc_result_t create_mapping(ipc_platform_t *platform, const char *mapping_name, size_t mapping_size)
{
    DWORD high = (DWORD)(((uint64_t)mapping_size) >> 32);
    DWORD low = (DWORD)((uint64_t)mapping_size & UINT64_C(0xffffffff));

    platform->mapping = CreateFileMappingA(
        INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, high, low, mapping_name);
    if (platform->mapping == NULL) {
        return IPC_ERR_SYSTEM;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        return IPC_ERR_SYSTEM;
    }

    platform->base = MapViewOfFile(platform->mapping, FILE_MAP_ALL_ACCESS, 0, 0, mapping_size);
    if (platform->base == NULL) {
        return IPC_ERR_SYSTEM;
    }

    platform->mapping_size = mapping_size;
    return IPC_OK;
}

ipc_result_t ipc_platform_server_create(
    ipc_platform_t *platform,
    const char *name,
    size_t mapping_size)
{
    char mapping_name[128];
    char request_name[128];
    char response_name[128];
    ipc_result_t result;

    if (!make_name(mapping_name, sizeof(mapping_name), name, "shm") ||
        !make_name(request_name, sizeof(request_name), name, "req") ||
        !make_name(response_name, sizeof(response_name), name, "rsp")) {
        return IPC_ERR_ARGUMENT;
    }

    result = create_mapping(platform, mapping_name, mapping_size);
    if (result != IPC_OK) {
        ipc_platform_close(platform);
        return result;
    }

    platform->request_event = CreateEventA(NULL, FALSE, FALSE, request_name);
    if (platform->request_event == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        ipc_platform_close(platform);
        return IPC_ERR_SYSTEM;
    }

    platform->response_event = CreateEventA(NULL, FALSE, FALSE, response_name);
    if (platform->response_event == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        ipc_platform_close(platform);
        return IPC_ERR_SYSTEM;
    }

    return IPC_OK;
}

ipc_result_t ipc_platform_client_open(ipc_platform_t *platform, const char *name)
{
    char mapping_name[128];
    char request_name[128];
    char response_name[128];
    MEMORY_BASIC_INFORMATION mbi;

    if (!make_name(mapping_name, sizeof(mapping_name), name, "shm") ||
        !make_name(request_name, sizeof(request_name), name, "req") ||
        !make_name(response_name, sizeof(response_name), name, "rsp")) {
        return IPC_ERR_ARGUMENT;
    }

    platform->mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, mapping_name);
    if (platform->mapping == NULL) {
        return GetLastError() == ERROR_FILE_NOT_FOUND ? IPC_ERR_NOT_FOUND : IPC_ERR_SYSTEM;
    }

    platform->base = MapViewOfFile(platform->mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (platform->base == NULL) {
        ipc_platform_close(platform);
        return IPC_ERR_SYSTEM;
    }

    if (VirtualQuery(platform->base, &mbi, sizeof(mbi)) == 0) {
        ipc_platform_close(platform);
        return IPC_ERR_SYSTEM;
    }
    platform->mapping_size = mbi.RegionSize;

    platform->request_event = OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, request_name);
    if (platform->request_event == NULL) {
        ipc_platform_close(platform);
        return IPC_ERR_NOT_FOUND;
    }

    platform->response_event = OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, response_name);
    if (platform->response_event == NULL) {
        ipc_platform_close(platform);
        return IPC_ERR_NOT_FOUND;
    }

    return IPC_OK;
}

static ipc_result_t wait_event(HANDLE event_handle, uint32_t timeout_ms)
{
    DWORD timeout = timeout_ms == IPC_WAIT_INFINITE ? INFINITE : (DWORD)timeout_ms;
    DWORD rc = WaitForSingleObject(event_handle, timeout);

    if (rc == WAIT_OBJECT_0) return IPC_OK;
    if (rc == WAIT_TIMEOUT) return IPC_ERR_TIMEOUT;
    return IPC_ERR_SYSTEM;
}

static ipc_result_t signal_event(HANDLE event_handle)
{
    return SetEvent(event_handle) ? IPC_OK : IPC_ERR_SYSTEM;
}

ipc_result_t ipc_platform_wait_request(ipc_platform_t *platform, uint32_t timeout_ms)
{
    return wait_event(platform->request_event, timeout_ms);
}

ipc_result_t ipc_platform_signal_request(ipc_platform_t *platform)
{
    return signal_event(platform->request_event);
}

ipc_result_t ipc_platform_wait_response(ipc_platform_t *platform, uint32_t timeout_ms)
{
    return wait_event(platform->response_event, timeout_ms);
}

ipc_result_t ipc_platform_signal_response(ipc_platform_t *platform)
{
    return signal_event(platform->response_event);
}

void ipc_platform_close(ipc_platform_t *platform)
{
    if (platform == NULL) {
        return;
    }

    if (platform->base != NULL) UnmapViewOfFile(platform->base);
    if (platform->request_event != NULL) CloseHandle(platform->request_event);
    if (platform->response_event != NULL) CloseHandle(platform->response_event);
    if (platform->mapping != NULL) CloseHandle(platform->mapping);

    memset(platform, 0, sizeof(*platform));
}

#endif
