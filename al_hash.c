// Simple hashtable based on David Kaplan's implementation <david[at]2of1.org> 
//
#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <string.h>

#include "al_data_struct.h"

static uint64_t
_hash(char *key) {
    int8_t c;
    uint64_t hash = 5381;

    while ((c = *key++))
        hash = ((hash << 5) + hash) + c;

    return hash;
}

// Returns the smallest power of 2 larger than x
static uint32_t _p2(uint32_t x) {
    return 1 << (32 - __builtin_clz(x - 1));
}

hash_table_t *
ht_create(int32_t size) {
    hash_table_t *ht = NULL;
    if ((ht = malloc(sizeof(hash_table_t))) == NULL)
        return NULL;

    ht->size = _p2(size);    

    if ((ht->tbl = calloc(1, ht->size * sizeof(ht_node_t *))) == NULL)
        return NULL;

    return ht;
}

void
ht_destroy(hash_table_t *ht) {
    ht_node_t *n, *old_n;
    if (!ht)
        return;

    for (int32_t i = 0; i < ht->size; i++) {
        n = ht->tbl[i];
        while (n) {
            old_n = n;
            n = n->next;
            if (old_n) {
                if (old_n->key)
                    free(old_n->key); 
                free(old_n);
            }
        }
    }

    free(ht->tbl);
    free(ht);

    ht = NULL;
}

void *
ht_get(hash_table_t *ht, char *key) {
    if (!ht)
        return NULL;
    
    uint64_t idx = _hash(key) & (ht->size - 1);
    ht_node_t *n = ht->tbl[idx];
    while (n) {
        if (strncmp(key, n->key, HT_MAX_KEYLEN) == 0)
            return n->val;
        n = n->next;
    }

    return NULL;
}

int32_t
ht_push(hash_table_t *ht, char *key, void *val) {
    if (!ht)
        return -1;

    uint64_t idx = _hash(key) & (ht->size - 1);
    ht_node_t *new_n;

    if ((new_n = calloc(1, sizeof(ht_node_t))) == NULL)
        return -1;

    new_n->val = val;
    if ((new_n->key = calloc(1, strnlen(key, HT_MAX_KEYLEN + 1))) == NULL)
        return -1;

    strcpy(new_n->key, key);

    new_n->next = ht->tbl[idx];
    ht->tbl[idx] = new_n;

    return 0;
}

void
ht_erase(hash_table_t *ht, char *key) {
    if (!ht)
        return;

    uint64_t idx = _hash(key) & (ht->size - 1);
    ht_node_t *p = NULL, *n = ht->tbl[idx];

    while (n) {
        if (strncmp(key, n->key, HT_MAX_KEYLEN) == 0) {
            if (p)
                p->next = n->next;
            free(n->key);
            n->key = NULL;

            if (ht->tbl[idx] == n) {
                if (n->next)
                    ht->tbl[idx] = n->next;
                else
                    ht->tbl[idx] = NULL;
            }

            free(n);
            n = NULL;

            break;
        }

        p = n;
        n = n->next;
    }
}
