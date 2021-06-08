string eps_opt()   { return ""; }
string eps_usage() { return "\n"; }
string eps_help()  { return "show processes\n"; }

#include <sched.h>

char stateChar(long state)
{
    int bit=1, idx=0;
    if(!state) return TASK_STATE_TO_CHAR_STR[0];
    for(bit=1, idx=1; bit<TASK_STATE_MAX; bit <<=1, idx++) if(bit==state) return TASK_STATE_TO_CHAR_STR[idx];
    return 'c';
}

void ps_print_one_task(struct task_struct *t) 
{
    
    printf("%s %5d %5d %5d %p  %c  ", t->on_cpu?">":" ", t->pid, t->parent->pid, t->cpu, t, stateChar(t->state));
    if(t->mm->rss_stat.count[0].counter>=0) {
        printf("%s\n", getstr(t->comm));
    } else {
        printf("[%s]\n", getstr(t->comm));
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
