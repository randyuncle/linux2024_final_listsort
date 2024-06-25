#include "list.h"

/**
 * A function to join the split list with the guarantee of the `next` pointer
 * in the `struct list_head` structure.
 */
static struct list_head *worst_merge_join(struct list_head *left_head,
                                          struct list_head *right_head)
{
    struct list_head *head = NULL;
    struct list_head **p = &head;
    for (; left_head; p = &((*p)->next), left_head = left_head->next)
        *p = left_head;
    for (; right_head; p = &((*p)->next), right_head = right_head->next)
        *p = right_head;
    return head;
}

/**
 * A function to reorganize a continious string list to the worst case of merge
 * sort with bottom-up recursive call implementation.
 */
static struct list_head *worst_merge_split(struct list_head *head)
{
    if (head != NULL && head->next != NULL) {
        struct list_head *left_head = NULL, *right_head = NULL;
        // Find the left_head and right_head
        struct list_head *curr = head;
        struct list_head **pl = &left_head;
        struct list_head **pr = &right_head;
        // apply the code with pointer of pointer
        for (int count = 1; curr != NULL; curr = curr->next, count++) {
            if (count % 2) {  // odd case
                *pl = curr;
                pl = &((*pl)->next);
            } else {  // even case
                *pr = curr;
                pr = &((*pr)->next);
            }
        }

        *pl = NULL;
        *pr = NULL;

        // Recursive split
        left_head = worst_merge_split(left_head);
        right_head = worst_merge_split(right_head);
        // List joining
        return worst_merge_join(left_head, right_head);
    }

    return head;
}

/* The function that reorganized the linked-list structure to
 * the worst case of merge sort */
void worst_case_generator(struct list_head *head)
{
    struct list_head *end = head->prev;
    // make the list no longer be circular
    end->next = NULL;
    head->next->prev = NULL;
    // reconstruct the sorted list to the worst case scenario
    head->next = worst_merge_split(head->next);
    // make the list be circular again
    struct list_head *curr;
    for (curr = head; curr->next; curr = curr->next)
        curr->next->prev = curr;
    curr->next = head;
    curr->next->prev = curr;
}