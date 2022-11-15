string eps_opt()   { return ""; }
string eps_usage() { return "\n"; }
string eps_help()  { return "show processes\n"; }

#include <sched.h>

char stateChar(long state)
{
    int bit=1, idx;
    string s="";
    
    for(bit=1, idx=1; bit<TASK_STATE_MAX; bit <<=1, idx++) if(bit&state) break;
    if(bit==TASK_STATE_MAX) idx=0;
    return task_state_array[idx][0];
}

void ps_print_one_task(struct task_struct *t) 
{
    char s=stateChar(t->state);
    printf("%s %5d %5d %5d %p  %c  ", t->on_cpu?">":" ", t->pid, t->parent->pid, t->cpu, t, s);
    if(t->mm) {
        printf("%s\n", getstr(&t->comm));
    } else {
        printf("[%s]\n", getstr(&t->comm));
    }
}

int eps()
{
    struct task_struct *p;
    printf("   PID    PPID  CPU %>TASK       ST COMM\n");
    for_each_process(p) {
        struct task_struct *t;
        for_each_thread(p, t) {
            ps_print_one_task(t);
        }
    }
    
    return 1;
}
