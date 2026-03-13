#ifndef LRHOST_ARGS_H__
#define LRHOST_ARGS_H__

typedef struct KvPair {
    char *key;
    const char *value;
} KvPair;

typedef struct {
    const char *rom_path;
    const char *so_path;
    const char *bios_path;
    bool background;
    bool show_fps;
    int max_clients;
    int output_width;
    int output_height;
    bool verbose;
    unsigned char scale_mode;
} ArgsOptions;

bool args_parse(int argc, const char **argv, ArgsOptions *opts, KvPair **kv_pairs, int *kv_count);
void args_free_kvs(KvPair *kv_pairs, int kv_count);
void args_dump_kvs(KvPair *kv_pairs, int kv_count);
const char* args_find_value(const KvPair *kv_pairs, int kv_count, const char *key);

#define SCALE_MODE_NONE            0
#define SCALE_MODE_SHORTESTXASPECT 1
#define SCALE_MODE_FIT             2
#define SCALE_MODE_HALF            3

#endif // LRHOST_ARGS_H__
