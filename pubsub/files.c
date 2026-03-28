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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "unzip.h"
#include "un7z.h"
#include "libretro.h"
#include "args.h"
#include "log.h"
#include "files.h"

#define LOG_TAG "files"

static const char *preload_blocklist = "iso|cue|mp3|chd";
static const char *system_path_name = "bios";
static const char *save_path_name = "save";
static char system_path[1024];
static char save_path[1024];

extern struct retro_system_info system_info;
extern const struct retro_system_content_info_override *content_info;
extern struct retro_game_info *game_info;
extern struct retro_game_info_ext *game_info_ext;
extern ArgsOptions args;

static bool is_path_supported(const char *path, const char *ext_delim);
static bool has_extension(const char *path, const char *ext);
static bool load_zip(const char *path);
static bool load_7z(const char *path);
static bool load_direct(const char *path);
static bool extension_one_of(const char *ext, const char *ext_delim);

static bool extension_one_of(const char *ext, const char *ext_delim)
{
    for (const char *p = ext_delim; *p; p++) {
        if (strncmp(p, ext, strlen(ext)) == 0 &&
            (*(p + strlen(ext)) == '|' || *(p + strlen(ext)) == '\0')) {
            return true;
        }
        // skip to next extension after pipe
        while (*p && *p != '|') p++;
    }
    return false;
}

static bool is_path_supported(const char *path, const char *ext_delim)
{
    const char *ext = strrchr(path, '.');
    if (!ext) {
        log_w(LOG_TAG, "No file extension found: %s\n", path);
        return false;
    }
    ext++; // skip the dot

    if (ext_delim && extension_one_of(ext, ext_delim)) {
        return true;
    }

    for (const struct retro_system_content_info_override *info = content_info; info && info->extensions; info++) {
        if (extension_one_of(ext, info->extensions)) {
            return true;
        }
    }

    return false;
}

static bool has_extension(const char *path, const char *ext)
{
    const char *file_ext = strrchr(path, '.');
    return file_ext && strcasecmp(file_ext + 1, ext) == 0;
}

static bool load_zip(const char *path)
{
    static struct retro_game_info info_local;
    static struct retro_game_info_ext info_local_ext;
    info_local = (struct retro_game_info){ 0 };
    info_local_ext = (struct retro_game_info_ext){ 0 };

    unzFile fd = unzOpen(path);
    if (!fd) {
        log_e(LOG_TAG, "Failed to open zip: %s\n", path);
        return false;
    }

    static char archived_path[1024];
    static char archived_filename[512];
    static char archived_ext[512];
    static char dir[1024];
    void *data = NULL;

    const char *slash = strrchr(path, '/');
    if (slash) {
        strncat(dir, path, slash - path);
    } else {
        strncat(dir, ".", sizeof(dir) - 1);
    }

    unz_file_info info;
    int ret = unzGoToFirstFile(fd);
    while (ret == UNZ_OK) {
        if (unzGetCurrentFileInfo(fd, &info, archived_path, sizeof(archived_path), 0, 0, 0, 0) == UNZ_OK) {
            if (is_path_supported(archived_path, system_info.valid_extensions)) {
                if ((ret = unzOpenCurrentFile(fd)) == UNZ_OK) {
                    size_t size = info.uncompressed_size;
                    if (!(data = malloc(size))) {
                        log_e(LOG_TAG, "Failed to allocate %zu bytes for %s\n", size, archived_path);
                        unzCloseCurrentFile(fd);
                        continue;
                    }

                    if ((ret = unzReadCurrentFile(fd, data, size)) == (int) size) {
                        log_i(LOG_TAG, "Loaded '%s'\n", archived_path);

                        const char *archived_slash = strrchr(archived_path, '/');
                        if (archived_slash) {
                            strncat(archived_filename, archived_slash + 1, sizeof(archived_filename) - 1);
                        } else {
                            strncat(archived_filename, archived_path, sizeof(archived_filename) - 1);
                        }

                        char *archived_dot = strrchr(archived_filename, '.');
                        if (archived_dot) {
                            *archived_dot = '\0';
                            strncat(archived_ext, archived_dot + 1, sizeof(archived_ext) - 1);
                        } else {
                            strncat(archived_ext, "", sizeof(archived_ext) - 1);
                        }

                        info_local = (struct retro_game_info){ path, data, size, NULL };
                        info_local_ext = (struct retro_game_info_ext){
                            /* full_path */ NULL,
                            /* archive_path */ path,
                            /* archive_file */ archived_path,
                            /* dir */ dir,
                            /* name */ archived_filename,
                            /* ext */ archived_ext,
                            /* meta */ NULL,
                            /* data */ data,
                            /* size */ size,
                            /* file_in_archive */ true,
                            /* persistent_data */ true
                        };

                        unzCloseCurrentFile(fd);
                        break;
                    }

                    free(data);
                    unzCloseCurrentFile(fd);
                }
            }
        }
        ret = unzGoToNextFile(fd);
    }
    unzClose(fd);

    if (!info_local.data) {
        log_w(LOG_TAG, "Nothing loaded from archive: %s\n", path);
        return false;
    }

    game_info = &info_local;
    game_info_ext = &info_local_ext;

    return true;
}

static bool load_7z(const char *path)
{
    static struct retro_game_info info_local;
    static struct retro_game_info_ext info_local_ext;
    info_local = (struct retro_game_info){ 0 };
    info_local_ext = (struct retro_game_info_ext){ 0 };

    _7z_file *z7 = NULL;
    _7z_error z7_err;

    z7_err = _7z_file_open(path, &z7);
    if (z7_err != _7ZERR_NONE) {
        return false;
    }

    static char archived_path[1024];
    static char archived_filename[512];
    static char archived_ext[512];
    static char dir[1024];

    const char *slash = strrchr(path, '/');
    if (slash) {
        strncat(dir, path, slash - path);
    } else {
        strncat(dir, ".", sizeof(dir) - 1);
    }

    unsigned short name_utf16[2048];

    for (int i = 0, n = z7->db.NumFiles; i < n; i++) {
        size_t len = SzArEx_GetFileNameUtf16(&z7->db, i, NULL);
        if (len > sizeof(name_utf16)) {
            log_e(LOG_TAG, "File name at index %d too long (%zu chars)\n", i, len);
            continue;
        }

        SzArEx_GetFileNameUtf16(&z7->db, i, name_utf16);
        for (size_t j = 0, size = sizeof(archived_path) - 1; j < len && j < size; j++) {
            archived_path[j] = name_utf16[j] & 0xff;
        }
        archived_path[len] = '\0';

        if (is_path_supported(archived_path, system_info.valid_extensions)) {
            unsigned long size = SzArEx_GetFileSize(&z7->db, i);
            void *data = malloc(size);
            if (!data) {
                log_e(LOG_TAG, "Failed to allocate %zu bytes for %s\n", size, archived_path);
                continue;
            }

            z7->curr_file_idx = i;
            unsigned int wrote = 0;

            z7_err = _7z_file_decompress(z7, data, size, &wrote);
            if (z7_err != _7ZERR_NONE) {
                free(data);
                log_e(LOG_TAG, "Failed to decompress '%s' (err: %d)\n", archived_path, z7_err);
                continue;
            }

            unsigned int crc = crc32(0, (unsigned char *) data, wrote);
            if (crc != z7->db.CRCs.Vals[i]) {
                free(data);
                log_e(LOG_TAG, "CRC mismatch for '%s' (expected: %08x, got: %08x)\n",
                    archived_path, z7->db.CRCs.Vals[i], crc);
                continue;
            }

            const char *archived_slash = strrchr(archived_path, '/');
            if (archived_slash) {
                strncat(archived_filename, archived_slash + 1, sizeof(archived_filename) - 1);
            } else {
                strncat(archived_filename, archived_path, sizeof(archived_filename) - 1);
            }

            char *archived_dot = strrchr(archived_filename, '.');
            if (archived_dot) {
                *archived_dot = '\0';
                strncat(archived_ext, archived_dot + 1, sizeof(archived_ext) - 1);
            } else {
                strncat(archived_ext, "", sizeof(archived_ext) - 1);
            }

            info_local = (struct retro_game_info){ path, data, size, NULL };
            info_local_ext = (struct retro_game_info_ext){
                /* full_path */ NULL,
                /* archive_path */ path,
                /* archive_file */ archived_path,
                /* dir */ dir,
                /* name */ archived_filename,
                /* ext */ archived_ext,
                /* meta */ NULL,
                /* data */ data,
                /* size */ size,
                /* file_in_archive */ true,
                /* persistent_data */ true
            };

            break;
        }
    }

    _7z_file_close(z7);

    if (!info_local.data) {
        log_w(LOG_TAG, "Nothing loaded from archive: %s\n", path);
        return false;
    }

    game_info = &info_local;
    game_info_ext = &info_local_ext;

    return true;
}

static bool load_direct(const char *path)
{
    static struct retro_game_info info_local;
    static struct retro_game_info_ext info_local_ext;
    info_local = (struct retro_game_info){ 0 };
    info_local_ext = (struct retro_game_info_ext){ 0 };

    static char dir[512];
    static char filename[512];
    static char ext[512];

    const char *slash = strrchr(path, '/');
    if (slash) {
        strncat(dir, path, slash - path);
        strncat(filename, slash + 1, sizeof(filename) - 1);
    } else {
        strncat(dir, ".", sizeof(dir) - 1);
        strncat(filename, path, sizeof(filename) - 1);
    }

    char *dot = strrchr(filename, '.');
    if (dot) {
        *dot = '\0';
        strncat(ext, dot + 1, sizeof(ext) - 1);
    } else {
        strncat(ext, "", sizeof(ext) - 1);
    }

    void *data = NULL;
    size_t size = 0;
    bool preload = (!args.disable_preloading && !extension_one_of(ext, preload_blocklist))
        || args.force_preloading;
    log_d(LOG_TAG, "Preloading %s for '%s'\n", preload ? "enabled" : "disabled", path);

    if (preload) {
        FILE *f = fopen(path, "rb");
        if (!f) {
            log_e(LOG_TAG, "Failed to read file: %s\n", path);
            return false;
        }

        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fseek(f, 0, SEEK_SET);

        data = malloc(size);
        if (!data) {
            log_e(LOG_TAG, "Failed to allocate %zu bytes for %s\n", size, path);
        } else {
            fread(data, 1, size, f);
        }
        fclose(f);
    }

    info_local = (struct retro_game_info){ path, data, size, NULL };
    info_local_ext = (struct retro_game_info_ext){
        /* full_path */ path,
        /* archive_path */ NULL,
        /* archive_file */ NULL,
        /* dir */ dir,
        /* name */ filename,
        /* ext */ ext,
        /* meta */ NULL,
        /* data */ data,
        /* size */ size,
        /* file_in_archive */ false,
        /* persistent_data */ true
    };

    if (preload && !info_local.data) {
        // preload failed
        log_e(LOG_TAG, "Preload failed\n");
        return false;
    }

    game_info = &info_local;
    game_info_ext = &info_local_ext;

    return true;
}

bool files_load(const char *path)
{
    if (is_path_supported(path, system_info.valid_extensions)) {
        return load_direct(path);
    } else if (has_extension(path, "7z")) {
        return load_7z(path);
    } else if (has_extension(path, "zip")) {
        return load_zip(path);
    } else {
        return false;
    }
}

void files_mkdirs(const char *base_path)
{
    size_t len = strlen(base_path);
    bool needs_slash = len > 0 && base_path[len - 1] != '/';

    snprintf(system_path, sizeof(system_path), "%s%s%s",
        base_path, needs_slash ? "/" : "", system_path_name);
    snprintf(save_path, sizeof(save_path), "%s%s%s",
        base_path, needs_slash ? "/" : "", save_path_name);

    struct stat st = {0};
    if (stat(system_path, &st) == -1) {
        mkdir(system_path, 0755);
    }
    if (stat(save_path, &st) == -1) {
        mkdir(save_path, 0755);
    }
}

void files_clean_up()
{
    if (game_info) {
        // data pointer is shared between game_info and game_info_ext
        free((void*)game_info->data);
        game_info = NULL;
        game_info_ext = NULL;
    }
}

const char* files_system_path()
{
    return system_path;
}

const char* files_save_path()
{
    return save_path;
}
