// Copyright (c) 2026 Akop Karapetyan
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
#include "aud_frame.h"
#include "log.h"
#include "audd_xm.h"
#include "subjects.h"

#define LOG_TAG "audd_xm"
#define DECOMP_MAX (1024 * 1024)

static void message_handler(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure);
static int decompress(const char *src, int src_len, size_t *out_len);

static natsConnection  *conn            = NULL;
static natsSubscription *sub_handle     = NULL;
static xm_callback_t    frame_callback  = NULL;
static uint8_t         *decomp_buf      = NULL;
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
    s = natsConnection_Subscribe(&sub_handle, conn, SUBJECT_AUDIO_FRAMES, message_handler, NULL);
    if (s != NATS_OK) {
        log_e(LOG_TAG, "Error subscribing to audio frames subject '%s': %s\n",
            SUBJECT_AUDIO_FRAMES, natsStatus_GetText(s));
        return;
    }

    log_i(LOG_TAG, "Connected to NATS server at %s\n", server_url);
    log_i(LOG_TAG, "Subscribed to '%s'\n", SUBJECT_AUDIO_FRAMES);
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

// Audio messages carry a header plus a raw LZ4 block. The uncompressed size is
// not on the wire, so the destination buffer grows until decompression succeeds.
static int decompress(const char *src, int src_len, size_t *out_len)
{
    size_t cap = decomp_buf_size > 0 ? decomp_buf_size : 65536;
    for (;;) {
        if (cap > decomp_buf_size) {
            uint8_t *grown = realloc(decomp_buf, cap);
            if (!grown) {
                log_e(LOG_TAG, "Failed to allocate decompression buffer\n");
                return -1;
            }
            decomp_buf = grown;
            decomp_buf_size = cap;
        }

        int result = LZ4_decompress_safe(src, (char *)decomp_buf, src_len, (int)cap);
        if (result >= 0) {
            *out_len = (size_t)result;
            return 0;
        }
        if (cap >= DECOMP_MAX) {
            log_e(LOG_TAG, "LZ4 decompression failed: %d\n", result);
            return -1;
        }
        cap *= 2;
    }
}

static void message_handler(natsConnection *nc, natsSubscription *sub, natsMsg *msg, void *closure)
{
    const uint8_t *data = (const uint8_t *)natsMsg_GetData(msg);
    int len = natsMsg_GetDataLength(msg);

    if (len < (int)sizeof(AudFrameHeader)) {
        log_e(LOG_TAG, "Message too short: %d bytes\n", len);
        natsMsg_Destroy(msg);
        return;
    }

    // Read header directly from message buffer (zero-copy)
    const AudFrameHeader *hdr = (const AudFrameHeader *)data;
    const char *compressed = (const char *)(data + sizeof(AudFrameHeader));
    int compressed_len = len - (int)sizeof(AudFrameHeader);

    size_t out_len = 0;
    if (decompress(compressed, compressed_len, &out_len) != 0) {
        natsMsg_Destroy(msg);
        return;
    }

    if (frame_callback) {
        AudFrame frame = {
            .header       = *hdr,
            .content      = decomp_buf,
            .content_size = out_len,
        };
        frame_callback(&frame);
    }

    natsMsg_Destroy(msg);
}
