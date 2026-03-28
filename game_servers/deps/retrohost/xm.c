// Copyright (c) 2024 Akop Karapetyan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <nats.h>
#include "frame.pb-c.h"
#include "args.h"
#include "log.h"
#include "xm.h"

#define LOG_TAG "xm"

static natsConnection *conn = NULL;
static u_int8_t *buffer = NULL;
static size_t buffer_size = 0;

extern ArgsOptions args;

// FIXME!! config
static const char *subject = "protobuf.topic";

inline static void* get_reusable_buffer(size_t size);

// sudo apt install libprotobuf-c-dev libnats-dev protobuf-c-compiler

void xm_init()
{
    if (conn) {
        log_e(LOG_TAG, "NATS connection already initialized\n");
        return;
    }

    // Connect to NATS server
    natsOptions *opts;
    natsOptions_Create(&opts);
    natsOptions_SetURL(opts, args.server_url);
    natsStatus s = natsConnection_Connect(&conn, opts);
    natsOptions_Destroy(opts);

    if (s != NATS_OK) {
        log_e(LOG_TAG, "Error connecting to NATS server: %s\n", natsStatus_GetText(s));
        return;
    }

    log_i(LOG_TAG, "Connected to NATS server at %s\n", args.server_url);
}

void xm_publish_frame(const struct FrameGeometry *frame, const unsigned char *content, size_t size)
{
    // Create and populate the Protobuf message
    Red__Geometry gm = RED__GEOMETRY__INIT;
    gm.pitch = frame->bitmap_pitch;
    gm.width = frame->bitmap_width;
    gm.height = frame->bitmap_height;
    // FIXME - this is an enum; for now, values align
    gm.pixel_format = frame->pixel_format;
    gm.attrs = frame->attrs;

    Red__Frame fm = RED__FRAME__INIT;
    fm.content.data = (unsigned char *) content;
    fm.content.len = size;
    fm.geometry = &gm;

    // Pack the message
    size_t len = red__frame__get_packed_size(&fm);
    uint8_t *buf = get_reusable_buffer(len);
    if (!buf) {
        return;
    }
    red__frame__pack(&fm, buf);

    // Publish to NATS
    natsStatus s = natsConnection_Publish(conn, subject, buf, len);
    if (s != NATS_OK) {
        log_e(LOG_TAG, "Error publishing to '%s': %s\n", subject, natsStatus_GetText(s));
    }
}

void xm_cleanup()
{
    if (conn) {
        natsConnection_Destroy(conn);
        conn = NULL;
    }
    free(buffer);
    buffer = NULL;
    buffer_size = 0;
}

inline static void* get_reusable_buffer(size_t size)
{
    if (buffer_size < size) {
        u_int8_t *new_buffer = realloc(buffer, size);
        if (!new_buffer) {
            log_e(LOG_TAG, "Failed to allocate buffer of size %zu\n", size);
            return NULL;
        }
        buffer = new_buffer;
        buffer_size = size;
    }

    return buffer;
}
