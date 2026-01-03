/*
   RGBClient
   Copyright 2024, Akop Karapetyan

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "led-matrix-c.h"
#include "rgbcommon.h"

#define RETRY_COUNT_DEFAULT    0
#define RETRY_DELAY_DEFAULT_MS 500
#define PORT_DEFAULT           3500

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

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
static int retry_count = RETRY_COUNT_DEFAULT;
static int reconnect = 0;
static int run_in_background = 0;
static struct timespec retry_delay_ts;
static unsigned short port = PORT_DEFAULT;
static struct FrameGeometry data;
static unsigned char bitmap_bpp = 0;
static unsigned char *buffer = NULL;

static const char *server;
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

static void render();
static const char *rect_string(const struct ViewRect *rect);

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

static int connect_es(const char *server)
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        fprintf(stderr, "socket() failed\n");
        return 0;
    }

    struct sockaddr_in dest_addr = {};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    dest_addr.sin_addr.s_addr = inet_addr(server);

    fprintf(stderr, "connecting to '%s' on port %d...\n", server, port);
    for (int i = 0; (i <= retry_count || retry_count < 0) && !exit_main_loop; i++) {
        if (connect(sockfd,(struct sockaddr*)&dest_addr, sizeof(struct sockaddr)) == 0) {
            fprintf(stderr, "connected!\n");
            return 1;
        }
        nanosleep(&retry_delay_ts, NULL);
        // fprintf(stderr, "retrying...\n");
    }

    fprintf(stderr, "connect() failed\n");
    close(sockfd);

    return 0;
}

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

static unsigned char bpp(unsigned char pixel_format)
{
    switch(pixel_format) {
        case PIXEL_FORMAT_RGBA8888:
        case PIXEL_FORMAT_ARGB8888:
            return 4;
        case PIXEL_FORMAT_RGB565:
        case PIXEL_FORMAT_RGBA5551:
            return 2;
        default:
            return 0;
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
        fprintf(stderr, "Error reading buffer data (read %d bytes)\n", r);
        return 0;
    }

    fprintf(stderr, "Buffer size: %d\npitch: %d\nwidth: %d\nheight: %d\npixel format: %s\nattrs: 0x%x\n",
        data.buffer_size,
        data.bitmap_pitch,
        data.bitmap_width,
        data.bitmap_height,
        pixel_format_str(data.pixel_format),
        data.attrs
    );

    if (data.magic != MAGIC_NUMBER) {
        fprintf(stderr, "Magic number mismatch (0x%x != 0x%x)\n",
            data.magic,
            MAGIC_NUMBER
        );
    }
    bitmap_bpp = bpp(data.pixel_format);
    if (bitmap_bpp == 0) {
        fprintf(stderr, "Unrecognized pixel format\n");
        return 0;
    }

    if (bitmap_bpp * data.bitmap_width != data.bitmap_pitch) {
        fprintf(stderr, "Bitmap pitch does not fempute (%d != %d)\n",
            bitmap_bpp * data.bitmap_width,
            data.bitmap_pitch
        );
        return 0;
    }
    fprintf(stderr, "Projected Mbps: %.02f\n",
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

static int create_buffer()
{
    if ((buffer = (unsigned char *)malloc(data.buffer_size)) == NULL) {
        fprintf(stderr, "Error allocating buffer\n");
        return 0;
    }

    return 1;
}

static void clear_buffer()
{
    if (buffer != NULL) {
        memset(buffer, 0, data.buffer_size);
    }
    if (matrix != NULL) {
        for (int i = 0; i < 2; i++) {
            if (canvas != NULL) {
                led_canvas_clear(canvas);
                canvas = led_matrix_swap_on_vsync(matrix, canvas);
            }
        }
    }
}

static void clean_up_connection()
{
    if (sockfd != -1) {
        close(sockfd);
        sockfd = -1;
    }
    if (buffer != NULL) {
        free(buffer);
        buffer = NULL;
    }
}

static void clean_up()
{
    clean_up_connection();
    if (matrix != NULL) {
        led_matrix_delete(matrix);
        matrix = NULL;
    }
}

static int init_rgb(int argc, char **argv)
{
    fprintf(stderr, "Initializing LED matrix...\n");
    struct RGBLedMatrixOptions options = {};
    matrix = led_matrix_create_from_options(&options, &argc, &argv);
    if (matrix == NULL) {
        fprintf(stderr, "Error initializing matrix\n");
        return 0;
    }

    canvas = led_matrix_create_offscreen_canvas(matrix);

    led_canvas_get_size(canvas, &screen_width, &screen_height);
    led_canvas_set_pixel(canvas, 0, 0, 0, 0, 0);

    fprintf(stderr, "Initialized %dx%d canvas...\n", screen_width, screen_height);
    return 1;
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
        fprintf(stderr, "\rfps: %.02f", fps);
        frames = 0;
        pms = ms;
    } else {
        frames++;
    }
}

static void main_loop()
{
    int nread = 0;
    while (!exit_main_loop) {
        while (nread < data.buffer_size) {
            int r = read(sockfd, buffer + nread, data.buffer_size - nread);
            if (r <= 0) {
                fprintf(stderr, "server disconnected\n");
                return;
            }
            nread += r;
        }
        if (show_server_fps) {
            log_fps();
        }
        nread = nread - data.buffer_size;
        render();
    }
}

static void sigint_handler(int s)
{
    fprintf(stderr, "Caught SIGINT, exiting...\n");
    exit_main_loop = 1;
}

static int parse_rect(const char *arg, struct ViewRect *rect)
{
    char temp[128];
    strncpy(temp, arg, 127);
    char *delim = strchr(temp, '-');
    if (delim == NULL) {
        return 0;
    }
    *delim = '\0';

    char *sdelim = strchr(temp, ',');
    if (sdelim == NULL) {
        return 0;
    }
    *sdelim = '\0';

    char *ddelim = strchr(delim + 1, ',');
    if (ddelim == NULL) {
        return 0;
    }
    *ddelim = '\0';

    rect->sx = strtol(temp, NULL, 10);
    rect->sy = strtol(sdelim + 1, NULL, 10);
    rect->dx = strtol(delim + 1, NULL, 10);
    rect->dy = strtol(ddelim + 1, NULL, 10);

    return 1;
}

static int validate_rect(struct ViewRect rect)
{
    if (rect.sx >= rect.dx) {
        return 0;
    } else if (rect.sy >= rect.dy) {
        return 0;
    }
    return 1;
}

static const char *rect_string(const struct ViewRect *rect)
{
    static char buf[64];
    snprintf(buf, 63, "%d,%d-%d,%d",
        rect->sx,
        rect->sy,
        rect->dx,
        rect->dy
    );
    return buf;
}

static void run_as_daemon()
{
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Error forking process\n");
        exit(1);
    } else if (pid > 0) {
        // Parent
        fprintf(stderr, "Running in background, PID: %d\n", pid);
        exit(0);
    } else {
        // Child
        if (setsid() < 0) {
            fprintf(stderr, "Error creating new session\n");
            exit(1);
        }
        fclose(stdin);
        fclose(stdout);
        fclose(stderr);
    }
}

static int parse_args(int argc, const char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Missing server IP\n");
        return 0;
    }

    server = argv[1];
    unsigned char src_set = 0;
    unsigned char dest_set = 0;
    unsigned char content_set = 0;
    unsigned int retry_delay_ms = RETRY_DELAY_DEFAULT_MS;

    fprintf(stderr, "%s \\\n", argv[0]);
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        fprintf(stderr, "\t%s \\\n", arg);
        if (i == 1) {
            continue;
        }
        if (strcmp(arg, "--sfps") == 0) {
            show_server_fps = 1;
        } else if (strcmp(arg, "--help") == 0) {
            fprintf(stderr, "Usage: %s <server-ip> [--src-rect=x1,y1-x2,y2] [--dest-rect=x1,y1-x2,y2] [--content-rect=x1,y1-x2,y2] [--port=port] [--retry-count=count] [--retry-delay=ms] [--sfps] [--reconnect]\n", argv[0]);
            fprintf(stderr, "  --sfps              Show server FPS\n");
            fprintf(stderr, "  --src-rect          Source rectangle in server bitmap\n");
            fprintf(stderr, "  --dest-rect         Destination rectangle on LED matrix\n");
            fprintf(stderr, "  --content-rect      Content rectangle on LED matrix\n");
            fprintf(stderr, "  --port              Server port (default: %d)\n", PORT_DEFAULT);
            fprintf(stderr, "  --retry-count       Connection retry count (default: %d, -1 for infinite)\n", RETRY_COUNT_DEFAULT);
            fprintf(stderr, "  --retry-delay       Connection retry delay in milliseconds (default: %d)\n", RETRY_DELAY_DEFAULT_MS);
            fprintf(stderr, "  --reconnect         Reconnect on disconnection\n");
            fprintf(stderr, "  --background        Run in background as a daemon\n");
            return 0;
        } else if (strcmp(arg, "--reconnect") == 0) {
            reconnect = 1;
        } else if (strcmp(arg, "--background") == 0) {
            run_in_background = 1;
        } else if (strstr(arg, "--src-rect=")) {
            const char *post_eq = arg + 11;
            if (!parse_rect(post_eq, &source)) {
                fprintf(stderr, "'%s' is not a valid rectangle\n", post_eq);
                return 0;
            }
            src_set = 1;
        } else if (strstr(arg, "--dest-rect=")) {
            const char *post_eq = arg + 12;
            if (!parse_rect(post_eq, &dest)) {
                fprintf(stderr, "'%s' is not a valid rectangle\n", post_eq);
                return 0;
            }
            dest_set = 1;
        } else if (strstr(arg, "--content-rect=")) {
            const char *post_eq = arg + 15;
            if (!parse_rect(post_eq, &content)) {
                fprintf(stderr, "'%s' is not a valid rectangle\n", post_eq);
                return 0;
            }
            content_set = 1;
        } else if (strstr(arg, "--port=")) {
            const char *post_eq = arg + 7;
            long p = strtol(post_eq, NULL, 10);
            if (p <= 0 || p > UINT16_MAX) {
                fprintf(stderr, "'%s' is not a valid port\n", post_eq);
                return 0;
            }
            port = (unsigned short) p;
        } else if (strstr(arg, "--retry-count=")) {
            const char *post_eq = arg + 14;
            long p = strtol(post_eq, NULL, 10);
            if (p < INT32_MIN || p > INT32_MAX) {
                fprintf(stderr, "'%s' is not a valid retry count\n", post_eq);
                return 0;
            }
            retry_count = (int) p;
        } else if (strstr(arg, "--retry-delay=")) {
            const char *post_eq = arg + 14;
            long p = strtol(post_eq, NULL, 10);
            if (p <= 0 || p > UINT32_MAX) {
                fprintf(stderr, "'%s' is not a valid retry delay\n", post_eq);
                return 0;
            }
            retry_delay_ms = p;
        }
    }

    if (!src_set) {
        fprintf(stderr, "Missing source rectangle\n");
        return 0;
    } else if (!dest_set) {
        fprintf(stderr, "Missing destination rectangle\n");
        return 0;
    }  else if (!content_set) {
        fprintf(stderr, "Missing content rectangle\n");
        return 0;
    }

    if (!validate_rect(source)) {
        fprintf(stderr, "Invalid source rectangle\n");
        return 0;
    } else if (!validate_rect(dest)) {
        fprintf(stderr, "Invalid destination rectangle\n");
        return 0;
    } else if (!validate_rect(content)) {
        fprintf(stderr, "Invalid content rectangle\n");
        return 0;
    }

    retry_delay_ts.tv_sec = 0;
    retry_delay_ts.tv_nsec = retry_delay_ms * 1000000;

    return 1;
}

int main(int argc, char **argv)
{
    if (!parse_args(argc, (const char **)argv)) {
        return 1;
    }

    if (run_in_background) {
        run_as_daemon();
    }

    if (!init_rgb(argc, argv)) {
        clean_up();
        return 1;
    }

    if (dest.dx > screen_width) {
        fprintf(stderr, "drect: end (%d) exceeds max (%d)\n",
            dest.dx, screen_width);
        clean_up();
        return 1;
    } else if (dest.dy > screen_height) {
        fprintf(stderr, "drect: end (%d) exceeds max (%d)\n",
            dest.dy, screen_height);
        clean_up();
        return 1;
    }

    signal(SIGINT, sigint_handler);
    unsigned int reconnect_attempt = 0;
    do {
        if (reconnect_attempt++ > 0) {
            clear_buffer();
            clean_up_connection();
            nanosleep(&retry_delay_ts, NULL);
        }
        if (!connect_es(server)) {
            if (reconnect && !exit_main_loop) {
                continue;
            } else {
                break;
            }
        }

        if (!read_preamble() || !create_buffer()) {
            if (reconnect && !exit_main_loop) {
                continue;
            } else {
                break;
            }
        }

        main_loop();
    } while (reconnect && !exit_main_loop);

    clean_up();
    fprintf(stderr, "done\n");

    return 0;
}
