/*
 * nrf24-crazyradio-recv.c Copyright (C) 2015 Ron Pedde <ron@pedde.com>
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
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include <crazyradio.h>

#include "nrf24-mqtt.h"
#include "debug.h"
#include "sensor.h"
#include "cfg.h"
#include "mqtt.h"

static cradio_device_t *radio;
static pthread_t nrf24_recv_tid;
static int radio_quit = 0;

static void nrf24_crazy_log(int level, char *format, va_list args) {
    debug_vprintf(level, format, args);
    debug_printf(level, "\n");
}

static void *nrf24_recv_thread(void *data) {
    int result;
    unsigned char buffer[64];

    DEBUG("nrf24 recv thread started");

    while(!radio_quit) {
        memset(buffer, 0x0, sizeof(buffer));

        result = cradio_read_packet(radio, buffer, sizeof(buffer)-1, 1);
        if(result > 0) {
            DEBUG("Got %d bytes of data", result);
            mqtt_dispatch((sensor_struct_t *)&buffer);
        } else if (result < 0) {
            /* error... */
            ERROR("Error: %s", cradio_get_errorstr());
            exit(EXIT_FAILURE);
        }
    }
    return NULL;
}

bool nrf24_recv_init(void) {
    cradio_address address;

    memcpy(address, config.listen_address, sizeof(cradio_address));

    DEBUG("Initializing nRF24 receiver");

    cradio_set_log_method(nrf24_crazy_log);
    cradio_init();
    radio = cradio_get(0);

    if(!radio) {
        ERROR("could not open device: %s", cradio_get_errorstr());
        return false;
    }

    if(cradio_set_address(radio, &address) ||
       cradio_set_data_rate(radio, DATA_RATE_1MBPS) ||
       cradio_set_channel(radio, 0x4c)) {
        ERROR("error setting up radio: %s", cradio_get_errorstr());
        return false;
    }

    pthread_create(&nrf24_recv_tid, NULL, nrf24_recv_thread, NULL);
    return true;
}

bool nrf24_recv_deinit(void) {
    DEBUG("Tearing down crazyradio receiver");
    radio_quit = 1;
    pthread_join(nrf24_recv_tid, NULL);
    return true;
}
