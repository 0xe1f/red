#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "args.h"

static void args_usage(const char *progname);

static void args_usage(const char *progname)
{
    fprintf(stderr, "Usage: %s <game>\n", progname);
}

bool args_parse(int argc, const char **argv, ArgsOptions *opts)
{
    int i;
    const char **arg;
    opts->output_width = 0;
    opts->output_height = 0;
    opts->max_clients = -1;
    opts->scale_mode = SCALE_MODE_NONE;
    char temp[512];
    for (i = 1, arg = argv + 1; i < argc; i++, arg++) {
        if (strcmp(*arg, "--help") == 0 || strcmp(*arg, "-h") == 0) {
            args_usage(*argv);
            exit(0);
        } else if (strcmp(*arg, "--core") == 0 || strcmp(*arg, "-c") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Missing argument for %s\n", *arg);
                return false;
            }
            opts->so_path = *(++arg);
        } else if (strncmp(*arg, "--output-dims=", 14) == 0 || strncmp(*arg, "-wxh=", 5) == 0) {
            const char *eq = strchr(*arg, '=') + 1;
            const char *sep = strchr(eq, 'x');
            if (sep && eq) {
                strncpy(temp, eq, sep - eq);
                temp[sep - eq] = '\0';
                opts->output_width = atoi(temp);
                opts->output_height = atoi(sep + 1);
            }

            if (opts->output_width <= 0 || opts->output_height <= 0) {
                fprintf(stderr, "Invalid output dimensions: '%s'\n", eq);
                return false;
            }
        } else if (strncmp(*arg, "--scale-mode=", 13) == 0 || strncmp(*arg, "-sm=", 4) == 0) {
            const char *eq = strchr(*arg, '=') + 1;
            if (strcmp(eq, "shortestxaspect") == 0) {
                opts->scale_mode = SCALE_MODE_SHORTESTXASPECT;
            } else if (strcmp(eq, "fit") == 0) {
                opts->scale_mode = SCALE_MODE_FIT;
            } else if (strcmp(eq, "half") == 0) {
                opts->scale_mode = SCALE_MODE_HALF;
            } else if (strcmp(eq, "none") == 0) {
                opts->scale_mode = SCALE_MODE_NONE;
            } else {
                fprintf(stderr, "Invalid scale mode: '%s'\n", eq);
                return false;
            }
        } else if (strncmp(*arg, "--max-clients=", 14) == 0 || strncmp(*arg, "-mc=", 4) == 0) {
            const char *eq = strchr(*arg, '=') + 1;
            opts->max_clients = atoi(eq);
            if (opts->max_clients <= 0) {
                fprintf(stderr, "Invalid max clients: '%s'\n", eq);
                return false;
            }
        } else if (strcmp(*arg, "--background") == 0 || strcmp(*arg, "-background") == 0) {
            opts->background = true;
        } else if (strcmp(*arg, "--show-fps") == 0 || strcmp(*arg, "-fps") == 0) {
            opts->show_fps = true;
        } else if (strcmp(*arg, "--verbose") == 0 || strcmp(*arg, "-v") == 0) {
            opts->verbose = true;
        } else if (**arg == '-') {
            fprintf(stderr, "Unrecognized switch: %s\n", *arg);
            return false;
        } else if (opts->rom_path == NULL) {
            opts->rom_path = *arg;
        } else {
            fprintf(stderr, "Unrecognized argument: %s\n", *arg);
            return false;
        }
    }
    return true;
}
