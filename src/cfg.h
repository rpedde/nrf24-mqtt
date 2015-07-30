/*
 * cfg.h Copyright (C) 2015 Ron Pedde <ron@pedde.com>
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

#ifndef _CFG_H_
#define _CFG_H_

#include <stdint.h>

#include "nrf24-mqtt.h"

typedef struct cfg_t {
    char *mqtt_host;
    uint16_t mqtt_port;
    uint16_t mqtt_keepalive;

    uint8_t *listen_address;

    addr_map_t map;
} cfg_t;

extern cfg_t config;

extern int cfg_load(char *file);
extern void cfg_dump(void);
extern const char *cfg_find_map(uint8_t *addr);

#endif /* _CFG_H_ */
