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
#include <nats/nats.h>
#include <lz4.h>
#include "frame.h"
#include "log.h"
#include "xm_sub.h"

#define LOG_TAG "xm_sub"

static void message_handler(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure);

static const char *subject = "red.frames";

static natsConnection  *conn           = NULL;
static natsSubscription *sub_handle    = NULL;
static xm_callback_t    frame_callback = NULL;
static uint8_t         *decomp_buf     = NULL;
static size_t           decomp_buf_size = 0;

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

    // Subscribe to frame subject
    s = natsConnection_Subscribe(&sub_handle, conn, subject, message_handler, NULL);
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
    if (sub_handle) {
        natsSubscription_Destroy(sub_handle);
        sub_handle = NULL;
    }
    if (conn) {
        natsConnection_Destroy(conn);
        conn = NULL;
    }
    free(decomp_buf);
    decomp_buf = NULL;
    decomp_buf_size = 0;
}

static void message_handler(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure)
{
    const uint8_t *data = (const uint8_t *)natsMsg_GetData(msg);
    int len = natsMsg_GetDataLength(msg);

    if (len < (int)sizeof(FrameHeader)) {
        log_e(LOG_TAG, "Message too short: %d bytes\n", len);
        natsMsg_Destroy(msg);
        return;
    }

    // Read header directly from message buffer (zero-copy)
    const FrameHeader *hdr = (const FrameHeader *)data;
    size_t decomp_size = (size_t)hdr->pitch * hdr->height;

    // Grow decompression buffer only when needed (rare: resolution change)
    if (decomp_size > decomp_buf_size) {
        free(decomp_buf);
        decomp_buf = malloc(decomp_size);
        if (!decomp_buf) {
            log_e(LOG_TAG, "Failed to allocate decompression buffer\n");
            decomp_buf_size = 0;
            natsMsg_Destroy(msg);
            return;
        }
        decomp_buf_size = decomp_size;
    }

    // Decompress content
    const char *compressed = (const char *)(data + sizeof(FrameHeader));
    int compressed_len = len - (int)sizeof(FrameHeader);
    int result = LZ4_decompress_safe(compressed, (char *)decomp_buf, compressed_len, decomp_size);
    if (result < 0) {
        log_e(LOG_TAG, "LZ4 decompression failed: %d\n", result);
        natsMsg_Destroy(msg);
        return;
    }

    if (frame_callback) {
        Frame frame = {
            .header       = *hdr,
            .content      = decomp_buf,
            .content_size = (size_t)result,
        };
        frame_callback(&frame);
    }

    natsMsg_Destroy(msg);
}
