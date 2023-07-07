#include "list.h"
#include "interrupt.h"

/* 初始化双向链表list */
void list_init(struct list *list)
{
    list->head.prev = NULL;
    list->head.next = &list->tail;
    list->tail.prev = &list->head;
    list->tail.next = NULL;
}

/* 将链表元素elem插入到链表元素before之前 */
void list_insert_before(struct list_elem *before, struct list_elem *elem)
{
    INTR_STATUS_T old_state = intr_disable();
    before->prev->next = elem;
    elem->prev = before->prev;
    elem->next = before;
    before->prev = elem;
    intr_set_status(old_state);
}

/* 添加元素到列表队首，类似栈push操作 */
void list_push(struct list *plist, struct list_elem *elem)
{
    list_insert_before(plist->head.next, elem); //在队头第一个元素前插入
}

/* 添加元素到队尾，类似于队列先进先出操作 */
void list_append(struct list *plist, struct list_elem *elem)
{
    list_insert_before(&plist->tail, elem); //在队尾的前面插入
}

/* 把元素elem从链表中移出 */
void list_remove(struct list_elem *elem)
{
    INTR_STATUS_T old_state = intr_disable();
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
    intr_set_status(old_state);
}

/* 将链表的第一个元素弹出并返回，类似栈的pop操作 */
struct list_elem *list_pop(struct list *plist)
{
    struct list_elem *elem = plist->head.next;
    list_remove(elem);
    return elem;
}

/* 在链表中查找obj_elem元素，找到返回true，未找到返回false */
bool elem_find(struct list *plist, struct list_elem *obj_elem)
{
    struct list_elem *elem = plist->head.next;
    while (elem != &plist->tail) {
        if (elem == obj_elem) {
            return true;
        }
        elem = elem->next;
    }
    return false;
}

/* 返回链表长度 */
uint32_t list_length(struct list *plist)
{
    struct list_elem *elem = plist->head.next;
    uint32_t length = 0;

    while (elem != &plist->tail) {
        length++;
        elem = elem->next;
    }
    return length;
}

/* 判断链表是否为空；为空返回true, 否则返回false */
bool list_empty(struct list *plist)
{
    return (plist->head.next == &plist->tail) ? true : false;
}

/*
 * 把列表plist中的每个元素elem和arg传给回调函数func，
 * arg给func用来判断elem是否符合条件。
 * 本函数的功能是遍历列表内所有元素，逐个判断是否有符合条件的元素
 * 打到符合条件的元素返回元素指针，否则返回NULL
*/
struct list_elem *list_traversal(struct list *plist, list_cb func, int arg)
{
    struct list_elem *elem = plist->head.next;

    if (list_empty(plist)) {
        return NULL;
    }
    while (elem != &plist->tail) {
        if (func(elem, arg)) {
            //func返回true，则认为该元素在回调函数中符合条件，命中，停止链表遍历
            return elem;
        }
        elem = elem->next;
    }
    return NULL;
}
