#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/param.h>

#include "al_data_struct.h"

// Defines
#define BST_NODE_POOL_SZ 1024000
#define MAX_ERR_LEN 2048

const int64_t BST_KEYS = BST_KPSTR | BST_KINT8 | BST_KINT16 | BST_KINT32 | BST_KINT64 | 
    BST_KUINT8 | BST_KUINT16 | BST_KUINT32 | BST_KUINT64 | BST_KINT128 | BST_KTME;

#define BST_EQUAL 0
#define BST_RIGHT_GT -1
#define BST_LEFT_GT 1

#define bst_get_height(x) (x != NULL ? x->height : 0)

#ifndef NO_LOCKS 
#define bst_new_node(node) do {                         \
    if (!bst_node_list)                                 \
        bst_create_node_pool();                         \
    pthread_rwlock_wrlock(&node_list_mutex);            \
    node = bst_node_list;                               \
    bst_node_list = bst_node_list->right;               \
    node->right = NULL;                                 \
    pthread_rwlock_unlock(&node_list_mutex);            \
 } while (0)
#else
#define bst_new_node(node) do {                         \
    if (!bst_node_list)                                 \
        bst_create_node_pool();                         \
    node = bst_node_list;                               \
    bst_node_list = bst_node_list->right;               \
    node->right = NULL;                                 \
 } while (0)
#endif

#ifndef NO_LOCKS
#define bst_free_node(node) do {                        \
    pthread_rwlock_wrlock(&node_list_mutex);            \
    node->right = bst_node_list;                        \
    node->left = NULL;                                  \
    bst_node_list = node;                               \
    pthread_rwlock_unlock(&node_list_mutex);            \
} while (0)
#else
#define bst_free_node(node) do {                        \
    node->right = bst_node_list;                        \
    node->left = NULL;                                  \
    bst_node_list = node;                               \
} while (0)
#endif

typedef union bst_key_u {
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

typedef struct bst_node_s {
    struct bst_node_s *left;
    struct bst_node_s *right;
    int8_t height;
    bst_key_t key;
    void *data;
} bst_node_t;

// Globals
bst_node_t *bst_node_list;
pthread_rwlock_t node_list_mutex = PTHREAD_RWLOCK_INITIALIZER;
char err_str[MAX_ERR_LEN];
list_t *tree_list = NULL;

static int32_t bst_create_node_pool();
static bst_node_t * bst_right_rotate(bst_node_t *y);
static bst_node_t * bst_left_rotate(bst_node_t *x);
static void bst_delete_data(bst_node_t *node, bst_free_t free_fn, void *fn_data);
static void bst_set_key_fn_ptrs(bst_tree_t *tree, int64_t flags);
static void bst_free_node_pool();
static int32_t bst_print_tree_r(bst_node_t *node, int32_t is_left, int32_t offset, int32_t depth, 
        int32_t compact, char s[128][512]);

// Callbacks
void
tree_free_cb(void *node, void *unused) {
    bst_tree_t *tree = (bst_tree_t *)node;

    // TODO: Remove multiple indexes
    for (int32_t i = 0; i < tree->idx_count; i++) {
        bst_delete_data(tree->root[i], tree->free_fn[i], NULL);
    }

    if (tree->name)
        free(tree->name);
}

int8_t
remove_tree_cb(void *node, void *data) {
    bst_tree_t *tree = (bst_tree_t *)node;
    bst_tree_t *doomed = (bst_tree_t *)data;

    if (tree == doomed)
        return LIST_REMOVE;

    return LIST_KEEP;
}

// Functions
bst_tree_t *
bst_find_by_name(char *name) {
    bst_tree_t *t;
    list_node_t *node;
    
    list_read_lock(tree_list);
    node = tree_list->head;
    while (node) {
        t = (bst_tree_t *)node->data;
        if (!strcmp(t->name, name)) {
            list_unlock(tree_list);
            return t;
        }
        node = node->next;
    }
    list_unlock(tree_list);

    return NULL;
}

bst_tree_t *
bst_create(char *tree_name, bst_free_t free_fn, int64_t flags) {
    int32_t rc;
    bst_tree_t *tree;

    if (bst_find_by_name(tree_name)) {
        snprintf(err_str, MAX_ERR_LEN - 1, "BST with name %s already exists", tree_name);
        return NULL;
    }

    // Create a new tree
    if ((tree = calloc(1, sizeof(bst_tree_t))) == NULL) {
        snprintf(err_str, MAX_ERR_LEN - 1, "Cannot allocate memory for new tree");
        goto error_return;
    }

    if (tree_name) {
        if ((tree->name = malloc(strlen(tree_name))) == NULL) {
            snprintf(err_str, MAX_ERR_LEN - 1, "Cannot allocate memory for new tree name");
            goto error_return;
        }

        strcpy(tree->name, tree_name);
    }

    tree->idx_count = 1;
    tree->flags[0] = flags;
    tree->free_fn[0] = free_fn;

    if (list_append(tree_list, tree) != 0) {
        snprintf(err_str, MAX_ERR_LEN - 1, "%s", list_get_last_err());
        goto error_return;
    }

    bst_set_key_fn_ptrs(tree, flags);

    return tree;

error_return:
    if (tree->name)
        free(tree->name);
    if (tree)
        free(tree);

    return NULL;
}

int32_t
bst_add_idx(bst_tree_t *tree, bst_free_t free_fn, int64_t flags) {
    int8_t idx = tree->idx_count++;

    tree->root[idx] = NULL;
    tree->flags[idx] = flags;
    tree->free_fn[idx] = free_fn;

    return idx;
}

void *
bst_fetch(bst_tree_t *tree, int32_t idx, void *key) {
    bst_node_t *node;
    int32_t rc = -1;

#ifndef NO_LOCKS
    pthread_rwlock_rdlock(&tree->mutex[idx]);
#endif
    node = tree->root[idx];
    while (node && rc != BST_EQUAL) {
        rc = tree->key_cmp_fn(&node->key, key);
        switch (rc) {
            case BST_RIGHT_GT:
                node = node->right;
                break;
            case BST_LEFT_GT:
                node = node->left;
                break;
        }
    }
#ifndef NO_LOCKS
    pthread_rwlock_unlock(&tree->mutex[idx]);
#endif

    return (node ? node->data : NULL);
}

static bst_node_t *
bst_insert_r(bst_tree_t *tree, bst_node_t *node, void *key, void *data) {
    bst_node_t *new_node;
    int64_t lh, rh;
    int32_t rc;
    static int32_t rotated = 0;

    if (!node) {
         bst_new_node(new_node); 
         tree->key_cpy_fn(&new_node->key, key);
         new_node->data = data;
         rotated = 0;
         return new_node;
    }

    rc = tree->key_cmp_fn(&node->key, key);
    if (rc == BST_LEFT_GT)
         node->left = bst_insert_r(tree, node->left, key, data);
    else if (rc == BST_RIGHT_GT) {
         node->right = bst_insert_r(tree, node->right, key, data);
    }
    else {
        // TODO:  Handle duplicate key
    }

    // If we've already done our rotations, just return from here
    if (rotated) {
        return node;
    }

    lh = bst_get_height(node->left);
    rh = bst_get_height(node->right);

    node->height = MAX(lh, rh) + 1;

    if ((lh - rh) > 1) {
        rotated = 1;
        rc = tree->key_cmp_fn(&node->left->key, key);
        // Left Left Case
        if (rc == BST_LEFT_GT) {
            return bst_right_rotate(node);
        }
        // Left Right Case
        else {
            node->left = bst_left_rotate(node->left);
            return bst_right_rotate(node);
        }
    }
    else if ((lh - rh) < -1) {
        rotated = 1;
        rc = tree->key_cmp_fn(&node->right->key, key);
        // Right Right Case
        if (rc == BST_RIGHT_GT) {
            return bst_left_rotate(node);
        }
        // Right Left Case
        else {
            node->right = bst_right_rotate(node->right);
            return bst_left_rotate(node);
        }
    }

    return node;
}

int32_t
bst_insert(bst_tree_t *tree, int32_t idx, void *key, void *data) {
    bst_node_t *new_node;

#ifndef NO_LOCKS
    pthread_rwlock_wrlock(&tree->mutex[idx]);
#endif
    if ((new_node = bst_insert_r(tree, tree->root[idx], key, data)) == NULL) {
        snprintf(err_str, MAX_ERR_LEN - 1, "Unable to insert new node.  Out of memory?");
        return -1;
    }

    tree->root[idx] = new_node;

#ifndef NO_LOCKS
    pthread_rwlock_unlock(&tree->mutex[idx]);
#endif

    return 0;
}

void 
bst_destroy(bst_tree_t *tree, void *fn_data) {
#ifndef NO_LOCKS
    pthread_rwlock_wrlock(&tree->mutex[0]);
#endif
    // Remove this tree from the list
    list_remove_if(tree_list, remove_tree_cb, tree, NULL);
#ifndef NO_LOCKS
    pthread_rwlock_unlock(&tree->mutex[0]);
#endif

}

int32_t
bst_init() {
    list_create(&tree_list, tree_free_cb);
    return bst_create_node_pool();
}

int32_t
bst_fini() {
    bst_free_node_pool();
    list_destroy(tree_list, NULL);
    return 0;
}

static void
bst_delete_data(bst_node_t *node, bst_free_t free_fn, void *fn_data) {
    if (node != NULL) {
        bst_delete_data(node->left, free_fn, fn_data);
        bst_delete_data(node->right, free_fn, fn_data);
        if (free_fn)
            free_fn(node, fn_data);
        else
            free(node->data);
    }
}

static int32_t
bst_key_cmp_str(bst_key_t *left, bst_key_t *right) {
    return strcmp(left->pstr, right->pstr);
}

static int32_t
bst_key_cmp_i8(bst_key_t *left, bst_key_t *right) {
    return (left->i8 == right->i8) ? BST_EQUAL : (left->i8 < right->i8) ?
        BST_RIGHT_GT : BST_LEFT_GT;
}

static int32_t
bst_key_cmp_i16(bst_key_t *left, bst_key_t *right) {
    return (left->i16 == right->i16) ? BST_EQUAL : (left->i16 < right->i16) ?
        BST_RIGHT_GT : BST_LEFT_GT;
}

static int32_t
bst_key_cmp_i32(bst_key_t *left, bst_key_t *right) {
    return (left->i32 == right->i32) ? BST_EQUAL : (left->i32 < right->i32) ?
        BST_RIGHT_GT : BST_LEFT_GT;
}

static int32_t
bst_key_cmp_i64(bst_key_t *left, bst_key_t *right) {
    return (left->i64 == right->i64) ? BST_EQUAL : (left->i64 < right->i64) ?
        BST_RIGHT_GT : BST_LEFT_GT;
}

static int32_t
bst_key_cmp_u8(bst_key_t *left, bst_key_t *right) {
    return (left->u8 == right->u8) ? BST_EQUAL : (left->u8 < right->u8) ?
        BST_RIGHT_GT : BST_LEFT_GT;
}

static int32_t
bst_key_cmp_u16(bst_key_t *left, bst_key_t *right) {
    return (left->u64 == right->u16) ? BST_EQUAL : (left->u16 < right->u16) ?
        BST_RIGHT_GT : BST_LEFT_GT;
}

static int32_t
bst_key_cmp_u32(bst_key_t *left, bst_key_t *right) {
    return (left->u32 == right->u32) ? BST_EQUAL : (left->u32 < right->u32) ?
        BST_RIGHT_GT : BST_LEFT_GT;
}

static int32_t
bst_key_cmp_u64(bst_key_t *left, bst_key_t *right) {
    return (left->u64 == right->u64) ? BST_EQUAL : (left->u64 < right->u64) ?
        BST_RIGHT_GT : BST_LEFT_GT;
}

static int32_t
bst_key_cmp_i128(bst_key_t *left, bst_key_t *right) {
    return (left->i128 == right->i128) ? BST_EQUAL : (left->i128 < right->i128) ?
        BST_RIGHT_GT : BST_LEFT_GT;
}

static int32_t
bst_key_cmp_tme(bst_key_t *left, bst_key_t *right) {
    if (left->tv.tv_sec != right->tv.tv_sec) {
        return (left->tv.tv_sec < right->tv.tv_sec) ? BST_RIGHT_GT : BST_LEFT_GT;
    }
    if (left->tv.tv_usec != right->tv.tv_usec) {
        return (left->tv.tv_usec < right->tv.tv_usec) ? BST_RIGHT_GT : BST_LEFT_GT;
    }
    return 0;
}

static void
bst_key_cpy_str(bst_key_t *dst, bst_key_t *src) {
    strcpy(dst->pstr, src->pstr);
}

static void
bst_key_cpy_i8(bst_key_t *dst, bst_key_t *src) {
    memcpy(dst, src, sizeof(int8_t));
}

static void
bst_key_cpy_i16(bst_key_t *dst, bst_key_t *src) {
    memcpy(dst, src, sizeof(int16_t));
}

static void
bst_key_cpy_i32(bst_key_t *dst, bst_key_t *src) {
    memcpy(dst, src, sizeof(int32_t));
}

static void
bst_key_cpy_i64(bst_key_t *dst, bst_key_t *src) {
    memcpy(dst, src, sizeof(int64_t));
}

static void
bst_key_cpy_u8(bst_key_t *dst, bst_key_t *src) {
    memcpy(dst, src, sizeof(uint8_t));
}

static void
bst_key_cpy_u16(bst_key_t *dst, bst_key_t *src) {
    memcpy(dst, src, sizeof(uint16_t));
}

static void
bst_key_cpy_u32(bst_key_t *dst, bst_key_t *src) {
    memcpy(dst, src, sizeof(uint32_t));
}

static void
bst_key_cpy_u64(bst_key_t *dst, bst_key_t *src) {
    memcpy(dst, src, sizeof(uint64_t));
}

static void
bst_key_cpy_i128(bst_key_t *dst, bst_key_t *src) {
    dst->i128 = src->i128;
}

static void
bst_key_cpy_tme(bst_key_t *dst, bst_key_t *src) {
    memcpy(dst, src, sizeof(struct timeval));
}

static void
bst_set_key_fn_ptrs(bst_tree_t *tree, int64_t flags) {
    switch (flags & BST_KEYS) {
        case BST_KPSTR:
            tree->key_cmp_fn = bst_key_cmp_str;
            tree->key_cpy_fn = bst_key_cpy_str;
            break;
        case BST_KINT8:
            tree->key_cmp_fn = bst_key_cmp_i8;
            tree->key_cpy_fn = bst_key_cpy_i8;
            break;
        case BST_KINT16:
            tree->key_cmp_fn = bst_key_cmp_i16;
            tree->key_cpy_fn = bst_key_cpy_i16;
            break;
        case BST_KINT32:
            tree->key_cmp_fn = bst_key_cmp_i32;
            tree->key_cpy_fn = bst_key_cpy_i32;
            break;
        case BST_KINT64:
            tree->key_cmp_fn = bst_key_cmp_i64;
            tree->key_cpy_fn = bst_key_cpy_i64;
            break;
        case BST_KUINT8:
            tree->key_cmp_fn = bst_key_cmp_u8;
            tree->key_cpy_fn = bst_key_cpy_u8;
            break;
        case BST_KUINT16:
            tree->key_cmp_fn = bst_key_cmp_u16;
            tree->key_cpy_fn = bst_key_cpy_u16;
            break;
        case BST_KUINT32:
            tree->key_cmp_fn = bst_key_cmp_u32;
            tree->key_cpy_fn = bst_key_cpy_u32;
            break;
        case BST_KUINT64:
            tree->key_cmp_fn = bst_key_cmp_u64;
            tree->key_cpy_fn = bst_key_cpy_u64;
            break;
        case BST_KINT128:
            tree->key_cmp_fn = bst_key_cmp_i128;
            tree->key_cpy_fn = bst_key_cpy_i128;
            break;
        case BST_KTME:
            tree->key_cmp_fn = bst_key_cmp_tme;
            tree->key_cpy_fn = bst_key_cpy_tme;
            break;
        default:
            snprintf(err_str, MAX_ERR_LEN -1, "Can't happen at: %s():%d", __FUNCTION__, __LINE__);
            break;
    }
}

static bst_node_t *
bst_right_rotate(bst_node_t *y) {
    bst_node_t *x = y->left;
    bst_node_t *z = x->right;

    x->right = y;
    y->left = z;

    y->height = MAX(bst_get_height(y->left), bst_get_height(y->right)) + 1;
    x->height = MAX(bst_get_height(x->left), bst_get_height(x->right)) + 1;

    return x;
}

static bst_node_t *
bst_left_rotate(bst_node_t *x) {
    bst_node_t *y = x->right;
    bst_node_t *z = y->left;

    y->left = x;
    x->right = z;

    x->height = MAX(bst_get_height(x->left), bst_get_height(x->right)) + 1;
    y->height = MAX(bst_get_height(y->left), bst_get_height(y->right)) + 1;

    return y;
}

/*static void
bst_print_tree_r(bst_node_t *node) {
    if (node != NULL) {
        fprintf(stdout, "%d ", node->key.u32);
        bst_print_tree_r(node->left);
        bst_print_tree_r(node->right);
    }
}*/

char *
bst_get_last_err() {
    return err_str;
}

static int32_t
bst_create_node_pool() {
    bst_node_t *node = NULL;

    pthread_rwlock_wrlock(&node_list_mutex);
    for (uint32_t j = 0; j < BST_NODE_POOL_SZ; j++) {
        if ((node = calloc(1, sizeof(bst_node_t))) == NULL) {
            snprintf(err_str, MAX_ERR_LEN -1, "%s: Could not allocate memory for bst node",
                    __FUNCTION__);
            pthread_rwlock_unlock(&node_list_mutex);
            return -1;
        }

        node->height = 1;
        node->right = bst_node_list;
        bst_node_list = node;

    }
    pthread_rwlock_unlock(&node_list_mutex);

    return 0;
}

static void
bst_free_node_pool() {
    bst_node_t *n, *head;

    pthread_rwlock_wrlock(&node_list_mutex);
    head = bst_node_list;
    while (head) {
        n = head->right;
        free(head);
        head = n;
    }
    pthread_rwlock_unlock(&node_list_mutex);
}

void
bst_print_tree(bst_tree_t *tree, int32_t idx, int32_t compact) {
    char s[128][512];
    char test[101];

    for (int32_t i = 0; i < 20; i++)
        sprintf(s[i], "%100s", " ");

    bst_print_tree_r(tree->root[idx], 0, 0, 0, compact, s);
    for (int32_t i = 0; i < 20; i++) {
        sprintf(test, "%100s", " ");
        if (strcmp(s[i], test))
            fprintf(stdout, "%s\n", s[i]);
    }
}

static int32_t
bst_print_tree_r(bst_node_t *node, int is_left, int offset, int depth, int32_t compact,
        char s[128][512]) {
    char b[20];
    int32_t width;

    if (!node) 
        return 0;

    if (compact) {
        width = 1;
        sprintf(b, "x", node->key.i32);
    }
    else {
        width = 5;
        sprintf(b, "(%03d)", node->key.i32);
    }

    int32_t left  = bst_print_tree_r(node->left,  1, offset, depth + 1, compact, s);
    int32_t right = bst_print_tree_r(node->right, 0, offset + left + width, depth + 1, 
            compact, s);

    if (compact) {
        for (int i = 0; i < width; i++)
            s[depth][offset + left + i] = b[i];

        if (depth && is_left) {

            for (int i = 0; i < width + right; i++)
                s[depth - 1][offset + left + width/2 + i] = '-';

            s[depth - 1][offset + left + width/2] = '.';

        } else if (depth && !is_left) {

            for (int i = 0; i < left + width; i++)
                s[depth - 1][offset - width/2 + i] = '-';

            s[depth - 1][offset + left + width/2] = '.';
        }
    }
    else {
        for (int32_t i = 0; i < width; i++)
            s[2 * depth][offset + left + i] = b[i];

        if (depth && is_left) {
            for (int i = 0; i < width + right; i++)
                s[2 * depth - 1][offset + left + width/2 + i] = '-';

            s[2 * depth - 1][offset + left + width/2] = '+';
            s[2 * depth - 1][offset + left + width + right + width/2] = '+';

        } else if (depth && !is_left) {
            for (int i = 0; i < left + width; i++)
                s[2 * depth - 1][offset - width/2 + i] = '-';

            s[2 * depth - 1][offset + left + width/2] = '+';
            s[2 * depth - 1][offset - width/2 - 1] = '+';
        }
    }

    return left + width + right;
}
