// This is a C program that implements a simple many-to-many user-level threading library
// with a first come first serve (FCFS) thread scheduler. It has two types of kernel-level
// threads that run the user-level threads : one/two compute executor(s) (C-EXEC), and
// one I/O executor (I-EXEC).
// Éléa Dufresne
// 2021-11-04


/* CHANGE THIS VARIABLE FOR PART A AND B : */
int number_of_c_executors=1; // 1 for part A, 2 for part B

/* imports */
#include "queue.h"
#include "sut.h"

#include <unistd.h>
#include <pthread.h>
#include <ucontext.h>
#include <fcntl.h>

#include <signal.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/* types */
typedef void (*sut_task_f)();
// used for the ready queue
typedef struct __threaddesc
{
    int threadid;
    char *threadstack;
    void *threadfunc;
    ucontext_t threadcontext;
} threaddesc;
// used for the wait queue
typedef struct io_operation
{
    int operation; // open 0, read 1, write 2, close 3
    int file_descriptor;
    char *buf;
    int size;
    bool completed;
} io_operation;

/* fields */
#define MAX_THREADS                        32
#define THREAD_STACK_SIZE                  1024*64
struct queue ready_queue, ready_queue_2; // task ready queues
struct queue wait_queue; // wait queue
struct queue_entry *ready_head, *wait_head, *ready_head_2; // current task, head of both queues
int thread_count, file_count; // number of threads, number of files that got opened
pthread_t C_EXEC1; // 1st compute executor (Part A and B)
pthread_t C_EXEC2; // 2nd compute executor (Part B)
pthread_t I_EXEC; // I/O executor
pthread_mutex_t c_exec_lock, c_exec2_lock, i_exec_lock; // locks c_exec2_lock,
ucontext_t c_exec_context, c_exec2_context;// c execs contexts
threaddesc threads[MAX_THREADS]; // descriptor
bool shutting_down, break_c_exec, break_i_exec, break_c_exec2; // boolean to determine when to stop
char *files[32];
int fds[32];

/* function for C-EXEC */
void *c_exec_function (){
    ucontext_t curr_context;
    // while there are tasks in the task ready queue, and we are not shutting down
    while(!break_c_exec || !break_i_exec || !shutting_down){
        // check if there are tasks in the ready_queue
        pthread_mutex_lock(&c_exec_lock);
        ready_head = queue_peek_front(&ready_queue);
        pthread_mutex_unlock(&c_exec_lock);
        // if the ready queue is not empty
        if(ready_head) {
            break_c_exec=false;
            // get the current task
            pthread_mutex_lock(&c_exec_lock);
            curr_context = *(ucontext_t *) ready_head->data;
            queue_pop_head(&ready_queue);
            pthread_mutex_unlock(&c_exec_lock);
            // make it the current context
            swapcontext(&c_exec_context, &curr_context);
            // free the current task
            free(ready_head);
        }
        else break_c_exec = true;
        //100 microseconds sleep to reduce CPU utilization
        nanosleep((struct timespec[]){{0,100000}}, NULL);
    }
    return 0;
}

/* function for C-EXEC 2 */
void *c_exec2_function (){
    ucontext_t curr_context;
    // while there are tasks in the task ready queue, and we are not shutting down
    while(!break_c_exec2 || !break_i_exec || !shutting_down){
        // check if there are tasks in the ready_queue
        pthread_mutex_lock(&c_exec2_lock);
        ready_head_2 = queue_peek_front(&ready_queue_2);
        pthread_mutex_unlock(&c_exec2_lock);
        // if the ready queue is not empty
        if(ready_head_2) {
            break_c_exec2=false;
            // get the current task
            pthread_mutex_lock(&c_exec2_lock);
            curr_context = *(ucontext_t *) ready_head_2->data;
            queue_pop_head(&ready_queue_2);
            pthread_mutex_unlock(&c_exec2_lock);
            // make it the current context
            swapcontext(&c_exec2_context, &curr_context);
            // free the current task
            free(ready_head_2);
        }
        else break_c_exec2 = true;
        //100 microseconds sleep to reduce CPU utilization
        nanosleep((struct timespec[]){{0,100000}}, NULL);
    }
    return 0;
}

/* function for I-EXEC */
void *i_exec_function (){
    io_operation *curr_task;
    // while there are tasks in the wait queue, and we are not shutting down
    while(!break_c_exec || !break_c_exec2 || !break_i_exec || !shutting_down){
        // check if there are tasks in the wait_queue
        pthread_mutex_lock(&i_exec_lock);
        wait_head = queue_peek_front(&wait_queue);
        pthread_mutex_unlock(&i_exec_lock);
        // if the wait queue is not empty
        if(wait_head) {
            // get the current task
            pthread_mutex_lock(&i_exec_lock);
            curr_task = (io_operation *) wait_head->data;
            queue_pop_head(&wait_queue);
            pthread_mutex_unlock(&i_exec_lock);
            // get its type
            int to_do = curr_task->operation;
            // do correct operation
            switch(to_do){
                case 0 : // open
                    pthread_mutex_lock(&i_exec_lock);
                    fds[curr_task->file_descriptor]=open((char *)files[curr_task->file_descriptor], O_RDWR , 0666);
                    //printf("we just opened the file %s\n", files[curr_task.file_descriptor]);
                    curr_task->completed = true;
                    pthread_mutex_unlock(&i_exec_lock);
                    break;
                case 1 : // read
                    pthread_mutex_lock(&i_exec_lock);
                    read(curr_task->file_descriptor, (curr_task->buf), curr_task->size);
                    //printf("we just read\n");
                    curr_task->completed = true;
                    pthread_mutex_unlock(&i_exec_lock);
                    break;
                case 2 : // write
                    pthread_mutex_lock(&i_exec_lock);
                    write(curr_task->file_descriptor, (curr_task->buf), curr_task->size);
                    //printf("we just wrote\n");
                    curr_task->completed = true;
                    pthread_mutex_unlock(&i_exec_lock);
                    break;
                case 3 : // close
                    pthread_mutex_lock(&i_exec_lock);
                    close(curr_task->file_descriptor);
                    //printf("we just closed\n");
                    curr_task->completed = true;
                    pthread_mutex_unlock(&i_exec_lock);
                    break;
                default :
                    break;
            }
            // free the current task
            free(wait_head);
        }
        else break_i_exec=true;
        //100 microseconds sleep to reduce CPU utilization
        nanosleep((struct timespec[]){{0,100000}}, NULL);
    }
    return 0;
}

/* initialize the SUT library */
void sut_init(){
    // task ready queue
    ready_queue = queue_create();
    queue_init(&ready_queue);
    // wait queue
    wait_queue = queue_create();
    queue_init(&wait_queue);
    // mutex locks
    pthread_mutex_init(&c_exec_lock, NULL);
    pthread_mutex_init(&i_exec_lock, NULL);
    // C-EXEC and I-EXEC
    pthread_create(&C_EXEC1, NULL, c_exec_function, NULL);
    pthread_create(&I_EXEC, NULL, i_exec_function, NULL);
    // context
    getcontext(&c_exec_context);
    // number of threads, of open files
    thread_count=0;
    file_count=0;
    // for now, we execute
    shutting_down=false;
    break_c_exec=false;
    break_i_exec=false;
    break_c_exec2=true;
    // PART B
    if(number_of_c_executors==2){
        // 2nd ready queue
        ready_queue_2 = queue_create();
        queue_init(&ready_queue_2);
        // 2nd C-EXEC
        pthread_create(&C_EXEC2, NULL, c_exec2_function, NULL);
        // 2nd lock
        pthread_mutex_init(&c_exec2_lock, NULL);
        // 2nd context
        getcontext(&c_exec2_context);
        // execute
        break_c_exec2=false;
    }
};

/* add a task to the ready queue */
bool sut_create(sut_task_f fn){
    // create a thread descriptor and context for the task
    threaddesc *thread_descriptor;
    thread_descriptor = &(threads[thread_count]);
    if(getcontext(&(thread_descriptor->threadcontext))) return false; // on failure return failure
    thread_descriptor->threadid = thread_count;
    thread_descriptor->threadstack = (char *)malloc(THREAD_STACK_SIZE);
    thread_descriptor->threadcontext.uc_stack.ss_sp = thread_descriptor->threadstack;
    thread_descriptor->threadcontext.uc_stack.ss_size = THREAD_STACK_SIZE;
    thread_descriptor->threadcontext.uc_stack.ss_flags = 0;
    thread_descriptor->threadfunc = &fn;
    // set context (PART B, we alternate)
    if(number_of_c_executors==1 || thread_count%2) {
        thread_descriptor->threadcontext.uc_link = &c_exec_context;
        makecontext(&(thread_descriptor->threadcontext), fn, 0);
        // put the task into the ready queue
        struct queue_entry *task;
        task=queue_new_node(&thread_descriptor->threadcontext);
        pthread_mutex_lock(&c_exec_lock);
        queue_insert_tail(&ready_queue, task);
        break_c_exec=false;
        pthread_mutex_unlock(&c_exec_lock);
    } else {
        thread_descriptor->threadcontext.uc_link = &c_exec2_context;
        makecontext(&(thread_descriptor->threadcontext), fn, 0);
        // put the task into the 2nd ready queue
        struct queue_entry *task;
        task=queue_new_node(&thread_descriptor->threadcontext);
        pthread_mutex_lock(&c_exec2_lock);
        queue_insert_tail(&ready_queue_2, task);
        break_c_exec2=false;
        pthread_mutex_unlock(&c_exec2_lock);
    }
    // increment the thread_count
    thread_count ++;
    // on success return true
    return true;
};

/* yield (pause) the execution of a task */
void sut_yield(){
    // current running task
    ucontext_t curr_context;
    getcontext(&curr_context);
    struct queue_entry *task;
    task=queue_new_node(&curr_context);
    // context that called this
    pthread_t thread = pthread_self();
    if(number_of_c_executors==1 || pthread_equal(thread,C_EXEC1)){
        // pause execution and put it back at the end of the task ready queue
        pthread_mutex_lock(&c_exec_lock);
        queue_insert_tail(&ready_queue, task);
        break_c_exec=false;
        pthread_mutex_unlock(&c_exec_lock);
        // save the current context and start C-EXEC1
        swapcontext(&curr_context, &c_exec_context);
    } else {
        // pause execution and put it back at the end of the task ready queue
        pthread_mutex_lock(&c_exec2_lock);
        queue_insert_tail(&ready_queue_2, task);
        break_c_exec2=false;
        pthread_mutex_unlock(&c_exec2_lock);
        // save the current context and start C-EXEC2
        swapcontext(&curr_context, &c_exec2_context);
    }
};

/* terminate the task execution */
void sut_exit(){
    // current context
    ucontext_t curr_context;
    getcontext(&curr_context);
    // context that called this
    pthread_t thread = pthread_self();
    if(number_of_c_executors==1 || pthread_equal(thread,C_EXEC1)){
        // pause execution (do NOT put back in the ready queue)
        swapcontext(&curr_context, &c_exec_context);
    } else {
        // pause execution (do NOT put back in the ready queue)
        swapcontext(&curr_context, &c_exec2_context);
    }
};

/* (IO function) open specified file, return the file descriptor */
int sut_open(char *dest){
    // save the task
    files[file_count]=dest;
    io_operation *io_task=(io_operation *)malloc(sizeof(io_operation));
    io_task->operation = 0;
    io_task->file_descriptor= file_count;
    io_task->completed = false;
    struct queue_entry *task;
    task=queue_new_node(io_task);
    // put it at the end of the wait queue
    pthread_mutex_lock(&i_exec_lock);
    queue_insert_tail(&wait_queue, task);
    break_i_exec=false;
    pthread_mutex_unlock(&i_exec_lock);
    // until the task is completed
    while(!io_task->completed){
        // current context
        ucontext_t curr_context;
        getcontext(&curr_context);
        struct queue_entry *task_context;
        task_context=queue_new_node(&curr_context);
        // context that called this
        pthread_t thread = pthread_self();
        if(number_of_c_executors==1 || pthread_equal(thread,C_EXEC1)){
            // pause execution and put it back at the end of the task ready queue
            pthread_mutex_lock(&c_exec_lock);
            queue_insert_tail(&ready_queue, task_context);
            break_c_exec=false;
            pthread_mutex_unlock(&c_exec_lock);
            // save the current context and start C-EXEC1
            swapcontext(&curr_context, &c_exec_context);
        } else {
            // pause execution and put it back at the end of the task ready queue
            pthread_mutex_lock(&c_exec2_lock);
            queue_insert_tail(&ready_queue_2, task_context);
            break_c_exec2=false;
            pthread_mutex_unlock(&c_exec2_lock);
            // save the current context and start C-EXEC2
            swapcontext(&curr_context, &c_exec2_context);
        }
    }
    free(io_task);
    // return fd
    return fds[file_count++];
};

/* (IO function) read from the file that is already open */
char *sut_read(int fd, char *buf, int size){
    // save the task
    io_operation *io_task=(io_operation *)malloc(sizeof(io_operation));
    io_task->operation = 1;
    io_task->file_descriptor=fd;
    io_task->buf=buf;
    io_task->size=size;
    io_task->completed=false;
    struct queue_entry *task;
    task=queue_new_node(io_task);
    // put current task at the end of the wait queue
    pthread_mutex_lock(&i_exec_lock);
    queue_insert_tail(&wait_queue, task);
    break_i_exec=false;
    pthread_mutex_unlock(&i_exec_lock);
    // until the task is completed
    while(!io_task->completed){
        // current context
        ucontext_t curr_context;
        getcontext(&curr_context);
        struct queue_entry *task_context;
        task_context=queue_new_node(&curr_context);
        // context that called this
        pthread_t thread = pthread_self();
        if(number_of_c_executors==1 || pthread_equal(thread,C_EXEC1)){
            // pause execution and put it back at the end of the task ready queue
            pthread_mutex_lock(&c_exec_lock);
            queue_insert_tail(&ready_queue, task_context);
            break_c_exec=false;
            pthread_mutex_unlock(&c_exec_lock);
            // save the current context and start C-EXEC1
            swapcontext(&curr_context, &c_exec_context);
        } else {
            // pause execution and put it back at the end of the task ready queue
            pthread_mutex_lock(&c_exec2_lock);
            queue_insert_tail(&ready_queue_2, task_context);
            break_c_exec2=false;
            pthread_mutex_unlock(&c_exec2_lock);
            // save the current context and start C-EXEC2
            swapcontext(&curr_context, &c_exec2_context);
        }
    }
    free(io_task);
    // on success return non-NULL
    return buf;
};

/* (IO function) write the bytes in buf to the disk file that is already open */
void sut_write(int fd, char *buf, int size){
    // save the task
    io_operation *io_task=(io_operation *)malloc(sizeof(io_operation));
    io_task->operation = 2;
    io_task->file_descriptor=fd;
    io_task->buf=buf;
    io_task->size=size;
    io_task->completed = false;
    struct queue_entry *task;
    task=queue_new_node(io_task);
    // put in the wait queue
    pthread_mutex_lock(&i_exec_lock);
    queue_insert_tail(&wait_queue, task);
    break_i_exec=false;
    pthread_mutex_unlock(&i_exec_lock);
    // until the task is completed
    while(!io_task->completed){
        // current context
        ucontext_t curr_context;
        getcontext(&curr_context);
        struct queue_entry *task_context;
        task_context=queue_new_node(&curr_context);
        // context that called this
        pthread_t thread = pthread_self();
        if(number_of_c_executors==1 || pthread_equal(thread,C_EXEC1)){
            // pause execution and put it back at the end of the task ready queue
            pthread_mutex_lock(&c_exec_lock);
            queue_insert_tail(&ready_queue, task_context);
            break_c_exec=false;
            pthread_mutex_unlock(&c_exec_lock);
            // save the current context and start C-EXEC1
            swapcontext(&curr_context, &c_exec_context);
        } else {
            // pause execution and put it back at the end of the task ready queue
            pthread_mutex_lock(&c_exec2_lock);
            queue_insert_tail(&ready_queue_2, task_context);
            break_c_exec2=false;
            pthread_mutex_unlock(&c_exec2_lock);
            // save the current context and start C-EXEC2
            swapcontext(&curr_context, &c_exec2_context);
        }
    }
    free(io_task);
};

/* (IO function) close the file that is pointed by the file descriptor */
void sut_close(int fd){
    // save the task
    io_operation *io_task=(io_operation *)malloc(sizeof(io_operation));
    io_task->operation = 3;
    io_task->file_descriptor=fd;
    io_task->completed = false;
    struct queue_entry *task;
    task=queue_new_node(io_task);
    // put current task at the end of the wait queue
    pthread_mutex_lock(&i_exec_lock);
    queue_insert_tail(&wait_queue, task);
    break_i_exec=false;
    pthread_mutex_unlock(&i_exec_lock);
    // until the task is completed
    while(!io_task->completed){
        // current context
        ucontext_t curr_context;
        getcontext(&curr_context);
        struct queue_entry *task_context;
        task_context=queue_new_node(&curr_context);
        // context that called this
        pthread_t thread = pthread_self();
        if(number_of_c_executors==1 || pthread_equal(thread,C_EXEC1)){
            // pause execution and put it back at the end of the task ready queue
            pthread_mutex_lock(&c_exec_lock);
            queue_insert_tail(&ready_queue, task_context);
            break_c_exec=false;
            pthread_mutex_unlock(&c_exec_lock);
            // save the current context and start C-EXEC1
            swapcontext(&curr_context, &c_exec_context);
        } else {
            // pause execution and put it back at the end of the task ready queue
            pthread_mutex_lock(&c_exec2_lock);
            queue_insert_tail(&ready_queue_2, task_context);
            break_c_exec2=false;
            pthread_mutex_unlock(&c_exec2_lock);
            // save the current context and start C-EXEC2
            swapcontext(&curr_context, &c_exec2_context);
        }
    }
    free(io_task);
};

/* shut down the library */
void sut_shutdown(){
    // flag the shutdown
    shutting_down=true;
    // "purge" the threads
    pthread_join(C_EXEC1, NULL);
    if(number_of_c_executors==2) pthread_join(C_EXEC2, NULL);
    pthread_join(I_EXEC, NULL);
    // terminate the threads
    pthread_exit(NULL);
};
