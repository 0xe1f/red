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

#ifndef __FILES_H__
#define __FILES_H__

#include <stdbool.h>

const char* files_system_path();
const char* files_save_path();

bool files_save_sram(const char *rom_path, const void *sram_data, size_t sram_size);
bool files_restore_sram(const char *rom_path, void *sram_data, size_t sram_size);

bool files_load(const char *path);
void files_mkdirs(const char *base_path);
void files_clean_up();

#endif // __FILES_H__
