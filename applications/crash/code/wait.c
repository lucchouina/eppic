#include <list.h>
void waiters(wait_queue_head_t *q)
{
wait_queue_t *curr, *next;
int i=0;
    list_for_each_entry_safe(curr, next, &q->task_list, task_list) {
        if(curr->func == &ep_poll_callback) {
            printf("Epoll\n");
        }
    }
}
