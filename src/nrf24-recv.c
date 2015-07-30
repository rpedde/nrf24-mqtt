/*
 * nrf24-recv.c Copyright (C) 2015 Ron Pedde <ron@pedde.com>
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

#include <gpio.h>
#include <rf24.h>

#include "nrf24-mqtt.h"
#include "debug.h"
#include "sensor.h"
#include "cfg.h"
#include "mqtt.h"

static rf24_t radio;
static pthread_t nrf24_recv_tid;

static void nrf24_recv_dispatch(void *data) {
    uint8_t len;
    char buf[32];

    rf24_sync_status(&radio);

    DEBUG("IRQ on pin %d. TX ok: %d, TX fail: %d RX ready: %d RX_LEN: %d PIPE: %d\n",
          radio.irq_pin,
          radio.status.tx_ok,
          radio.status.tx_fail_retries,
          radio.status.rx_data_available,
          radio.status.rx_data_len,
          radio.status.rx_data_pipe);

    if(radio.status.rx_data_available) {
        len = radio.status.rx_data_len;
        DEBUG("Got %d bytes of data", len);

        rf24_receive(&radio, &buf, len);
        usleep(20);
        rf24_reset_status(&radio);

        /* push the packet to mqtt */
        mqtt_dispatch((sensor_struct_t *)&buf);

        rf24_stop_listening(&radio);
        usleep(20);
        rf24_start_listening(&radio);
    } else {
        DEBUG("IRQ with no data read.  Resetting");
        rf24_stop_listening(&radio);
        rf24_reset_status(&radio);
        usleep(20);
        rf24_start_listening(&radio);
    }

    DEBUG("Dispatch complete");
}

static void *nrf24_recv_thread(void *data) {
    int result;

    DEBUG("nrf24 recv thread started");

    result = rf24_irq_poll(&radio, &nrf24_recv_dispatch);
    if(result == -1) {
        ERROR("nrf24 irq polling error: %s", strerror(errno));
    }
    exit(EXIT_FAILURE);
}

bool nrf24_recv_init(void) {
    uint64_t address;

    address = config.listen_address[0];
    address = (address << 8) | config.listen_address[1];
    address = (address << 8) | config.listen_address[2];
    address = (address << 8) | config.listen_address[3];
    address = (address << 8) | config.listen_address[4];

    DEBUG("Initializing nRF24 receiver");

    rf24_initialize(&radio, RF24_SPI_DEV_0, 25, 24);
    rf24_set_retries(&radio, 0, 0);
    rf24_set_autoack(&radio, 0);
    rf24_set_data_rate(&radio, RF24_1MBPS);
    rf24_set_payload_size(&radio, sizeof(sensor_struct_t));
    rf24_open_reading_pipe(&radio, 0, address);

    rf24_dump(&radio);
    rf24_start_listening(&radio);

    pthread_create(&nrf24_recv_tid, NULL, nrf24_recv_thread, NULL);
    return true;
}

bool nrf24_recv_deinit(void) {
    DEBUG("Tearing down nRF receiver");
    gpio_unexport(radio.irq_pin);
    pthread_join(nrf24_recv_tid, NULL);
    return true;
}
