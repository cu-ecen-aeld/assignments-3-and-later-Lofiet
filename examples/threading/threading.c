#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...) printf("threading DEBUG: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)
#define MS2NS 1000000
#define S2MS 1000
#define MAX_LOOP 10

bool wait_for_ms(int wait_time)
{
    struct timespec wait_timespec;
    struct timespec rem_timespec;
    int             same_count = 0;
    bool            return_value = true;

    wait_timespec.tv_sec = wait_time / S2MS;
    wait_timespec.tv_nsec = (wait_time % S2MS) * MS2NS;

    while (nanosleep(&wait_timespec, &rem_timespec) != 0) {
        if (errno == EINTR) {
            
            // Make sure we don't get stuck in an infinite loop
            if ((wait_timespec.tv_sec == rem_timespec.tv_sec) && (wait_timespec.tv_nsec == rem_timespec.tv_nsec)) {
                same_count++;
            }
            else {
                same_count = 0;
            }
            if (same_count >= 10) {
                ERROR_LOG("Sleep has returned with the same remaining time %d times", MAX_LOOP);
                return_value = false;
            }
            wait_timespec.tv_sec = rem_timespec.tv_sec;
            wait_timespec.tv_nsec = rem_timespec.tv_nsec;
        }
        else {
            ERROR_LOG("Sleep returned an error case: %s", strerror(errno));
            return_value = false;
            break;
        }
    }

    return return_value;
}

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    int errno_storage = 0;
    bool status = true;
    
    thread_func_args->thread_complete_success = false;

    if (status) {
        status = wait_for_ms(thread_func_args->wait_to_obtain_ms);
    }

    if (status) {
        // obtain mutex
        errno_storage = pthread_mutex_lock(thread_func_args->mutex);
        if (errno_storage != 0) {
            status = false;
            ERROR_LOG("Failed to lock mutex: %s", strerror(errno_storage));
        }
    }

    if (status) {
        status = wait_for_ms(thread_func_args->wait_to_release_ms);
    }

    if (status) {
        // obtain mutex
        errno_storage = pthread_mutex_unlock(thread_func_args->mutex);
        if (errno_storage != 0) {
            status = false;
            ERROR_LOG("Failed to unlock mutex: %s", strerror(errno_storage));
        }
    }

    thread_func_args->thread_complete_success = status;

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    int errno_storage = 0;
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */

    bool return_val = false;
    struct thread_data *thread_data_ptr = malloc(sizeof(struct thread_data));

    if (thread_data_ptr != NULL) {
        thread_data_ptr->mutex = mutex;
        thread_data_ptr->wait_to_obtain_ms = wait_to_obtain_ms;
        thread_data_ptr->wait_to_release_ms = wait_to_release_ms;
        errno_storage = pthread_create(thread, NULL, &threadfunc, thread_data_ptr);
        if (errno_storage != 0){
            ERROR_LOG("pthread_create failed: %s", strerror(errno_storage));
            return_val = false;
        }
        else {
            return_val = true;
        }
    }
    else {
        ERROR_LOG("Unable to allocate memory for thread data");
        return_val = false;
    }
    return return_val;
}

