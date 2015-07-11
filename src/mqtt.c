/*
 * mqtt.c Copyright (C) 2015 Ron Pedde <ron@pedde.com>
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

#include "nrf24-mqtt.h"
#include "work_queue.h"
#include "debug.h"

static work_queue_t *queue;


int mqtt_worker_init(work_queue_t *queue) {
    DEBUG("Initializing worker");
    return TRUE;
}

int mqtt_worker_deinit(work_queue_t *queue) {
    DEBUG("Deinitializing worker");
    return TRUE;
}


int mqtt_worker_dispatch(work_queue_t *queue, void *payload) {
    DEBUG("Got work item");
    return TRUE;
}


int mqtt_init(void) {
    /* start up the work queue and go */
    DEBUG("Starting mqtt worker queue");

    queue = work_queue_init(2, TRUE,
                            mqtt_worker_init,
                            mqtt_worker_deinit,
                            mqtt_worker_dispatch);

    if(!queue) {
        ERROR("Could not initialize work queue");
        return -1;
    }

    DEBUG("Worker queue started");
    return 0;
}

int mqtt_deinit(void) {
    DEBUG("Tearing down work queue");
    work_queue_deinit(queue, FALSE);

    return 0;
}
