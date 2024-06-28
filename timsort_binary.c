#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/compiler.h>
#include <linux/export.h>
#include <linux/string.h>
#include <linux/list.h>

#include "sort.h"

int minrun_b = 0;

static inline size_t run_size(struct list_head *head)
{
    if (!head)
        return 0;
    if (!head->next)
        return 1;
    return (size_t) (head->next->prev);
}

struct pair {
    struct list_head *head, *next;
};

static size_t stk_size;

static struct list_head *merge(void *priv,
                               list_cmp_func_t cmp,
                               struct list_head *a,
                               struct list_head *b)
{
    struct list_head *head = NULL;
    struct list_head **tail = &head;

    for (;;) {
        /* if equal, take 'a' -- important for sort stability */
        if (cmp(priv, a, b) <= 0) {
            *tail = a;
            tail = &a->next;
            a = a->next;
            if (!a) {
                *tail = b;
                break;
            }
        } else {
            *tail = b;
            tail = &b->next;
            b = b->next;
            if (!b) {
                *tail = a;
                break;
            }
        }
    }
    return head;
}

static void build_prev_link(struct list_head *head,
                            struct list_head *tail,
                            struct list_head *list)
{
    tail->next = list;
    do {
        list->prev = tail;
        tail = list;
        list = list->next;
    } while (list);

    /* The final links to make a circular doubly-linked list */
    tail->next = head;
    head->prev = tail;
}

static void merge_final(void *priv,
                        list_cmp_func_t cmp,
                        struct list_head *head,
                        struct list_head *a,
                        struct list_head *b)
{
    struct list_head *tail = head;

    for (;;) {
        /* if equal, take 'a' -- important for sort stability */
        if (cmp(priv, a, b) <= 0) {
            tail->next = a;
            a->prev = tail;
            tail = a;
            a = a->next;
            if (!a)
                break;
        } else {
            tail->next = b;
            b->prev = tail;
            tail = b;
            b = b->next;
            if (!b) {
                b = a;
                break;
            }
        }
    }

    /* Finish linking remainder of list b on to tail */
    build_prev_link(head, tail, b);
}

static struct pair find_run(void *priv,
                            struct list_head *list,
                            list_cmp_func_t cmp)
{
    // printf("start find run\n");
    size_t len = 1;
    struct list_head *next = list->next, *head = list;
    struct pair result;

    if (!next) {
        result.head = head, result.next = next;
        return result;
    }

    if (cmp(priv, list, next) > 0) {
        /* decending run, also reverse the list */
        struct list_head *prev = NULL;
        do {
            len++;
            list->next = prev;
            prev = list;
            list = next;
            next = list->next;
            head = list;
        } while (next && cmp(priv, list, next) > 0);
        list->next = prev;
    } else {
        do {
            len++;
            list = next;
            next = list->next;
        } while (next && cmp(priv, list, next) <= 0);
        list->next = NULL;
    }

    /* Trigger this piece of code to fill the node in the run until its size
     * equals to `minrun_b` */
    if (len < minrun_b) {
        /* rebuild the prev links for each node to ensure we won't meet issues
         * with infinite loops or segmentation fault during binary insertion
         * sort.*/
        for (struct list_head *curr = head; curr && curr->next;
             curr = curr->next)
            curr->next->prev = curr;

        /* the binary insertion sort */
        for (struct list_head *in_node = next; in_node && len < minrun_b;
             len++) {
            struct list_head *safe = in_node->next;

            /* holding special case for being smaller than the head node */
            if (cmp(priv, in_node, head) <= 0) {
                in_node->prev = head->prev;
                in_node->next = head;
                head->prev = in_node;
                head = in_node;

                in_node = safe;
                next = in_node;
                continue;
            }

            int x = 1, y = len;
            int middle = (x & y) + ((x ^ y) >> 1);

            struct list_head *curr = head;
            int direction = -1; /* 0 -> left ; 1 -> right */
            while (1) {
                /* moving the pointer to the middle node of the current section
                 */
                if (direction) {
                    for (int n = x; n != middle; n++)
                        curr = curr->next;

                } else {
                    for (int n = y; n != middle; n--)
                        curr = curr->prev;
                }

                /* check if it meets the break condition (this step won't be
                 * used in the first step) */
                if (direction >= 0 && (x == y || x == middle)) {
                    in_node->prev = curr;
                    in_node->next = curr->next;
                    if (in_node->next)
                        in_node->next->prev = in_node;
                    curr->next = in_node;
                    break;
                }

                /* decide the direction of the next move */
                if (cmp(priv, in_node, curr) > 0) {
                    x = middle;
                    direction = 1;
                } else {
                    y = middle;
                    direction = 0;
                }
                /* update the information of the middle node (takes the ceiling
                 * of the result) */
                middle = (x & y) + ((x ^ y) >> 1);

                if (x == middle && y - x == 1) {
                    if (!direction) {
                        /* hold the insertion slot is before the first node of
                         * the section */
                        if (cmp(priv, in_node, curr->prev) <= 0) {
                            curr = curr->prev;
                            in_node->prev = curr->prev;
                            in_node->next = curr;
                            break;
                        }
                    } else {
                        /* hold the insertion slot is after the last node of the
                         * section  */
                        if (cmp(priv, in_node, curr->next) > 0) {
                            curr = curr->next;
                            in_node->prev = curr;
                            in_node->next = curr->next;
                            if (in_node->next)
                                in_node->next->prev = in_node;
                            curr->next = in_node;
                            break;
                        }
                    }
                }
            }

            in_node = safe;
            next = in_node;
        }
    }

    head->prev = NULL;
    head->next->prev = (struct list_head *) len;
    result.head = head, result.next = next;
    return result;
}

static struct list_head *merge_at(void *priv,
                                  list_cmp_func_t cmp,
                                  struct list_head *at)
{
    size_t len = run_size(at) + run_size(at->prev);
    struct list_head *prev = at->prev->prev;
    struct list_head *list = merge(priv, cmp, at->prev, at);
    list->prev = prev;
    list->next->prev = (struct list_head *) len;
    --stk_size;
    return list;
}

static struct list_head *merge_force_collapse(void *priv,
                                              list_cmp_func_t cmp,
                                              struct list_head *tp)
{
    while (stk_size >= 3) {
        if (run_size(tp->prev->prev) < run_size(tp)) {
            tp->prev = merge_at(priv, cmp, tp->prev);
        } else {
            tp = merge_at(priv, cmp, tp);
        }
    }
    return tp;
}

static struct list_head *merge_collapse(void *priv,
                                        list_cmp_func_t cmp,
                                        struct list_head *tp)
{
    int n;
    while ((n = stk_size) >= 2) {
        if ((n >= 3 &&
             run_size(tp->prev->prev) <= run_size(tp->prev) + run_size(tp)) ||
            (n >= 4 && run_size(tp->prev->prev->prev) <=
                           run_size(tp->prev->prev) + run_size(tp->prev))) {
            if (run_size(tp->prev->prev) < run_size(tp)) {
                tp->prev = merge_at(priv, cmp, tp->prev);
            } else {
                tp = merge_at(priv, cmp, tp);
            }
        } else if (run_size(tp->prev) <= run_size(tp)) {
            tp = merge_at(priv, cmp, tp);
        } else {
            break;
        }
    }

    return tp;
}

static int find_minrun(int size)
{
    int one = 0;
    if (size) {
        // To get the first five bits (MAX_minrun_b = 32)
        while (size > 0x001F) {
            one = (size & 0x01) ? 1 : one;  // holding carry
            size >>= 1;
        }
    }

    return size + one;
}

void timsort_binary(void *priv, struct list_head *head, list_cmp_func_t cmp)
{
    stk_size = 0;
    minrun_b = find_minrun(list_count_nodes(head));

    struct list_head *list = head->next, *tp = NULL;
    if (head == head->prev)
        return;

    /* Convert to a null-terminated singly-linked list. */
    head->prev->next = NULL;

    do {
        /* Find next run */
        struct pair result = find_run(priv, list, cmp);
        result.head->prev = tp;
        tp = result.head;
        list = result.next;
        stk_size++;
        tp = merge_collapse(priv, cmp, tp);
    } while (list);

    /* End of input; merge together all the runs. */
    tp = merge_force_collapse(priv, cmp, tp);

    /* The final merge; rebuild prev links */
    struct list_head *stk0 = tp, *stk1 = stk0->prev;
    while (stk1 && stk1->prev)
        stk0 = stk0->prev, stk1 = stk1->prev;
    if (stk_size <= 1) {
        build_prev_link(head, head, stk0);
        return;
    }
    merge_final(priv, cmp, head, stk1, stk0);
}