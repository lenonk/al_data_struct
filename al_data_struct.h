#ifndef __AL_DATA_STRUCT_H__
#define __AL_DATA_STRUCT_H__

// ****************************************************
// *                    Hash table                    *
// ****************************************************
#define HT_MAX_KEYLEN 64

typedef struct ht_node_s {
    void *val;
    char *key;
    struct ht_node_s *next;
} ht_node_t;

typedef struct {
    ht_node_t **tbl;
    int32_t size;
} hash_table_t;

hash_table_t *ht_create(int32_t size);
int32_t ht_push(hash_table_t *ht, char *key, void *val);
void ht_destroy(hash_table_t *ht);
void ht_erase(hash_table_t *ht, char *key);
void *ht_get(hash_table_t *ht, char *key);

// ****************************************************
// *                   Linked List                    *
// ****************************************************

// ****************************************************
// *                Double Linked List                *
// ****************************************************

// ****************************************************
// *                        BST                       *
// ****************************************************
//
#define BST_KPSTR    (1 << 0)
#define BST_KSTR     (1 << 1)
#define BST_KINT8    (1 << 2)
#define BST_KINT16   (1 << 3)
#define BST_KINT32   (1 << 4)
#define BST_KINT64   (1 << 5)
#define BST_KUINT8   (1 << 6)
#define BST_KUINT16  (1 << 7)
#define BST_KUINT32  (1 << 8)
#define BST_KUINT64  (1 << 9)
#define BST_KINT128  (1 << 10)
#define BST_KTME     (1 << 11)

#endif
