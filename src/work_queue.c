/*
 * work_queue.c Copyright (C) 2015 Ron Pedde <ron@pedde.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "work_queue.h"
#include "debug.h"

/* Structs */

#ifndef SIGCLD
# define SIGCLD SIGCHLD
#endif

#define PAYLOAD_TYPE_USER   0
#define PAYLOAD_TYPE_SYSTEM 1

#define TRUE 1
#define FALSE 0

struct work_item_struct {
    int work_item_type;
    void *payload;
    struct work_item_struct *next;
    struct work_item_struct *prev;
};

struct work_queue_struct {
    int total_workers;            /*< Total worker thread count */
    int waiting_workers;          /*< How many are waiting for new work items */
    int busy_workers;             /*< How many are processing work items */
    int block_signals;            /*< Should we block signals from workers @see work_queue_init */

    int initialization_errors;    /*< To track successful worker startups */
    int must_quit;                /*< Marks whether the queue is in shutdown */
    int refuse_enqueues;          /*< Whether or not to block enqueues (for shutdown, for example) */

    work_item_t payloads;         /*< Work items to be processed */
    int queued_items;             /*< Number of work items queued */
    int max_queued_items;         /*< Max number of queued_items */

    worker_init_cb init;          /*< Worker callback for thread startup */
    worker_deinit_cb deinit;      /*< Worker callback for thread shutdown */
    worker_dispatch_cb dispatch;  /*< Worker callback for job processing */

    pthread_key_t tls_key;        /*< TLS key (@see work_item_set_tls) */

    pthread_mutex_t queue_lock;   /*< Job lock, cond locks */
    pthread_mutex_t conv_lock;    /*< Convenience lock (@see work_queue_lock) */
    pthread_cond_t add_cond;      /*< Notification of work item enqueue */
    pthread_cond_t remove_cond;   /*< Notification of work item dequeue/dispatch */
    pthread_cond_t count_cond;    /*< Notification of worker thread total change */
};

/* Forwards */
static int work_queue_add_worker(work_queue_t *queue);
static void *work_queue_proc(void *arg);

static void work_queue_job_lock(work_queue_t *queue);
static void work_queue_job_unlock(work_queue_t *queue);


/**
 * Lock the queue_lock (job struct lock) on the specified queue
 *
 * @param queue queue to lock
 */
static void work_queue_job_lock(work_queue_t *queue) {
    int result;

    assert(queue);

    if(!queue)
        return;

    if((result=pthread_mutex_lock(&queue->queue_lock))) {
        ERROR("pthread_mutex_lock failed: %s.  Aborting", strerror(result));
        exit(EXIT_FAILURE);
    }
}


/**
 * unlock the specified queue
 *
 * @param queue queue to unlock
 */
static void work_queue_job_unlock(work_queue_t *queue) {
    int result;

    assert(queue);

    if(!queue)
        return;

    if((result=pthread_mutex_unlock(&queue->queue_lock))) {
        ERROR("pthread_mutex_unlock failed: %s.  Aborting", strerror(result));
        exit(EXIT_FAILURE);
    }
}

/**
 * Create a new work queue, spin up workers, and start
 * initial worker threads.  If this function fails, it
 * *should* clean up the outstanding worker threads, but
 * if this function fails, likely there is no point in
 * continuing the application.
 *
 * @param workers number of initial workers to create
 * @param worker_init_cb initialize callback for worker
 * @param worker_deinit_cb deinitialize callback for worker
 * @param worker_dispatch_cb dispatch callback for payloads
 * @returns TRUE on success, FALSE otherwise.
 */
work_queue_t *work_queue_init(int workers, int block_signals,
                              worker_init_cb init,
                              worker_deinit_cb deinit,
                              worker_dispatch_cb dispatch) {
    work_queue_t *queue;
    int count;

    queue = (work_queue_t*)calloc(1,sizeof(work_queue_t));
    if(!queue) {
        ERROR("Malloc error creating work queue");
        return NULL;
    }

    if(pthread_cond_init(&queue->add_cond, NULL) < 0) {
        ERROR("Error initializing condition variable: %s", strerror(errno));
        free(queue);
        return NULL;
    }

    if(pthread_cond_init(&queue->remove_cond, NULL) < 0) {
        ERROR("Error initializing condition variable: %s", strerror(errno));
        free(queue);
        return NULL;
    }

    if(pthread_cond_init(&queue->count_cond, NULL) < 0) {
        ERROR("Error initializing condition variable: %s", strerror(errno));
        free(queue);
        return NULL;
    }

    if(pthread_mutex_init(&queue->queue_lock, NULL) < 0) {
        ERROR("Error initializing mutex: %s", strerror(errno));
        free(queue);
        return NULL;
    }

    if(pthread_mutex_init(&queue->conv_lock, NULL) < 0) {
        ERROR("Error initializing mutex: %s", strerror(errno));
        free(queue);
        return NULL;
    }

    if(pthread_key_create(&queue->tls_key, NULL) < 0) {
        ERROR("Error initializing tls key: %s", strerror(errno));
        free(queue);
        return NULL;
    }


    queue->block_signals = block_signals;
    queue->init = init;
    queue->deinit = deinit;
    queue->dispatch = dispatch;

    queue->payloads.next = queue->payloads.prev = &queue->payloads;

    for(count = 0; count < workers; count++) {
        if(!work_queue_add_worker(queue)) {
            work_queue_deinit(queue, TRUE); /* No items to abandon.  :) */
            return NULL;
        }
    }

    return queue;
}

/**
 * spin up a new worker thread for a queue.  This increments the queue
 * thread counters, etc.
 *
 * @param queue queue to add a new thread for
 * @returns TRUE on success, FALSE otherwise
 */
static int work_queue_add_worker(work_queue_t *queue) {
    pthread_t tid;
    int total_workers, initialization_errors;

    work_queue_job_lock(queue);

    total_workers = queue->total_workers;
    initialization_errors = queue->initialization_errors;

    if(pthread_create(&tid, NULL, work_queue_proc, queue) < 0) {
        ERROR("Error creating pthread: %s", strerror(errno));
        return FALSE;
    }

    /* count_cond *MUST* be signalled whenever thread count
     * changes.
     */

    while(total_workers == queue->total_workers &&
          initialization_errors == queue->initialization_errors) {
        pthread_cond_wait(&queue->count_cond, &queue->queue_lock);
    }

    /* mutex is held.  See if it succeeded or failed. */
    if(total_workers == queue->total_workers) { /* failed to initialize */
        work_queue_job_unlock(queue);
        return FALSE;
    }

    work_queue_job_unlock(queue);
    return TRUE;
}

/**
 * Attempt to dequeue a work item.  This cannot be dequeued
 * if the payload is in the process of being dispatched.
 * Likewise, it cannot be dequeued if it has already been
 * dispatched and disposed of.  We'll track active work items
 * and verify it's a real task before we try and dequeue it.
 *
 * @param queue queue to dequeue the work item from
 * @param work_item work _item to dequeue from the queue
 * @returns TRUE on success, FALSE if the payload is already in dispatch
 */
int work_queue_dequeue(work_queue_t *queue, work_item_t *work_item) {
    work_item_t *current;

    assert(queue);
    assert(work_item);

    if((!queue) || (!work_item))
        return FALSE;

    work_queue_job_lock(queue);

    /* walk the work items and make sure this item is there. */
    current = queue->payloads.next;
    while(current != &queue->payloads && current != work_item) {
        current = current->next;
    }

    if(current == &queue->payloads) {
        /* not found */
        work_queue_job_unlock(queue);
        return FALSE;
    }

    /* otherwise, we are free to unlink it */
    work_item->next->prev = work_item->prev;
    work_item->prev->next = work_item->next;

    queue->queued_items--;
    /* signal that we pulled off a work item */
    pthread_cond_signal(&queue->remove_cond);
    work_queue_job_unlock(queue);

    return TRUE;
}


/**
 * main dispatch proc for worker threads
 *
 * @param arg the work queue this thread is working
 */
static void *work_queue_proc(void *arg) {
    work_queue_t *queue = (work_queue_t *)arg;
    work_item_t *payload;
    sigset_t set;

    /* mask off signals */
    if(queue->block_signals) {
        if((sigemptyset(&set) == -1) ||
           (sigaddset(&set,SIGINT) == -1) ||
           (sigaddset(&set,SIGHUP) == -1) ||
           (sigaddset(&set,SIGCLD) == -1) ||
           (sigaddset(&set,SIGTERM) == -1) ||
           (sigaddset(&set,SIGPIPE) == -1) ||
           (pthread_sigmask(SIG_BLOCK, &set, NULL) == -1)) {
            WARN("Error setting signal set");
        }
    }

    pthread_detach(pthread_self());

    DEBUG("Started thread %04x", pthread_self());

    /* run the init function */
    if(!queue->init(queue)) {
        ERROR("Could not initialize thread %04x", pthread_self());
        work_queue_job_lock(queue);
        queue->initialization_errors++;
        pthread_cond_signal(&queue->count_cond);
        work_queue_job_unlock(queue);
        /* already detached, so just return */
        return NULL;
    }

    /* mark new thread as successful */
    work_queue_job_lock(queue);
    queue->total_workers++;
    queue->waiting_workers++;
    pthread_cond_signal(&queue->count_cond);
    work_queue_job_unlock(queue);

    work_queue_job_lock(queue);

    /* do work */
    while(1) {
        /* we are already marked as waiting, so let's do that */
        while(queue->payloads.next == &queue->payloads && (!queue->must_quit)) {
            pthread_cond_wait(&queue->add_cond, &queue->queue_lock);
        }

        if(queue->must_quit)
            break;

        /* got a work unit, let's pull it off the queue (top) */
        payload = queue->payloads.next;
        payload->next->prev = payload->prev;
        payload->prev->next = payload->next;

        queue->waiting_workers--;
        queue->busy_workers++;
        queue->queued_items--;

        /* signal a work item removal */
        pthread_cond_signal(&queue->remove_cond);

        work_queue_job_unlock(queue);

        /* do the playload stuff now */
        if(payload->work_item_type == PAYLOAD_TYPE_USER) {
            /* run it through the user dispatcher */
            queue->dispatch(queue, payload->payload);
        } else {
            /* FIXME: system payloads */
        }

        free(payload);

        work_queue_job_lock(queue);
        queue->busy_workers--;
        queue->waiting_workers++;
    }

    work_queue_job_unlock(queue);

    /* we're out */
    queue->deinit(queue);

    work_queue_job_lock(queue);
    queue->total_workers--;
    queue->waiting_workers--;
    pthread_cond_signal(&queue->count_cond);
    work_queue_job_unlock(queue);

    DEBUG("Terminating thread %04x", pthread_self());

    return NULL;
}


/**
 * tear down a work queue.  Mark the queue as needing to quit, and
 * continually wake up workers until they all voluntarily exit.  This
 * requires that the payload processing threads either run quickly, or
 * quickly check work_queue_must_quit().
 *
 * Another problem is the "What to do about outstanding work units" problem.
 *
 * FIXME: this should really have a timeout and/or a way to ungracefully.
 *
 * As it stands right now, a dead thread will hang the shutdown
 *
 * @param queue queue to deinitialize
 */
int work_queue_deinit(work_queue_t *queue, int abandon) {
    assert(queue);

    if(!queue)
        return FALSE;

    if(!abandon) {
        DEBUG("Waiting for queue to empty");

        work_queue_job_lock(queue);

        queue->refuse_enqueues++;

        while(queue->payloads.next != &queue->payloads) {
            pthread_cond_wait(&queue->remove_cond, &queue->queue_lock);
        }

        DEBUG("Queue is empty");
        work_queue_job_unlock(queue);
    }

    DEBUG("Waiting for threads to die");

    /* kick wakeups until all the workers are gone */
    work_queue_job_lock(queue);

    queue->must_quit++;

    while(queue->total_workers) {
        pthread_cond_signal(&queue->add_cond);
        pthread_cond_wait(&queue->count_cond, &queue->queue_lock);
        DEBUG("Current workers: %d", queue->total_workers);
    }

    work_queue_job_unlock(queue);

    pthread_key_delete(queue->tls_key);

    free(queue);
    return TRUE;
}

/**
 * Check to see if the queue is in a "must_quit" state.
 * This should really only be called as a utility function from inside
 * a thread dispatcher, otherwise, you run the risk of dereferencing a freed
 * queue pointer.
 *
 * @param queue queue to check
 */
int work_queue_must_quit(work_queue_t *queue) {
    return queue->must_quit;
}

/**
 * enqueue a new work item.  Payloads get enqueued at the "bottom"
 * of the queue, while new tasks are pulled from the "top".  This
 * would probably have more utility as a priority queue.  Meh.
 *
 * Note that it is the dispatch function's responsibility to
 * dipose of (free) the payload object.  On either successful or unsuccessful
 * dispatch, the queue assumes that the payload has been either disposed
 * of, or re-enqueued, as appropriate.  The worker proc just bindly calls
 * the dispatch function, and completely forgets about what happend to the
 * payload.
 *
 * @param queue queue to enqueue new payload/work item on
 * @param payload opaque payload object
 */
work_item_t *work_queue_enqueue(work_queue_t *queue, void *payload) {
    work_item_t *payload_item;

    assert(queue);
    assert(payload);

    if((!queue) || (!payload))
        return NULL;

    if(queue->refuse_enqueues) {
        ERROR("Enqueue after queue shut down");
        return NULL;
    }

    payload_item = (work_item_t *)calloc(1, sizeof(work_item_t));
    if(!payload_item) {
        ERROR("Malloc error enqueueing new work item");
        return NULL;
    }

    payload_item->payload = payload;
    payload_item->work_item_type = PAYLOAD_TYPE_USER;

    work_queue_job_lock(queue);

    payload_item->next = queue->payloads.prev->next;
    payload_item->prev = queue->payloads.prev;
    payload_item->prev->next = payload_item;
    queue->payloads.prev = payload_item;
    queue->queued_items++;
    if(queue->queued_items > queue->max_queued_items)
        queue->max_queued_items = queue->queued_items;

    pthread_cond_signal(&queue->add_cond);
    work_queue_job_unlock(queue);

    return payload_item;
}

/**
 * Fetch the tls object associated with the running thread
 *
 * @param queue the queue this thread belongs to
 */
void *work_queue_get_tls(work_queue_t *queue) {
    void *result;

    assert(queue);

    if(!queue)
        return NULL;

    result = pthread_getspecific(queue->tls_key);
    return result;
}

/**
 * set the tls object associated with the running thread.  Note
 * that cleanup for this object should be done on the deinit callback
 * for the thread.
 *
 * @param queue queue this thread is associated with
 * @param item tls object to set
 * @returns TRUE on success, FALSE otherwise
 */
int work_queue_set_tls(work_queue_t *queue, void *item) {
    assert(queue);
    assert(item);

    if((!queue)||(!item))
        return FALSE;

    if(pthread_setspecific(queue->tls_key, item) < 0) {
        ERROR("Error setting thread-specific storage: %s", strerror(errno));
        return FALSE;
    }

    return TRUE;
}

/**
 * Every queue has a simple mutex for convenient locking.  The
 * dispatch (and other) callbacks are free to implement their own
 * locking, but this is a simple and convenient lock that can be used
 * should it be required.  Verify likely if this fails, something
 * fundamentally broken is going on, and very likely the caller
 * should exit(3)
 *
 * @param queue queue to lock
 * @returns TRUE on success, FALSE otherwise
 */
int work_queue_lock(work_queue_t *queue) {
    assert(queue);

    if(!queue)
        return FALSE;

    if(pthread_mutex_lock(&queue->conv_lock) < 0) {
        ERROR("Error locking convenience mutex: %s", strerror(errno));
        return FALSE;
    }

    return TRUE;
}

/**
 * Unlocks the convenience lock.  As mentioned in work_queue_lock(),
 * if this fails, something is horrifically wrong, and the caller
 * should probably exit(3).
 *
 * @param queue queue to lock
 * @returns TRUE on success, FALSE otherwise
 */
int work_queue_unlock(work_queue_t *queue) {
    assert(queue);

    if(!queue)
        return FALSE;

    if(pthread_mutex_unlock(&queue->conv_lock) < 0) {
        ERROR("Error unlocking convenience mutex: %s", strerror(errno));
        return FALSE;
    }

    return TRUE;
}


/**
 * Return the work queue stats.  This is a function so as to
 * better return coherent stats
 *
 * NOTE: it is the responsibility of the caller to free the returned
 * structure
 *
 * @param queue queue to get stats for
 * @returns stats struct (which caller must free), or NULL on error
 */
work_queue_stats_t *work_queue_stats(work_queue_t *queue) {
    work_queue_stats_t *retval;

    assert(queue);

    if(!queue)
        return NULL;

    retval = (work_queue_stats_t *)calloc(sizeof(work_queue_stats_t), 1);
    if(retval) {
        work_queue_job_lock(queue);

        retval->total_workers = queue->total_workers;
        retval->waiting_workers = queue->waiting_workers;
        retval->busy_workers = queue->busy_workers;
        retval->max_queued_items = queue->max_queued_items;

        queue->max_queued_items = queue->queued_items;

        queue->max_queued_items = 0;

        work_queue_job_unlock(queue);
    }

    return retval;
}
