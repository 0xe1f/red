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

typedef struct {
    void *data;
    unsigned short width = 0;
    unsigned short height = 0;
    unsigned short bpp = 0;
    unsigned short pitch = 0;
    unsigned int size = 0;
} VideoBuffer;

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
static ArgsOptions opts;
static VideoBuffer video_buffer = {0};

static void callback_log(enum retro_log_level level, const char *fmt, ...)
{
    time_t now = time(NULL);
    struct tm tm = *localtime(&now);
    FILE *out = (level == RETRO_LOG_ERROR) ? stderr : stdout;
    fprintf(out, "%d-%02d-%02d %02d:%02d:%02d ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    if (level == RETRO_LOG_DEBUG) {
        fprintf(out, "D ");
    } else if (level == RETRO_LOG_INFO) {
        fprintf(out, "I ");
    } else if (level == RETRO_LOG_WARN) {
        fprintf(out, "W ");
    } else if (level == RETRO_LOG_ERROR) {
        fprintf(out, "E ");
    }
    va_list va;
    va_start(va, fmt);
    vfprintf(out, fmt, va);
    va_end(va);
}

static void realloc_buffer_if_needed()
{
    if (video_buffer.data) {
        free(video_buffer.data);
        video_buffer.data = NULL;
    }

    if (opts.output_width > 0 && opts.output_height > 0) {
        video_buffer.width = opts.output_width;
        video_buffer.height = opts.output_height;
        video_buffer.bpp = PIXEL_FORMAT_BPP(pixel_format);
        fprintf(stderr, "Creating interim buffer of size %ux%u (bpp=%d)\n",
            video_buffer.width, video_buffer.height, video_buffer.bpp);
        video_buffer.pitch = video_buffer.width * video_buffer.bpp;
        video_buffer.size = video_buffer.pitch * video_buffer.height;
        video_buffer.data = calloc(video_buffer.size, 1);
    }
}

static void blit_buffer(
    const void *data, unsigned width, unsigned height, size_t pitch,
    const unsigned char **out, size_t *out_size
)
{
    if (!video_buffer.data) {
        // No interim buffer, use source directly
        *out = (const unsigned char *) data;
        *out_size = (int) pitch * height;
        return;
    }

    unsigned char *dst = (unsigned char *) video_buffer.data;
    const unsigned char *src = (const unsigned char *) data;

    // Calculate source dimensions to blit
    unsigned int src_width = width;
    bool halve_width = src_width > video_buffer.width && opts.scale_mode == SCALE_MODE_HALF;
    if (halve_width) {
        src_width /= 2;
    }
    unsigned int src_height = height;
    bool halve_height = src_height > video_buffer.height && opts.scale_mode == SCALE_MODE_HALF;
    if (halve_height) {
        src_height /= 2;
    }
    unsigned int dst_width = video_buffer.width;
    unsigned int dst_height = video_buffer.height;

    // If source is larger, center it in destination
    int offset_x = (dst_width > src_width) ? (dst_width - src_width) / 2 : 0;
    int offset_y = (dst_height > src_height) ? (dst_height - src_height) / 2 : 0;

    // Clamp source dimensions to destination if larger
    unsigned int blit_width = (src_width > dst_width) ? dst_width : src_width;
    unsigned int blit_height = (src_height > dst_height) ? dst_height : src_height;

    // If source is larger, start from center
    unsigned int src_offset_x = (src_width > dst_width) ? (src_width - dst_width) / 2 : 0;
    unsigned int src_offset_y = (src_height > dst_height) ? (src_height - dst_height) / 2 : 0;

    // Blit source to destination
    size_t bpp = video_buffer.bpp;
    for (unsigned int y = 0; y < blit_height; y++) {
        unsigned int sy = y;
        if (halve_height) {
            sy <<= 1;
        }
        unsigned char *dr = dst + (offset_y + y) * video_buffer.pitch + offset_x * bpp;
        const unsigned char *sr = src + (src_offset_y + sy) * pitch + src_offset_x * bpp * 2;
        if (halve_width) {
            for (unsigned int x = 0; x < blit_width; x++) {
                if (bpp == 2) {
                    *(unsigned short *)dr = *(const unsigned short *)sr;
                } else if (bpp == 4) {
                    *(unsigned int *)dr = *(const unsigned int *)sr;
                }
                sr += bpp << 1;
                dr += bpp;
            }
        } else {
            memcpy(dr, sr, blit_width * bpp);
        }
    }

    *out = (const unsigned char *) video_buffer.data;
    *out_size = video_buffer.size;
}

static void callback_video_refresh(const void *data, unsigned width, unsigned height, size_t pitch)
{
    static unsigned pw = 0, ph = 0;
    static size_t pp = 0;
    bool dims_changed = false;
    if (pw != width || ph != height || pp != pitch) {
        fprintf(stderr, "size changed: %ux%u pitch=%zu => %ux%u pitch=%zu\n", pw, ph, pp, width, height, pitch);
        pw = width;
        ph = height;
        pp = pitch;
        dims_changed = true;
    }

    if (video_buffer.data && geometry.bitmap_width == 0) {
        geometry = (struct FrameGeometry) {
            (unsigned int) video_buffer.pitch * video_buffer.height,
            (unsigned short) video_buffer.pitch,
            (unsigned short) video_buffer.width,
            (unsigned short) video_buffer.height,
            pixel_format,
            0,
            MAGIC_NUMBER
        };
        rgbs_set_buffer_data(geometry);
    } else if (!video_buffer.data && dims_changed) {
        geometry = (struct FrameGeometry) {
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

    const unsigned char *out;
    size_t out_size;
    blit_buffer(data, width, height, pitch, &out, &out_size);

    rgbs_poll();
    rgbs_send(out, out_size);
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
    fprintf(stdout, "----\n");
    fprintf(stdout, "%s %s (api v.%u)\n",
        system_info.library_name, system_info.library_version, api_version);

    if (content_info) {
        fprintf(stdout, "content_info_override:\n");
        for (const struct retro_system_content_info_override *info = content_info; info->extensions; info++) {
            fprintf(stdout, "  %s (need_fullpath=%s)\n",
                info->extensions,
                info->need_fullpath ? "true" : "false");
        }
    }

    if (subsystem_info) {
        fprintf(stdout, "subsystem_info:\n");
        for (const struct retro_subsystem_info *info = subsystem_info; info->ident; info++) {
            fprintf(stdout, "  %s (%s)\n", info->desc, info->ident);
        }
    }

    fprintf(stdout, "AV:\n");
    fprintf(stdout, "  geometry: %ux%u\n", av_info.geometry.base_width, av_info.geometry.base_height);
    fprintf(stdout, "  fps: %.02f\n", av_info.timing.fps);
    fprintf(stdout, "  sample_rate: %.02f\n", av_info.timing.sample_rate);
    fprintf(stdout, "  pixel_format: %s\n",
        pixel_format == PIXEL_FORMAT_ARGB8888 ? "ARGB8888" :
        pixel_format == PIXEL_FORMAT_RGB565 ? "RGB565" :
        "UNKNOWN");
    fprintf(stdout, "----\n");
}

static bool callback_environment_set(unsigned cmd, void *data)
{
    switch (cmd) {
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION\n");
        }
        *((unsigned *)data) = 1;
        break;
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_SET_CONTROLLER_INFO\n");
        }
        ports = (struct retro_controller_info *)data;
        break;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_GET_LOG_INTERFACE\n");
        }
        ((struct retro_log_callback *)data)->log = callback_log;
        break;
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_GET_INPUT_BITMASKS\n");
        }
        return true;
    case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION\n");
        }
        *(unsigned *)data = 1;
        break;
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL: %d\n", *(unsigned *)data);
        }
        break;
    case RETRO_ENVIRONMENT_GET_LANGUAGE:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_GET_LANGUAGE\n");
        }
        *((unsigned *)data) = RETRO_LANGUAGE_ENGLISH;
        break;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL\n");
        }
        core_options_v1_intl = *(struct retro_core_options_intl *)data;
        break;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL\n");
        }
        core_options_v2_intl = *(struct retro_core_options_v2_intl *)data;
        break;
    case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS\n");
        }
        // bool support_achievements = *(bool *)data;
        break;
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS: {
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS\n");
        }
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
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY\n");
        }
        *(const char **)data = system_path;
        break;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY\n");
        }
        *(const char **)data = save_path;
        break;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_SET_PIXEL_FORMAT\n");
        }
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
        realloc_buffer_if_needed();
        break;
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO\n");
        }
        // FIXME!! this can fire within retro_run()
        // av_info = *(struct retro_system_av_info *)data;
        // break;
        return false;
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_SET_MEMORY_MAPS\n");
        }
        // const struct retro_memory_map *maps = (struct retro_memory_map *)data;
        break;
    case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE\n");
        }
        // struct retro_rumble_interface *rumble = (struct retro_rumble_interface *)data;
        return false;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable *var = (struct retro_variable *)data;
        var->value = NULL;
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_GET_VARIABLE: %s\n", var->key);
        }
        return false;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE\n");
        }
        *((bool *)data) = false;
        break;
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE\n");
        }
        // 0x01: Enable Video
        // 0x02: Enable Audio
        // 0x04: Use Fast Savestates.
        // 0x08: Hard Disable Audio
        *((unsigned *)data) = 0x3;
        break;
    case RETRO_ENVIRONMENT_SET_VARIABLES:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_SET_VARIABLES\n");
        }
        break;
    case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
        subsystem_info = (const struct retro_subsystem_info *)data;
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO\n");
        }
        break;
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
        av_info = *(struct retro_system_av_info *)data;
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_SET_GEOMETRY\n");
        }
        break;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY\n");
        }
        // struct retro_core_option_display *display = (struct retro_core_option_display *)data;
        break;
    case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE:
        content_info = (const struct retro_system_content_info_override *)data;
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE\n");
        }
        break;
    case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS\n");
        }
        // uint64_t quirks = *(uint64_t *)data;
        break;
    case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE\n");
        }
        // struct retro_disk_control_callback *disk_interface = (struct retro_disk_control_callback *)data;
        break;
    case RETRO_ENVIRONMENT_GET_GAME_INFO_EXT:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_GET_GAME_INFO_EXT\n");
        }
        *(const struct retro_game_info_ext **)data = game_info_ext;
        break;
    case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK\n");
        }
        // FIXME
        // struct retro_audio_buffer_status_callback *audio_buffer_status = (struct retro_audio_buffer_status_callback *)data;
        return false;
    case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY\n");
        }
        // FIXME
        // unsigned latency = *(unsigned *)data;
        return false;
    case RETRO_ENVIRONMENT_GET_VFS_INTERFACE:
        if (opts.verbose) {
            fprintf(stderr, "RETRO_ENVIRONMENT_GET_VFS_INTERFACE\n");
        }
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
    if (opts.show_fps && now_ms - start_ms >= 1000000.0) {
        fprintf(stdout, "FPS: %u\n", frame_count);
        frame_count = 0;
        start_ms = now_ms;
    }
}

static void clean_up()
{
    rgbs_end();
    dlclose(solib);
    free(video_buffer.data);
    video_buffer.data = NULL;

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

    if (opts.background) {
        fprintf(stdout, "Entering background mode...\n");
        pid_t pid = fork();
        if (pid < 0) {
            fprintf(stderr, "fork() failed\n");
            clean_up();
            return 1;
        } else if (pid == 0) {
            // Child; keep going
            // Redirect stdout and stderr to /dev/null
            freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);
        } else {
            fprintf(stdout, "Continuing in background as pid %d\n", pid);
            clean_up();
            return 0;
        }
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
    fprintf(stdout, "Successfully loaded: %s\n", opts.rom_path);

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
