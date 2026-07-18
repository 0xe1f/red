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

#ifndef __CORE_H__
#define __CORE_H__

#include "retro_symbols.h"

typedef struct {
    void *solib;
#define DEFINE_STRUCT_MEMBER(name, ret_type, ...) \
    ret_type (*name)(__VA_ARGS__);
    
    RETRO_SYMBOLS(DEFINE_STRUCT_MEMBER)
    
#undef DEFINE_STRUCT_MEMBER
} CoreFn;

bool core_open(CoreFn* core, const char* so_path);
void core_close(CoreFn* core);

#endif // __CORE_H__
