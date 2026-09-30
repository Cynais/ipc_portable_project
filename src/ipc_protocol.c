#include "ipc_protocol.h"

#include <string.h>

ipc_result_t ipc_packet_build(
    void *destination,
    uint32_t destination_capacity,
    uint32_t api_code,
    const void *payload,
    uint32_t payload_size,
    uint32_t *packet_size)
{
    ipc_packet_header_t header;
    uint32_t total_size;

    if (destination == NULL || packet_size == NULL) {
        return IPC_ERR_ARGUMENT;
    }
    if (payload_size > 0 && payload == NULL) {
        return IPC_ERR_ARGUMENT;
    }
    if (payload_size > UINT32_MAX - (uint32_t)sizeof(header)) {
        return IPC_ERR_TOO_LARGE;
    }

    total_size = (uint32_t)sizeof(header) + payload_size;
    if (total_size > destination_capacity) {
        return IPC_ERR_BUFFER_TOO_SMALL;
    }

    header.api_code = api_code;
    header.payload_size = payload_size;

    memcpy(destination, &header, sizeof(header));
    if (payload_size > 0) {
        memcpy((uint8_t *)destination + sizeof(header), payload, payload_size);
    }

    *packet_size = total_size;
    return IPC_OK;
}

ipc_result_t ipc_packet_parse(
    const void *packet,
    uint32_t packet_size,
    ipc_packet_view_t *view)
{
    ipc_packet_header_t header;
    uint32_t expected_size;

    if (packet == NULL || view == NULL) {
        return IPC_ERR_ARGUMENT;
    }
    if (packet_size < sizeof(header)) {
        return IPC_ERR_PROTOCOL;
    }

    memcpy(&header, packet, sizeof(header));

    if (header.payload_size > UINT32_MAX - (uint32_t)sizeof(header)) {
        return IPC_ERR_PROTOCOL;
    }

    expected_size = (uint32_t)sizeof(header) + header.payload_size;
    if (expected_size != packet_size) {
        return IPC_ERR_PROTOCOL;
    }

    view->api_code = header.api_code;
    view->payload_size = header.payload_size;
    view->payload = (const uint8_t *)packet + sizeof(header);
    return IPC_OK;
}
