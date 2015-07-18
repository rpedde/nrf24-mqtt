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

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>

#include <libconfig.h>

#include "nrf24-mqtt.h"
#include "debug.h"
#include "cfg.h"
#include "mqtt.h"

#define DEFAULT_CONFIG_FILE "/etc/nrf24-mqtt.conf"

void usage(char *a0) {
    fprintf(stderr, "Usage: %s [args]\n\n", a0);
    fprintf(stderr, "Valid args:\n\n");
    fprintf(stderr, " -c <configfile>     config file to load\n");
    fprintf(stderr, " -d                  daemonize\n");
    fprintf(stderr, " -v <level>          verbose level (1-5)\n");
    fprintf(stderr, "\n");
}

int main(int argc, char *argv[]) {
    int opt;
    int daemonize = FALSE;
    int verbose_level = 2;

    char *configfile = DEFAULT_CONFIG_FILE;

    while((opt = getopt(argc, argv, "c:dv:")) != -1) {
        switch(opt) {
        case 'c':
            configfile = optarg;
            break;
        case 'd':
            daemonize = TRUE;
            break;
        case 'v':
            verbose_level = atoi(optarg);
            break;
        default:
            usage(argv[0]);
            exit(EXIT_FAILURE);
            break;
        }
    }

    debug_level(verbose_level);
    DEBUG("Loading config from %s", configfile);

    if(cfg_load(configfile) == -1) {
        ERROR("Error loading config.  Aborting");
        return EXIT_FAILURE;
    }

    cfg_dump();

    DEBUG("Starting mqtt workers");

    mqtt_init();

    int count = 100;
    while(count--) {
        incoming_message_t msg;
        msg.sensor_address.addr[0] = 0xAE;
        msg.sensor_address.addr[1] = 0xAE;
        msg.sensor_address.addr[2] = 0xAE;
        msg.sensor_address.addr[3] = 0xAE;
        msg.sensor_address.addr[4] = 0x00;

        msg.sensor_message.type = SENSOR_TYPE_RO_SWITCH;
        msg.sensor_message.type_instance = 0;
        msg.sensor_message.value.uint8_value = count % 2;

        mqtt_dispatch(&msg);
        sleep(1);
    }

    mqtt_deinit();

    return(EXIT_SUCCESS);
}
