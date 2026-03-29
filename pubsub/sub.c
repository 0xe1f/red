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
static Red__Geometry geometry;

static bool init_rgb(int argc, char **argv);
static void render(const Red__Frame *frame);
static inline void log_fps();
static void run_as_daemon();
static void sigint_callback(int s);
static void inspect_geometry(const Red__Geometry *fg);
static void xm_callback(const Red__Frame *frame);
static void clean_up();

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
    Pixel pixel = { 255, 255, 255 };

    for (int ry = blit_src.sy, yo = 0; ry < blit_src.dy; ry++, yo++) {
        if (ry >= fg->height) {
            break; // out of bounds
        }
        int rry = ry;
        if (fg->attrs & RED__GEOMETRY__ATTRS__ATTR_ROT180) {
            rry = fg->height - 1 - ry;
        }
        unsigned char *rrow = frame->content.data + rry * fg->pitch;
        for (int rx = blit_src.sx, xo = 0; rx < blit_src.dx; rx++, xo++) {
            if (rx >= fg->width) {
                break; // out of bounds
            }
            int wy = blit_dest.sy + yo;
            int wx = blit_dest.sx + xo;
            if (wx < screen_width && wy < screen_height) {
                pixel_unpack(&pixel, fg->pixel_format, rrow, rx);
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

static void xm_callback(const Red__Frame *frame)
{
    if (args.show_fps) {
        log_fps();
    }
    inspect_geometry(frame->geometry);
    render(frame);
}

static void inspect_geometry(const Red__Geometry *fg)
{
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

    led_canvas_clear(canvas);
    blit_src = args.source;
    blit_dest = args.dest;

    // Center content
    int x_delta = (args.content.dx - fg->width) / 2;
    if (blit_src.dx - x_delta < 0) {
        x_delta -= blit_src.dx - x_delta;
    }
    int y_delta = (args.content.dy - fg->height) / 2;
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

    if (fg->attrs & RED__GEOMETRY__ATTRS__ATTR_ROT180) {
        vw_origin = blit_dest.dx;
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

    // FIXME: clear canvas when geometry changes and/or we don't receive any
    //        frames for a while, to avoid showing stale content
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
    }

    log_i(LOG_TAG, "done\n");
    clean_up();

    return 0;
}
