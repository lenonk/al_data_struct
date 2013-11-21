#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "al_data_struct.h"

#define HDR_MAGIC 0xAABBCCDD
#define FTR_MAGIC 0xDDCCBBAA

#define LIST_SIZE 16

typedef struct {
    int32_t hdr_magic;
    int32_t a;
    int32_t b;
    int32_t c;
    int32_t d;
    int32_t ftr_magic;
} test_struct_t;

int8_t
list_cb(void *node, void *data) {
    test_struct_t *t = (test_struct_t *)node;

    if ((t->hdr_magic != HDR_MAGIC) || t->ftr_magic != FTR_MAGIC) {
        fprintf(stdout, "TEST 1: Header magic corrupted in list!\n");
        return -1;
    }
    if ((t->hdr_magic != HDR_MAGIC) || t->ftr_magic != FTR_MAGIC) {
        fprintf(stdout, "TEST 1: Footer magic corrupted in list!\n");
        return -1;
    }

    fprintf(stdout, "List node: %d\n", t->a);
    return 0;
}

void
delete_node_cb(void *data) {
    test_struct_t *t = (test_struct_t *)data;

    //fprintf(stdout, "Destroying data at node: %d\n", t->a);
    free(t);
}

int8_t
sort_cb(void *n1, void *n2) {
    test_struct_t *t1 = (test_struct_t *)n1;
    test_struct_t *t2 = (test_struct_t *)n2;

    return ((t1->a == t2->a) ? 0 : (t1->a > t2->a) ? -1 : 1);
}

int32_t test_list() {
    list_t *the_list = NULL;
    test_struct_t *t = NULL;
    
    srand(time(NULL));

    // Create a new list
    list_create(&the_list, delete_node_cb);

    // Populate the list with pointers to test structures
    for (int32_t i = 0; i < LIST_SIZE; i++) {
        if ((t = calloc(1, sizeof(test_struct_t))) == NULL) {
            fprintf(stdout, "Error:  Unable to allocate memory for test structures.\n");
            return -1;
        }
        t->hdr_magic = HDR_MAGIC;
        t->ftr_magic = FTR_MAGIC;
        t->a = i + random();
        t->b = 256;
        t->c = 512;
        t->d = 1024;
        list_append(the_list, t);
    }

    // Verify the size
    if (list_size(the_list) != LIST_SIZE) {
        fprintf(stdout, "List size is: %d.  Expected 32768\n", list_size(the_list));
        return -1;
    }

    fprintf(stdout, "*****************************************************\n");
    list_for_each(the_list, list_cb, NULL);
    list_sort(the_list, sort_cb);
    fprintf(stdout, "*****************************************************\n");
    list_for_each(the_list, list_cb, NULL);
    fprintf(stdout, "*****************************************************\n");

    while (list_size(the_list)) {
        list_head(the_list, (void **)&t, 1);
        if ((t->hdr_magic != HDR_MAGIC) || t->ftr_magic != FTR_MAGIC) {
            fprintf(stdout, "TEST 2: Header magic corrupted in list!\n");
        }
        free(t);

        list_tail(the_list, (void **)&t, 1);
        if ((t->hdr_magic != HDR_MAGIC) || t->ftr_magic != FTR_MAGIC) {
            fprintf(stdout, "TEST 2: Footer magic corrupted in list!\n");
        }
        free(t);
    }

    // Build the list back up again before destroying it to test the free callback.
    for (int32_t i = 0; i < 32768; i++) {
        if ((t = calloc(1, sizeof(test_struct_t))) == NULL) {
            fprintf(stdout, "Error:  Unable to allocate memory for test structures.\n");
            return -1;
        }
        t->hdr_magic = HDR_MAGIC;
        t->ftr_magic = FTR_MAGIC;
        t->a = i;
        t->b = 256;
        t->c = 512;
        t->d = 1024;
        list_append(the_list, t);
    }

    list_destroy(the_list);

    return 0;
}

int32_t
main(int32_t argc, char **argv) {

    if (test_list() < 0)
        return -1;

    return 0;
}
