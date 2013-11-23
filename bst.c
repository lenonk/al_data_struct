#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include "al_data_struct.h"

// Defines
#define BST_NODE_POOL_SZ 102400
#define MAX_ERR_LEN 2048
#define BST_MAX_IDX 16

const int64_t BST_KEYS = BST_KPSTR | BST_KINT8 | BST_KINT16 | BST_KINT32 | BST_KINT64 | 
    BST_KUINT8 | BST_KUINT16 | BST_KUINT32 | BST_KUINT64 | BST_KINT128 | BST_KTME;

#define BST_EQUAL 0
#define BST_RIGHT_GT -1
#define BST_LEFT_GT 1
#define BST_NOTFOUND -2


typedef union {
    char *pstr;
    //char str;
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
    pthread_rwlock_t mutex[BST_MAX_IDX];
} bst_tree_t;

// Globals
list_t *bst_node_list = NULL;

int32_t top_tree_id = 0;
char err_str[MAX_ERR_LEN];
bst_tree_t **tree_ids = NULL;
list_t *tree_list = NULL;
pthread_rwlock_t reg_lock = PTHREAD_RWLOCK_INITIALIZER;

static int32_t bst_grow_node_pool();
static bst_node_t *bst_get_node();
static int32_t bst_key_cmp(bst_key_t *left, bst_key_t *right, int64_t flags);
static void bst_key_cpy(bst_key_t *dst, bst_key_t *src, int64_t flags);
static int64_t bst_get_height(bst_node_t *node);
static bst_node_t * bst_right_rotate(bst_node_t *y);
static bst_node_t * bst_left_rotate(bst_node_t *x);
static int64_t max(int64_t x, int64_t y);

// Callbacks
void
tree_free_cb(void *node, void *unused) {
    bst_tree_t *tree = (bst_tree_t *)node;

    if (tree->name)
        free(tree->name);

    // TODO:  Destroy the node's data 
}

int8_t
tree_sort_by_id_cb(void *n1, void *n2) {
    bst_tree_t *t1 = (bst_tree_t *)n1;
    bst_tree_t *t2 = (bst_tree_t *)n2;

    return ((t1->id == t2->id) ? 0 : (t1->id > t2->id) ? -1 : 1);
}

int8_t
remove_tree_cb(void *node, void *tree_id) {
    bst_tree_t *tree = (bst_tree_t *)node;
    int32_t *id = (int32_t *)tree_id;

    if (tree->id == *id)
        return LIST_REMOVE;

    return LIST_KEEP;
}

int8_t
build_tree_ids_cb(void *node, void *data) {
    bst_tree_t *tree = (bst_tree_t *)node;

    tree_ids[tree->id] = tree;

    return LIST_CONTINUE;
}

// Functions
int32_t
find_bst_by_name(char *name) {
    bst_tree_t *t;
    list_node_t *node;
    
    list_read_lock(tree_list);
    node = tree_list->head;
    while (node) {
        t = (bst_tree_t *)node->data;
        if (!strcmp(t->name, name)) {
            return t->id;
        }
        node = node->next;
    }
    list_unlock(tree_list);

    return -1;
}

static int32_t
bst_add_tree(char *tree_name, bst_free_t free_fn, uint64_t flags) {
    bst_tree_t *tree = NULL;

    // Create a new tree
    if ((tree = calloc(1, sizeof(bst_tree_t))) == NULL) {
        snprintf(err_str, MAX_ERR_LEN - 1, "Cannot allocate memory for new tree", tree_name);
        goto error_return;
    }

    if ((tree->name = malloc(strlen(tree_name))) == NULL) {
        snprintf(err_str, MAX_ERR_LEN - 1, "Cannot allocate memory for new tree name", tree_name);
        goto error_return;
    }

    strcpy(tree->name, tree_name);
    tree->idx_count = 1;
    tree->flags[0] = flags;
    tree->free_fn = free_fn;

    // There is a hole somewhere.
    if (list_size(tree_list) != top_tree_id) {
        list_read_lock(tree_list);
        list_node_t *node = tree_list->head;
        bst_tree_t *t = NULL;
        int32_t idx = 0;

        tree->id = -1;
        while (node) {
            t = (bst_tree_t *)node->data;
            if (t->id != idx) {
                tree->id = idx;
                break;
            }
            node = node->next;
            idx++;
        }
        list_unlock(tree_list);

        if (tree->id == -1) {
            snprintf(err_str, MAX_ERR_LEN - 1, "Internal error! (1000)",
                    tree_name);
            goto error_return;
        }
    }
    else {
        tree->id = top_tree_id++;
    }

    if (list_append(tree_list, tree) != 0) {
        snprintf(err_str, MAX_ERR_LEN - 1, "%s", list_get_last_err());
        goto error_return;
    }

    // Always sort the list after an insert.  Things are easier if this list remains in order.
    list_sort(tree_list, tree_sort_by_id_cb);

    if ((tree_ids = realloc(tree_ids, sizeof(bst_tree_t **) * top_tree_id)) == NULL) {
        snprintf(err_str, MAX_ERR_LEN - 1, "Cannot allocate memory for new tree name", tree_name);
        goto error_return;
    }

    list_for_each(tree_list, build_tree_ids_cb, NULL);

    return tree->id;

error_return:
    if (tree->name)
        free(tree->name);
    if (tree)
        free(tree);

    return -1;
}

int32_t
bst_create(char *tree_name, bst_free_t free_fn, int64_t flags) {
    int32_t rc;

    if (top_tree_id != -1 && find_bst_by_name(tree_name) >= 0) {
        snprintf(err_str, MAX_ERR_LEN - 1, "BST with name %s already exists", tree_name);
        return -1;
    }

    pthread_rwlock_wrlock(&reg_lock);
    rc = bst_add_tree(tree_name, free_fn, flags);
    pthread_rwlock_unlock(&reg_lock);

    return rc;
}

int32_t
bst_get(int32_t tree_id, int32_t idx, void **node, void *key) {
    bst_tree_t *bst = tree_ids[tree_id];
    bst_node_t *cur_node = NULL;
    int8_t stop = 0, rc;

    if (!bst) {
        snprintf(err_str, MAX_ERR_LEN - 1, "Invalid tree id: %d", tree_id);
        return -1;
    }

    pthread_rwlock_rdlock(&bst->mutex[idx]);
    if (bst->root[idx] == NULL) {
        *node = NULL;
        return BST_NOTFOUND;
    }
    
    cur_node = bst->root[idx];
    do {
        rc = bst_key_cmp(&cur_node->key, key, bst->flags[idx]);
        switch (rc) {
            case BST_EQUAL:
                *node = cur_node->data;
                stop = 1;
                break;
            case BST_RIGHT_GT:
                if (cur_node->right != NULL) {
                    cur_node = cur_node->right;
                }
                else {
                    *node = cur_node->data;
                    stop = 1;
                }
                break;
            case BST_LEFT_GT:
                if (cur_node->left != NULL) {
                    cur_node = cur_node->left;
                }
                else {
                    *node = cur_node->data;
                    stop = 1;
                }
                break;
        }
       
    } while (!stop);
    pthread_rwlock_unlock(&bst->mutex[idx]);

    return rc;
}

bst_node_t *
bst_insert_recurse(bst_node_t *node, void *key, void *data, int64_t flags) {
    bst_node_t *new_node;
    int32_t rc;
    int64_t lh, rh;
    int8_t balance_factor;

    if (!node) {
        fprintf(stdout, "Adding key: %d\n", *((int32_t *)key));
        new_node = bst_get_node(); 
        if (new_node) {
            bst_key_cpy(&new_node->key, key, flags);
            new_node->height = 1;
            new_node->left = NULL;
            new_node->right = NULL;
            new_node->data = data;
        }
        return new_node;
    }

    rc = bst_key_cmp(&node->key, key, flags);
    if (rc == BST_LEFT_GT) {
        fprintf(stdout, "%d is less than %d, going left\n", *((int32_t *)key), node->key.u32);
        if ((new_node = bst_insert_recurse(node->left, key, data, flags)) == NULL)
            return NULL;
        node->left = new_node;
    }
    else if (rc == BST_RIGHT_GT) {
        fprintf(stdout, "%d is greater than %d, going right\n", *((int32_t *)key), node->key.u32);
        if ((new_node= bst_insert_recurse(node->right, key, data, flags)) == NULL)
            return NULL;
        node->right = new_node;
    }
    else {
        // TODO:  Handle duplicate key
    }

    lh = bst_get_height(node->left);
    rh = bst_get_height(node->right);

    node->height = max(lh, rh) + 1;
    balance_factor = lh - rh;
    fprintf(stdout, "Key: %d: height: %d, bf = %d ( %d - %d)\n", 
            node->key.i32, node->height, balance_factor, lh, rh);

    if (balance_factor > 1 && node->left) {
        rc = bst_key_cmp(&node->left->key, key, flags);
        // Left Left Case
        if (rc == BST_LEFT_GT) {
            fprintf(stdout, "bf == %d and n->l->k(%d) > k(%d), rotating right\n",
                    balance_factor, node->left->key.i32, *((int32_t *)key));
            return bst_right_rotate(node);
        }
        // Left Right Case
        if (rc == BST_RIGHT_GT) {
            fprintf(stdout, "bf == %d and n->l->k(%d) < k(%d), rotating left, then right\n",
                    balance_factor, node->left->key.i32, *((int32_t *)key));
            node->left =  bst_left_rotate(node->left);
            return bst_right_rotate(node);
        }
    }
    if (balance_factor < -1 && node->right) {
        rc = bst_key_cmp(&node->right->key, key, flags);
        // Right Right Case
        if (rc == BST_RIGHT_GT) {
            fprintf(stdout, "bf == %d and n->r->k(%d) < k(%d), rotating left\n",
                    balance_factor, node->right->key.i32, *((int32_t *)key));
            return bst_left_rotate(node);
        }
    
        // Right Left Case
        if (rc == BST_LEFT_GT) {
            fprintf(stdout, "bf == %d and n->r->k(%d) > k(%d), rotating right, then left\n",
                    balance_factor, node->right->key.i32, *((int32_t *)key));
            node->right = bst_right_rotate(node->right);
            return bst_left_rotate(node);
        }
    }

    return node;
}

int32_t
bst_insert(int32_t tree_id, int32_t idx, void *key, void *data) {
    bst_tree_t *bst = tree_ids[tree_id];
    bst_node_t *new_node;

    if (!bst) {
        snprintf(err_str, MAX_ERR_LEN - 1, "Invalid tree id: %d", tree_id);
        return -1;
    }

    pthread_rwlock_wrlock(&bst->mutex[idx]);
    if ((new_node = bst_insert_recurse(bst->root[idx], key, data, bst->flags[idx])) == NULL) {
        snprintf(err_str, MAX_ERR_LEN - 1, "Unable to insert new node.  Out of memory?");
        return -1;
    }
    else
        bst->root[idx] = new_node;
    pthread_rwlock_unlock(&bst->mutex[idx]);

    return 0;

}

void 
bst_destroy(int32_t tree_id, bst_free_t cb, void *cb_data) {
    // TODO: Walk the tree deleting data
    
    // Remove this tree from the list
    pthread_rwlock_wrlock(&reg_lock);
    // TODO:  I need a way to properly delete any trees that get removed
    list_remove_if(tree_list, remove_tree_cb, &tree_id, NULL);
    pthread_rwlock_unlock(&reg_lock);
}

int32_t
bst_init() {
    list_create(&tree_list, tree_free_cb);
    return bst_grow_node_pool();
}

int32_t
bst_fini() {
    list_destroy(bst_node_list, NULL);
    list_destroy(tree_list, NULL);
    return 0;
}

static bst_node_t *
bst_get_node() {
    bst_node_t *node = NULL;

    list_head(bst_node_list, (void **)(&node), 1);
    if (!node) {
        bst_grow_node_pool();
        list_head(bst_node_list, (void **)(&node), 1);
    }

    return node;
}

static int32_t
bst_grow_node_pool() {
    bst_node_t *node = NULL;

    if (!bst_node_list)
        list_create(&bst_node_list, NULL);

    for (int32_t i = 0; i < BST_NODE_POOL_SZ; i++) {
        if ((node = calloc(1, sizeof(bst_node_t))) == NULL) {
            snprintf(err_str, MAX_ERR_LEN - 1, "Unable to allocate memory for node pool");
            return -1;
        }
        list_append(bst_node_list, node); 
    }

    return 0;
}

static int32_t
bst_key_cmp(bst_key_t *left, bst_key_t *right, int64_t flags) {
    switch (flags & BST_KEYS) {
        case BST_KPSTR: 
            return strcmp(left->pstr, right->pstr);
        case BST_KINT8:
            return (left->i8 == right->i8) ? BST_EQUAL : (left->i8 < right->i8) ? 
                BST_RIGHT_GT : BST_LEFT_GT;
        case BST_KINT16:
            return (left->i16 == right->i16) ? BST_EQUAL : (left->i16 < right->i16) ? 
                BST_RIGHT_GT : BST_LEFT_GT;
        case BST_KINT32:
            return (left->i32 == right->i32) ? BST_EQUAL : (left->i32 < right->i32) ? 
                BST_RIGHT_GT : BST_LEFT_GT;
        case BST_KINT64:
            return (left->i32 == right->i32) ? BST_EQUAL : (left->i32 < right->i32) ? 
                BST_RIGHT_GT : BST_LEFT_GT;
        case BST_KUINT8:
            return (left->u8 == right->u8) ? BST_EQUAL : (left->u8 < right->u8) ? 
                BST_RIGHT_GT : BST_LEFT_GT;
        case BST_KUINT16:
            return (left->u16 == right->u16) ? BST_EQUAL : (left->u16 < right->u16) ? 
                BST_RIGHT_GT : BST_LEFT_GT;
        case BST_KUINT32:
            return (left->u32 == right->u32) ? BST_EQUAL : (left->u32 < right->u32) ?
                BST_RIGHT_GT : BST_LEFT_GT;
        case BST_KUINT64:
            return (left->u64 == right->u64) ? BST_EQUAL : (left->u64 < right->u64) ? 
                BST_RIGHT_GT : BST_LEFT_GT;
        case BST_KINT128:
            return (left->i128 == right->i128) ? BST_EQUAL : (left->i128 < right->i128) ? 
                BST_RIGHT_GT : BST_LEFT_GT;
        case BST_KTME:
            if (left->tv.tv_sec != right->tv.tv_sec) {
                return (left->tv.tv_sec < right->tv.tv_sec) ? BST_RIGHT_GT : BST_LEFT_GT;
            }
            if (left->tv.tv_usec != right->tv.tv_usec) {
                return (left->tv.tv_usec < right->tv.tv_usec) ? BST_RIGHT_GT : BST_LEFT_GT;
            }
            return 0;
        default:
            return 0;
    }

    return 0;
}

static void
bst_key_cpy(bst_key_t *dst, bst_key_t *src, int64_t flags) {
    switch (flags & BST_KEYS) {
        case BST_KPSTR: 
            strcpy(dst->pstr, src->pstr);
            break;
        case BST_KINT8:
            dst->i8 = src->i8;
            break;
        case BST_KINT16:
            dst->i16 = src->i16;
            break;
        case BST_KINT32:
            dst->i32 = src->i32;
            break;
        case BST_KINT64:
            dst->i64 = src->i64;
            break;
        case BST_KUINT8:
            dst->u8 = src->u8;
            break;
        case BST_KUINT16:
            dst->u16 = src->u16;
            break;
        case BST_KUINT32:
            dst->u32 = src->u32;
            break;
        case BST_KUINT64:
            dst->u64 = src->u64;
            break;
        case BST_KINT128:
            dst->i128 = src->i128;
            break;
        case BST_KTME:
            dst->tv.tv_sec = src->tv.tv_sec;
            dst->tv.tv_usec = src->tv.tv_usec;
            break;
    }
}

static int64_t
bst_get_height(bst_node_t *node) {
    if (!node)
        return 0;

    return (node->height);
}

static bst_node_t *
bst_right_rotate(bst_node_t *y) {
    bst_node_t *x = y->left;
    bst_node_t *z = x->right;

    x->right = y;
    y->left = z;

    y->height = max(bst_get_height(y->left), bst_get_height(y->right)) + 1;
    x->height = max(bst_get_height(x->left), bst_get_height(x->right)) + 1;

    return x;
}

static bst_node_t *
bst_left_rotate(bst_node_t *x) {
    bst_node_t *y = x->right;
    bst_node_t *z = y->left;

    y->left = x;
    x->right = z;

    x->height = max(bst_get_height(x->left), bst_get_height(x->right)) + 1;
    y->height = max(bst_get_height(y->left), bst_get_height(y->right)) + 1;
    
    fprintf(stdout, "node: %d(x) height == %d, node: %d(y) height == %d\n", 
            x->key.u32, x->height, y->key.u32, y->height);

    return y;
}

static void
bst_print_tree_recurse(bst_node_t *node) {
    if (node != NULL) {
        fprintf(stdout, "%d ", node->key.u32);
        bst_print_tree_recurse(node->left);
        bst_print_tree_recurse(node->right);
    }
}

inline static int64_t
max(int64_t x, int64_t y) {
    return ((x > y) ? x : y);
}

void
bst_print_tree(int32_t tree_id, int32_t idx) {
    bst_tree_t *bst = tree_ids[tree_id];

    bst_print_tree_recurse(bst->root[idx]);
}

char *
bst_get_last_err() {
    return err_str;
}


