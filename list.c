#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "al_data_struct.h"

#define MAX_ERR_SZ 2048

char err_str[MAX_ERR_SZ];

char *
list_get_last_err() {
    return err_str;
}

int32_t
list_create(list_t **list, free_fn_t free_fn) {

    if ((*list = calloc(1, sizeof(list_t))) == NULL) {
        snprintf(err_str, MAX_ERR_SZ - 1, "Failed to allocate memory for new list");
        return -1;
    }

    (*list)->list_size = 0;
    (*list)->head = NULL;
    (*list)->tail = NULL;
    (*list)->free_fn = free_fn;

    return 0;
}

void 
list_destroy(list_t *list) {
    list_node_t *cur_node;

    while (list->head) {
        cur_node = list->head;
        list->head = cur_node->next;

        if (list->free_fn)
            list->free_fn(cur_node->data);
        else
            free(cur_node->data);

        free(cur_node);
    }
}

int32_t
list_prepend(list_t *list, void *elem) {
    list_node_t *new_node;

    if ((new_node = calloc(1, sizeof(list_node_t))) == NULL) {
        snprintf(err_str, MAX_ERR_SZ - 1, "Failed to allocate memory for new list");
        return -1;
    }

    new_node->data = elem;

    if (list->list_size == 0) {
        list->head = list->tail = new_node;
        return 0;
    }

    new_node->data = elem;
    new_node->next = list->head;
    list->head->prev = new_node;
    list->head = new_node;

    list->list_size++;

    return 0;
}

int32_t
list_append(list_t *list, void *elem) {
    list_node_t *new_node;

    if ((new_node = calloc(1, sizeof(list_node_t))) == NULL) {
        snprintf(err_str, MAX_ERR_SZ - 1, "Failed to allocate memory for new list");
        return -1;
    }

    new_node->data = elem;

    if (list->list_size == 0) {
        list->head = list->tail = new_node;
        return 0;
    }

    list->tail->next = new_node;
    new_node->prev = list->tail;
    list->tail = new_node;

    list->list_size++;

    return 0;
}

void
list_for_each(list_t *list, list_iterator_t it, void *data) {
    int8_t rc;
    list_node_t *node = NULL;

    if (!(it && (node = list->head)))
        return;

    do {
        rc = it(node->data, data);
    } while (node && rc);

    return;
}

void
list_head(list_t *list, void **node, int8_t remove) {
    list_node_t *cur_node = NULL;

    if (!list->head)
        return;

    cur_node = list->head;

    if (remove) {
        list->head = cur_node->next;
        list->head->prev = NULL;
        cur_node->next = cur_node->prev = NULL;
    }

    *node = cur_node->data;

    return;
}

void
list_tail(list_t *list, void **node, int8_t remove) {
    list_node_t *cur_node = NULL;

    if (!list->tail)
        return;

    cur_node = list->tail;

    if (remove) {
        list->tail = cur_node->prev;
        list->tail->next = NULL;
        cur_node->next = cur_node->prev = NULL;
    }

    *node = cur_node->data;

    return;
}

int32_t
list_size(list_t *list) {
    return list->list_size;
}
