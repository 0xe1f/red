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
#include "log.h"
#include "xm.h"

#define LOG_TAG "xm"

static natsConnection *conn = NULL;
static natsSubscription *sub = NULL;

// FIXME!! config
static const char *subject = "protobuf.topic";

static xm_callback_t frame_callback = NULL;
static struct FrameGeometry geometry;

static void message_handler(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure);

void xm_init(const char *server_url)
{
    if (conn) {
        log_e(LOG_TAG, "NATS connection already initialized\n");
        return;
    }

    geometry.magic = MAGIC_NUMBER;

    // Connect to NATS server
    natsOptions *opts;
    natsOptions_Create(&opts);
    natsOptions_SetURL(opts, server_url);
    natsStatus s = natsConnection_Connect(&conn, opts);
    natsOptions_Destroy(opts);

    if (s != NATS_OK) {
        log_e(LOG_TAG, "Error connecting to NATS server: %s\n", natsStatus_GetText(s));
        return;
    }

    // Subscribe to a subject with a message handler
    s = natsConnection_Subscribe(&sub, conn, subject, message_handler, NULL);
    if (s != NATS_OK) {
        log_e(LOG_TAG, "Error subscribing to subject '%s': %s\n",
            subject, natsStatus_GetText(s));
        return;
    }

    log_i(LOG_TAG, "Connected to NATS server at %s\n", server_url);
    log_i(LOG_TAG, "Subscribed to '%s'\n", subject);
}

void xm_set_callback(xm_callback_t callback)
{
    frame_callback = callback;
}

void xm_cleanup()
{
    if (sub) {
        natsSubscription_Destroy(sub);
        sub = NULL;
    }
    if (conn) {
        natsConnection_Destroy(conn);
        conn = NULL;
    }
}

static void message_handler(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure)
{
    // Receive payload
    const uint8_t *data = (const uint8_t *) natsMsg_GetData(msg);
    size_t len = natsMsg_GetDataLength(msg);

    // Unpack message
    Red__Frame *frame;
    frame = red__frame__unpack(NULL, len, data);

    if (frame == NULL) {
        log_e(LOG_TAG, "Failed to unpack message\n");
    } else {
        Red__Geometry *fg = frame->geometry;
        geometry.buffer_size = frame->content.len;
        geometry.pixel_format = fg->pixel_format;
        geometry.attrs = fg->attrs;
        geometry.bitmap_width = fg->width;
        geometry.bitmap_height = fg->height;
        geometry.bitmap_pitch = fg->pitch;

        if (frame_callback) {
            frame_callback(&geometry, frame->content.data);
        }

        // 4. Free the unpacked message structure
        red__frame__free_unpacked(frame, NULL);
    }

    // Destroy the NATS message object
    natsMsg_Destroy(msg);
}
