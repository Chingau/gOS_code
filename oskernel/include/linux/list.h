#ifndef __GOS_OSKERNEL_LIST_H_
#define __GOS_OSKERNEL_LIST_H_
#include "types.h"

#define offset(struct_type, member) (int)(&((struct_type *)0)->member)
#define elem2entry(struct_type, struct_member_name, elem_ptr)   (struct_type *)((int)elem_ptr - offset(struct_type, struct_member_name))

struct list_elem {
    struct list_elem *prev;
    struct list_elem *next;
};

struct list {
    struct list_elem head;  //head是队首，其位置固定，第一个元素为 head.next
    struct list_elem tail;  //tail是队尾，其位置固定
};

/* 自定义list_cb，用于在list_traversal中做回调函数 */
typedef bool (*list_cb)(struct list_elem *, int arg);

void list_init(struct list *list);
void list_insert_before(struct list_elem *before, struct list_elem *elem);
void list_push(struct list *plist, struct list_elem *elem);
void list_append(struct list *plist, struct list_elem *elem);
void list_remove(struct list_elem *elem);
struct list_elem *list_pop(struct list *plist);
bool elem_find(struct list *plist, struct list_elem *obj_elem);
uint32_t list_length(struct list *plist);
bool list_empty(struct list *plist);
struct list_elem *list_traversal(struct list *plist, list_cb func, int arg);
#endif
