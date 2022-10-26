#include <list.h>
void waiters(wait_queue_head_t *q)
{
struct wait_queue_entry *curr, *next;

    list_for_each_entry_safe(curr, next, &q->task_list, task_list) {
        if(curr->func == &ep_poll_callback) {
            printf("Epoll\n");
        }
    }
}
