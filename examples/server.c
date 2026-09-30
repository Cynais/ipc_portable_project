#include "ipc_protocol.h"
#include "ipc_transport.h"
#include "demo_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHANNEL_NAME "demo_ipc"
#define CHANNEL_CAPACITY (16u * 1024u * 1024u)
#define IO_TIMEOUT_MS 10000u

static ipc_result_t send_packet(
    ipc_transport_t *ipc,
    uint8_t *tx,
    uint32_t tx_capacity,
    uint32_t api_code,
    const void *payload,
    uint32_t payload_size)
{
    uint32_t packet_size = 0;
    ipc_result_t rc = ipc_packet_build(
        tx, tx_capacity, api_code, payload, payload_size, &packet_size);

    if (rc != IPC_OK) return rc;
    return ipc_transport_send(ipc, tx, packet_size);
}

static ipc_result_t handle_set_value(
    ipc_transport_t *ipc,
    const ipc_packet_view_t *request,
    uint8_t *tx,
    uint32_t tx_capacity)
{
    demo_set_value_request_t input;
    demo_set_value_response_t output;

    if (request->payload_size != sizeof(input)) {
        return IPC_ERR_PROTOCOL;
    }

    memcpy(&input, request->payload, sizeof(input));
    printf("SET_VALUE: channel=%d value=%.3f\n", input.channel, input.value);

    output.status = 0;
    output.applied_value = input.value;
    return send_packet(ipc, tx, tx_capacity, API_SET_VALUE, &output, sizeof(output));
}

static ipc_result_t build_image_payload(
    const demo_image_request_t *request,
    uint8_t *payload,
    uint32_t payload_capacity,
    uint32_t *payload_size)
{
    demo_image_header_t header;
    uint64_t pixel_count = (uint64_t)request->width * request->height;
    uint64_t needed = sizeof(header) + pixel_count * sizeof(float);
    float *pixels;
    uint64_t i;

    if (request->width == 0 || request->height == 0 || needed > payload_capacity) {
        return IPC_ERR_TOO_LARGE;
    }

    header.width = request->width;
    header.height = request->height;
    memcpy(payload, &header, sizeof(header));

    pixels = (float *)(void *)(payload + sizeof(header));
    for (i = 0; i < pixel_count; ++i) {
        pixels[i] = (float)i * 0.25f;
    }

    *payload_size = (uint32_t)needed;
    return IPC_OK;
}

static ipc_result_t handle_get_image(
    ipc_transport_t *ipc,
    const ipc_packet_view_t *request,
    uint8_t *tx,
    uint32_t tx_capacity)
{
    demo_image_request_t image_request;
    uint32_t payload_size = 0;
    uint32_t header_size = (uint32_t)sizeof(ipc_packet_header_t);
    ipc_result_t rc;

    if (request->payload_size != sizeof(image_request) || tx_capacity <= header_size) {
        return IPC_ERR_PROTOCOL;
    }

    memcpy(&image_request, request->payload, sizeof(image_request));
    rc = build_image_payload(
        &image_request,
        tx + header_size,
        tx_capacity - header_size,
        &payload_size);
    if (rc != IPC_OK) return rc;

    return send_packet(
        ipc,
        tx,
        tx_capacity,
        API_GET_IMAGE,
        tx + header_size,
        payload_size);
}

static ipc_result_t handle_quit(
    ipc_transport_t *ipc,
    uint8_t *tx,
    uint32_t tx_capacity)
{
    demo_status_response_t response = {0};
    return send_packet(ipc, tx, tx_capacity, API_QUIT, &response, sizeof(response));
}

static ipc_result_t dispatch_request(
    ipc_transport_t *ipc,
    const ipc_packet_view_t *request,
    uint8_t *tx,
    uint32_t tx_capacity,
    int *should_quit)
{
    *should_quit = 0;

    switch (request->api_code) {
        case API_SET_VALUE:
            return handle_set_value(ipc, request, tx, tx_capacity);
        case API_GET_IMAGE:
            return handle_get_image(ipc, request, tx, tx_capacity);
        case API_QUIT:
            *should_quit = 1;
            return handle_quit(ipc, tx, tx_capacity);
        default:
            return IPC_ERR_PROTOCOL;
    }
}

static int server_loop(ipc_transport_t *ipc)
{
    uint8_t *rx = (uint8_t *)malloc(CHANNEL_CAPACITY);
    uint8_t *tx = (uint8_t *)malloc(CHANNEL_CAPACITY);
    int should_quit = 0;

    if (rx == NULL || tx == NULL) {
        free(rx);
        free(tx);
        return 1;
    }

    while (!should_quit) {
        uint32_t received = 0;
        ipc_packet_view_t request;
        ipc_result_t rc = ipc_transport_receive(
            ipc, rx, CHANNEL_CAPACITY, &received, IPC_WAIT_INFINITE);

        if (rc == IPC_OK) rc = ipc_packet_parse(rx, received, &request);
        if (rc == IPC_OK) rc = dispatch_request(ipc, &request, tx, CHANNEL_CAPACITY, &should_quit);

        if (rc != IPC_OK) {
            fprintf(stderr, "Erreur serveur: %s\n", ipc_result_string(rc));
            free(rx);
            free(tx);
            return 1;
        }
    }

    free(rx);
    free(tx);
    return 0;
}

int main(void)
{
    ipc_transport_t *ipc = NULL;
    ipc_result_t rc = ipc_transport_open(
        &ipc, CHANNEL_NAME, IPC_ROLE_SERVER, CHANNEL_CAPACITY);

    if (rc != IPC_OK) {
        fprintf(stderr, "Ouverture serveur impossible: %s\n", ipc_result_string(rc));
        return 1;
    }

    printf("Serveur pret. Canal '%s', capacite %u octets.\n",
           CHANNEL_NAME, ipc_transport_capacity(ipc));

    {
        int result = server_loop(ipc);
        ipc_transport_close(ipc);
        return result;
    }
}
