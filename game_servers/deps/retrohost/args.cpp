#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "args.h"

#define KVPAIR_INITIAL_SIZE 100
#define KVPAIR_GROW_SIZE 100

static void args_usage(const char *progname);
static int compare_kv_pairs(const void *a, const void *b);

static void args_usage(const char *progname)
{
    fprintf(stderr, "Usage: %s <game>\n", progname);
}

bool args_parse(int argc, const char **argv, ArgsOptions *opts, KvPair **kv_pairs, int *kv_count)
{
    int i;
    const char **arg;
    opts->output_width = 0;
    opts->output_height = 0;
    opts->max_clients = -1;
    opts->scale_mode = SCALE_MODE_NONE;
    KvPair *head = NULL;
    int size = 0;
    int count = 0;
    *kv_count = 0;
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
        } else if (strcmp(*arg, "--keyvalue") == 0 || strcmp(*arg, "-kv") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Missing argument for %s\n", *arg);
                return false;
            }
            const char *kv = *(++arg);
            const char *eq = strchr(kv, '=');
            strncpy(temp, kv, eq - kv);
            temp[eq - kv] = '\0';

            if (head == NULL) {
                size = KVPAIR_INITIAL_SIZE;
                head = (KvPair *)malloc(sizeof(KvPair) * size);
            } else if (count >= size) {
                size += KVPAIR_GROW_SIZE;
                head = (KvPair *)realloc(head, sizeof(KvPair) * size);
            }
            KvPair *pair = (KvPair *)malloc(sizeof(KvPair));
            pair->key = strdup(temp);
            pair->value = eq + 1;
            head[count] = *pair;
            count++;
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
    if (kv_pairs && count > 0) {
        *kv_count = count;
        *kv_pairs = head;
        qsort(head, count, sizeof(KvPair), compare_kv_pairs);
    }
    return true;
}

void args_free_kvs(KvPair *kv_pairs, int kv_count)
{
    for (int i = 0; i < kv_count; i++) {
        free(kv_pairs[i].key);
    }
    free(kv_pairs);
}

void args_dump_kvs(KvPair *kv_pairs, int kv_count)
{
    for (int i = 0; i < kv_count; i++) {
        const KvPair *pair = &kv_pairs[i];
        fprintf(stderr, "'%s': '%s'\n", pair->key, pair->value);
    }
    fprintf(stderr, "  %d key/value pair(s)\n", kv_count);
}

const char* args_find_value(const KvPair *kv_pairs, int kv_count, const char *key)
{
    static KvPair needle;
    needle.key = (char *)key;
    KvPair *found = (KvPair *)bsearch(&needle, kv_pairs, kv_count, sizeof(KvPair), compare_kv_pairs);
    return found ? found->value : NULL;
}

static int compare_kv_pairs(const void *a, const void *b)
{
    return strcmp(((const KvPair *)a)->key, ((const KvPair *)b)->key);
}
