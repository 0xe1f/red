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

#include "core.h"

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

bool core_open(CoreFn *core, const char *so_path) {
    memset(core, 0, sizeof(*core));

    if (!(core->solib = dlopen(so_path, RTLD_LAZY | RTLD_LOCAL))) {
        fprintf(stderr, "Failed to load core: %s\n", dlerror());
        return false;
    }

#define LOAD_STRUCT_MEMBER(name, ret_type, ...) \
    core->name = (ret_type (*)(__VA_ARGS__))dlsym(core->solib, #name);

    RETRO_SYMBOLS(LOAD_STRUCT_MEMBER)

#undef LOAD_STRUCT_MEMBER

    return true;
}

void core_close(CoreFn *core) {
    if (core->solib) {
        dlclose(core->solib);
        core->solib = NULL;
    }
    memset(core, 0, sizeof(*core));
}
