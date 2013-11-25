#ifndef __AL_DATA_STRUCT_H__
#define __AL_DATA_STRUCT_H__

#include <pthread.h>
#include <sys/time.h>
#include <stdint.h>

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

#define LIST_CONTINUE 0
#define LIST_STOP 1

#define LIST_KEEP 0
#define LIST_REMOVE 1

// This callback will be called on each node by list_destroy.  Free any memory that you have
// allocated here.  If this param is NULL when list_create is called, list destroy will call
// free on your data stucture for you as a courtesy.
typedef void (*list_free_t)(void *, void*);

// This callback always needs to return LIST_CONTINUE, or LIST_STOP.  LIST_STOP will cause the
// iteration to cease, while LIST_CONTINUE causes the opposite.
typedef int8_t (*list_iterator_t)(void *, void *);

// This function should have return values identical to strcmp, that is, 0 if the params are
// considered equal, -1  if a > b, and 1 is b > a.
typedef int8_t (*list_cmp_t)(void *, void *);

// This function should return LIST_REMOVE to remove the current element, or LIST_KEEP to keep it.
typedef int8_t (*list_remove_t)(void *, void *);

typedef struct list_node_s {
    struct list_node_s *next;
    struct list_node_s *prev;
    void *data;
} list_node_t;

typedef struct {
    int32_t list_size;
    list_node_t *head;
    list_node_t *tail;
    list_free_t free_fn;
    pthread_rwlock_t mutex;
} list_t;

int32_t list_create(list_t **list, list_free_t free_fn);
int32_t list_prepend(list_t *list, void *elem);
int32_t list_append(list_t *list, void *elem);
int32_t list_size(list_t *list);
int32_t list_head(list_t *list, void **node, int8_t remove);
int32_t list_tail(list_t *list, void **node, int8_t remove);
int32_t list_next(list_t *list, void *from, void **node);
void list_remove_if(list_t *list, list_remove_t cmp, void *cmp_data, void *free_data);
void list_sort(list_t *list, list_cmp_t cmp);
void list_destroy(list_t *list, void *free_fn_data);
void list_for_each(list_t *list, list_iterator_t iterator, void *data);
void list_read_lock(list_t *list);
void list_write_lock(list_t *list);
void list_unlock(list_t *list);
char *list_get_last_err();

// ****************************************************
// *                        BST                       *
// ****************************************************
//
#define BST_KPSTR    (1 << 0)
#define BST_KINT8    (1 << 1)
#define BST_KINT16   (1 << 2)
#define BST_KINT32   (1 << 3)
#define BST_KINT64   (1 << 4)
#define BST_KUINT8   (1 << 5)
#define BST_KUINT16  (1 << 6)
#define BST_KUINT32  (1 << 7)
#define BST_KUINT64  (1 << 8)
#define BST_KINT128  (1 << 9)
#define BST_KTME     (1 << 10)

#define BST_MAX_IDX 16

// This callback will be called on each node by bst_destroy.  Free any memory that you have
// allocated here.  If this param is NULL when bst_create is called, bst destroy will call
// free on your data stucture for you as a courtesy.
typedef void (*bst_free_t)(void *, void*);

typedef union {
    char *pstr;
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    __int128_t i128;
    struct timeval tv;
} bst_key_t;

typedef void (*bst_key_cpy_t)(bst_key_t *, bst_key_t *);
typedef int32_t (*bst_key_cmp_t)(bst_key_t *, bst_key_t *);

typedef struct bst_node_s {
    struct bst_node_s *left;
    struct bst_node_s *right;
    int64_t height;
    bst_key_t key;
    void *data;
} bst_node_t;

typedef struct {
    uint8_t idx_count;
    int32_t id;
    uint64_t flags[BST_MAX_IDX];
    char *name;
    bst_node_t *root[BST_MAX_IDX];
    bst_free_t free_fn;
    bst_key_cpy_t key_cpy_fn;
    bst_key_cmp_t key_cmp_fn;
    pthread_rwlock_t mutex[BST_MAX_IDX];
} bst_tree_t;

int32_t bst_init();
int32_t bst_fini();
int32_t find_bst_by_name(char *name);
int32_t bst_insert(bst_tree_t *tree, int32_t idx, void *key, void *data);
bst_tree_t *bst_create(char *tree_name, bst_free_t free_fn, int64_t flags);
void *bst_fetch(bst_tree_t *tree, int32_t idx, void *key, int32_t *result);
void *bst_delete(bst_tree_t *tree, int32_t idx, void *key); 
void bst_destroy(bst_tree_t *tree, void *fn_data);
void bst_print_tree(bst_tree_t *tree, int32_t idx);
char *bst_get_last_err();

#endif
