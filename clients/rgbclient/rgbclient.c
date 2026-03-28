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

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <nats.h>

#include "log.h"
#include "xm.h"
#include "frame.pb-c.h"
#include "led-matrix-c.h"
#include "rgbcommon.h"

#define LOG_TAG "client"

struct RGB {
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

struct ViewRect {
    short sx;
    short sy;
    short dx;
    short dy;
};

static int show_server_fps = 0;
static int sockfd = -1;
static int exit_main_loop = 0;
static int run_in_background = 0;
static struct FrameGeometry data;
static unsigned char bitmap_bpp = 0;
static unsigned char *buffer = NULL;
static const char *server_url = NULL;

static int screen_width;
static int screen_height;
static int vw_origin = 0;

static struct RGBLedMatrix *matrix = NULL;
static struct LedCanvas *canvas = NULL;
static struct ViewRect content;
static struct ViewRect source;
static struct ViewRect dest;
static struct ViewRect blitSrc;
static struct ViewRect blitDest;

// RGB565
#define RED_RGB565(x) (((x>>11)&0x1f)*255/31)
#define GREEN_RGB565(x) (((x>>5)&0x3f)*255/63)
#define BLUE_RGB565(x) ((x&0x1f)*255/31)
// RGBA8888
#define RED_RGBA8888(x) ((x>>24)&0xff)
#define GREEN_RGBA8888(x) ((x>>16)&0xff)
#define BLUE_RGBA8888(x) ((x>>8)&0xff)
// ARGB8888
#define RED_ARGB8888(x) ((x>>16)&0xff)
#define GREEN_ARGB8888(x) ((x>>8)&0xff)
#define BLUE_ARGB8888(x) ((x)&0xff)
// RGBA5551
#define RED_RGBA5551(x) (((x>>11)&0x1f)*255/31)
#define GREEN_RGBA5551(x) (((x>>6)&0x1f)*255/31)
#define BLUE_RGBA5551(x) (((x>>1)&0x1f)*255/31)

static const char* pixel_format_str(unsigned char pixel_format);
static inline void unpack(struct RGB *dest, unsigned char *src, int offset);
static bool init_rgb(int argc, char **argv);
static void render();
static unsigned long long current_millis();
static inline void log_fps();
static bool parse_rect(const char *arg, struct ViewRect *rect);
static bool validate_rect(struct ViewRect rect);
static void run_as_daemon();
static bool parse_args(int argc, const char **argv);
static void sigint_callback(int s);
static void xm_callback(const struct FrameGeometry *frame, const unsigned char *content);
static void clean_up();

static const char* pixel_format_str(unsigned char pixel_format)
{
    switch(pixel_format) {
        case PIXEL_FORMAT_RGB565:
            return "RGB565";
        case PIXEL_FORMAT_RGBA8888:
            return "RGBA8888";
        case PIXEL_FORMAT_ARGB8888:
            return "ARGB8888";
        case PIXEL_FORMAT_RGBA5551:
            return "RGBA5551";
        default:
            return "UNKNOWN";
    }
}

static inline void unpack(struct RGB *dest, unsigned char *src, int offset)
{
    if (data.pixel_format == PIXEL_FORMAT_RGB565) {
        unsigned short rcol = *((unsigned short *)src + offset);
        dest->r = RED_RGB565(rcol);
        dest->g = GREEN_RGB565(rcol);
        dest->b = BLUE_RGB565(rcol);
    } else if (data.pixel_format == PIXEL_FORMAT_RGBA8888) {
        unsigned int rcol = *((unsigned int *)src + offset);
        dest->r = RED_RGBA8888(rcol);
        dest->g = GREEN_RGBA8888(rcol);
        dest->b = BLUE_RGBA8888(rcol);
    } else if (data.pixel_format == PIXEL_FORMAT_ARGB8888) {
        unsigned int rcol = *((unsigned int *)src + offset);
        dest->r = RED_ARGB8888(rcol);
        dest->g = GREEN_ARGB8888(rcol);
        dest->b = BLUE_ARGB8888(rcol);
    } else if (data.pixel_format == PIXEL_FORMAT_RGBA5551) {
        unsigned short rcol = *((unsigned short *)src + offset);
        dest->r = RED_RGBA5551(rcol);
        dest->g = GREEN_RGBA5551(rcol);
        dest->b = BLUE_RGBA5551(rcol);
    }
}

static int read_preamble()
{
    blitSrc = source;
    blitDest = dest;
    int r = read(sockfd, &data, sizeof(data));
    if (r != sizeof(data)) {
        log_e(LOG_TAG, "Error reading buffer data (read %d bytes)\n", r);
        return 0;
    }

    log_i(LOG_TAG, "Buffer size: %d\npitch: %d\nwidth: %d\nheight: %d\npixel format: %s\nattrs: 0x%x\n",
        data.buffer_size,
        data.bitmap_pitch,
        data.bitmap_width,
        data.bitmap_height,
        pixel_format_str(data.pixel_format),
        data.attrs
    );

    if (data.magic != MAGIC_NUMBER) {
        log_e(LOG_TAG, "Magic number mismatch (0x%x != 0x%x)\n",
            data.magic,
            MAGIC_NUMBER
        );
    }
    bitmap_bpp = PIXEL_FORMAT_BPP(data.pixel_format);
    if (bitmap_bpp == 0) {
        log_e(LOG_TAG, "Unrecognized pixel format\n");
        return 0;
    }

#if 0
    if (bitmap_bpp * data.bitmap_width != data.bitmap_pitch) {
        log_e(LOG_TAG, "Bitmap pitch does not fempute (%d != %d)\n",
            bitmap_bpp * data.bitmap_width,
            data.bitmap_pitch
        );
        return 0;
    }
#endif
    log_i(LOG_TAG, "Projected Mbps: %.02f\n",
        (float)(data.buffer_size * 60) / 1024 / 1024
    );

    // Center content
    int xdelta = (content.dx - data.bitmap_width) / 2;
    if (blitSrc.dx - xdelta < 0) {
        xdelta -= blitSrc.dx - xdelta;
    }
    int ydelta = (content.dy - data.bitmap_height) / 2;
    if (blitSrc.dy - ydelta < 0) {
        ydelta -= blitSrc.dy - ydelta;
    }

    if (blitSrc.sx == 0) {
        blitDest.sx += xdelta;
        blitSrc.dx -= xdelta;
    } else {
        blitDest.dx += xdelta;
        blitSrc.sx -= xdelta;
    }
    if (blitSrc.sy == 0) {
        blitDest.sy += ydelta;
        blitSrc.dy -= ydelta;
    } else {
        blitDest.dy += ydelta;
        blitSrc.sy -= ydelta;
    }

    if (data.attrs & ATTR_ROT180) {
        vw_origin = blitDest.dx;
    }

    return 1;
}

static bool init_rgb(int argc, char **argv)
{
    log_i(LOG_TAG, "Initializing LED matrix...\n");
    struct RGBLedMatrixOptions options = {};
    matrix = led_matrix_create_from_options(&options, &argc, &argv);
    if (matrix == NULL) {
        log_e(LOG_TAG, "Error initializing matrix\n");
        return false;
    }

    canvas = led_matrix_create_offscreen_canvas(matrix);

    led_canvas_get_size(canvas, &screen_width, &screen_height);
    led_canvas_set_pixel(canvas, 0, 0, 0, 0, 0);

    log_i(LOG_TAG, "Initialized %dx%d canvas...\n", screen_width, screen_height);
    return true;
}

static void render()
{
    struct RGB pixel = { 255, 255, 255 };
    for (int ry = blitSrc.sy, yo = 0; ry < blitSrc.dy; ry++, yo++) {
        if (ry >= data.bitmap_height) {
            break; // out of bounds
        }
        int rry = ry;
        if (data.attrs & ATTR_ROT180) {
            rry = data.bitmap_height - 1 - ry;
        }
        unsigned char *rrow = buffer + rry * data.bitmap_pitch;
        for (int rx = blitSrc.sx, xo = 0; rx < blitSrc.dx; rx++, xo++) {
            if (rx >= data.bitmap_width) {
                break; // out of bounds
            }
            int wy = blitDest.sy + yo;
            int wx = blitDest.sx + xo;
            if (wx < screen_width && wy < screen_height) {
                unpack(&pixel, rrow, rx);
                led_canvas_set_pixel(
                    canvas,
                    (data.attrs & ATTR_ROT180)
                        ? vw_origin - wx
                        : wx,
                    wy,
                    pixel.r,
                    pixel.g,
                    pixel.b
                );
            }
        }
    }
    canvas = led_matrix_swap_on_vsync(matrix, canvas);
}

static unsigned long long current_millis()
{
    struct timeval te;
    gettimeofday(&te, NULL);
    return te.tv_sec * 1000LL + te.tv_usec / 1000;
}

static inline void log_fps()
{
    static unsigned long long pms = 0;
    static int frames = 0;
    unsigned long long ms = current_millis();
    unsigned long long delta = ms - pms;
    if (delta > 1000L) {
        float fps = (float) frames / (delta / 1000L);
        log_v(LOG_TAG, "fps: %.02f\r", fps);
        frames = 0;
        pms = ms;
    } else {
        frames++;
    }
}

static bool parse_rect(const char *arg, struct ViewRect *rect)
{
    char temp[128];
    strncpy(temp, arg, 127);
    char *delim = strchr(temp, '-');
    if (delim == NULL) {
        return false;
    }
    *delim = '\0';

    char *sdelim = strchr(temp, ',');
    if (sdelim == NULL) {
        return false;
    }
    *sdelim = '\0';

    char *ddelim = strchr(delim + 1, ',');
    if (ddelim == NULL) {
        return false;
    }
    *ddelim = '\0';

    rect->sx = strtol(temp, NULL, 10);
    rect->sy = strtol(sdelim + 1, NULL, 10);
    rect->dx = strtol(delim + 1, NULL, 10);
    rect->dy = strtol(ddelim + 1, NULL, 10);

    return true;
}

static bool validate_rect(struct ViewRect rect)
{
    if (rect.sx >= rect.dx) {
        return false;
    } else if (rect.sy >= rect.dy) {
        return false;
    }
    return true;
}

static void run_as_daemon()
{
    pid_t pid = fork();
    if (pid < 0) {
        log_e(LOG_TAG, "Error forking process\n");
        exit(1);
    } else if (pid > 0) {
        // Parent
        log_i(LOG_TAG, "Running in background, PID: %d\n", pid);
        exit(0);
    } else {
        // Child
        if (setsid() < 0) {
            log_e(LOG_TAG, "Error creating new session\n");
            exit(1);
        }
        fclose(stdin);
        fclose(stdout);
        fclose(stderr);
    }
}

static bool parse_args(int argc, const char **argv)
{
    unsigned char src_set = 0;
    unsigned char dest_set = 0;
    unsigned char content_set = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--sfps") == 0) {
            show_server_fps = 1;
        } else if (strcmp(arg, "--help") == 0) {
            fprintf(stdout, "Usage: %s <server-url> [--src-rect=x1,y1-x2,y2] [--dest-rect=x1,y1-x2,y2] [--content-rect=x1,y1-x2,y2] [--background] [--sfps]\n", argv[0]);
            fprintf(stdout, "  --src-rect          Source rectangle in server bitmap\n");
            fprintf(stdout, "  --dest-rect         Destination rectangle on LED matrix\n");
            fprintf(stdout, "  --content-rect      Content rectangle on LED matrix\n");
            fprintf(stdout, "  --background        Run in background as a daemon\n");
            fprintf(stdout, "  --sfps              Show server FPS\n");
            return false;
        } else if (strcmp(arg, "--background") == 0) {
            run_in_background = 1;
        } else if (strstr(arg, "--src-rect=")) {
            const char *post_eq = arg + 11;
            if (!parse_rect(post_eq, &source)) {
                log_f(LOG_TAG, "'%s' is not a valid rectangle\n", post_eq);
                return false;
            }
            src_set = 1;
        } else if (strstr(arg, "--dest-rect=")) {
            const char *post_eq = arg + 12;
            if (!parse_rect(post_eq, &dest)) {
                log_f(LOG_TAG, "'%s' is not a valid rectangle\n", post_eq);
                return false;
            }
            dest_set = 1;
        } else if (strstr(arg, "--content-rect=")) {
            const char *post_eq = arg + 15;
            if (!parse_rect(post_eq, &content)) {
                log_f(LOG_TAG, "'%s' is not a valid rectangle\n", post_eq);
                return false;
            }
            content_set = 1;
        } else if (strstr(arg, "--led-")) {
            continue; // skip led matrix options
        } else if (strstr(arg, "--") || strstr(arg, "-")) {
            log_f(LOG_TAG, "Unrecognized argument: '%s'\n", arg);
            return false;
        } else if (server_url) {
            log_f(LOG_TAG, "Server URL already specified\n");
            return false;
        } else {
            server_url = arg;
        }
    }

    if (!server_url) {
         log_f(LOG_TAG, "Missing server URL\n");
         return false;
    } else if (!src_set) {
        log_f(LOG_TAG, "Missing source rectangle\n");
        return false;
    } else if (!dest_set) {
        log_f(LOG_TAG, "Missing destination rectangle\n");
        return false;
    }  else if (!content_set) {
        log_f(LOG_TAG, "Missing content rectangle\n");
        return false;
    }

    if (!validate_rect(source)) {
        log_f(LOG_TAG, "Invalid source rectangle\n");
        return false;
    } else if (!validate_rect(dest)) {
        log_f(LOG_TAG, "Invalid destination rectangle\n");
        return false;
    } else if (!validate_rect(content)) {
        log_f(LOG_TAG, "Invalid content rectangle\n");
        return false;
    }

    return true;
}

static void sigint_callback(int s)
{
    log_i(LOG_TAG, "Caught SIGINT, exiting...\n");
    exit_main_loop = 1;
}

static void xm_callback(const struct FrameGeometry *frame, const unsigned char *content)
{
    data = *frame;
    buffer = content;
    if (show_server_fps) {
        log_fps();
    }
    render();
}

static void clean_up()
{
    if (matrix != NULL) {
        led_matrix_delete(matrix);
        matrix = NULL;
        canvas = NULL;
    }
    xm_cleanup();
}

int main(int argc, char **argv)
{
    if (!parse_args(argc, (const char **)argv)) {
        return 1;
    }

    // FIXME
    log_set_level(LOG_VERBOSE);
    if (run_in_background) {
        run_as_daemon();
    }

    if (!init_rgb(argc, argv)) {
        clean_up();
        return 1;
    }

    signal(SIGINT, sigint_callback);

    if (dest.dx > screen_width) {
        log_f(LOG_TAG, "drect: end (%d) exceeds max (%d)\n",
            dest.dx, screen_width);
        clean_up();
        return 1;
    } else if (dest.dy > screen_height) {
        log_f(LOG_TAG, "drect: end (%d) exceeds max (%d)\n",
            dest.dy, screen_height);
        clean_up();
        return 1;
    }

    blitSrc = source;
    blitDest = dest;

    xm_init(server_url);
    xm_set_callback(xm_callback);
    while (!exit_main_loop) {
        nats_Sleep(10);
    }

    clean_up();
    log_i(LOG_TAG, "done\n");

    return 0;
}
