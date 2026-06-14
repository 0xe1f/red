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

#include <stdlib.h>
#include <string.h>
#include <nats/nats.h>
#include <lz4.h>
#include "frame.h"
#include "log.h"
#include "xm_pub.h"

#define LOG_TAG "xm_pub"

static natsConnection *conn = NULL;
static uint8_t *msg_buf      = NULL;
static size_t   msg_buf_size = 0;

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
    if ((s = natsOptions_SetSendAsap(opts, true)) != NATS_OK) {
        log_e(LOG_TAG, "Error setting send asap: %s\n", natsStatus_GetText(s));
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

void xm_publish_frame(const FrameHeader *geometry, const unsigned char *content, size_t size)
{
    // Ensure message buffer is large enough for header + worst-case compressed content
    int max_compressed = LZ4_compressBound(size);
    size_t needed = sizeof(FrameHeader) + max_compressed;
    if (needed > msg_buf_size) {
        free(msg_buf);
        msg_buf = malloc(needed);
        if (!msg_buf) {
            log_e(LOG_TAG, "Failed to allocate message buffer\n");
            msg_buf_size = 0;
            return;
        }
        msg_buf_size = needed;
    }

    // Write header, then compress pixel data directly into the remainder
    memcpy(msg_buf, geometry, sizeof(FrameHeader));
    int compressed_size = LZ4_compress_fast(
        (const char *)content,
        (char *)msg_buf + sizeof(FrameHeader),
        size, max_compressed, 4
    );
    if (compressed_size <= 0) {
        log_e(LOG_TAG, "LZ4 compression failed\n");
        return;
    }

    // Publish to NATS
    natsStatus s = natsConnection_Publish(conn, subject, msg_buf, sizeof(FrameHeader) + compressed_size);
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
    free(msg_buf);
    msg_buf = NULL;
    msg_buf_size = 0;
}
