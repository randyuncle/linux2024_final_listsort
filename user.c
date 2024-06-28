/** 
 * The code that test the sorting algorithms in user space
*/
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "harness.h"
#include "list.h"
#include "listsort.h"
#include "queue.h"
#include "random.h"
#include "sort_test.h"
#include "sort_test_impl.h"
#include "timsort.h"

#define MIN_RANDSTR_LEN 5
#define MAX_STR_LEN 10
#define MAX_LOOP 10
#define N_MEASURES 15 /* loop for testing each number of nodes */
#define MAX_CASES 6

/* The range of the elements is cited from the `listsort.txt` in cpython */
#define MIN_NODE 0
#define MAX_NODE 1048576  // 2^20

#define COMP_OUT "comparison_output.txt"
#define K_OUT "kvalue_output.txt"
#define DUR_OUT "time_output.txt"

static const char charset[] = "abcdefghijklmnopqrstuvwxyz";
char *prefix = ""; /* prefix of the file name by the form of test case */

/* TODO: Add a buf_size check of if the buf_size may be less
 * than MIN_RANDSTR_LEN.
 */
void fill_rand_string(char *buf, size_t buf_size)
{
    size_t len = 0;
    while (len < MIN_RANDSTR_LEN)
        len = rand() % buf_size;

    uint64_t randstr_buf_64[MAX_STR_LEN] = {0};
    randombytes((uint8_t *) randstr_buf_64, len * sizeof(uint64_t));
    for (size_t n = 0; n < len; n++)
        buf[n] = charset[randstr_buf_64[n] % (sizeof(charset) - 1)];

    buf[len] = '\0';
}

/* set the bias for generating continious ascending string */
size_t set_bias(int count, size_t max_buf)
{
    int num = count - 1;
    size_t size = 0;
    do {
        if ((num - 1) % 26 >= 0 && size != 0)
            num++;
        size++;
        num = (num - 1) / 26;
    } while (num != 0);
    return max_buf - size;
}

/**
 * A function to generate the continious strings.
 * Start from `aaaaaaaaaa` to maximum of the given string length [num].
 */
void fill_cont_full_string(char *buf, size_t buf_size, int counter, size_t bias)
{
    for (size_t i = 0; i < buf_size; i++) {
        buf[i] = charset[0];
    }

    int num = counter + 1;
    size_t index = buf_size - 1 - bias;
    do {
        if ((num - 1) % 26 >= 0 && index != (buf_size - 1 - bias))
            num++;
        buf[index--] = charset[(num - 1) % 26];
        num = (num - 1) / 26;
    } while (num != 0);
}

void create_sample(struct list_head *head,
                   element_t *space,
                   int samples,
                   int case_id)
{
    char randstr_buf[MAX_STR_LEN], worststr_buf[MAX_STR_LEN];
    char *inserts = NULL;

    int cnt = 0, exch[100000]; /* variables for random slots */
    size_t bias = set_bias(samples, MAX_STR_LEN);
    bool dup = false;
    /* defining the place to fill random values, and decide which buffer to use
     * in each cases */
    switch (case_id) {
    case 0: /* Worst case of merge sort */
        inserts = worststr_buf;
        prefix = "w_";
        break;
    case 1: /* Random 3 elements */
        for (int i = 0; i < 3; i++) {
            exch[i] = rand() % samples;
            for (int j = 0; j < i; j++) {
                if (exch[i] == exch[j])
                    i--;
            }
        }

        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < i; j++) {
                if (exch[i] < exch[j]) {
                    int temp = exch[i];
                    exch[i] = exch[j];
                    exch[j] = temp;
                }
            }
        }

        inserts = worststr_buf;
        prefix = "r3_";
        break;
    case 2: /* Random last 10 elements */
        inserts = worststr_buf;
        prefix = "rl10_";
        break;
    case 3: /* Random 1% elements */
        int num = samples * 0.01;

        for (int i = 0; i < num; i++) {
            exch[i] = rand() % samples;
            for (int j = 0; j < i; j++) {
                if (exch[i] == exch[j]) {
                    i--;
                    break;
                }
            }
        }

        for (int i = 0; i < num; i++) {
            for (int j = 0; j < i; j++) {
                if (exch[i] < exch[j]) {
                    int temp = exch[i];
                    exch[i] = exch[j];
                    exch[j] = temp;
                }
            }
        }

        inserts = worststr_buf;
        prefix = "r1p_";
        break;
    case 4: /* Duplicate */
        inserts = randstr_buf;
        prefix = "dup_";
        break;
    default: /* Random elements */
        inserts = randstr_buf;
        prefix = "r_";
        break;
    }

    /* Start to create the samples for the testing list */
    for (int i = 0; i < samples; i++) {
        element_t *elem = space + i;
        switch (case_id) {
        case 0: /* Worst case of merge sort */
            fill_cont_full_string(worststr_buf, sizeof(worststr_buf), i, bias);
            break;
        case 1: /* Random 3 elements */
            if (i == exch[cnt]) {
                fill_rand_string(worststr_buf, sizeof(worststr_buf));
                cnt++;
            } else
                fill_cont_full_string(worststr_buf, sizeof(worststr_buf), i,
                                      bias);
            break;
        case 2: /* Random last 10 elements */
            if (i < samples - 10)
                fill_cont_full_string(worststr_buf, sizeof(worststr_buf), i,
                                      bias);
            else
                fill_rand_string(worststr_buf, sizeof(worststr_buf));
            break;
        case 3: /* Random 1% elements */
            if (i == exch[cnt]) {
                fill_rand_string(worststr_buf, sizeof(worststr_buf));
                cnt++;
            } else
                fill_cont_full_string(worststr_buf, sizeof(worststr_buf), i,
                                      bias);
            break;
        case 4: /* Duplicate */
            if (i == 0 || (!dup && !(rand() % 2))) {
                cnt = !cnt ? (rand() % 100 + 1) : cnt;
                fill_rand_string(randstr_buf, sizeof(randstr_buf));
            } else
                dup = (--cnt) ? true : false;
            break;
        default: /* Random elements */
            fill_rand_string(randstr_buf, sizeof(randstr_buf));
            break;
        }

        int s_len = strlen(inserts) + 1;
        elem->value = (char *) malloc(s_len * sizeof(char));
        memcpy(elem->value, inserts, s_len);
        elem->seq = i;
        list_add_tail(&elem->list, head);
    }

    /* handle special cases for furthur modification */
    if (!case_id)
        worst_case_generator(head);
    else if (case_id == 4 && samples < 25000)
        shuffle(head);
}

void copy_list(struct list_head *from, struct list_head *to, element_t *space)
{
    if (list_empty(from))
        return;

    element_t *entry;
    list_for_each_entry (entry, from, list) {
        element_t *copy = space++;
        int s_len = strlen(entry->value) + 1;
        copy->value = (char *) malloc(s_len * sizeof(char));
        memcpy(copy->value, entry->value, s_len);
        copy->seq = entry->seq;
        list_add_tail(&copy->list, to);
    }
}

int compare(void *priv, const struct list_head *a, const struct list_head *b)
{
    element_t *element_a = list_entry(a, element_t, list);
    element_t *element_b = list_entry(b, element_t, list);

    int res = strcmp(element_a->value, element_b->value);

    if (!res)
        return 0;

    if (priv)
        *((int *) priv) += 1;

    return res;
}

bool check_list(struct list_head *head, int count)
{
    if (list_empty(head))
        return 0 == count;

    element_t *entry, *safe;
    size_t ctr = 0;
    list_for_each_entry_safe (entry, safe, head, list) {
        ctr++;
    }
    int unstable = 0;
    list_for_each_entry_safe (entry, safe, head, list) {
        if (entry->list.next != head) {
            if (strcmp(entry->value, safe->value) > 0) {
                fprintf(stderr, "\nERROR: Wrong order\n");
                return false;
            }
            if (!strcmp(entry->value, safe->value) && entry->seq > safe->seq)
                unstable++;
        }
    }
    if (unstable) {
        fprintf(stderr, "\nERROR: unstable %d\n", unstable);
        return false;
    }

    if (ctr < MIN_NODE && ctr > MAX_NODE) {
        fprintf(stderr, "\nERROR: Inconsistent number of elements: %ld\n", ctr);
        return false;
    }
    return true;
}

typedef void (*test_func_t)(void *priv,
                            struct list_head *head,
                            list_cmp_func_t cmp);

typedef struct {
    char *name;
    test_func_t impl;
} test_t;

/* To get the k-value from the current number of comparisons and nodes */
double k_value(int n, int comp)
{
    return log2(n) - (double) (comp - 1) / n;
}

/* output the results to the specific files */
void file_output(int samples,
                 char *name_prefix,
                 int64_t duration,
                 int count,
                 double k)
{
    int n_len = strlen(name_prefix) + 1;
    char *file_name = (char *) malloc((n_len + 100) * sizeof(char));
    if (!file_name) {
        free(file_name);
        perror("Out of memmory while copying file name");
        exit(EXIT_FAILURE);
    }

    memcpy(file_name, name_prefix, n_len);
    strcat(file_name, DUR_OUT);
    FILE *dur_output = fopen(file_name, "a");
    if (!dur_output) {
        perror("The output file `time_output.txt` might have been collapsed");
        exit(EXIT_FAILURE);
    }

    memset(file_name, '\0', strlen(file_name));
    memcpy(file_name, name_prefix, n_len);
    strcat(file_name, COMP_OUT);
    FILE *comp_output = fopen(file_name, "a");
    if (!dur_output) {
        perror(
            "The output file `comparison_output.txt` might have been "
            "collapsed");
        exit(EXIT_FAILURE);
    }

    memset(file_name, '\0', strlen(file_name));
    memcpy(file_name, name_prefix, n_len);
    strcat(file_name, K_OUT);
    FILE *k_output = fopen(file_name, "a");
    if (!k_output) {
        perror(
            "The output file `comparison_output.txt` might have been "
            "collapsed");
        exit(EXIT_FAILURE);
    }

    fprintf(dur_output, "%10d %10ld\n", samples, duration);
    fprintf(comp_output, "%10d %10d\n", samples, count);
    fprintf(k_output, "%10d %10f\n", samples, k);

    fclose(dur_output);
    fclose(comp_output);
    fclose(k_output);

    free(file_name);
}

/** Sorting test
 *
 * Apply a test program in user space for the given sorting algorithms.
 *
 * In this function, it expects to do the following tests for the sorting
 * algorithms:
 *  - Sorting stability
 *  - Worst case scenario in merge sort
 *  - Tim sort cases
 *
 * With the following expected outputs:
 *  - Comparisons
 *  - Durations
 */
bool sort_test(int case_id, int nodes)
{
    struct list_head sample_head, testdata_head, warmdata_head;
    int count;

    /* Assume ASLR */
    // srand((uintptr_t) &sort_test);

    test_t tests[] = {
        // {.name = "timsort", .impl = timsort},
        // {.name = "listsort", .impl = list_sort},
        // {.name = "timsort_old", .impl = timsort_old},
        // {.name = "timsort_gallop", .impl = timsort_gallop},
        // {.name = "timsort_b_gallop", .impl = timsort_b_gallop},
        // {.name = "timsort_binary", .impl = timsort_binary},
        {.name = "adaptive_shiverssort", .impl = shiverssort},
        // {.name = "qsort", .impl = sort},
        {NULL, NULL},
    };
    test_t *test = tests;

    while (test->impl) {
        element_t *samples = malloc(sizeof(*samples) * nodes);
        element_t *testdata = malloc(sizeof(*testdata) * nodes);
        element_t *warmdata = malloc(sizeof(*warmdata) * nodes);

        /* The execution time measurement refers to that from dudect, which
           is different from the measurement in intepreter `qtest` */
        int64_t *before_ticks = calloc(N_MEASURES + 1, sizeof(int64_t));
        int64_t *after_ticks = calloc(N_MEASURES + 1, sizeof(int64_t));
        int64_t *exec_times = calloc(N_MEASURES, sizeof(int64_t));
        for (int i = 0; i < N_MEASURES; i++) {
            // printf("==== Testing %s in %d nodes #%d ====\n", test->name,
            // nodes, i);
            /* initialize the linked-list */
            INIT_LIST_HEAD(&sample_head);
            create_sample(&sample_head, samples, nodes, case_id);

            INIT_LIST_HEAD(&testdata_head);
            INIT_LIST_HEAD(&warmdata_head);
            copy_list(&sample_head, &testdata_head, testdata);
            copy_list(&sample_head, &warmdata_head, warmdata);
            /* Warming */
            // printf("Warming\n");
            test->impl(&count, &warmdata_head, compare);

            /* Test */
            // printf("Testing\n");
            count = 0;
            before_ticks[i] = cpucycles();
            test->impl(&count, &testdata_head, compare);
            after_ticks[i] = cpucycles();

            exec_times[i] = after_ticks[i] - before_ticks[i];
            double k = k_value(nodes, count);

            // printf("  Durations:    %ld ticks\n", exec_times[i]);
            // printf("  Comparisons:    %d\n", count);
            // printf("  K-value:    %f\n", k);
            // printf("  List is %s\n",
            //     check_list(&testdata_head, nodes) ? "sorted" : "not sorted");
            char LS_PREFIX[100] = "k_";
            char TIM_PREFIX[100] = "t_";
            char TIMO_PREFIX[100] = "to_";
            char TIMB_PREFIX[100] = "tb_";
            char TIMG_PREFIX[100] = "tg_";
            char TIMBG_PREFIX[100] = "tbg_";
            char TIMAS_PREFIX[100] = "as_";
            char Q_PREFIX[100] = "q_";
            if (!strcmp(test->name, "timsort"))
                file_output(nodes, strcat(TIM_PREFIX, prefix), exec_times[i],
                            count, k);
            else if (!strcmp(test->name, "listsort"))
                file_output(nodes, strcat(LS_PREFIX, prefix), exec_times[i],
                            count, k);
            else if (!strcmp(test->name, "timsort_old"))
                file_output(nodes, strcat(TIMO_PREFIX, prefix), exec_times[i],
                            count, k);
            else if (!strcmp(test->name, "timsort_binary"))
                file_output(nodes, strcat(TIMB_PREFIX, prefix), exec_times[i],
                            count, k);
            else if (!strcmp(test->name, "timsort_gallop"))
                file_output(nodes, strcat(TIMG_PREFIX, prefix), exec_times[i],
                            count, k);
            else if (!strcmp(test->name, "timsort_b_gallop"))
                file_output(nodes, strcat(TIMBG_PREFIX, prefix), exec_times[i],
                            count, k);
            else if (!strcmp(test->name, "adaptive_shiverssort"))
                file_output(nodes, strcat(TIMAS_PREFIX, prefix), exec_times[i],
                            count, k);
            else if (!strcmp(test->name, "qsort"))
                file_output(nodes, strcat(Q_PREFIX, prefix), exec_times[i],
                            count, k);

            /* Clean the value and list in the current `element_t` structure */
            element_t *iterator, *next;

            list_for_each_entry_safe (iterator, next, &sample_head, list) {
                list_del(&iterator->list);
                test_free(iterator->value);
            }

            list_for_each_entry_safe (iterator, next, &warmdata_head, list) {
                list_del(&iterator->list);
                test_free(iterator->value);
            }

            list_for_each_entry_safe (iterator, next, &testdata_head, list) {
                list_del(&iterator->list);
                test_free(iterator->value);
            }

            count = 0;
        }

        free(samples);
        free(testdata);
        free(warmdata);

        free(before_ticks);
        free(after_ticks);
        free(exec_times);

        test++;
    }

    return true;
}