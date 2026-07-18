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

#ifndef __MQ_H__
#define __MQ_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int fifo_fd;
} MessageQueue;

typedef struct {
    uint32_t message_id;
    uint8_t padding[60];
    uint8_t data[4032];
} MessagePayload;

bool mq_init(MessageQueue *mq, const char *fifo_path);
bool mq_cleanup(MessageQueue *mq);
bool mq_read_message(const MessageQueue *mq, MessagePayload *payload);

#endif // __MQ_H__
