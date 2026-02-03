#include "collections.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

u64 fnv1a_bytes_64(const void *data, u32 len) {
    const unsigned char *p = (const unsigned char *)data;
    u64 hash = 14695981039346656037ULL;

    for (u32 i = 0; i < len; i++) {
        hash ^= (u64)p[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

cback_hash_map *cback_hashmap_create(u32 capacity, u32 key_size, u32 value_size) {
    cback_hash_map *map = malloc(sizeof(cback_hash_map));
    if (!map) return NULL;

    map->bucket = calloc(capacity, sizeof(cback_hash_node *));
    map->capacity = capacity;
    map->key_size = key_size;
    map->value_size = value_size;

    return map;
}

u32 hash_index(cback_hash_map *map, void *key) {
    u64 hash = fnv1a_bytes_64(key, map->key_size);
    return hash % map->capacity;
}

void cback_hash_map_put(cback_hash_map *map, void *key, void *value) {
    u32 index = hash_index(map, key);
    cback_hash_node *node = map->bucket[index];

    while (node) {
        if (node->key == key) {
            node->value = value;
            return;
        }
        node = node->next;
    }
    cback_hash_node *new_node = malloc(sizeof(cback_hash_node));
    new_node->key = key;
    new_node->value = value;
    new_node->next = map->bucket[index];
    map->bucket[index] = new_node;
}

int cback_hash_map_get(cback_hash_map *map, void *key, void **out_value) {
    u32 index = hash_index(map, key);
    cback_hash_node *node = map->bucket[index];

    while (node) {
        if (memcmp(node->key, key, map->key_size) == 0) {
            if (out_value) *out_value = node->value;
            return 1;
        }
        node = node->next;
    }
    return 0;
}

void cback_hash_map_destroy(cback_hash_map *map) {
    for (u32 i = 0; i < map->capacity; i++) {
        cback_hash_node *node = map->bucket[i];
        while (node) {
            cback_hash_node *temp = node;
            node = node->next;
            free(temp);
        }
    }
    free(map->bucket);
    free(map);
}
