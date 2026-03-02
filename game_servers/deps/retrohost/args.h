#ifndef LRHOST_ARGS_H__
#define LRHOST_ARGS_H__

typedef struct {
    const char *rom_path;
    const char *so_path;
} ArgsOptions;

bool args_parse(int argc, const char **argv, ArgsOptions *opts);

#endif // LRHOST_ARGS_H__
