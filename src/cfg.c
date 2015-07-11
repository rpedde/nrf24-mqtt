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

static sensor_address_t *cfg_addr_from_string(const char *hex) {
    sensor_address_t *retval;
    int pos;

    if(strlen(hex) != 10)
        return NULL;

    retval = (sensor_address_t *)malloc(sizeof(sensor_address_t));
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

        retval->addr[pos] = (uint8_t)((high << 4) | low);
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

    config_destroy(&cfg);
    return 0;
}

void cfg_dump(void) {
    DEBUG("Listen address: 0x%02x%02x%02x%02x%02x",
          config.listen_address->addr[0],
          config.listen_address->addr[1],
          config.listen_address->addr[2],
          config.listen_address->addr[3],
          config.listen_address->addr[4]);
    DEBUG("MQTT Address: %s:%d", config.mqtt_host,
          config.mqtt_port);
    DEBUG("MQTT Keepalive: %d", config.mqtt_keepalive);
}
