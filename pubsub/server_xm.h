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

#ifndef __SERVER_XM_H__
#define __SERVER_XM_H__

#include <stddef.h>
#include "vid_frame.h"
#include "aud_frame.h"
#include "requests.pb-c.h"
#include "responses.pb-c.h"

typedef void (*RequestHandler)(const RequestEnvelope *request, ResponseEnvelope *response);

void xm_init(const char *server_url);
void xm_publish_vid_frame(const VidFrameHeader *geometry, const void *content, size_t size);
void xm_publish_aud_frame(const AudFrameHeader *header, const void *content, size_t size);
void xm_poll_requests(const RequestHandler handler);
void xm_cleanup();

#endif // __SERVER_XM_H__
