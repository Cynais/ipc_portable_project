#ifndef IPC_PROTOCOL_H
#define IPC_PROTOCOL_H

#include <stdint.h>
#include "ipc_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t api_code;
    uint32_t payload_size;
} ipc_packet_header_t;

typedef struct {
    uint32_t api_code;
    const uint8_t *payload;
    uint32_t payload_size;
} ipc_packet_view_t;

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(ipc_packet_header_t) == 8, "ipc_packet_header_t doit faire 8 octets");
#endif

ipc_result_t ipc_packet_build(
    void *destination,
    uint32_t destination_capacity,
    uint32_t api_code,
    const void *payload,
    uint32_t payload_size,
    uint32_t *packet_size);

ipc_result_t ipc_packet_parse(
    const void *packet,
    uint32_t packet_size,
    ipc_packet_view_t *view);

#ifdef __cplusplus
}
#endif

#endif
