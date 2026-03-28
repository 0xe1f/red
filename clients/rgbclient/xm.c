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

#define ARENA_IMPLEMENTATION

#include <nats/nats.h>
#include "frame.pb-c.h"
#include "arena.h"
#include "log.h"
#include "xm.h"

#define LOG_TAG "xm"

static natsConnection *conn = NULL;
static natsSubscription *sub = NULL;
static xm_callback_t frame_callback = NULL;
static Arena default_arena = {0};
static ProtobufCAllocator allocator = {
    .alloc = allocator_alloc,
    .free = allocator_free,
    .allocator_data = &default_arena
};

static void message_handler(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure);
inline static void* allocator_alloc(void *allocator_data, size_t size);
inline static void allocator_free(void *allocator_data, void *pointer);

static const char *subject = "red.frames";

void xm_init(const char *server_url)
{
    if (conn) {
        log_e(LOG_TAG, "NATS connection already initialized\n");
        return;
    }

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
    arena_free(&default_arena);
}

static void message_handler(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure)
{
    // Receive payload
    const uint8_t *data = (const uint8_t *) natsMsg_GetData(msg);
    size_t len = natsMsg_GetDataLength(msg);

    // Unpack message
    Red__Frame *frame = red__frame__unpack(&allocator, len, data);
    if (frame == NULL) {
        log_e(LOG_TAG, "Failed to unpack message\n");
    } else {
        if (frame_callback) {
            // Invoke the callback with the unpacked frame
            frame_callback(frame);
        }
        // Free the unpacked frame
        red__frame__free_unpacked(frame, &allocator);
    }

    // Destroy the NATS message object
    natsMsg_Destroy(msg);
}

inline static void* allocator_alloc(void *allocator_data, size_t size)
{
    return arena_alloc((Arena *) allocator_data, size);
}

inline static void allocator_free(void *allocator_data, void *pointer)
{
    arena_free((Arena *) allocator_data);
}
