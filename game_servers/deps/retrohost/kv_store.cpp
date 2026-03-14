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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "kv_store.h"

#define KVPAIR_INITIAL_CAPACITY 100
#define KVPAIR_GROW_SIZE 100

static int kvstore_compare_pair(const void *a, const void *b);
static const KvPair* kvstore_find_pair(const KvStore *kv_store, const char *key);

void kvstore_free(KvStore *kv_store)
{
    for (int i = 0; i < kv_store->count; i++) {
        free(kv_store->pairs[i].key);
        free(kv_store->pairs[i].value);
    }
    free(kv_store->pairs);

    kv_store->count = 0;
    kv_store->capacity = 0;
    kv_store->pairs = NULL;
}

void kvstore_dump(const KvStore *kv_store)
{
    for (int i = 0; i < kv_store->count; i++) {
        const KvPair *pair = &kv_store->pairs[i];
        fprintf(stderr, "'%s': '%s'\n", pair->key, pair->value);
    }
    fprintf(stderr, "  %d key/value pair(s)\n", kv_store->count);
}

const char* kvstore_find_value(const KvStore *kv_store, const char *key)
{
    const KvPair *kvpair = kvstore_find_pair(kv_store, key);
    return kvpair ? kvpair->value : NULL;
}

void kvstore_put(KvStore *kv_store, const char *key, const char *value)
{
    if (kv_store->pairs == NULL) {
        kv_store->capacity = KVPAIR_INITIAL_CAPACITY;
        kv_store->count = 0;
        kv_store->pairs = (KvPair *)malloc(sizeof(KvPair) * kv_store->capacity);
    }

    // lower_bound: first index where pairs[idx].key >= key
    int lo = 0;
    int hi = kv_store->count;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        int cmp = strcmp(kv_store->pairs[mid].key, key);
        if (cmp < 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    int idx = lo;

    // Replace existing key
    if (idx < kv_store->count && strcmp(kv_store->pairs[idx].key, key) == 0) {
        char *new_value = strdup(value);
        free(kv_store->pairs[idx].value);
        kv_store->pairs[idx].value = new_value;
        return;
    }

    // Grow if needed
    if (kv_store->count >= kv_store->capacity) {
        kv_store->capacity += KVPAIR_GROW_SIZE;
        kv_store->pairs = (KvPair *)realloc(kv_store->pairs, sizeof(KvPair) * kv_store->capacity);
    }

    // Insert in sorted position
    if (idx < kv_store->count) {
        memmove(&kv_store->pairs[idx + 1],
                &kv_store->pairs[idx],
                (size_t)(kv_store->count - idx) * sizeof(KvPair));
    }

    kv_store->pairs[idx].key = strdup(key);
    kv_store->pairs[idx].value = strdup(value);
    kv_store->count++;
}

static int kvstore_compare_pair(const void *a, const void *b)
{
    return strcmp(((const KvPair *)a)->key, ((const KvPair *)b)->key);
}

static const KvPair* kvstore_find_pair(const KvStore *kv_store, const char *key)
{
    static KvPair needle;
    needle.key = (char *)key;
    return (const KvPair *)bsearch(&needle, kv_store->pairs, kv_store->count, sizeof(KvPair), kvstore_compare_pair);
}
