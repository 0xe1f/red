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

#include "mq.h"

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "log.h"

#define LOG_TAG "mq"

bool mq_init(MessageQueue *mq, const char *fifo_path)
{
    if (mkfifo(fifo_path, 0666) == -1 && errno != EEXIST) {
        log_e(LOG_TAG, "Failed to create FIFO at path: %s\n", fifo_path);
        return false;
    }

    if ((mq->fifo_fd = open(fifo_path, O_RDONLY | O_NONBLOCK)) == -1) {
        log_e(LOG_TAG, "Failed to open FIFO at path: %s\n", fifo_path);
        return false;
    }

    log_d(LOG_TAG, "Message queue initialized at path: %s\n", fifo_path);

    return true;
}

bool mq_cleanup(MessageQueue *mq)
{
    if (mq->fifo_fd == -1) {
        return false;
    }

    close(mq->fifo_fd);
    mq->fifo_fd = -1;
    log_d(LOG_TAG, "Message queue destroyed\n");

    return true;
}

bool mq_read_message(const MessageQueue *mq, MessagePayload *payload)
{
    ssize_t bytes_read = read(mq->fifo_fd, payload, sizeof(MessagePayload));
    if (bytes_read == sizeof(MessagePayload)) {
        // success
        return true;
    } else if (bytes_read == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        // Pipe is empty, but writers are connected. Normal state.
    } else if (bytes_read == 0) {
        // EOF: No writers currently connected to the pipe. Normal state.
    } else if (bytes_read > 0) {
        // Partial read (should never happen for payloads under 4KB on Linux FIFOs)
        log_w(LOG_TAG, "Partial read of %zd bytes!\n", bytes_read);
    }

    return false;
}
