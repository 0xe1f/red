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
#include <unistd.h>
#include <nats.h>

#include "log.h"
#include "xm.h"
#include "led-matrix-c.h"

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

static bool show_server_fps = false;
static bool exit_main_loop = false;
static bool run_in_background = false;
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
static Red__Geometry geometry;

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

static const char* pixel_format_str(Red__Geometry__PixelFormat pixel_format);
static inline void unpack(struct RGB *dest, unsigned char *src, int offset);
static bool init_rgb(int argc, char **argv);
static void render(const Red__Frame *frame);
static unsigned long long current_millis();
static inline void log_fps();
static bool parse_rect(const char *arg, struct ViewRect *rect);
static bool validate_rect(struct ViewRect rect);
static void run_as_daemon();
static bool parse_args(int argc, const char **argv);
static void sigint_callback(int s);
static void xm_callback(const Red__Frame *frame);
static void clean_up();

static const char* pixel_format_str(Red__Geometry__PixelFormat pixel_format)
{
    switch(pixel_format) {
        case RED__GEOMETRY__PIXEL_FORMAT__PF_RGB565:
            return "RGB565";
        case RED__GEOMETRY__PIXEL_FORMAT__PF_RGBA8888:
            return "RGBA8888";
        case RED__GEOMETRY__PIXEL_FORMAT__PF_ARGB8888:
            return "ARGB8888";
        case RED__GEOMETRY__PIXEL_FORMAT__PF_RGBA5551:
            return "RGBA5551";
        default:
            return "UNKNOWN";
    }
}

static inline void unpack(struct RGB *dest, unsigned char *src, int offset)
{
    if (geometry.pixel_format == RED__GEOMETRY__PIXEL_FORMAT__PF_RGB565) {
        unsigned short rcol = *((unsigned short *)src + offset);
        dest->r = RED_RGB565(rcol);
        dest->g = GREEN_RGB565(rcol);
        dest->b = BLUE_RGB565(rcol);
    } else if (geometry.pixel_format == RED__GEOMETRY__PIXEL_FORMAT__PF_RGBA8888) {
        unsigned int rcol = *((unsigned int *)src + offset);
        dest->r = RED_RGBA8888(rcol);
        dest->g = GREEN_RGBA8888(rcol);
        dest->b = BLUE_RGBA8888(rcol);
    } else if (geometry.pixel_format == RED__GEOMETRY__PIXEL_FORMAT__PF_ARGB8888) {
        unsigned int rcol = *((unsigned int *)src + offset);
        dest->r = RED_ARGB8888(rcol);
        dest->g = GREEN_ARGB8888(rcol);
        dest->b = BLUE_ARGB8888(rcol);
    } else if (geometry.pixel_format == RED__GEOMETRY__PIXEL_FORMAT__PF_RGBA5551) {
        unsigned short rcol = *((unsigned short *)src + offset);
        dest->r = RED_RGBA5551(rcol);
        dest->g = GREEN_RGBA5551(rcol);
        dest->b = BLUE_RGBA5551(rcol);
    }
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

static void render(const Red__Frame *frame)
{
    const Red__Geometry *fg = frame->geometry;
    struct RGB pixel = { 255, 255, 255 };

    for (int ry = blitSrc.sy, yo = 0; ry < blitSrc.dy; ry++, yo++) {
        if (ry >= fg->height) {
            break; // out of bounds
        }
        int rry = ry;
        if (fg->attrs & RED__GEOMETRY__ATTRS__ATTR_ROT180) {
            rry = fg->height - 1 - ry;
        }
        unsigned char *rrow = frame->content.data + rry * fg->pitch;
        for (int rx = blitSrc.sx, xo = 0; rx < blitSrc.dx; rx++, xo++) {
            if (rx >= fg->width) {
                break; // out of bounds
            }
            int wy = blitDest.sy + yo;
            int wx = blitDest.sx + xo;
            if (wx < screen_width && wy < screen_height) {
                unpack(&pixel, rrow, rx);
                led_canvas_set_pixel(
                    canvas,
                    (fg->attrs & RED__GEOMETRY__ATTRS__ATTR_ROT180)
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
    bool src_set = false;
    bool dest_set = false;
    bool content_set = false;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--sfps") == 0) {
            show_server_fps = true;
        } else if (strcmp(arg, "--help") == 0) {
            fprintf(stdout, "Usage: %s <server-url> [--src-rect=x1,y1-x2,y2] [--dest-rect=x1,y1-x2,y2] [--content-rect=x1,y1-x2,y2] [--background] [--sfps]\n", argv[0]);
            fprintf(stdout, "  --src-rect          Source rectangle in server bitmap\n");
            fprintf(stdout, "  --dest-rect         Destination rectangle on LED matrix\n");
            fprintf(stdout, "  --content-rect      Content rectangle on LED matrix\n");
            fprintf(stdout, "  --background        Run in background as a daemon\n");
            fprintf(stdout, "  --sfps              Show server FPS\n");
            return false;
        } else if (strcmp(arg, "--background") == 0) {
            run_in_background = true;
        } else if (strstr(arg, "--src-rect=")) {
            const char *post_eq = arg + 11;
            if (!parse_rect(post_eq, &source)) {
                log_f(LOG_TAG, "'%s' is not a valid rectangle\n", post_eq);
                return false;
            }
            src_set = true;
        } else if (strstr(arg, "--dest-rect=")) {
            const char *post_eq = arg + 12;
            if (!parse_rect(post_eq, &dest)) {
                log_f(LOG_TAG, "'%s' is not a valid rectangle\n", post_eq);
                return false;
            }
            dest_set = true;
        } else if (strstr(arg, "--content-rect=")) {
            const char *post_eq = arg + 15;
            if (!parse_rect(post_eq, &content)) {
                log_f(LOG_TAG, "'%s' is not a valid rectangle\n", post_eq);
                return false;
            }
            content_set = true;
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
    exit_main_loop = true;
}

static void verify_frame(const Red__Frame *frame);
static void xm_callback(const Red__Frame *frame)
{
    if (show_server_fps) {
        log_fps();
    }
    verify_frame(frame);
    render(frame);
}

static void verify_frame(const Red__Frame *frame)
{
    const Red__Geometry *fg = frame->geometry;
    if (fg->width == geometry.width &&
        fg->height == geometry.height &&
        fg->pixel_format == geometry.pixel_format &&
        fg->attrs == geometry.attrs
    ) {
        return; // same geometry, no need to recalculate blit rectangles
    }

    log_d(LOG_TAG, "Received frame with geometry: %dx%d (pitch: %d), %s, attrs: 0x%02x\n",
        fg->width, fg->height, fg->pitch,
        pixel_format_str(fg->pixel_format),
        fg->attrs);

    blitSrc = source;
    blitDest = dest;

    // Center content
    int x_delta = (content.dx - fg->width) / 2;
    if (blitSrc.dx - x_delta < 0) {
        x_delta -= blitSrc.dx - x_delta;
    }
    int y_delta = (content.dy - fg->height) / 2;
    if (blitSrc.dy - y_delta < 0) {
        y_delta -= blitSrc.dy - y_delta;
    }

    if (blitSrc.sx == 0) {
        blitDest.sx += x_delta;
        blitSrc.dx -= x_delta;
    } else {
        blitDest.dx += x_delta;
        blitSrc.sx -= x_delta;
    }
    if (blitSrc.sy == 0) {
        blitDest.sy += y_delta;
        blitSrc.dy -= y_delta;
    } else {
        blitDest.dy += y_delta;
        blitSrc.sy -= y_delta;
    }

    if (fg->attrs & RED__GEOMETRY__ATTRS__ATTR_ROT180) {
        vw_origin = blitDest.dx;
    }

    geometry = *fg;
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
    // log level should be configurable, but for now we want verbose logging to debug issues
    // FIXME: optimize memory alloc in xm.c by reusing unpacked structure and only unpacking content data on each frame
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
