#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "unzip.h"
#include "libretro.h"
#include "files.h"

static const char *system_path_name = "bios";
static const char *save_path_name = "save";
static char system_path[1024];
static char save_path[1024];

struct retro_game_info *game_info = NULL;
struct retro_game_info_ext *game_info_ext = NULL;

//

static bool is_extension_supported(const char *path, const struct retro_system_content_info_override *content_info);
static bool has_extension(const char *path, const char *ext);
static bool load_zip(const char *path, const struct retro_system_content_info_override *content_info);
static bool load_direct(const char *path, const struct retro_system_content_info_override *content_info);

//

static bool is_extension_supported(const char *path, const struct retro_system_content_info_override *content_info)
{
    if (!content_info) {
        return true;
    }

    for (const struct retro_system_content_info_override *info = content_info; info->extensions; info++) {
        const char *ext = strrchr(path, '.');
        if (!ext) {
            fprintf(stderr, "No file extension found: %s\n", path);
            return false;
        }
        ext++; // skip the dot

        bool found = false;
        for (const char *p = info->extensions; *p; p++) {
            if (strncmp(p, ext, strlen(ext)) == 0 &&
                (*(p + strlen(ext)) == '|' || *(p + strlen(ext)) == '\0')) {
                found = true;
                break;
            }
            // skip to next extension after pipe
            while (*p && *p != '|') p++;
        }

        if (!found) {
            return false;
        }
    }

    return true;
}

static bool has_extension(const char *path, const char *ext)
{
    const char *file_ext = strrchr(path, '.');
    return file_ext && strcasecmp(file_ext + 1, ext) == 0;
}

static bool load_zip(const char *path, const struct retro_system_content_info_override *content_info)
{
    static struct retro_game_info info_local;
    static struct retro_game_info_ext info_local_ext;
    info_local = (struct retro_game_info){ 0 };
    info_local_ext = (struct retro_game_info_ext){ 0 };

    unzFile fd = unzOpen(path);
    if (!fd) {
        fprintf(stderr, "Failed to open zip: %s\n", path);
        return false;
    }

    static char archived_path[512];
    static char archived_filename[512];
    static char archived_ext[512];
    static char dir[512];
    void *data = NULL;
    unz_file_info info;
    int ret = unzGoToFirstFile(fd);
    while (ret == UNZ_OK) {
        if (unzGetCurrentFileInfo(fd, &info, archived_path, sizeof(archived_path), 0, 0, 0, 0) == UNZ_OK) {
            if (is_extension_supported(archived_path, content_info)) {
                if ((ret = unzOpenCurrentFile(fd)) == UNZ_OK) {
                    size_t size = info.uncompressed_size;
                    if (!(data = malloc(size))) {
                        fprintf(stderr, "Failed to allocate %zu bytes for %s\n", size, archived_path);
                        unzCloseCurrentFile(fd);
                        continue;
                    }

                    if ((ret = unzReadCurrentFile(fd, data, size)) == (int) size) {
                        fprintf(stderr, "Loaded '%s'\n", archived_path);

                        const char *slash = strrchr(path, '/');
                        if (slash) {
                            strncat(dir, path, slash - path);
                        } else {
                            strncat(dir, ".", sizeof(dir) - 1);
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
        fprintf(stderr, "Nothing loaded from zip: %s\n", path);
        return false;
    }

    game_info = &info_local;
    game_info_ext = &info_local_ext;

    return true;
}

static bool load_direct(const char *path, const struct retro_system_content_info_override *content_info)
{
    static struct retro_game_info info_local;
    static struct retro_game_info_ext info_local_ext;
    info_local = (struct retro_game_info){ 0 };
    info_local_ext = (struct retro_game_info_ext){ 0 };

    static char dir[512];
    static char filename[512];
    static char ext[512];

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to read file: %s\n", path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    void *data = malloc(size);
    if (!data) {
        fprintf(stderr, "Failed to allocate %zu bytes for %s\n", size, path);
    } else {
        fread(data, 1, size, f);

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
    }
    fclose(f);

    if (!info_local.data) {
        return false;
    }

    game_info = &info_local;
    game_info_ext = &info_local_ext;

    return true;
}

bool files_load(const char *path, const struct retro_system_content_info_override *content_info)
{
    if (has_extension(path, "zip")) {
        return load_zip(path, content_info);
    }
    return load_direct(path, content_info);
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
