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
#include <lz4.h>
#include "frame.pb-c.h"
#include "log.h"
#include "arena.h"
#include "xm_pub.h"

#define LOG_TAG "xm_pub"

static natsConnection *conn = NULL;
static Arena nats_arena = {0};
static Arena compress_arena = {0};

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

    natsStatus s;
    if ((s = natsOptions_SetMaxReconnect(opts, 50)) != NATS_OK) {
        log_e(LOG_TAG, "Error setting max reconnect: %s\n", natsStatus_GetText(s));
        natsOptions_Destroy(opts);
        return;
    }
    if ((s = natsOptions_SetReconnectWait(opts, 250)) != NATS_OK) {
        log_e(LOG_TAG, "Error setting reconnect wait: %s\n", natsStatus_GetText(s));
        natsOptions_Destroy(opts);
        return;
    }
    if ((s = natsOptions_SetRetryOnFailedConnect(opts, true, NULL, NULL)) != NATS_OK) {
        log_e(LOG_TAG, "Error setting retry on failed connect: %s\n", natsStatus_GetText(s));
        natsOptions_Destroy(opts);
        return;
    }
    if ((s = natsOptions_SetURL(opts, server_url)) != NATS_OK) {
        log_e(LOG_TAG, "Error setting NATS server URL: %s\n", natsStatus_GetText(s));
        natsOptions_Destroy(opts);
        return;
    }

    s = natsConnection_Connect(&conn, opts);
    natsOptions_Destroy(opts);

    if (s != NATS_OK) {
        log_e(LOG_TAG, "Error connecting to NATS server: %s\n", natsStatus_GetText(s));
        return;
    }

    log_i(LOG_TAG, "Connected to NATS server at %s\n", server_url);
    log_i(LOG_TAG, "Will publish frames to '%s'\n", subject);
}

void xm_publish_frame(const Red__Geometry *geometry, const unsigned char *content, size_t size)
{
    // Prepare space to compress content
    const int max_compressed_size = LZ4_compressBound(size);
    arena_reset(&compress_arena);
    u_int8_t *compressed_data = arena_alloc(&compress_arena, max_compressed_size);
    if (!compressed_data) {
        log_v(LOG_TAG, "Failed to get compression buffer\n");
        return;
    }

    // Compress content
    const int compressed_size = LZ4_compress_default((char *) content, (char *) compressed_data,
        size, max_compressed_size);

    // log_i(LOG_TAG, "size: %d => %d\n", size, compressed_size);

    // Set up frame
    Red__Frame fm = RED__FRAME__INIT;
    fm.content.data = compressed_data;
    fm.content.len = compressed_size;
    fm.geometry = (Red__Geometry *) geometry;

    // Pack the message
    size_t len = red__frame__get_packed_size(&fm);
    arena_reset(&nats_arena);
    uint8_t *buf = arena_alloc(&nats_arena, len);
    if (!buf) {
        log_v(LOG_TAG, "Failed to get NATS buffer\n");
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
    arena_free(&nats_arena);
    arena_free(&compress_arena);
}
