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
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <mosquitto.h>

#include "nrf24-mqtt.h"
#include "debug.h"
#include "cfg.h"

struct mosquitto *mosq;

char *mqtt_type_lookup[] = {
    "switch",
    "switch",
    "temp",
    "humidity",
    "light",
    "motion",
    "voltage"
};

void mqtt_dump_message(sensor_struct_t *pmsg) {
    char *type;
    uint8_t *ptr;

    DEBUG("Address:      %02x%02x%02x%02x%02x",
          pmsg->addr[0],
          pmsg->addr[1],
          pmsg->addr[2],
          pmsg->addr[3],
          pmsg->addr[4]);
    if(pmsg->type > (sizeof(mqtt_type_lookup) / sizeof(char*))) {
        type = "unknown";
    } else {
        type = mqtt_type_lookup[pmsg->type];
    }
    DEBUG("Type:         %s", type);
    DEBUG("Instance:     %d", pmsg->type_instance);
    DEBUG("Model:        %d", pmsg->model);
    switch(pmsg->type) {
    case SENSOR_TYPE_RO_SWITCH:
    case SENSOR_TYPE_RW_SWITCH:
    case SENSOR_TYPE_LIGHT:
    case SENSOR_TYPE_MOTION:
        DEBUG("Value:        %d", pmsg->value.uint8_value);
        break;
    case SENSOR_TYPE_TEMP:
    case SENSOR_TYPE_HUMIDITY:
        ptr = &pmsg->value.uint8_value;
        DEBUG("Value:        %d (%02x %02x)", pmsg->value.uint16_value, ptr[0], ptr[1]);
        break;
    case SENSOR_TYPE_VOLTAGE:
        DEBUG("Value:        %f", pmsg->value.float_value);
        break;
    default:
        DEBUG("Value:        unknown");
        break;
    }
}


bool mqtt_init(void) {
    int rc;

    DEBUG("Initializing mosquitto lib");
    mosquitto_lib_init();
    mosq = mosquitto_new(NULL, true, NULL);
    rc = mosquitto_connect(mosq, config.mqtt_host,
                           config.mqtt_port, config.mqtt_keepalive);
    if(rc !=- MOSQ_ERR_SUCCESS) {
        ERROR("Cannot connect to mosquito server");
        exit(1);
    }

    mosquitto_loop_start(mosq);
    return true;
}

bool mqtt_deinit(void) {
    DEBUG("Tearing down mosquitto");
    mosquitto_loop_stop(mosq, true);
    mosquitto_lib_cleanup();
    return true;
}

bool mqtt_dispatch(sensor_struct_t *pmsg) {
    const char *sensor_name;
    char *topic;
    char *value;
    int rc;

    DEBUG("Got work item");

    mqtt_dump_message(pmsg);

    sensor_name = cfg_find_map(pmsg->addr);

    if(!sensor_name) {
        WARN("Got message from unknown sensor: %s", sensor_name);
        return true;
    }

    if(pmsg->type > (sizeof(mqtt_type_lookup) / sizeof(char*))) {
        WARN("Unknown sensor type: %d from %s",
             pmsg->type, sensor_name);
        return true;
    }

    asprintf(&topic, "%s/%s%d", sensor_name,
             mqtt_type_lookup[pmsg->type],
             pmsg->type_instance);

    switch(pmsg->type) {
    case SENSOR_TYPE_RO_SWITCH:
    case SENSOR_TYPE_RW_SWITCH:
    case SENSOR_TYPE_LIGHT:
    case SENSOR_TYPE_MOTION:
        asprintf(&value, "%d", pmsg->value.uint8_value);
        break;
    case SENSOR_TYPE_VOLTAGE:
        asprintf(&value, "%f", pmsg->value.float_value);
        break;
    case SENSOR_TYPE_TEMP:
        if (pmsg->model == SENSOR_MODEL_DHT11) {
            asprintf(&value, "%d.%d", (pmsg->value.uint16_value >> 8),
                     pmsg->value.uint16_value & 0xFF00);
        } else if (pmsg->model == SENSOR_MODEL_DHT22) {
            float t_float=0.0;

            if(pmsg->value.uint16_value & 0x8000) {
                t_float = -(pmsg->value.uint16_value & 0x7fff);
            } else {
                t_float = pmsg->value.uint16_value;
            }

            t_float = (t_float / 10.0) * 1.8 + 32.0;

            asprintf(&value, "%02.1f", t_float);
        } else {
            ERROR("Unhandled temp model: %d", pmsg->model);
        }
        break;
    case SENSOR_TYPE_HUMIDITY:
        if (pmsg->model == SENSOR_MODEL_DHT11) {
            asprintf(&value, "%d.%d", (pmsg->value.uint16_value >> 8),
                     pmsg->value.uint16_value & 0xFF00);
        } else if (pmsg->model == SENSOR_MODEL_DHT22) {
            asprintf(&value, "%0.1f", pmsg->value.uint16_value / 10.0);
        } else {
            ERROR("Unhandled temp model: %d", pmsg->model);
        }
        break;
    default:
        ERROR("Unhandled sensor type: %d", pmsg->type);
        free(topic);
        return true;
    }

    /* send the message */
    DEBUG("Sending message %s -> %s", topic, value);

    rc = mosquitto_publish(mosq, NULL, topic, strlen(value), value, 0, true);
    if (rc != MOSQ_ERR_SUCCESS)
        ERROR("Got mosquitto error: %d", rc);

    free(topic);
    free(value);

    return true;
}
