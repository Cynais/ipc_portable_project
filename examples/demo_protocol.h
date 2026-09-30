#ifndef DEMO_PROTOCOL_H
#define DEMO_PROTOCOL_H

#include <stdint.h>

enum {
    API_SET_VALUE = 1,
    API_GET_IMAGE = 2,
    API_QUIT = 3
};

typedef struct {
    int32_t channel;
    float value;
} demo_set_value_request_t;

typedef struct {
    int32_t status;
    float applied_value;
} demo_set_value_response_t;

typedef struct {
    uint32_t width;
    uint32_t height;
} demo_image_request_t;

typedef struct {
    uint32_t width;
    uint32_t height;
} demo_image_header_t;

typedef struct {
    int32_t status;
} demo_status_response_t;

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
_Static_assert(sizeof(demo_set_value_request_t) == 8, "layout inattendu");
_Static_assert(sizeof(demo_set_value_response_t) == 8, "layout inattendu");
_Static_assert(sizeof(demo_image_request_t) == 8, "layout inattendu");
_Static_assert(sizeof(demo_image_header_t) == 8, "layout inattendu");
_Static_assert(sizeof(demo_status_response_t) == 4, "layout inattendu");
#endif

#endif
