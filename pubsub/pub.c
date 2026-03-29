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

#include <dlfcn.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libretro.h"
#include "audio.h"
#include "pub_args.h"
#include "files.h"
#include "video.h"
#include "input.h"
#include "log.h"
#include "xm_pub.h"
#include "timing.h"

static void *solib = NULL;

#define LOG_TAG  "host"
#define LOG_CORE "core"

#define DEFINE_SYMBOL(name, ret_type, ...) \
    typedef ret_type (*name##_t)(__VA_ARGS__);
#define LOAD_SYMBOL(name) \
    name##_t name = (name##_t)dlsym(solib, #name);

DEFINE_SYMBOL(retro_api_version, unsigned, void)
DEFINE_SYMBOL(retro_get_system_info, void, struct retro_system_info*)
DEFINE_SYMBOL(retro_load_game, bool, const struct retro_game_info *)
DEFINE_SYMBOL(retro_set_video_refresh, void, retro_video_refresh_t)
DEFINE_SYMBOL(retro_set_audio_sample, void, retro_audio_sample_t)
DEFINE_SYMBOL(retro_set_audio_sample_batch, void, retro_audio_sample_batch_t)
DEFINE_SYMBOL(retro_set_input_poll, void, retro_input_poll_t)
DEFINE_SYMBOL(retro_set_input_state, void, retro_input_state_t)
DEFINE_SYMBOL(retro_set_environment, void, retro_environment_t)
DEFINE_SYMBOL(retro_init, void, void)
DEFINE_SYMBOL(retro_run, void, void)
DEFINE_SYMBOL(retro_unload_game, void, void)
DEFINE_SYMBOL(retro_deinit, void, void)
DEFINE_SYMBOL(retro_get_system_av_info, void, struct retro_system_av_info*)

struct retro_system_info system_info;
const struct retro_system_content_info_override *content_info;
struct retro_game_info *game_info = NULL;
struct retro_game_info_ext *game_info_ext = NULL;
VideoBuffer video_buffer = {0};
struct retro_system_av_info av_info;
Rotation rotation = ROTATE_NONE;
Red__Geometry__PixelFormat pixel_format = RED__GEOMETRY__PIXEL_FORMAT__PF_UNKNOWN; // default is 1555
struct retro_input_descriptor *input_descriptors = NULL;
ArgsOptions args;
retro_keyboard_event_t keyboard_event_callback = NULL;

static unsigned int api_version = 0;
static struct retro_controller_info *ports;
static struct retro_core_options_v2_intl core_options_v2_intl;
static const struct retro_subsystem_info *subsystem_info;
static Audio audio;
static Red__Geometry geometry = RED__GEOMETRY__INIT;
static bool is_running = true;
static KvStore kv_store = {0};
static retro_core_options_update_display_callback_t core_options_update_display_callback = NULL;
static bool supports_no_game = false;
static bool variables_updated = false;
struct retro_disk_control_ext_callback *disk_ext_interface;
static FILE *log_file = NULL;

static void set_core_options(const struct retro_core_option_definition *option_defs);
static void set_variables(const struct retro_variable *vars, bool single);
static void callback_set_led_state(int led, int state);

// FIXME! move this
#define PIXEL_FORMAT_BPP(pf) \
    ((pf) == RED__GEOMETRY__PIXEL_FORMAT__PF_RGBA8888 || (pf) == RED__GEOMETRY__PIXEL_FORMAT__PF_ARGB8888 ? 4 : \
     (pf) == RED__GEOMETRY__PIXEL_FORMAT__PF_RGB565 || (pf) == RED__GEOMETRY__PIXEL_FORMAT__PF_RGBA5551 ? 2 : 0)

static void callback_log(enum retro_log_level level, const char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    if (level == RETRO_LOG_DEBUG) {
        vlog_d(LOG_CORE, fmt, va);
    } else if (level == RETRO_LOG_INFO) {
        vlog_i(LOG_CORE, fmt, va);
    } else if (level == RETRO_LOG_WARN) {
        vlog_w(LOG_CORE, fmt, va);
    } else if (level == RETRO_LOG_ERROR) {
        vlog_e(LOG_CORE, fmt, va);
    }
    va_end(va);
}

static void realloc_buffer_if_needed()
{
    if (video_buffer.data) {
        free(video_buffer.data);
        video_buffer.data = NULL;
    }

    if (args.output_width > 0 && args.output_height > 0) {
        video_buffer.width = args.output_width;
        video_buffer.height = args.output_height;
        video_buffer.bpp = PIXEL_FORMAT_BPP(pixel_format);
        log_d(LOG_TAG, "Creating interim buffer of size %ux%u (bpp=%d)\n",
            video_buffer.width, video_buffer.height, video_buffer.bpp);
        video_buffer.pitch = video_buffer.width * video_buffer.bpp;
        video_buffer.size = video_buffer.pitch * video_buffer.height;
        video_buffer.data = calloc(video_buffer.size, 1);
    }
}

static void callback_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch)
{
    if (!data) {
        return;
    }

    static unsigned pw = 0, ph = 0;
    static size_t pp = 0;
    bool dims_changed = false;
    if (pw != width || ph != height || pp != pitch) {
        log_d(LOG_TAG, "size changed: %ux%u pitch=%zu => %ux%u pitch=%zu\n", pw, ph, pp, width, height, pitch);
        pw = width;
        ph = height;
        pp = pitch;
        dims_changed = true;
        if (video_buffer.data) {
            // clear buffer to avoid artifacts when size changes
            memset(video_buffer.data, 0, video_buffer.size);
        }
    }

    if (video_buffer.data && geometry.width == 0) {
        geometry.pitch = video_buffer.pitch;
        geometry.width = video_buffer.width;
        geometry.height = video_buffer.height;
        geometry.pixel_format = pixel_format;
        geometry.attrs = (rotation == ROTATE_CCW90 || rotation == ROTATE_CCW180)
            ? RED__GEOMETRY__ATTRS__ATTR_ROT180 
            : RED__GEOMETRY__ATTRS__ATTR_NONE;
    } else if (!video_buffer.data && dims_changed) {
        geometry.pitch = pitch;
        geometry.width = width;
        geometry.height = height;
        geometry.pixel_format = pixel_format;
        geometry.attrs = (rotation == ROTATE_CCW90 || rotation == ROTATE_CCW180)
            ? RED__GEOMETRY__ATTRS__ATTR_ROT180 
            : RED__GEOMETRY__ATTRS__ATTR_NONE;
    }

    const unsigned char *out;
    size_t out_size;
    blit(args.scale_mode, data, width, height, pitch, &out, &out_size);

    xm_publish_frame(&geometry, out, out_size);
}

static void callback_audio_sample(int16_t left, int16_t right)
{
    int16_t data[2] = { left, right };
    audio_write(&audio, data, 2, true);
}

static size_t callback_audio_sample_batch(const int16_t *data, size_t frames)
{
    audio_write(&audio, data, frames * 2, true);
    return frames;
}

static void callback_input_poll()
{
    input_poll();
}

static void callback_set_led_state(int led, int state)
{
    log_v(LOG_TAG, "LED state changed: led=%1$d (0x%1$x) state=%2$s\n",
        led, state ? "ON" : "OFF");
}

static void dump_env()
{
    if (args.log_level < LOG_DEBUG) {
        return;
    }

    log_d(LOG_TAG, "----\n");
    log_d(LOG_TAG, "%s %s (api v.%u)\n",
        system_info.library_name, system_info.library_version, api_version);
    log_d(LOG_TAG, "  extensions: %s\n", system_info.valid_extensions);

    if (content_info) {
        log_d(LOG_TAG, "  content_info_override:\n");
        for (const struct retro_system_content_info_override *info = content_info; info->extensions; info++) {
            log_d(LOG_TAG, "    %s (need_fullpath=%s)\n",
                info->extensions,
                info->need_fullpath ? "true" : "false");
        }
    }

    if (subsystem_info) {
        log_d(LOG_TAG, "  subsystem_info:\n");
        for (const struct retro_subsystem_info *info = subsystem_info; info->ident; info++) {
            log_d(LOG_TAG, "    %s (%s)\n", info->desc, info->ident);
        }
    }

    log_d(LOG_TAG, "  av_info:\n");
    log_d(LOG_TAG, "    geometry: %ux%u (aspect_ratio: %.02f)\n",
        av_info.geometry.base_width,
        av_info.geometry.base_height,
        av_info.geometry.aspect_ratio);
    log_d(LOG_TAG, "    fps: %.02f\n", av_info.timing.fps);
    log_d(LOG_TAG, "    sample_rate: %.02f\n", av_info.timing.sample_rate);
    log_d(LOG_TAG, "    pixel_format: %s\n",
        pixel_format == RED__GEOMETRY__PIXEL_FORMAT__PF_ARGB8888 ? "ARGB8888" :
        pixel_format == RED__GEOMETRY__PIXEL_FORMAT__PF_RGB565 ? "RGB565" :
        "UNKNOWN");
    log_d(LOG_TAG, "----\n");
}

static bool callback_environment_set(unsigned cmd, void *data)
{
    switch (cmd) {
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
        log_v(LOG_TAG, "RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE\n");
        // 0x01: Enable Video
        // 0x02: Enable Audio
        // 0x04: Use Fast Savestates.
        // 0x08: Hard Disable Audio
        *((unsigned *)data) = 0x3;
        break;
    case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY\n");
        *(char **)data = NULL;
        break;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION\n");
        *((unsigned *)data) = 1;
        break;
    case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION\n");
        *(unsigned *)data = 0;
        return false;
    case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
        log_v(LOG_TAG, "RETRO_ENVIRONMENT_GET_FASTFORWARDING\n");
        *((bool *)data) = false;
        break;
    case RETRO_ENVIRONMENT_GET_GAME_INFO_EXT:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_GET_GAME_INFO_EXT\n");
        *(const struct retro_game_info_ext **)data = game_info_ext;
        break;
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_GET_INPUT_BITMASKS\n");
        return true;
    case RETRO_ENVIRONMENT_GET_LANGUAGE:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_GET_LANGUAGE\n");
        *((unsigned *)data) = RETRO_LANGUAGE_ENGLISH;
        break;
    case RETRO_ENVIRONMENT_GET_LED_INTERFACE:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_GET_LED_INTERFACE\n");
        ((struct retro_led_interface *)data)->set_led_state = callback_set_led_state;
        return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_GET_LOG_INTERFACE\n");
        ((struct retro_log_callback *)data)->log = callback_log;
        break;
    case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION\n");
        *(unsigned *)data = 1;
        break;
    case RETRO_ENVIRONMENT_GET_PERF_INTERFACE:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_GET_PERF_INTERFACE\n");
        // struct retro_perf_callback *perf = (struct retro_perf_callback *)data;
        return false;
    case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE\n");
        // struct retro_rumble_interface *rumble = (struct retro_rumble_interface *)data;
        return false;
    case RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT\n");
        // FIXME
        // struct retro_savestate_context *savestate = (struct retro_savestate_context *)data;
        return false;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY\n");
        *(const char **)data = files_save_path();
        break;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY\n");
        *(const char **)data = files_system_path();
        break;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable *var = (struct retro_variable *)data;
        var->value = kvstore_get(&kv_store, var->key);
        log_v(LOG_TAG, "RETRO_ENVIRONMENT_GET_VARIABLE: %s = %s\n", var->key, var->value);
        variables_updated = false;
        return var->value != NULL;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        log_v(LOG_TAG, "RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE\n");
        *((bool *)data) = variables_updated;
        break;
    case RETRO_ENVIRONMENT_GET_VFS_INTERFACE:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_GET_VFS_INTERFACE\n");
        // FIXME
        // struct retro_vfs_interface_info *vfs = (struct retro_vfs_interface_info *)data;
        return false;
    case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK\n");
        // FIXME
        // struct retro_audio_buffer_status_callback *audio_buffer_status = (struct retro_audio_buffer_status_callback *)data;
        return false;
    case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE:
        content_info = (const struct retro_system_content_info_override *)data;
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE\n");
        break;
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_CONTROLLER_INFO\n");
        ports = (struct retro_controller_info *)data;
        break;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_CORE_OPTIONS\n");
        set_core_options((struct retro_core_option_definition *)data);
        break;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY: {
        struct retro_core_option_display *display = (struct retro_core_option_display *)data;
        log_v(LOG_TAG, "RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY: %s\n", display->key);
        return false;
    }
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL: {
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL\n");
        const struct retro_core_options_intl *opts = (struct retro_core_options_intl *)data;
        set_core_options(opts->local ? opts->local : opts->us);
        break;
    }
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK\n");
        core_options_update_display_callback = ((struct retro_core_options_update_display_callback *)data)->callback;
        break;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL\n");
        core_options_v2_intl = *(struct retro_core_options_v2_intl *)data;
        break;
    case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE\n");
        disk_ext_interface = (struct retro_disk_control_ext_callback *)data;
        break;
    case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE\n");
        // struct retro_disk_control_callback *disk_interface = (struct retro_disk_control_callback *)data;
        break;
    case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK\n");
        // struct retro_frame_time_callback *frame_time = (struct retro_frame_time_callback *)data;
        return false;
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
        av_info = *(struct retro_system_av_info *)data;
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_GEOMETRY (%dx%d)\n",
            av_info.geometry.base_width, av_info.geometry.base_height);
        break;
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: {
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS\n");

        struct retro_input_descriptor *desc = (struct retro_input_descriptor *)data;

        // FIXME: is this right?
        unsigned count = 0;
        for (struct retro_input_descriptor *d = desc; d->description; d++, count++);

        input_descriptors = (struct retro_input_descriptor *)malloc(sizeof(struct retro_input_descriptor) * (count + 1));
        if (input_descriptors) {
            for (unsigned i = 0; i < count; i++) {
                input_descriptors[i] = desc[i];
            }
            input_descriptors[count] = (struct retro_input_descriptor){ 0 };
        }
        break;
    }
    case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK\n");
        keyboard_event_callback = ((struct retro_keyboard_callback *)data)->callback;
        break;
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_MEMORY_MAPS\n");
        // const struct retro_memory_map *maps = (struct retro_memory_map *)data;
        break;
    case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY\n");
        // FIXME
        // unsigned latency = *(unsigned *)data;
        return false;
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL: %d\n", *(unsigned *)data);
        break;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_PIXEL_FORMAT\n");
        switch (*(enum retro_pixel_format *)data) {
        case RETRO_PIXEL_FORMAT_XRGB8888:
            pixel_format = RED__GEOMETRY__PIXEL_FORMAT__PF_ARGB8888;
            break;
        case RETRO_PIXEL_FORMAT_RGB565:
            pixel_format = RED__GEOMETRY__PIXEL_FORMAT__PF_RGB565;
            break;
        default:
            pixel_format = RED__GEOMETRY__PIXEL_FORMAT__PF_UNKNOWN;
            log_d(LOG_TAG, "Unsupported pixel format: %d\n",
                *(enum retro_pixel_format *)data);
            return false;
        }
        realloc_buffer_if_needed();
        break;
    case RETRO_ENVIRONMENT_SET_ROTATION:
        rotation = (Rotation)(*(unsigned *)data);
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_ROTATION: %d\n", rotation);
        break;
    case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS\n");
        // uint64_t quirks = *(uint64_t *)data;
        break;
    case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
        subsystem_info = (const struct retro_subsystem_info *)data;
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO\n");
        break;
    case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS\n");
        // bool support_achievements = *(bool *)data;
        break;
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME\n");
        supports_no_game = *(bool *)data;
        break;
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
        av_info = *(struct retro_system_av_info *)data;
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO (%dx%d)\n",
            av_info.geometry.base_width, av_info.geometry.base_height);
        break;
    case RETRO_ENVIRONMENT_SET_VARIABLE: {
        const struct retro_variable *v = (struct retro_variable *)data;
        if (v) {
            log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_VARIABLE: %s=%s\n", v->key, v->value);
        } else {
            log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_VARIABLE: (null)\n");
            break;
        }
        set_variables(v, true);
        variables_updated = true;
        break;
    }
    case RETRO_ENVIRONMENT_SET_VARIABLES:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SET_VARIABLES\n");
        set_variables((struct retro_variable *)data, false);
        break;
    case RETRO_ENVIRONMENT_SHUTDOWN:
        log_d(LOG_TAG, "RETRO_ENVIRONMENT_SHUTDOWN\n");
        log_f(LOG_TAG, "Shutdown requested by core\n");
        is_running = false;
        break;
    default:
        if (cmd & RETRO_ENVIRONMENT_EXPERIMENTAL) {
            int exp_cmd = cmd & ~RETRO_ENVIRONMENT_EXPERIMENTAL;
            log_w(LOG_TAG, "retro_environment_set(): unrecognized cmd=0x%1$x | %2$d\n",
                RETRO_ENVIRONMENT_EXPERIMENTAL, exp_cmd);
        } else {
            log_w(LOG_TAG, "retro_environment_set(): unrecognized cmd=%1$u (0x%1$x)\n",
                cmd);
        }
        return false;
    }

    return true;
}

static void clean_up()
{
    xm_cleanup();
    dlclose(solib);
    free(video_buffer.data);
    video_buffer.data = NULL;
    free(input_descriptors);
    input_descriptors = NULL;
    kvstore_free(&kv_store);
    files_clean_up();
    input_clean_up();
    audio_cleanup(&audio);
    args_free(&args);
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

static void sigint_handler(int s)
{
    log_i(LOG_TAG, "Caught SIGINT, shutting down...\n");
    is_running = false;
}

static void reset_audio()
{
    audio_stop(&audio);
    audio_start(&audio, av_info.timing.sample_rate, 2);
}

static void set_variables(const struct retro_variable *vars, bool single)
{
    static char value_buf[1024];
    for (const struct retro_variable *v = vars; v->key; v++) {
        const char *title_end = strstr(v->value, "; ");
        if (!title_end) {
            continue;
        }
        const char *value_start = title_end + 2;
        const char *value_end = strchr(value_start, '|');
        if (value_end) {
            strncpy(value_buf, value_start, value_end - value_start);
            value_buf[value_end - value_start] = '\0';
        } else {
            strncpy(value_buf, value_start, sizeof(value_buf) - 1);
            value_buf[sizeof(value_buf) - 1] = '\0';
        }
        if (!kvstore_get(&kv_store, v->key)) {
            kvstore_put(&kv_store, v->key, value_buf);
        }
        if (single) {
            break;
        }
    }
}

static void set_core_options(const struct retro_core_option_definition *option_defs)
{
    for (const struct retro_core_option_definition *def = option_defs; def->key; def++) {
        // Don't override existing values
        if (def->default_value && !kvstore_get(&kv_store, def->key)) {
            kvstore_put(&kv_store, def->key, def->default_value);
        }
    }
}

int main(int argc, const char **argv)
{
    if (!args_parse(argc, argv, &args, &kv_store)) {
        log_f(LOG_TAG, "Argument parsing failed\n");
        return 1;
    }
    log_set_level(args.log_level);

    if (!args.so_path || access(args.so_path, F_OK) == -1) {
        log_f(LOG_TAG, "Missing or invalid core\n");
        return 1;
    } else if (!args.server_url) {
        log_f(LOG_TAG, "Missing server URL\n");
        return 1;
    }

    if (args.background) {
        log_i(LOG_TAG, "Entering background mode...\n");
        pid_t pid = fork();
        if (pid < 0) {
            log_f(LOG_TAG, "fork() failed\n");
            clean_up();
            return 1;
        } else if (pid == 0) {
            // Child; keep going
            // Redirect stdout and stderr to /dev/null
            freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);
        } else {
            log_i(LOG_TAG, "Continuing in background as pid %d\n", pid);
            clean_up();
            return 0;
        }
    }

    if (args.log_path) {
        FILE *f = fopen(args.log_path, args.log_overwrite ? "w" : "a");
        if (!f) {
            log_f(LOG_TAG, "Failed to open log file '%s'\n", args.log_path);
            clean_up();
            return 1;
        }
        log_file = f;
        log_set_fd(f);

        log_i(LOG_TAG, "%s output to '%s'\n",
            args.log_overwrite ? "Writing" : "Appending", args.log_path);
    }

    if (!(solib = dlopen(args.so_path, RTLD_NOW))) {
        log_f(LOG_TAG, "Failed to load libretro core: %s\n", dlerror());
        return 1;
    }

    LOAD_SYMBOL(retro_api_version)
    LOAD_SYMBOL(retro_get_system_info)
    LOAD_SYMBOL(retro_load_game)
    LOAD_SYMBOL(retro_set_video_refresh)
    LOAD_SYMBOL(retro_set_audio_sample)
    LOAD_SYMBOL(retro_set_audio_sample_batch)
    LOAD_SYMBOL(retro_set_input_poll)
    LOAD_SYMBOL(retro_set_input_state)
    LOAD_SYMBOL(retro_set_environment)
    LOAD_SYMBOL(retro_init)
    LOAD_SYMBOL(retro_run)
    LOAD_SYMBOL(retro_unload_game)
    LOAD_SYMBOL(retro_deinit)
    LOAD_SYMBOL(retro_get_system_av_info)

    audio_init(&audio);
    files_mkdirs(dirname((char *)args.so_path));

    api_version = retro_api_version();
    retro_get_system_info(&system_info);
    retro_set_video_refresh(callback_video_refresh);
    retro_set_audio_sample(callback_audio_sample);
    retro_set_audio_sample_batch(callback_audio_sample_batch);
    retro_set_input_poll(callback_input_poll);
    retro_set_input_state(callback_input_state);
    retro_set_environment(callback_environment_set);

    // void retro_set_controller_port_device(unsigned port, unsigned device)
    // void retro_reset(void)
    // unsigned retro_get_region(void)
    // bool retro_load_game_special(unsigned, const struct retro_game_info *, size_t)
    // size_t retro_serialize_size(void)
    // bool retro_serialize(void *data, size_t size)
    // bool retro_unserialize(const void *data, size_t size)
    // size_t retro_get_memory_size(unsigned id)
    // void retro_cheat_reset(void)
    // void retro_cheat_set(unsigned index, bool enabled, const char *code)

    retro_init();

    if (!args.rom_path && !supports_no_game) {
        log_f(LOG_TAG, "Missing rom path\n");
        return 1;
    }

    if (args.rom_path && !files_load(args.rom_path)) {
        log_f(LOG_TAG, "Failed to load file '%s'\n", args.rom_path);
        dump_env();
        clean_up();
        return 1;
    }

    if (!retro_load_game(game_info)) {
        log_f(LOG_TAG, "Core returned error while loading '%s'\n", args.rom_path);
        dump_env();
        clean_up();
        return 1;
    }

    if (args.rom_path) {
        log_i(LOG_TAG, "Successfully loaded: %s\n", args.rom_path);
    }

    retro_get_system_av_info(&av_info);
    reset_audio();

    if (args.log_level >= LOG_DEBUG) {
        dump_env();
        kvstore_dump(&kv_store);
    }

    xm_init(args.server_url);
    input_init();

    if (args.autopress) {
        input_schedule_keypress(args.autopress);
    }

    signal(SIGINT, sigint_handler);
    while (is_running) {
        retro_run();
        timing_throttle(av_info.timing.fps);
        if (args.show_fps) {
            static double last_us = 0;
            double current_us = micros();
            if (current_us - last_us >= 1000000.0) {
                log_i(LOG_TAG, "FPS: %u\n", timing_fps());
                last_us = current_us;
            }
        }
    }

    retro_unload_game();
    retro_deinit();

    clean_up();

    return 0;
}
