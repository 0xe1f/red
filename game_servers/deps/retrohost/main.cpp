#include <dlfcn.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include "libretro.h"
#include "sound_queue.h"
#include "rgbserver.h"

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

static struct retro_controller_info *ports;
static struct retro_core_options_intl core_options_v1_intl;
static struct retro_core_options_v2_intl core_options_v2_intl;
static struct retro_system_info system_info;
static struct retro_system_av_info av_info;
static struct retro_input_descriptor *input_descriptors = NULL;
struct retro_system_content_info_override content_info;
static const char *system_path = "./roms";
static const char *save_path = "./save";
static SoundQueue sound_queue;
static struct FrameGeometry geometry;
static unsigned char pixel_format = PIXEL_FORMAT_UNKNOWN;

static void callback_log(enum retro_log_level level, const char *fmt, ...)
{
    fprintf(stderr, "retro: ");
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
        }
        break;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *(const char **)data = system_path;
        break;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *(const char **)data = save_path;
        break;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            enum retro_pixel_format fmt = *(enum retro_pixel_format *)data;
            switch (fmt) {
            case RETRO_PIXEL_FORMAT_XRGB8888:
                pixel_format = PIXEL_FORMAT_ARGB8888;
                fprintf(stderr, "Pixel format: ARGB8888\n");
                break;
            case RETRO_PIXEL_FORMAT_RGB565:
                pixel_format = PIXEL_FORMAT_RGB565;
                fprintf(stderr, "Pixel format: RGB565\n");
                break;
            default:
                pixel_format = PIXEL_FORMAT_UNKNOWN;
                fprintf(stderr, "Unsupported pixel format: %d\n", fmt);
                return false;
            }
        }
        break;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
            struct retro_variable *var = (struct retro_variable *)data;
            // FIXME
            if (strcmp(var->key, "nestopia_favored_system") == 0) {
                fprintf(stderr, "FIXME FIXME FIXME %s\n", var->key);
                var->value = "auto";
                return true;
            } else {
                fprintf(stderr, "RETRO_ENVIRONMENT_GET_VARIABLE: %s\n", var->key);
                var->value = NULL;
            }
            return false;
        }
        break;
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE: {
            bool *updated = (bool *)data;
            // FIXME -- stopped here --
            // fprintf(stderr, "RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE\n");
            *updated = false;
        }
        break;
    case RETRO_ENVIRONMENT_SET_VARIABLES:
        fprintf(stderr, "RETRO_ENVIRONMENT_SET_VARIABLES\n");
        break;
    case RETRO_ENVIRONMENT_SET_GEOMETRY: {
            av_info = *(struct retro_system_av_info *)data;
            fprintf(stderr, "gfx: %ux%u @ %f fps\n",
                av_info.geometry.base_width,
                av_info.geometry.base_height,
                av_info.timing.fps);

            int channels = 2;
            fprintf(stderr, "audio: %.02f %d channels\n",
                av_info.timing.sample_rate,
                channels);

            sound_queue.Stop();
            sound_queue.Start(av_info.timing.sample_rate, channels);
        }
        break;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
        // struct retro_core_option_display *display = (struct retro_core_option_display *)data;
        break;
    case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE:
        content_info = *((struct retro_system_content_info_override *)data);
        break;
    case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
        // uint64_t quirks = *(uint64_t *)data;
        break;
    case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
        // struct retro_disk_control_callback *disk_interface = (struct retro_disk_control_callback *)data;
        break;
    case RETRO_ENVIRONMENT_GET_GAME_INFO_EXT:
        // struct retro_game_info_ext *info = (struct retro_game_info_ext *)data;
        return false; // FIXME
    case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK:
        // struct retro_audio_buffer_status_callback *audio_buffer_status = (struct retro_audio_buffer_status_callback *)data;
        return false; // FIXME
    case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY:
        // unsigned latency = *(unsigned *)data;
        return false; // FIXME
    default:
        fprintf(stderr, "W: retro_environment_set(): unrecognized cmd=%u\n", cmd);
        return false;
    }

    return true;
}

static void mkdirs()
{
    struct stat st = {0};
    if (stat(system_path, &st) == -1) {
        mkdir(system_path, 0755);
    }
    if (stat(save_path, &st) == -1) {
        mkdir(save_path, 0755);
    }
}

static struct retro_game_info* init_game_info(const char *path)
{
    static struct retro_game_info game_info;

    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open game: %s\n", path);
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    size_t game_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    void *game_data = malloc(game_size);
    if (!game_data) {
        fprintf(stderr, "Failed to allocate memory for game: %s\n", path);
        fclose(f);
        return NULL;
    }

    fread(game_data, 1, game_size, f);
    fclose(f);

    game_info = (struct retro_game_info){ path, game_data, game_size, NULL };

    return &game_info;
}

static double millis()
{
    struct timeval tp;
    gettimeofday(&tp, NULL);
    return tp.tv_sec * 1000000.0 + tp.tv_usec;
}

static void throttle(bool show_fps)
{
    if (av_info.timing.fps <= 0) {
        return;
    }

    double now_ms = millis();
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
            now_ms = millis();
        }

        while (next_frame_ms <= now_ms) {
            next_frame_ms += target_frame_time_ms;
        }
    }
    now_ms = millis();

    // Calculate (and optionally display) FPS every second
    if (now_ms - start_ms >= 1000000) {
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
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <game>\n", argv[0]);
        return 1;
    }

    solib = dlopen("./nestopia_libretro.so", RTLD_NOW);
    if (!solib) {
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

    // void (*retro_get_system_info)(struct retro_system_info*) = (void (*)(struct retro_system_info*)) dlsym(solib, "retro_get_system_info");
    // bool (*retro_load_game)(const struct retro_game_info *) = (bool (*)(const struct retro_game_info *)) dlsym(solib, "retro_load_game");
    // void (*retro_set_video_refresh)(retro_video_refresh_t) = (void (*)(retro_video_refresh_t)) dlsym(solib, "retro_set_video_refresh");
    // void (*retro_set_audio_sample)(retro_audio_sample_t) = (void (*)(retro_audio_sample_t)) dlsym(solib, "retro_set_audio_sample");
    // void (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t) = (void (*)(retro_audio_sample_batch_t)) dlsym(solib, "retro_set_audio_sample_batch");
    // void (*retro_set_input_poll)(retro_input_poll_t) = (void (*)(retro_input_poll_t)) dlsym(solib, "retro_set_input_poll");
    // void (*retro_set_input_state)(retro_input_state_t) = (void (*)(retro_input_state_t)) dlsym(solib, "retro_set_input_state");
    // void (*retro_set_environment)(retro_environment_t) = (void (*)(retro_environment_t)) dlsym(solib, "retro_set_environment");
    // void (*retro_init)(void) = (void (*)(void)) dlsym(solib, "retro_init");
    // void (*retro_run)(void) = (void (*)(void)) dlsym(solib, "retro_run");
    // void (*retro_unload_game)(void) = (void (*)(void)) dlsym(solib, "retro_unload_game");
    // void (*retro_deinit)(void) = (void (*)(void)) dlsym(solib, "retro_deinit");

    mkdirs();
    rgbs_start();

    unsigned int version = retro_api_version();
    fprintf(stderr, "api version: %u\n", version);

    retro_get_system_info(&system_info);
    fprintf(stderr, "%s %s (api v.%u)\n",
        system_info.library_name,
        system_info.library_version,
        version);

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

    const char *game_path = argv[1];
    const struct retro_game_info *game_info = init_game_info(game_path);
    if (!game_info) {
        fprintf(stderr, "Failed to initialize game info for: %s\n", game_path);
        clean_up();
        return 1;
    }

    if (!retro_load_game(game_info)) {
        fprintf(stderr, "Failed to load %s\n", game_path);
        clean_up();
        return 1;
    }

    fprintf(stderr, "Successfully loaded: %s\n", game_path);

    while (true) {
        retro_run();
        throttle(true);
    }

    retro_unload_game();
    free((void *)game_info->data);

    retro_deinit();

    clean_up();

    return 0;
}
