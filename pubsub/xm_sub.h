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

#ifndef __XM_SUB_H__
#define __XM_SUB_H__

#include <stdbool.h>
#include <stddef.h>
#include "frame.pb-c.h"

typedef void (*xm_callback_t)(const Red__Frame *);

void xm_init(const char *server_url);
void xm_set_callback(xm_callback_t callback);
void xm_cleanup();

#endif // __XM_SUB_H__
