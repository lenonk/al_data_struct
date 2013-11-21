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

// Structures
typedef struct {
    uint64_t low;
    uint64_t high;
} int128_t;

typedef union {
    char *pstr;
    char str;
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    int128_t u128;
    struct timeval tv;
} bst_key_t;

typedef struct {
    list_t *node_list;
    pthread_mutex_t lock;  
} bst_node_pool_t;

typedef struct bst_node_s {
    struct bst_node_s *left;
    struct bst_node_s *right;
    struct bst_node_s *parent;
    int8_t balance;
    void *key;
    void *data;
} bst_node_t;

typedef struct {
    uint8_t idx_count;
    int32_t id;
    uint64_t flags[BST_MAX_IDX];
    char *name;
    bst_node_t *root[BST_MAX_IDX];
    pthread_rwlock_t mutex[BST_MAX_IDX];
} bst_tree_t;

// Globals
bst_node_pool_t bst_pool = {
    .node_list = NULL, 
    .lock = PTHREAD_MUTEX_INITIALIZER
};

int32_t top_tree_id = 0;
char err_str[MAX_ERR_LEN];
bst_tree_t **tree_ids = NULL;
list_t *tree_list = NULL;
pthread_rwlock_t reg_lock = PTHREAD_RWLOCK_INITIALIZER;

static int32_t bst_grow_node_pool();

// Functions
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
bst_add_tree(char *tree_name, uint64_t flags) {
    bst_tree_t *tree = NULL;

    // Create the tree list if necessary
    if (!tree_list) {
        list_create(&tree_list, tree_free_cb);
    }

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
bst_create(char *tree_name, int64_t flags) {
    int32_t rc;

    if (find_bst_by_name(tree_name) >= 0) {
        snprintf(err_str, MAX_ERR_LEN - 1, "BST with name %s already exists", tree_name);
        return -1;
    }

    pthread_rwlock_wrlock(&reg_lock);
    rc = bst_add_tree(tree_name, flags);
    pthread_rwlock_unlock(&reg_lock);

    return rc;
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
    return bst_grow_node_pool();
}

int32_t
bst_fini() {
    pthread_mutex_lock(&bst_pool.lock);
    list_destroy(bst_pool.node_list, NULL);
    pthread_mutex_unlock(&bst_pool.lock);

    list_destroy(tree_list, NULL);

    return 0;
}

static int32_t
bst_grow_node_pool() {
    bst_node_t *node = NULL;

    pthread_mutex_lock(&bst_pool.lock);
    if (!bst_pool.node_list)
        list_create(&bst_pool.node_list, NULL);

    for (int32_t i = 0; i < BST_NODE_POOL_SZ; i++) {
        if ((node = calloc(1, sizeof(bst_node_t))) == NULL) {
            pthread_mutex_unlock(&bst_pool.lock);
            snprintf(err_str, MAX_ERR_LEN - 1, "Unable to allocate memory for node pool");
            return -1;
        }
        list_append(bst_pool.node_list, node); 
    }
    pthread_mutex_unlock(&bst_pool.lock);

    return 0;
}

char *
bst_get_last_err() {
    return err_str;
}


