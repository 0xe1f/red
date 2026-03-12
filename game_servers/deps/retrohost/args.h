#ifndef LRHOST_ARGS_H__
#define LRHOST_ARGS_H__

typedef struct {
    const char *rom_path;
    const char *so_path;
    bool background;
    bool show_fps;
    int max_clients;
    int output_width;
    int output_height;
    bool verbose;
    unsigned char scale_mode;
} ArgsOptions;

bool args_parse(int argc, const char **argv, ArgsOptions *opts);

#define SCALE_MODE_NONE            0
#define SCALE_MODE_SHORTESTXASPECT 1
#define SCALE_MODE_FIT             2
#define SCALE_MODE_HALF            3

#endif // LRHOST_ARGS_H__
