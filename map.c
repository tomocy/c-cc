#include "cc.h"

#define INIT_BUCKET_LEN 16

#define BUCKET_HIGH_WATERMARK 70
#define BUCKET_LOW_WATERMARK 50

// NOLINTNEXTLINE
#define BUCKET_TOMBSTONE_KEY ((char*)-1)

#define FNV_BITOFFSET_BASIS 14695981039346656037UL
#define FNV_PRIME 1099511628211

// FNV hash (https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function)
// NOLINTNEXTLINE
static uint64_t hash(char* s, int len) {
  uint64_t hash = FNV_BITOFFSET_BASIS;
  for (int i = 0; i < len; i++) {
    hash *= FNV_PRIME;
    hash ^= (unsigned char)s[i];
  }
  return hash;
}

static bool is_alive_map_entry(MapEntry* entry) {
  return entry->key && entry->key != BUCKET_TOMBSTONE_KEY;
}

static bool equal_to_map_entry_key(MapEntry* entry, char* key, int len) {
  return is_alive_map_entry(entry) && entry->key_len == len && memcmp(entry->key, key, len) == 0;
}

static MapEntry* get_map_entry(Map* map, char* key, int len) {
  if (!map->buckets) {
    return NULL;
  }

  uint64_t hashed_key = hash(key, len);

  for (int i = 0; i < map->cap; i++) {
    MapEntry* entry = &map->buckets[(hashed_key + i) % map->cap];
    if (equal_to_map_entry_key(entry, key, len)) {
      return entry;
    }

    if (!entry->key) {
      return NULL;
    }
  }

  UNREACHABLE("expected a map entry to be got somewhere");
  return NULL;
}

void* get_from_map(Map* map, char* key) {
  MapEntry* entry = get_map_entry(map, key, strlen(key));
  return entry ? entry->val : NULL;
}

static void put_to_map_with(Map* map, char* key, int key_len, void* val);

static void enlarge_map(Map* map) {
  int alive_key_len = 0;
  for (int i = 0; i < map->cap; i++) {
    if (!is_alive_map_entry(&map->buckets[i])) {
      continue;
    }

    alive_key_len++;
  }

  int cap = map->cap;
  while ((alive_key_len * 100) / cap >= BUCKET_LOW_WATERMARK) {
    cap *= 2;
  }

  Map enlarged = {};
  enlarged.buckets = calloc(cap, sizeof(MapEntry));
  enlarged.cap = cap;
  for (int i = 0; i < map->cap; i++) {
    MapEntry* entry = &map->buckets[i];
    if (!is_alive_map_entry(entry)) {
      continue;
    }

    put_to_map_with(&enlarged, entry->key, entry->key_len, entry->val);
  }

  *map = enlarged;
}

static MapEntry* get_or_insert_map_entry(Map* map, char* key, int len) {
  if (!map->buckets) {
    map->cap = INIT_BUCKET_LEN;
    map->buckets = calloc(map->cap, sizeof(MapEntry));
  }
  if ((map->len * 100) / map->cap >= BUCKET_HIGH_WATERMARK) {
    enlarge_map(map);
  }

  uint64_t hashed_key = hash(key, len);
  for (int i = 0; i < map->cap; i++) {
    MapEntry* entry = &map->buckets[(hashed_key + i) % map->cap];
    if (equal_to_map_entry_key(entry, key, len)) {
      return entry;
    }

    if (entry->key == BUCKET_TOMBSTONE_KEY) {
      entry->key = key;
      entry->key_len = len;
      return entry;
    }

    if (!entry->key) {
      entry->key = key;
      entry->key_len = len;
      map->len++;
      return entry;
    }
  }

  UNREACHABLE("expected a map entry to be got or inserted somewhere");
  return NULL;
}

static void put_to_map_with(Map* map, char* key, int key_len, void* val) {
  MapEntry* entry = get_or_insert_map_entry(map, key, key_len);
  entry->val = val;
}

void put_to_map(Map* map, char* key, void* val) {
  put_to_map_with(map, key, strlen(key), val);
}

void delete_from_map(Map* map, char* key) {
  MapEntry* entry = get_map_entry(map, key, strlen(key));
  if (!entry) {
    return;
  }

  entry->key = BUCKET_TOMBSTONE_KEY;
}

void test_map(void) {
  Map* map = calloc(1, sizeof(Map));

  for (int i = 0; i < 5000; i++) {
    put_to_map(map, format("key %d", i), (void*)(size_t)i);  // NOLINT
  }
  for (int i = 1000; i < 2000; i++) {
    delete_from_map(map, format("key %d", i));
  }
  for (int i = 1500; i < 1600; i++) {
    put_to_map(map, format("key %d", i), (void*)(size_t)i);  // NOLINT
  }
  for (int i = 6000; i < 7000; i++) {
    put_to_map(map, format("key %d", i), (void*)(size_t)i);  // NOLINT
  }

  for (int i = 0; i < 1000; i++) {
    assert((size_t)get_from_map(map, format("key %d", i)) == i);
  }
  for (int i = 1000; i < 1500; i++) {
    assert(get_from_map(map, "no such key") == NULL);
  }
  for (int i = 1500; i < 1600; i++) {
    assert((size_t)get_from_map(map, format("key %d", i)) == i);
  }
  for (int i = 1600; i < 2000; i++) {
    assert(get_from_map(map, "no such key") == NULL);
  }
  for (int i = 2000; i < 5000; i++) {
    assert((size_t)get_from_map(map, format("key %d", i)) == i);
  }
  for (int i = 5000; i < 6000; i++) {
    assert(get_from_map(map, "no such key") == NULL);
  }
  for (int i = 6000; i < 7000; i++) {
    put_to_map(map, format("key %d", i), (void*)(size_t)i);  // NOLINT
  }

  assert(get_from_map(map, "no such key") == NULL);

  exit(0);
}