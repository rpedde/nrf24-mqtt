/*
 * nrf24-mqtt Copyright (C) 2015 Ron Pedde <ron@pedde.com>
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

#ifndef _NRF24_MQTT_H_
#define _NRF24_MQTT_H_

#include <stdint.h>
#include "sensor.h"

#define TRUE 1
#define FALSE 0

typedef struct sensor_address_t {
    uint8_t addr[5];
} sensor_address_t;

typedef struct incoming_message_t {
    sensor_address_t sensor_address;
    sensor_struct_t sensor_message;
} incoming_message_t;

typedef struct addr_map_t {
    sensor_address_t *addr;
    char *sensor_name;
    struct addr_map_t *next;
} addr_map_t;

#endif /* _NRF24_MQTT_H_ */
