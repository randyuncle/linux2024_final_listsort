#ifndef SORT_H
#define SORT_H

#include <linux/types.h>

struct list_head;

#define MAX_LEN ((1 << 20) + 20)
#define MIN_LEN 4
#define LOOP 10000

/* In the compare function, `void *` is a argument that could be used to
 * record the number of comparisons
 */
typedef int __attribute__((nonnull(2,3))) (*list_cmp_func_t)(void *,
		const struct list_head *, const struct list_head *);

typedef void (*test_func_t)(void *priv,
                            struct list_head *head,
                            list_cmp_func_t cmp);

/* Structure for the test cases */
typedef struct {
    char *name;
    test_func_t impl;
} test_t;

/* The structure for the device driver from `copy_from_user` */
typedef struct {
    size_t nodes;
    size_t case_id;
} st_dev;

/* The structure for the user space data from `copy_to_user` */
typedef struct {
    unsigned long long int time[LOOP];
    size_t count[LOOP];
} st_usr;


/* The function declarations of sorting algorithms 
 * 
 * TODO:
 *  - Adding back the merge only adaptive shiverssort
 *  - Adding the alpha-stack merge sort to do the comparisons
 * */
void list_sort(void *priv, struct list_head *head, list_cmp_func_t cmp);
void timsort_merge(void *priv, struct list_head *head, list_cmp_func_t cmp);
void timsort_binary(void *priv, struct list_head *head, list_cmp_func_t cmp);
void timsort_linear(void *priv, struct list_head *head, list_cmp_func_t cmp);
void timsort_l_gallop(void *priv, struct list_head *head, list_cmp_func_t cmp);
void timsort_b_gallop(void *priv, struct list_head *head, list_cmp_func_t cmp);
void shiverssort(void *priv, struct list_head *head, list_cmp_func_t cmp);
void shiverssort_merge(void *priv, struct list_head *head, list_cmp_func_t cmp);

#endif
