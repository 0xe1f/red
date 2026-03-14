#ifndef KVSTORE_H__
#define KVSTORE_H__

typedef struct KvPair {
    char *key;
    char *value;
} KvPair;

typedef struct {
    KvPair *pairs;
    int count;
    int capacity;
} KvStore;

void kvstore_free(KvStore *kv_store);
void kvstore_dump(const KvStore *kv_store);
const char* kvstore_find_value(const KvStore *kv_store, const char *key);
void kvstore_put(KvStore *kv_store, const char *key, const char *value);

#endif // KVSTORE_H__
