#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

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
list_count_cb(void *node, void *data) {
    test_struct_t *t = (test_struct_t *)node;
    int32_t *count = (int32_t *)data;

    if ((t->hdr_magic != HDR_MAGIC) || t->ftr_magic != FTR_MAGIC) {
        fprintf(stdout, "Data Integrity FAILED: Header magic corrupted in list!\n");
        return LIST_STOP;
    }
    if ((t->hdr_magic != HDR_MAGIC) || t->ftr_magic != FTR_MAGIC) {
        fprintf(stdout, "Data Integrity FAILED: Footer magic corrupted in list!\n");
        return LIST_STOP;
    }

    (*count)++; 

    return LIST_CONTINUE;
}

int8_t
list_check_sort_cb(void *node, void *data) {
    test_struct_t *t = (test_struct_t *)node;
    int32_t *count = (int32_t *)data;
    static int32_t last = -1;

    if ((t->hdr_magic != HDR_MAGIC) || t->ftr_magic != FTR_MAGIC) {
        fprintf(stdout, "Data Integrity FAILED: Header magic corrupted in list!\n");
        return LIST_STOP;
    }
    if ((t->hdr_magic != HDR_MAGIC) || t->ftr_magic != FTR_MAGIC) {
        fprintf(stdout, "Data Integrity FAILED: Footer magic corrupted in list!\n");
        return LIST_STOP;
    }

    if (last > -1 && last > t->a) {
        (*count)++;
        return LIST_CONTINUE;
    }

    return LIST_CONTINUE;
}

void
delete_node_cb(void *data, void *unused) {
    test_struct_t *t = (test_struct_t *)data;

    free(t);
}

int8_t
sort_cb(void *n1, void *n2) {
    test_struct_t *t1 = (test_struct_t *)n1;
    test_struct_t *t2 = (test_struct_t *)n2;

    return ((t1->a == t2->a) ? 0 : (t1->a > t2->a) ? -1 : 1);
}

int32_t
populate_list(list_t *the_list, int32_t n_elm, int8_t randomize) {
    test_struct_t *t = NULL;
    
    // Populate the list with pointers to test structures
    for (int32_t i = 0; i < n_elm; i++) {
        if ((t = calloc(1, sizeof(test_struct_t))) == NULL) {
            fprintf(stdout, "Error:  Unable to allocate memory for test structures.\n");
            return -1;
        }
        t->hdr_magic = HDR_MAGIC;
        t->ftr_magic = FTR_MAGIC;
        if (!randomize)
            t->a = i;
        else
            t->a = i + random();
        t->b = 256;
        t->c = 512;
        t->d = 1024;
        list_append(the_list, t);
    }
}

int32_t
test_large_list(list_t *the_list) {
    int32_t count = 0;

    populate_list(the_list, 32768, 0);

    // Verify the size
    if (list_size(the_list) != 32768) {
        fprintf(stdout, "List Size: FAILED. Expected 32768, got %d\n", list_size(the_list));
        return -1;
    }

    list_for_each(the_list, list_count_cb, &count);

    if (count != 32768) {
        fprintf(stdout, "List Count: FAILED. Expected 32768, got %d\n", count);
        return -1;
    }

    fprintf(stdout, "List Size:\tPASSED\n");
    fprintf(stdout, "List Count:\tPASSED\n");

    return 0;
}

int32_t
test_list_sort(list_t *the_list) {
    int32_t count = 0;

    populate_list(the_list, 1024, 1);
    list_sort(the_list, sort_cb);
    list_for_each(the_list, list_count_cb, &count);

    if (count != 1024) {
        fprintf(stdout, "List Count (Sort): FAILED. Expected 1024, got %d\n", count);
        return -1;
    }

    count = 0;
    list_for_each(the_list, list_check_sort_cb, &count);

    if (count != 0) {
        fprintf(stdout, "List Sort: FAILED. %d nodes are out of order\n", count);
        return -1;
    }
    
    fprintf(stdout, "List Sort:\tPASSED\n");

    return 0;
}

int8_t
remove_if(void *node, void *data) {
    test_struct_t *t = (test_struct_t *)node;

    if (t->a >= 10)
        return LIST_REMOVE;

    return LIST_KEEP;
}

int32_t
test_list_remove_if(list_t *the_list) {
    test_struct_t *t = NULL;

    populate_list(the_list, 1024, 0);

    list_remove_if(the_list, remove_if, NULL, NULL);

    if (list_size(the_list) != 10) {
        fprintf(stdout, "List Remove If:\tFAILED. Expected size 10, got %d\n", 
                list_size(the_list));
        return -1;
    }


    fprintf(stdout, "List Remove If:\tPASSED\n");
}

int32_t
test_list_head_and_tail(list_t *the_list) {
    populate_list(the_list, 32768, 0);
    test_struct_t *t = NULL;

    while (list_size(the_list)) {
        list_head(the_list, (void **)&t, 1);
        if (t->hdr_magic != HDR_MAGIC) {
            fprintf(stdout, "List Head FAILED: Header magic corrupted in list!\n");
            return -1;
        }
        if (t->ftr_magic != FTR_MAGIC) {
            fprintf(stdout, "List Head FAILED: Footer magic corrupted in list!\n");
            return -1;
        }

        free(t);

        list_tail(the_list, (void **)&t, 1);
        if (t->hdr_magic != HDR_MAGIC) {
            fprintf(stdout, "List Tail FAILED: Header magic corrupted in list!\n");
            return -1;
        }
        if (t->ftr_magic != FTR_MAGIC) {
            fprintf(stdout, "List Tail FAILED: Footer magic corrupted in list!\n");
            return -1;
        }
        free(t);
    }

    fprintf(stdout, "List Head:\tPASSED\n");
    fprintf(stdout, "List Tail:\tPASSED\n");
    return 0;
}

int32_t test_list() {
    list_t *the_list = NULL;
    
    srand(time(NULL));

    fprintf(stdout, "********** LIST TESTS **********\n");
    // Create a new list
    list_create(&the_list, delete_node_cb);
    test_large_list(the_list);
    list_destroy(the_list, NULL);

    list_create(&the_list, delete_node_cb);
    test_list_sort(the_list);
    list_destroy(the_list, NULL);

    list_create(&the_list, delete_node_cb);
    test_list_head_and_tail(the_list);
    list_destroy(the_list, NULL);

    list_create(&the_list, delete_node_cb);
    test_list_remove_if(the_list);
    list_destroy(the_list, NULL);
    fprintf(stdout, "List Destroy:\tPASSED\n");

    fprintf(stdout, "\n********** BST TESTS **********\n");
    int32_t tree_hdl;
    test_struct_t *t = NULL;

    bst_init();
    tree_hdl = bst_create("The Tree", delete_node_cb, BST_KINT32);
    for (uint32_t i = 0; i < 6; i++) {
        if ((t = calloc(1, sizeof(test_struct_t))) == NULL) {
            fprintf(stdout, "Error:  Unable to allocate memory for test structures.\n");
            return -1;
        }
        t->hdr_magic = HDR_MAGIC;
        t->ftr_magic = FTR_MAGIC;
        if (i < 5)
            t->a = (i + 1) * 10;
        else
            t->a = 25;
        t->b = 256;
        t->c = 512;
        t->d = 1024;

        bst_insert(tree_hdl, 0, &t->a, t);
    }

    bst_print_tree(tree_hdl, 0);
    return 0;
}

int32_t
main(int32_t argc, char **argv) {

    if (test_list() < 0)
        return -1;

    return 0;
}
