#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"
#include "queue.h"

/*
 * Following code are for linux_sort
 */
typedef unsigned char u8;
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
void linux_sort(struct list_head *head);