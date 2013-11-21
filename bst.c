#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#include "al_data_struct.h"

// Structures
typedef struct bst_node_s {
    struct bst_node_s *left;
    struct bst_node_s *right;
    struct bst_node_s *parent;
    int8_t balance;
    void *data;
} bst_node_t;

typedef struct {
    bst_node_t *head;
    pthread_mutex_t lock;  
} bst_node_pool_t;

typedef struct {
    bst_node_t *root;
    char *pool_name;
    bst_node_t *next;
    bst_node_t *prev;
} bst_tree_t;

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

// Defines
#define BST_NODE_POOL_SZ 102400
#define MAX_ERR_LEN 2048

// Globals
char err_str[MAX_ERR_LEN];
bst_node_pool_t bst_pool = {
    NULL, 
    PTHREAD_MUTEX_INITIALIZER
};


char *
bst_get_last_error() {
    return err_str;
}

static int32_t
bst_create_node_pool() {
    bst_node_t *node = NULL;

    pthread_mutex_lock(&bst_pool.lock);
    for (uint32_t i = 0; i < BST_NODE_POOL_SZ; i++) {
        // Create a new node
        if ((node = calloc(1, sizeof(bst_node_t))) == NULL) {
            pthread_mutex_unlock(&bst_pool.lock);
            snprintf(err_str, MAX_ERR_LEN - 1, "Unable to allocate memory for node pool");
            return -1;
        }

        if (bst_pool.head) {
            bst_pool.head->left = node;
            node->right = bst_pool.head;
        }
        bst_pool.head = node;
    } 
    pthread_mutex_unlock(&bst_pool.lock);

    return 0;
}

int32_t
bst_init() {
    return bst_create_node_pool();
}

int32_t
bst_fini() {
    bst_node_t *next_node = NULL;

    pthread_mutex_lock(&bst_pool.lock);
    while (bst_pool.head) {
        next_node = bst_pool.head->right;
        free(bst_pool.head);
        bst_pool.head = next_node;
    }
    pthread_mutex_unlock(&bst_pool.lock);

    return 0;
}
