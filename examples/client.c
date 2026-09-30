#include "ipc_protocol.h"
#include "ipc_transport.h"
#include "demo_protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHANNEL_NAME "demo_ipc"
#define CHANNEL_CAPACITY (16u * 1024u * 1024u)
#define IO_TIMEOUT_MS 5000u

static ipc_result_t transact(
    ipc_transport_t *ipc,
    uint8_t *buffer,
    uint32_t capacity,
    uint32_t api_code,
    const void *payload,
    uint32_t payload_size,
    ipc_packet_view_t *response)
{
    uint32_t packet_size = 0;
    uint32_t received = 0;
    ipc_result_t rc;

    rc = ipc_packet_build(
        buffer, capacity, api_code, payload, payload_size, &packet_size);
    if (rc != IPC_OK) return rc;

    rc = ipc_transport_send(ipc, buffer, packet_size);
    if (rc != IPC_OK) return rc;

    rc = ipc_transport_receive(ipc, buffer, capacity, &received, IO_TIMEOUT_MS);
    if (rc != IPC_OK) return rc;

    return ipc_packet_parse(buffer, received, response);
}

static ipc_result_t test_set_value(ipc_transport_t *ipc, uint8_t *buffer)
{
    demo_set_value_request_t request = {7, 12.5f};
    demo_set_value_response_t response;
    ipc_packet_view_t packet;
    ipc_result_t rc = transact(
        ipc, buffer, CHANNEL_CAPACITY,
        API_SET_VALUE, &request, sizeof(request), &packet);

    if (rc != IPC_OK) return rc;
    if (packet.api_code != API_SET_VALUE || packet.payload_size != sizeof(response)) {
        return IPC_ERR_PROTOCOL;
    }

    memcpy(&response, packet.payload, sizeof(response));
    printf("Reponse SET_VALUE: status=%d applied=%.3f\n",
           response.status, response.applied_value);
    return IPC_OK;
}

static ipc_result_t test_get_image(ipc_transport_t *ipc, uint8_t *buffer)
{
    demo_image_request_t request = {5, 3};
    demo_image_header_t image;
    ipc_packet_view_t packet;
    ipc_result_t rc = transact(
        ipc, buffer, CHANNEL_CAPACITY,
        API_GET_IMAGE, &request, sizeof(request), &packet);
    uint32_t expected_payload;
    const float *pixels;
    uint32_t i;

    if (rc != IPC_OK) return rc;
    if (packet.api_code != API_GET_IMAGE || packet.payload_size < sizeof(image)) {
        return IPC_ERR_PROTOCOL;
    }

    memcpy(&image, packet.payload, sizeof(image));
    expected_payload = (uint32_t)sizeof(image) + image.width * image.height * (uint32_t)sizeof(float);
    if (packet.payload_size != expected_payload) {
        return IPC_ERR_PROTOCOL;
    }

    pixels = (const float *)(const void *)(packet.payload + sizeof(image));
    printf("Image recue: %ux%u\n", image.width, image.height);
    printf("Premiers pixels:");
    for (i = 0; i < image.width * image.height && i < 8; ++i) {
        printf(" %.2f", pixels[i]);
    }
    printf("\n");
    return IPC_OK;
}

static ipc_result_t test_quit(ipc_transport_t *ipc, uint8_t *buffer)
{
    ipc_packet_view_t packet;
    demo_status_response_t response;
    ipc_result_t rc = transact(
        ipc, buffer, CHANNEL_CAPACITY,
        API_QUIT, NULL, 0, &packet);

    if (rc != IPC_OK) return rc;
    if (packet.api_code != API_QUIT || packet.payload_size != sizeof(response)) {
        return IPC_ERR_PROTOCOL;
    }

    memcpy(&response, packet.payload, sizeof(response));
    printf("Arret serveur: status=%d\n", response.status);
    return IPC_OK;
}

int main(void)
{
    ipc_transport_t *ipc = NULL;
    uint8_t *buffer = NULL;
    ipc_result_t rc;

    rc = ipc_transport_open(&ipc, CHANNEL_NAME, IPC_ROLE_CLIENT, CHANNEL_CAPACITY);
    if (rc != IPC_OK) {
        fprintf(stderr, "Connexion client impossible: %s\n", ipc_result_string(rc));
        fprintf(stderr, "Lance d'abord le serveur.\n");
        return 1;
    }

    buffer = (uint8_t *)malloc(CHANNEL_CAPACITY);
    if (buffer == NULL) {
        ipc_transport_close(ipc);
        return 1;
    }

    rc = test_set_value(ipc, buffer);
    if (rc == IPC_OK) rc = test_get_image(ipc, buffer);
    if (rc == IPC_OK) rc = test_quit(ipc, buffer);

    if (rc != IPC_OK) {
        fprintf(stderr, "Erreur client: %s\n", ipc_result_string(rc));
    }

    free(buffer);
    ipc_transport_close(ipc);
    return rc == IPC_OK ? 0 : 1;
}
