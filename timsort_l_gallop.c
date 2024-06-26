#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/compiler.h>
#include <linux/export.h>
#include <linux/string.h>

#include "list.h"
#include "sort.h"

#define MIN_GALLOP 7

int minrun_g = 0;

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
    
    /* parameters for saving the continious visit in each list */
    int gallop_cnt_a = 0, gallop_cnt_b = 0;
    int min_gallop = MIN_GALLOP;

    for (;;) {
        /* if equal, take 'a' -- important for sort stability */
        if (cmp(priv, a, b) <= 0) {
            *tail = a;
            tail = &a->next;
            a = a->next;

            gallop_cnt_b = 0;
            gallop_cnt_a++;

            if (!a) {
                *tail = b;
                break;
            }
        } else {
            *tail = b;
            tail = &b->next;
            b = b->next;

            gallop_cnt_a = 0;
            gallop_cnt_b++;

            if (!b) {
                *tail = a;
                break;
            }
        }

        /* Trigger galloping mode */
        if (gallop_cnt_a >= MIN_GALLOP || gallop_cnt_b >= MIN_GALLOP) {
            struct list_head *p, *insert;
            p = (gallop_cnt_a >= MIN_GALLOP) ? a : b;
            insert = (gallop_cnt_a >= MIN_GALLOP) ? b : a;

            /* the exponential searching*/
            int n_prev = 0, n_curr = 0;
            struct list_head *p_prev = p;
            for (;;) {
                if (cmp(priv, insert, p) <= 0) {
                    break;
                } else {
                    if (!n_curr)
                        *tail = p;

                    n_prev = n_curr;
                    p_prev = p;

                    if (!p_prev->next)
                        break;

                    n_curr = ((n_curr + 1) << 1) - 1;

                    int cnt = n_curr - n_prev;
                    /* update the `p` pointer to the next upper bound */
                    while (--cnt) {
                        if (!p->next) {
                            n_curr -= cnt;
                            break;
                        }
                        p = p->next;
                    }
                }
            }

            /* update the address of `tail` pointer to the `p_prev->next` */
            if (n_curr)
                tail = &p_prev->next;

            /* adress of the value of the current miinimum gallop */
            int gallop = min_gallop;
            /* linear insertion */
            struct list_head *g_curr = n_curr ? p_prev->next : p_prev;
            for (; g_curr && insert && gallop; gallop--) {
                /* loops to trigger the insertion */
                while (insert && g_curr && cmp(priv, insert, g_curr) <= 0) {
                    gallop = min_gallop;/* reset the min_gallop */
                    *tail = insert;
                    tail = &insert->next;
                    insert = insert->next;
                }
                *tail = g_curr;
                tail = &g_curr->next;
                g_curr = g_curr->next;
            }

            /* NULL pointer handler */
            if (!insert) 
                break;
            else if (!g_curr) {
                if (!insert)
                    break;
                else {
                    *tail = insert;
                    break;
                }
            }
            
            /* quit the gallopping mode */
            a = (gallop_cnt_a >= MIN_GALLOP) ? g_curr : insert;
            b = (gallop_cnt_a >= MIN_GALLOP) ? insert : g_curr;

            min_gallop++; /* update the counter of the minimum gallop */
            gallop_cnt_a = 0;
            gallop_cnt_b = 0;
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

    // rebuild the prev links for each node (important step if need to do
    // insertion sort)
    for (struct list_head *curr = head; curr && curr->next; curr = curr->next)
        curr->next->prev = curr;

    // insertion sort for inserting the elements for making every run be
    // approximately equal length.
    for (struct list_head *in_node = next; in_node && len < minrun_g; len++) {
        struct list_head *safe = in_node->next;

        // case for first node hit
        if (!(cmp(priv, in_node, head) > 0)) {
            in_node->next = head;
            head->prev = in_node;
            head = in_node;

            in_node = safe;
            next = in_node;

            continue;
        }

        struct list_head *prev = head, *curr = head->next;

        // Compare and find the space to insert the node by "galloping"-like
        // searching (the two nodes eager finding) .
        while (curr && prev) {
            if (cmp(priv, in_node, curr) > 0) {
                if (curr->next) {
                    if (curr->next->next) {
                        prev = curr->next;
                        curr = curr->next->next;
                    } else {
                        prev = curr;
                        curr = curr->next;
                    }
                } else {
                    prev = curr;
                    curr = NULL;
                    break;
                }
            } else {
                if (!(cmp(priv, in_node, prev) > 0)) {
                    curr = prev;
                    prev = curr->prev;
                }
                break;
            }
        }

        // insert to the list
        in_node->next = curr;
        in_node->prev = prev;
        prev->next = in_node;
        if (curr) {
            curr->prev = in_node;
        }

        in_node = safe;
        next = in_node;
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
        // To get the first five bits (MAX_MINRUN = 32)
        while (size > 0b11111) {
            one = (size & 0x01) ? 1 : one;  // holding carry
            size >>= 1;
        }
    }

    return size + one;
}

void timsort_l_gallop(void *priv, struct list_head *head, list_cmp_func_t cmp)
{
    stk_size = 0;
    minrun_g = find_minrun(list_count_nodes(head));

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
    // printf("going to final merge\n");

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