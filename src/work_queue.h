/*
 * work_queue.h Copyright (C) 2015 Ron Pedde <ron@pedde.com>
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

#ifndef _WORK_QUEUE_H_
#define _WORK_QUEUE_H_

typedef struct work_queue_struct work_queue_t;
typedef struct work_item_struct work_item_t;

typedef struct work_queue_stats_t {
    uint16_t total_workers;
    uint16_t waiting_workers;
    uint16_t busy_workers;
    uint16_t max_queued_items;
} work_queue_stats_t;

typedef int(*worker_init_cb)(work_queue_t *queue);
typedef int(*worker_deinit_cb)(work_queue_t *queue);
typedef int(*worker_dispatch_cb)(work_queue_t *queue, void *payload);

extern work_queue_t *work_queue_init(int workers, int block_signals,
                                     worker_init_cb init,
                                     worker_deinit_cb deinit,
                                     worker_dispatch_cb dispatch);
extern int work_queue_deinit(work_queue_t *queue, int abandon);
extern work_item_t *work_queue_enqueue(work_queue_t *queue, void *payload);
extern int work_queue_dequeue(work_queue_t *queue, work_item_t *work_item);
extern int work_queue_must_quit(work_queue_t *queue);

extern void *work_queue_get_tls(work_queue_t *queue);
extern int work_queue_set_tls(work_queue_t *queue, void *item);

extern int work_queue_lock(work_queue_t *queue);
extern int work_queue_unlock(work_queue_t *queue);

extern work_queue_stats_t *work_queue_stats(work_queue_t *queue);


#endif /* _WORK_QUEUE_H_ */
