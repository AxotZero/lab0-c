#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "queue.h"

/* Notice: sometimes, Cppcheck would find the potential NULL pointer bugs,
 * but some of them cannot occur. You can suppress them by adding the
 * following line.
 *   cppcheck-suppress nullPointer
 */

/*
 * Create empty queue.
 * Return NULL if could not allocate space.
 */
struct list_head *q_new()
{
    struct list_head *q = malloc(sizeof(struct list_head));
    if (q)
        INIT_LIST_HEAD(q);
    return q;
}

/* Free all storage used by queue */
void q_free(struct list_head *l)
{
    if (!l)
        return;

    element_t *safe = NULL, *prev = NULL;
    list_for_each_entry_safe (prev, safe, l, list)
        q_release_element(prev);
    free(l);
}

/*
 * New an element_t given the string.
 */
element_t *new_element(char *s)
{
    element_t *q = malloc(sizeof(element_t));
    if (!q)
        return NULL;

    q->value = strdup(s);
    if (!q->value) {
        free(q);
        return NULL;
    }
    return q;
}

/*
 * Attempt to insert element at head of queue.
 * Return true if successful.
 * Return false if q is NULL or could not allocate space.
 * Argument s points to the string to be stored.
 * The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_head(struct list_head *head, char *s)
{
    // check if it is null
    if (!head)
        return false;

    // initialize the new node
    element_t *q = new_element(s);
    if (!q)
        return false;

    // add new node to head;
    list_add(&q->list, head);
    return true;
}

/*
 * Attempt to insert element at tail of queue.
 * Return true if successful.
 * Return false if q is NULL or could not allocate space.
 * Argument s points to the string to be stored.
 * The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_tail(struct list_head *head, char *s)
{
    // check if it is null
    if (!head)
        return false;

    // initialize the new node
    element_t *q = new_element(s);
    if (!q)
        return false;

    // add new node to tail;
    list_add_tail(&q->list, head);
    return true;
}

/*
 * remove target node from the list and copy the removed string to sp
 * (up to a maximum of bufsize-1 characters, plus a null terminator.)
 */
element_t *q_remove(struct list_head *target, char *sp, size_t bufsize)
{
    // remove target
    list_del_init(target);

    // get head element
    element_t *ret = container_of(target, element_t, list);

    // copy remove element value to sp
    if (sp && ret->value) {
        strncpy(sp, ret->value, bufsize - 1);
        sp[bufsize - 1] = '\0';
    }
    return ret;
}

/*
 * Attempt to remove element from head of queue.
 * Return target element.
 * Return NULL if queue is NULL or empty.
 * If sp is non-NULL and an element is removed, copy the removed string to *sp
 * (up to a maximum of bufsize-1 characters, plus a null terminator.)
 *
 * NOTE: "remove" is different from "delete"
 * The space used by the list element and the string should not be freed.
 * The only thing "remove" need to do is unlink it.
 *
 * REF:
 * https://english.stackexchange.com/questions/52508/difference-between-delete-and-remove
 */
element_t *q_remove_head(struct list_head *head, char *sp, size_t bufsize)
{
    // check queue is null of empty;
    if (!head || list_empty(head))
        return NULL;

    // remove head
    element_t *ret = q_remove(head->next, sp, bufsize);
    return ret;
}

/*
 * Attempt to remove element from tail of queue.
 * Other attribute is as same as q_remove_head.
 */
element_t *q_remove_tail(struct list_head *head, char *sp, size_t bufsize)
{
    // check queue is null of empty;
    if (!head || list_empty(head))
        return NULL;

    // remove tail
    element_t *ret = q_remove(head->prev, sp, bufsize);
    return ret;
}

/*
 * WARN: This is for external usage, don't modify it
 * Attempt to release element.
 */
void q_release_element(element_t *e)
{
    free(e->value);
    free(e);
}

/*
 * Return number of elements in queue.
 * Return 0 if q is NULL or empty
 */
int q_size(struct list_head *head)
{
    // check null or empty
    if (!head || list_empty(head))
        return 0;

    // compute len
    int len = 0;
    struct list_head *node = NULL;
    list_for_each (node, head)
        ++len;
    return len;
}

/*
 * Delete the middle node in list.
 * The middle node of a linked list of size n is the
 * ⌊n / 2⌋th node from the start using 0-based indexing.
 * If there're six element, the third member should be return.
 * Return true if successful.
 * Return false if list is NULL or empty.
 */
bool q_delete_mid(struct list_head *head)
{
    // check if it's NULL or empty
    if (!head || list_empty(head))
        return false;

    // search middle node by both sids
    struct list_head *r = head->next, *l = head->prev;
    for (; r != l && r->next != l; r = r->next)
        l = l->prev;

    // remove node and release element
    list_del_init(l);
    q_release_element(container_of(l, element_t, list));
    return true;
}

/*
 * Delete all nodes that have duplicate string,
 * leaving only distinct strings from the original list.
 * Return true if successful.
 * Return false if list is NULL.
 *
 * Note: this function always be called after sorting, in other words,
 * list is guaranteed to be sorted in ascending order.
 */
bool q_delete_dup(struct list_head *head)
{
    // check if it's NULL or empty
    if (!head || list_empty(head))
        return false;

    // check if it's singular
    if (list_is_singular(head))
        return true;

    struct list_head *first_node = head->next, *end_node = head->next;
    struct list_head *remove_head = q_new(), *tmp = q_new();
    while (end_node != head) {
        // search the start and end of duplicates string node
        element_t *first_element = container_of(first_node, element_t, list);
        element_t *end_element = NULL;
        do {
            end_node = end_node->next;
            end_element = container_of(end_node, element_t, list);
        } while (end_node != head &&
                 !strcmp(first_element->value, end_element->value));

        // concat duplicates node to remove_head
        if (first_node->next != end_node) {
            list_cut_position(tmp, first_node->prev, end_node->prev);
            list_splice_init(tmp, remove_head);
        }
        first_node = end_node;
    }
    q_free(remove_head);
    q_free(tmp);
    return true;
}

/*
 * Attempt to swap every two adjacent nodes.
 */
void q_swap(struct list_head *head)
{
    // check if it's NULL or empty
    if (!head || list_empty(head))
        return;

    // q_swap
    struct list_head *node = head->next, *curr = NULL, *next = NULL;
    while (node != head && node->next != head) {
        next = node->next;
        curr = node;
        node = node->next->next;

        // swap adjacent nodes.
        curr->next = next->next;
        next->prev = curr->prev;
        curr->prev->next = next;
        curr->prev = next;
        next->next->prev = curr;
        next->next = curr;
    }
}

void swap_list_head(struct list_head **a, struct list_head **b)
{
    struct list_head *tmp = *a;
    *a = *b;
    *b = tmp;
}

/*
 * Reverse elements in queue
 * No effect if q is NULL or empty
 * This function should not allocate or free any list elements
 * (e.g., by calling q_insert_head, q_insert_tail, or q_remove_head).
 * It should rearrange the existing ones.
 */
void q_reverse(struct list_head *head)
{
    // check if it's NULL or empty
    if (!head || list_empty(head))
        return;

    struct list_head *safe = NULL, *curr = NULL;
    list_for_each_safe (curr, safe, head)
        swap_list_head(&curr->prev, &curr->next);
    swap_list_head(&curr->prev, &curr->next);
}

/*
 * Merge two sorted list.
 * Each list_head of the list has a container element_t.
 */
struct list_head *merge_two_list(struct list_head *a, struct list_head *b)
{
    struct list_head *head = NULL, **ptr = &head;
    for (struct list_head **node = NULL; a && b; *node = (*node)->next) {
        node = strcmp(container_of(a, element_t, list)->value,
                      container_of(b, element_t, list)->value) < 0
                   ? &a
                   : &b;
        *ptr = *node;
        ptr = &(*ptr)->next;
    }
    *ptr = (struct list_head *) ((uintptr_t) a | (uintptr_t) b);
    return head;
}
/*
 * recursive function for q_sort.
 */
struct list_head *merge_sort(struct list_head *head)
{
    if (!head || !head->next)
        return head;

    // search middle node
    struct list_head *slow = head, *fast = head->next;
    for (; fast && fast->next; fast = fast->next->next)
        slow = slow->next;

    struct list_head *middle = slow->next;
    slow->next = NULL;

    return merge_two_list(merge_sort(head), merge_sort(middle));
}

/*
 * Sort elements of queue in ascending order
 * No effect if q is NULL or empty. In addition, if q has only one
 * element, do nothing.
 */
void q_sort(struct list_head *head)
{
    // check if it's NULL or empty
    if (!head || list_empty(head) || list_is_singular(head))
        return;

    // remove the link of tail and head to create stop condition for merge_sort
    head->prev->next = NULL;

    // merge_sort
    head->next = merge_sort(head->next);

    // assign prev to each node
    struct list_head *tmp = head;
    while (tmp->next) {
        tmp->next->prev = tmp;
        tmp = tmp->next;
    }
    // concat head and tail
    tmp->next = head;
    head->prev = tmp;
}