#include <dlfcn.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "libretro.h"
#include "sound_queue.h"
#include "rgbserver.h"
#include "args.h"
#include "files.h"

static void *solib = NULL;

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

static unsigned int api_version = 0;
static struct retro_controller_info *ports;
static struct retro_core_options_intl core_options_v1_intl;
static struct retro_core_options_v2_intl core_options_v2_intl;
static struct retro_system_info system_info;
static struct retro_system_av_info av_info;
static struct retro_input_descriptor *input_descriptors = NULL;
static const struct retro_system_content_info_override *content_info;
static const struct retro_subsystem_info *subsystem_info;
static SoundQueue sound_queue;
static struct FrameGeometry geometry;
static unsigned char pixel_format = PIXEL_FORMAT_UNKNOWN; // default is 1555
static bool is_running = true;

static void callback_log(enum retro_log_level level, const char *fmt, ...)
{
    time_t now = time(NULL);
    struct tm tm = *localtime(&now);
    fprintf(stderr, "%d-%02d-%02d %02d:%02d:%02d ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    if (level == RETRO_LOG_DEBUG) {
        fprintf(stderr, "D ");
    } else if (level == RETRO_LOG_INFO) {
        fprintf(stderr, "I ");
    } else if (level == RETRO_LOG_WARN) {
        fprintf(stderr, "W ");
    } else if (level == RETRO_LOG_ERROR) {
        fprintf(stderr, "E ");
    }
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
}

static void callback_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch)
{
    if (geometry.bitmap_width != width 
        || geometry.bitmap_height != height 
        || geometry.bitmap_pitch != pitch 
        || geometry.pixel_format != pixel_format
    ) {
        geometry = (struct FrameGeometry){
            (unsigned int) pitch * height,
            (unsigned short) pitch,
            (unsigned short) width,
            (unsigned short) height,
            pixel_format,
            0,
            MAGIC_NUMBER
        };
        rgbs_set_buffer_data(geometry);
    }

    rgbs_poll();
    rgbs_send((const unsigned char *) data, (int) pitch * height);
}

static void callback_audio_sample(int16_t left, int16_t right)
{
    fprintf(stderr, "audio_sample: left=%d right=%d\n", left, right);
}

static size_t callback_audio_sample_batch(const int16_t *data, size_t frames)
{
    sound_queue.Write((int16_t *) data, frames * 2, true);
    return frames;
}

static void callback_input_poll()
{
    // fprintf(stderr, "input_poll\n");
}

static int16_t callback_input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
    // fprintf(stderr, "input_state: port=%u device=%u index=%u id=%u\n", port, device, index, id);
    return 0;
}

static void dump_env()
{
    fprintf(stderr, "----\n");
    fprintf(stderr, "%s %s (api v.%u)\n",
        system_info.library_name, system_info.library_version, api_version);

    if (content_info) {
        fprintf(stderr, "content_info_override:\n");
        for (const struct retro_system_content_info_override *info = content_info; info->extensions; info++) {
            fprintf(stderr, "  %s (need_fullpath=%s)\n",
                info->extensions,
                info->need_fullpath ? "true" : "false");
        }
    }

    if (subsystem_info) {
        fprintf(stderr, "subsystem_info:\n");
        for (const struct retro_subsystem_info *info = subsystem_info; info->ident; info++) {
            fprintf(stderr, "  %s (%s)\n", info->desc, info->ident);
        }
    }

    fprintf(stderr, "AV:\n");
    fprintf(stderr, "  geometry: %ux%u\n", av_info.geometry.base_width, av_info.geometry.base_height);
    fprintf(stderr, "  fps: %.02f\n", av_info.timing.fps);
    fprintf(stderr, "  sample_rate: %.02f\n", av_info.timing.sample_rate);
    fprintf(stderr, "  pixel_format: %s\n",
        pixel_format == PIXEL_FORMAT_ARGB8888 ? "ARGB8888" :
        pixel_format == PIXEL_FORMAT_RGB565 ? "RGB565" :
        "UNKNOWN");
    fprintf(stderr, "----\n");
}

static bool callback_environment_set(unsigned cmd, void *data)
{
    switch (cmd) {
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        *((unsigned *)data) = 1;
        break;
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
        ports = (struct retro_controller_info *)data;
        break;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        ((struct retro_log_callback *)data)->log = callback_log;
        break;
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
        return true;
    case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION:
        *(unsigned *)data = 1;
        break;
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
        fprintf(stderr, "RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL: %d\n", *(unsigned *)data);
        break;
    case RETRO_ENVIRONMENT_GET_LANGUAGE:
        *((unsigned *)data) = RETRO_LANGUAGE_ENGLISH;
        break;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
        core_options_v1_intl = *(struct retro_core_options_intl *)data;
        break;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
        core_options_v2_intl = *(struct retro_core_options_v2_intl *)data;
        break;
    case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
        // bool support_achievements = *(bool *)data;
        break;
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: {
        struct retro_input_descriptor *desc = (struct retro_input_descriptor *)data;

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
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *(const char **)data = system_path;
        break;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *(const char **)data = save_path;
        break;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        switch (*(enum retro_pixel_format *)data) {
        case RETRO_PIXEL_FORMAT_XRGB8888:
            pixel_format = PIXEL_FORMAT_ARGB8888;
            break;
        case RETRO_PIXEL_FORMAT_RGB565:
            pixel_format = PIXEL_FORMAT_RGB565;
            break;
        default:
            pixel_format = PIXEL_FORMAT_UNKNOWN;
            fprintf(stderr, "Unsupported pixel format: %d\n",
                *(enum retro_pixel_format *)data);
            return false;
        }
        break;
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
        fprintf(stderr, "FIXME: RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO\n");
        // FIXME!! this can fire within retro_run()
        // av_info = *(struct retro_system_av_info *)data;
        // break;
        return false;
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
        // const struct retro_memory_map *maps = (struct retro_memory_map *)data;
        break;
    case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
        // struct retro_rumble_interface *rumble = (struct retro_rumble_interface *)data;
        return false;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable *var = (struct retro_variable *)data;
        var->value = NULL;
        fprintf(stderr, "RETRO_ENVIRONMENT_GET_VARIABLE: %s\n", var->key);
        return false;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        // fprintf(stderr, "RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE\n");
        *((bool *)data) = false;
        break;
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
        // 0: Enable Video
        // 1: Enable Audio
        // 2: Use Fast Savestates.
        // 3: Hard Disable Audio
        *((unsigned *)data) = 0x3;
        break;
    case RETRO_ENVIRONMENT_SET_VARIABLES:
        fprintf(stderr, "RETRO_ENVIRONMENT_SET_VARIABLES\n");
        break;
    case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
        subsystem_info = (const struct retro_subsystem_info *)data;
        break;
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
        av_info = *(struct retro_system_av_info *)data;
        break;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
        // struct retro_core_option_display *display = (struct retro_core_option_display *)data;
        break;
    case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE:
        content_info = (const struct retro_system_content_info_override *)data;
        break;
    case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
        // uint64_t quirks = *(uint64_t *)data;
        break;
    case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
        // struct retro_disk_control_callback *disk_interface = (struct retro_disk_control_callback *)data;
        break;
    case RETRO_ENVIRONMENT_GET_GAME_INFO_EXT:
        *(const struct retro_game_info_ext **)data = game_info_ext;
        break;
    case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK:
        // FIXME
        // struct retro_audio_buffer_status_callback *audio_buffer_status = (struct retro_audio_buffer_status_callback *)data;
        return false;
    case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY:
        // FIXME
        // unsigned latency = *(unsigned *)data;
        return false;
    case RETRO_ENVIRONMENT_GET_VFS_INTERFACE:
        // FIXME
        // struct retro_vfs_interface_info *vfs = (struct retro_vfs_interface_info *)data;
        return false;
    default:
        fprintf(stderr, "W: retro_environment_set(): unrecognized cmd=%1$u (0x%1$x)\n", cmd);
        return false;
    }

    return true;
}

static double nanos()
{
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return tp.tv_sec * 1000000.0 + tp.tv_usec;
}

static void throttle(bool show_fps)
{
    if (av_info.timing.fps <= 0.01) {
        return;
    }

    double now_ms = nanos();
    static uint32_t frame_count = 0;
    frame_count++;
    static double start_ms = 0;
    static double next_frame_ms = 0;
    double target_frame_time_ms = 1000000.0 / (av_info.timing.fps);

    if (next_frame_ms <= 0) {
        next_frame_ms = now_ms + target_frame_time_ms;
    } else {
        if (now_ms < next_frame_ms) {
            usleep((int)(next_frame_ms - now_ms));
            now_ms = nanos();
        }

        while (next_frame_ms <= now_ms) {
            next_frame_ms += target_frame_time_ms;
        }
    }
    now_ms = nanos();

    // Calculate (and optionally display) FPS every second
    if (now_ms - start_ms >= 1000000.0) {
        if (show_fps) {
            fprintf(stderr, "FPS: %u\n", frame_count);
        }
        frame_count = 0;
        start_ms = now_ms;
    }
}

static void clean_up()
{
    rgbs_end();
    dlclose(solib);

    files_clean_up();
}

static void sigint_handler(int s)
{
    fprintf(stderr, "Caught SIGINT, shutting down...\n");
    is_running = false;
}

static void reset_audio()
{
    sound_queue.Stop();
    sound_queue.Start(av_info.timing.sample_rate, 2);
}

int main(int argc, const char **argv)
{
    ArgsOptions opts = {0};
    if (!args_parse(argc, argv, &opts)) {
        return 1;
    }

    if (!opts.rom_path) {
        fprintf(stderr, "Missing rom path\n");
        return 1;
    } else if (!opts.so_path || access(opts.so_path, F_OK) == -1) {
        fprintf(stderr, "Missing or invalid core\n");
        return 1;
    }

    if (!(solib = dlopen(opts.so_path, RTLD_NOW))) {
        fprintf(stderr, "Failed to load libretro core: %s\n", dlerror());
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

    files_mkdirs();
    rgbs_start();

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

    if (!files_load(opts.rom_path, content_info)) {
        fprintf(stderr, "Failed to load file '%s'\n", opts.rom_path);
        clean_up();
        return 1;
    }

    if (!retro_load_game(game_info)) {
        fprintf(stderr, "Failed to load '%s'\n", opts.rom_path);
        clean_up();
        return 1;
    }
    fprintf(stderr, "Successfully loaded: %s\n", opts.rom_path);

    retro_get_system_av_info(&av_info);
    reset_audio();

    dump_env();

    signal(SIGINT, sigint_handler);
    while (is_running) {
        retro_run();
        throttle(true);
    }

    retro_unload_game();
    retro_deinit();

    clean_up();

    return 0;
}
