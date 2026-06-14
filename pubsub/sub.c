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

#include "sub_args.h"
#include "log.h"
#include "timing.h"
#include "xm_sub.h"
#include "led-matrix-c.h"

#define LOG_TAG "client"

#define FRAME_TIMEOUT_US 1000000.0 // 1s

static ArgsOptions args = {0};
static bool exit_main_loop = false;

static int screen_width;
static int screen_height;
static int vw_origin = 0;
static FILE *log_file = NULL;
static struct RGBLedMatrix *matrix = NULL;
static struct LedCanvas *canvas = NULL;
static ViewRect blit_src;
static ViewRect blit_dest;
static FrameHeader geometry;
static double last_frame_time = 0;

// x_dir is +1 for normal left-to-right, -1 for horizontally mirrored (rot180)
typedef void (*RowRenderFn)(struct LedCanvas *canvas, const uint8_t *row,
                            int src_x, int dst_x, int dst_y, int count, int x_dir);

static bool init_rgb(int argc, char **argv);
static void render(const Frame *frame);
static inline void log_fps();
static void run_as_daemon();
static void sigint_callback(int s);
static void xm_callback(const Frame *frame);
static void inspect_geometry(const FrameHeader *hdr);
static void matrix_clear();
static void clean_up();

static void render_row_rgb565(struct LedCanvas *canvas, const uint8_t *row,
                              int src_x, int dst_x, int dst_y, int count, int x_dir);
static void render_row_argb8888(struct LedCanvas *canvas, const uint8_t *row,
                                int src_x, int dst_x, int dst_y, int count, int x_dir);
static void render_row_rgba8888(struct LedCanvas *canvas, const uint8_t *row,
                                int src_x, int dst_x, int dst_y, int count, int x_dir);
static void render_row_rgba5551(struct LedCanvas *canvas, const uint8_t *row,
                                int src_x, int dst_x, int dst_y, int count, int x_dir);

static RowRenderFn row_render_fn = NULL;

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

static void render(const Frame *frame)
{
    if (!row_render_fn) {
        return;
    }

    const FrameHeader *hdr = &frame->header;
    bool rot180 = hdr->attrs & ATTR_ROT180;
    int row_count = blit_src.dy - blit_src.sy;
    int col_count = blit_src.dx - blit_src.sx;

    // For rot180: vertical flip is handled by rry; horizontal flip uses x_dir=-1
    // starting from vw_origin - blit_dest.sx so pixel i lands at vw_origin - blit_dest.sx - i
    int dst_x = rot180 ? (vw_origin - blit_dest.sx) : blit_dest.sx;
    int x_dir = rot180 ? -1 : 1;

    for (int yo = 0; yo < row_count; yo++) {
        int ry  = blit_src.sy + yo;
        int rry = rot180 ? (hdr->height - 1 - ry) : ry;
        const uint8_t *row = frame->content + rry * hdr->pitch;
        row_render_fn(canvas, row, blit_src.sx, dst_x, blit_dest.sy + yo, col_count, x_dir);
    }
    canvas = led_matrix_swap_on_vsync(matrix, canvas);
}

static inline void log_fps()
{
    static double pmu = 0;
    static int frames = 0;
    double mu = micros();
    double delta = mu - pmu;
    frames++;
    if (delta >= 1000000L) {
        float fps = (float) (frames / (delta / 1000000L));
        log_v(LOG_TAG, "fps: %.02f\r", fps);
        frames = 0;
        pmu = mu;
    }
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

static void sigint_callback(int s)
{
    log_i(LOG_TAG, "Caught SIGINT, exiting...\n");
    exit_main_loop = true;
}

static void xm_callback(const Frame *frame)
{
    last_frame_time = micros();
    if (args.show_fps) {
        log_fps();
    }
    inspect_geometry(&frame->header);
    render(frame);
}

static void inspect_geometry(const FrameHeader *hdr)
{
    if (hdr->width == geometry.width &&
        hdr->height == geometry.height &&
        hdr->pixel_format == geometry.pixel_format &&
        hdr->attrs == geometry.attrs
    ) {
        return; // same geometry, no need to recalculate blit rectangles
    }

    log_d(LOG_TAG, "Received frame with geometry: %dx%d (pitch: %d), %s, attrs: 0x%02x\n",
        hdr->width, hdr->height, hdr->pitch,
        pixel_format_str(hdr->pixel_format),
        hdr->attrs);

    matrix_clear();

    blit_src = args.source;
    blit_dest = args.dest;

    // Center content
    int x_delta = (args.content.dx - hdr->width) / 2;
    if (blit_src.dx - x_delta < 0) {
        x_delta -= blit_src.dx - x_delta;
    }
    int y_delta = (args.content.dy - hdr->height) / 2;
    if (blit_src.dy - y_delta < 0) {
        y_delta -= blit_src.dy - y_delta;
    }

    if (blit_src.sx == 0) {
        blit_dest.sx += x_delta;
        blit_src.dx -= x_delta;
    } else {
        blit_dest.dx += x_delta;
        blit_src.sx -= x_delta;
    }
    if (blit_src.sy == 0) {
        blit_dest.sy += y_delta;
        blit_src.dy -= y_delta;
    } else {
        blit_dest.dy += y_delta;
        blit_src.sy -= y_delta;
    }

    // Clamp src rect to actual frame dimensions
    if (blit_src.dx > (int)hdr->width) {
        blit_src.dx = (int)hdr->width;
    }
    if (blit_src.dy > (int)hdr->height) {
        blit_src.dy = (int)hdr->height;
    }
    // Clamp dst rect to canvas dimensions
    int col_count = blit_src.dx - blit_src.sx;
    int row_count = blit_src.dy - blit_src.sy;
    if (blit_dest.sx + col_count > screen_width) {
        blit_src.dx -= (blit_dest.sx + col_count - screen_width);
    }
    if (blit_dest.sy + row_count > screen_height) {
        blit_src.dy -= (blit_dest.sy + row_count - screen_height);
    }

    if (hdr->attrs & ATTR_ROT180) {
        vw_origin = blit_dest.dx;
    }

    // Select format-specific row renderer (no branching in the hot loop)
    switch ((PixelFormat)hdr->pixel_format) {
        case PF_RGB565:
            row_render_fn = render_row_rgb565;
            break;
        case PF_ARGB8888:
            row_render_fn = render_row_argb8888;
            break;
        case PF_RGBA8888:
            row_render_fn = render_row_rgba8888;
            break;
        case PF_RGBA5551:
            row_render_fn = render_row_rgba5551;
            break;
        default:
            log_w(LOG_TAG, "Unsupported pixel format: %d\n", hdr->pixel_format);
            row_render_fn = NULL;
            break;
    }

    geometry = *hdr;
}

static void matrix_clear()
{
    for (int i = 0; i < 2; i++) {
        led_canvas_clear(canvas);
        canvas = led_matrix_swap_on_vsync(matrix, canvas);
    }
}

static void render_row_rgb565(struct LedCanvas *canvas, const uint8_t *row,
                               int src_x, int dst_x, int dst_y, int count, int x_dir)
{
    const uint16_t *p = (const uint16_t *)row + src_x;
    for (int i = 0; i < count; i++, p++) {
        uint16_t c = *p;
        led_canvas_set_pixel(canvas, dst_x + i * x_dir, dst_y,
            ((c >> 11) & 0x1f) * 255 / 31,
            ((c >>  5) & 0x3f) * 255 / 63,
            ( c        & 0x1f) * 255 / 31
        );
    }
}

static void render_row_argb8888(struct LedCanvas *canvas, const uint8_t *row,
                                 int src_x, int dst_x, int dst_y, int count, int x_dir)
{
    const uint32_t *p = (const uint32_t *)row + src_x;
    for (int i = 0; i < count; i++, p++) {
        uint32_t c = *p;
        led_canvas_set_pixel(canvas, dst_x + i * x_dir, dst_y,
            (c >> 16) & 0xff,
            (c >>  8) & 0xff,
             c        & 0xff
        );
    }
}

static void render_row_rgba8888(struct LedCanvas *canvas, const uint8_t *row,
                                 int src_x, int dst_x, int dst_y, int count, int x_dir)
{
    const uint32_t *p = (const uint32_t *)row + src_x;
    for (int i = 0; i < count; i++, p++) {
        uint32_t c = *p;
        led_canvas_set_pixel(canvas, dst_x + i * x_dir, dst_y,
            (c >> 24) & 0xff,
            (c >> 16) & 0xff,
            (c >>  8) & 0xff
        );
    }
}

static void render_row_rgba5551(struct LedCanvas *canvas, const uint8_t *row,
                                 int src_x, int dst_x, int dst_y, int count, int x_dir)
{
    const uint16_t *p = (const uint16_t *)row + src_x;
    for (int i = 0; i < count; i++, p++) {
        uint16_t c = *p;
        led_canvas_set_pixel(canvas, dst_x + i * x_dir, dst_y,
            ((c >> 11) & 0x1f) * 255 / 31,
            ((c >>  6) & 0x1f) * 255 / 31,
            ((c >>  1) & 0x1f) * 255 / 31
        );
    }
}

static void clean_up()
{
    if (matrix != NULL) {
        led_matrix_delete(matrix);
        matrix = NULL;
        canvas = NULL;
    }
    xm_cleanup();
    args_free(&args);
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
}

int main(int argc, char **argv)
{
    if (!args_parse(argc, (const char **)argv, &args)) {
        return 1;
    }

    log_set_level(args.log_level);
    if (args.background) {
        run_as_daemon();
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

    if (!init_rgb(argc, argv)) {
        clean_up();
        return 1;
    }

    signal(SIGINT, sigint_callback);

    if (args.dest.dx > screen_width) {
        log_f(LOG_TAG, "drect: end (%d) exceeds max (%d)\n",
            args.dest.dx, screen_width);
        clean_up();
        return 1;
    } else if (args.dest.dy > screen_height) {
        log_f(LOG_TAG, "drect: end (%d) exceeds max (%d)\n",
            args.dest.dy, screen_height);
        clean_up();
        return 1;
    }

    blit_src = args.source;
    blit_dest = args.dest;

    xm_init(args.server_url);
    xm_set_callback(xm_callback);
    while (!exit_main_loop) {
        // Virtually all work is done via callbacks, and SIGINT
        // will cause sleep to be interrupted anyway
        sleep(1);
        if (last_frame_time > 0 && micros() - last_frame_time > FRAME_TIMEOUT_US) {
            log_d(LOG_TAG, "No frames received for %.02f seconds, clearing canvas...\n",
                (micros() - last_frame_time) / 1000000.0);
            matrix_clear();
            last_frame_time = 0.0;
        }
    }

    log_i(LOG_TAG, "done\n");
    clean_up();

    return 0;
}
