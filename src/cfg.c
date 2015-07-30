/*
 * cfg.c Copyright (C) 2015 Ron Pedde <ron@pedde.com>
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
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include <libconfig.h>

#include "nrf24-mqtt.h"
#include "debug.h"
#include "cfg.h"

cfg_t config;

static int cfg_hex_digit(const char digit) {
    if(digit >= 'a' && digit <= 'f')
        return digit - 'a' + 10;
    if(digit >= 'A' && digit <= 'F')
        return digit - 'A' + 10;
    if(digit >= '0' && digit <= '9')
        return digit - '0';

    return -1;
}

static uint8_t *cfg_addr_from_string(const char *hex) {
    uint8_t *retval;
    int pos;

    if(strlen(hex) != 10)
        return NULL;

    retval = (uint8_t *)malloc(5);
    if(!retval) {
        perror("malloc");
        return NULL;
    }

    for(pos = 0; pos < 5; pos++) {
        int high = cfg_hex_digit(hex[pos * 2]);
        int low = cfg_hex_digit(hex[pos * 2 + 1]);

        if(high == -1 || low == -1) {
            free(retval);
            return NULL;
        }

        retval[pos] = (uint8_t)((high << 4) | low);
    }

    return retval;
}

int cfg_load(char *file) {
    config_t cfg;
    config_setting_t *setting;
    const char *svalue;
    int ivalue;

    memset(&config, 0, sizeof(config));
    config.mqtt_port = 1883;
    config.mqtt_host = strdup("127.0.0.1");
    config.mqtt_keepalive = 60;

    config_init(&cfg);
    if(!config_read_file(&cfg, file)) {
        ERROR("%s:%d - %s", config_error_file(&cfg),
              config_error_line(&cfg), config_error_text(&cfg));
        config_destroy(&cfg);
        return -1;
    }

    if(config_lookup_string(&cfg, "mqtt_host", &svalue))
        config.mqtt_host = strdup(svalue);

    if(config_lookup_int(&cfg, "mqtt_port", &ivalue))
        config.mqtt_port = (uint16_t)ivalue;

    if(config_lookup_int(&cfg, "mqtt_keepalive", &ivalue))
        config.mqtt_keepalive = (uint16_t)ivalue;

    if(config_lookup_string(&cfg, "listen_address", &svalue)) {
        config.listen_address = cfg_addr_from_string(svalue);
        if(!config.listen_address) {
            ERROR("Invalid listen address: %s", svalue);
            config_destroy(&cfg);
            return -1;
        }
    }

    /* build the map */
    setting = config_lookup(&cfg, "mqtt_map");
    if(setting) {
        addr_map_t *map;

        int count = config_setting_length(setting);
        for(int i = 0; i < count; i++) {
            config_setting_t *entry = config_setting_get_elem(setting, i);
            const char *c_addr, *c_name;

            if(!config_setting_lookup_string(entry, "address", &c_addr)) {
                ERROR("Missing address entry in mqtt_map");
                exit(EXIT_FAILURE);
            }

            if(!config_setting_lookup_string(entry, "name", &c_name)) {
                ERROR("Missing name entry in mqtt_map");
                exit(EXIT_FAILURE);
            }

            map = (addr_map_t *)malloc(sizeof(addr_map_t));
            if(!map) {
                ERROR("Malloc error");
                exit(EXIT_FAILURE);
            }

            map->sensor_name = strdup(c_name);
            map->addr = cfg_addr_from_string(c_addr);

            if(!map->sensor_name) {
                ERROR("Malloc error");
                exit(EXIT_FAILURE);
            }

            if(!map->addr) {
                ERROR("Badly formatted address: %s", c_addr);
                exit(EXIT_FAILURE);
            }
            map->next = config.map.next;
            config.map.next = map;
        }
    }

    config_destroy(&cfg);
    return 0;
}

void cfg_dump(void) {
    addr_map_t *pmap;

    DEBUG("Listen address: 0x%02x%02x%02x%02x%02x",
          config.listen_address[0],
          config.listen_address[1],
          config.listen_address[2],
          config.listen_address[3],
          config.listen_address[4]);
    DEBUG("MQTT Address: %s:%d", config.mqtt_host,
          config.mqtt_port);
    DEBUG("MQTT Keepalive: %d", config.mqtt_keepalive);
    pmap = config.map.next;
    while(pmap) {
        DEBUG("Map 0x%02x%02x%02x%02x%02x -> %s",
              pmap->addr[0],
              pmap->addr[1],
              pmap->addr[2],
              pmap->addr[3],
              pmap->addr[4],
              pmap->sensor_name);
        pmap = pmap->next;
    }
}

const char *cfg_find_map(uint8_t *addr) {
    addr_map_t *pmap;
    pmap = config.map.next;
    while(pmap) {
        if(memcmp(addr, pmap->addr, 5) == 0)
            return pmap->sensor_name;
        pmap = pmap->next;
    }
    return NULL;
}
