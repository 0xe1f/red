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
        } else if (strncmp(*arg, "--output-dims=", 14) == 0) {
            // FIXME: parse output dimensions
        } else if (strncmp(*arg, "--scale-mode=", 13) == 0) {
            // FIXME: parse scale mode
        } else if (strncmp(*arg, "--max-clients=", 14) == 0) {
            // FIXME: parse max clients
        } else if (strcmp(*arg, "--background") == 0 || strcmp(*arg, "-background") == 0) {
            // FIXME: parse background flag
            opts->background = true;
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
