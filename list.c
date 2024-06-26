#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>

#include "al_data_struct.h"

#define MAX_ERR_SZ 2048

char err_str[MAX_ERR_SZ];

char *
list_get_last_err() {
    return err_str;
}

int32_t
list_create(list_t **list, list_free_t free_fn) {

    if ((*list = calloc(1, sizeof(list_t))) == NULL) {
        snprintf(err_str, MAX_ERR_SZ - 1, "Failed to allocate memory for new list");
        return -1;
    }

    (*list)->list_size = 0;
    (*list)->head = NULL;
    (*list)->tail = NULL;
    (*list)->free_fn = free_fn;

    pthread_rwlock_init(&((*list)->mutex), NULL);

    return 0;
}

void 
list_destroy(list_t *list, void *fn_data) {
    list_node_t *cur_node;

    pthread_rwlock_wrlock(&list->mutex);
    while (list->head) {
        cur_node = list->head;
        list->head = cur_node->next;

        if (list->free_fn)
            list->free_fn(cur_node->data, fn_data);
        else
            free(cur_node->data);

        free(cur_node);
    }

    free(list);
    pthread_rwlock_unlock(&list->mutex);
}

int32_t
list_prepend(list_t *list, void *elem) {
    list_node_t *new_node;

    if ((new_node = calloc(1, sizeof(list_node_t))) == NULL) {
        snprintf(err_str, MAX_ERR_SZ - 1, "Failed to allocate memory for new list");
        return -1;
    }

    new_node->data = elem;

    pthread_rwlock_wrlock(&list->mutex);
    if (list->list_size == 0) {
        list->head = list->tail = new_node;
    }
    else {
        new_node->data = elem;
        new_node->next = list->head;
        list->head->prev = new_node;
        list->head = new_node;
    }

    list->list_size++;
    pthread_rwlock_unlock(&list->mutex);

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

    pthread_rwlock_wrlock(&list->mutex);
    if (list->list_size == 0) {
        list->head = list->tail = new_node;
    }
    else {
        list->tail->next = new_node;
        new_node->prev = list->tail;
        list->tail = new_node;
    }

    list->list_size++;
    pthread_rwlock_unlock(&list->mutex);

    return 0;
}

void
list_for_each(list_t *list, list_iterator_t it, void *data) {
    int8_t rc;
    list_node_t *cur_node = NULL;

    if (!(it && (cur_node = list->head)))
        return;

    pthread_rwlock_rdlock(&list->mutex);
    do {
        rc = it(cur_node->data, data);
        cur_node = cur_node->next;
    } while (cur_node && rc != LIST_STOP);
    pthread_rwlock_unlock(&list->mutex);

    return;
}

int32_t
list_pop_head(list_t *list, void **node) {
    list_node_t *cur_node;

    pthread_rwlock_wrlock(&list->mutex);
    cur_node = list->head;
    
    if (!cur_node)
        return -1;

    if (list->head != list->tail) {
        list->head = cur_node->next;
        list->head->prev = NULL;
    }
    else {
        list->head = list->tail = NULL;
    }

    list->list_size--;

    (cur_node) ? (*node = cur_node->data) : (*node = NULL);

    //free(cur_node);

    return 0;
}

int32_t
list_pop_tail(list_t *list, void **node) {
    list_node_t *cur_node;

    pthread_rwlock_wrlock(&list->mutex);
    cur_node = list->tail;
    
    if (!cur_node)
        return -1;

    if (list->tail != list->head) {
        list->tail = cur_node->prev;
        list->tail->next = NULL;
    }
    else {
        list->head = list->tail = NULL;
    }

    list->list_size--;

    (cur_node) ? (*node = cur_node->data) : (*node = NULL);

    //free(cur_node);

    return 0;
}

int32_t
list_head(list_t *list, void **node) {
    list_node_t *cur_node;
 
    pthread_rwlock_rdlock(&list->mutex);
    cur_node = list->head;
    pthread_rwlock_unlock(&list->mutex);

    (cur_node) ? (*node = cur_node->data) : (*node = NULL);

    return 0;
}

int32_t
list_tail(list_t *list, void **node) {
    list_node_t *cur_node;

    pthread_rwlock_rdlock(&list->mutex);
    cur_node = list->tail;
    pthread_rwlock_unlock(&list->mutex);

    (cur_node) ? (*node = cur_node->data) : (*node = NULL);

    return 0;
}

int32_t
list_next(list_t *list, void *from, void **node) {
    list_node_t *from_node = (list_node_t *)from;

    if (!list && !from_node) {
        *node = NULL;
        return -1;
    }

    pthread_rwlock_rdlock(&list->mutex);
    if (list != NULL) {
        if (!list->head || !list->head->data)
            *node = NULL;
        else
            *node = list->head->data;
    }
    else {
        if (!from_node->next || !from_node->next->data)
            *node = NULL;
        else
            *node = from_node->next->data;
    }
    pthread_rwlock_unlock(&list->mutex);

    return 0;
}

void
list_sort(list_t *list, list_cmp_t cmp) {
    list_node_t *cur, *next;
    int32_t swapped = 0;

    pthread_rwlock_wrlock(&list->mutex);
    // TODO:  Make this a quicksort when I have time.  Bubble sort will have to do for now
    for (int32_t i = 0; i < list->list_size; i++) {
        swapped = 0;
        cur = list->head;
        next = cur->next;
        while (cur && next) {
            if (cmp(cur->data, next->data) < 0) {
                if (next->next)
                    next->next->prev = cur;
                else
                    list->tail = cur;
                if (cur->prev)
                    cur->prev->next = next;
                else
                    list->head = next;
                next->prev = cur->prev;
                cur->next = next->next;
                next->next = cur;
                cur->prev = next;
                swapped = 1;
            }
            cur = next;
            next = cur->next;
        }
        if (!swapped)
            break;
    }
    pthread_rwlock_unlock(&list->mutex);

    return;
}

void
list_remove_if(list_t *list, list_remove_t cmp, void *cmp_data, void *free_data) {
    list_node_t *cur = list->head, *next;

    if (!list->head)
        return;

    pthread_rwlock_wrlock(&list->mutex);
    while (cur) {
        next = cur->next;
        if (cmp(cur->data, cmp_data) == LIST_REMOVE) {
            if (cur == list->head) {
                list->head = cur->next;
                if (list->head)
                    list->head->prev = NULL;
            } 
            else {
                cur->prev->next = cur->next;
            }

            if (cur == list->tail) {
                list->tail = cur->prev;
                if (list->tail)
                    list->tail->next = NULL;
            }
            else {
                cur->next->prev = cur->prev;
            }
            cur->next = cur->prev = NULL;

            list->free_fn(cur->data, free_data);
            free(cur);
            list->list_size--;
        }
        cur = next;
        //cur = cur->next;
    }
    pthread_rwlock_unlock(&list->mutex);
}

void list_read_lock(list_t *list) {
    pthread_rwlock_rdlock(&list->mutex);
}

void list_write_lock(list_t *list) {
    pthread_rwlock_wrlock(&list->mutex);
}

void list_unlock(list_t *list) {
    pthread_rwlock_unlock(&list->mutex);
}

