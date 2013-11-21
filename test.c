#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "al_data_struct.h"

#define HDR_MAGIC 0xAABBCCDD
#define FTR_MAGIC 0xDDCCBBAA

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
        return 0;
    }
    if ((t->hdr_magic != HDR_MAGIC) || t->ftr_magic != FTR_MAGIC) {
        fprintf(stdout, "TEST 1: Footer magic corrupted in list!\n");
        return 0;
    }

    fprintf(stdout, "Element: %d is valid\n", t->a);
    return 1;
}

int32_t test_list() {
    list_t *the_list = NULL;
    test_struct_t *t = NULL;

    list_create(&the_list, NULL);

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

    fprintf(stdout, "List Size is: %d\n", list_size(the_list));

    list_for_each(the_list, list_cb, NULL);

    while (list_size(the_list)) {
        list_head(the_list, (void **)&t, 1);
        if ((t->hdr_magic != HDR_MAGIC) || t->ftr_magic != FTR_MAGIC) {
            fprintf(stdout, "TEST 2: Header magic corrupted in list!\n");
        }
        list_tail(the_list, (void **)&t, 1);
        if ((t->hdr_magic != HDR_MAGIC) || t->ftr_magic != FTR_MAGIC) {
            fprintf(stdout, "TEST 2: Footer magic corrupted in list!\n");
        }
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
