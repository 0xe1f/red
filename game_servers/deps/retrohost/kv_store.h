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
