/*
 * sensor.h Copyright (C) 2015 Ron Pedde <ron@pedde.com>
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

#ifndef __SENSOR_H__
#define __SENSOR_H__

#include <stdint.h>

#define SENSOR_TYPE_RO_SWITCH   0  /* uint8: 0 or 1 */
#define SENSOR_TYPE_RW_SWITCH   1  /* uint8: 0 or 1 */
#define SENSOR_TYPE_TEMP        2  /* model specific */
#define SENSOR_TYPE_HUMIDITY    3  /* model specific */
#define SENSOR_TYPE_LIGHT       4  /* uint8 (?) */
#define SENSOR_TYPE_MOTION      5  /* uint8: 0 or 1 */
#define SENSOR_TYPE_VOLTAGE     6  /* float, type_instance 0 is battery */

#define SENSOR_MODEL_NONE       0
#define SENSOR_MODEL_DHT11      1
#define SENSOR_MODEL_DHT22      2
#define SENSOR_MODEL_DS18B20    3
#define SENSOR_MODEL_TMP36      4

#ifndef __AVR__
#pragma pack(push, 1)
#endif
typedef struct {
    uint8_t addr[5];
    uint8_t type;
    uint8_t model;
    uint8_t type_instance;
    union {
        uint8_t uint8_value;
        uint16_t uint16_value;
        float float_value;
    } value;
} sensor_struct_t;
#ifndef __AVR__
#pragma pack(pop)
#endif

#endif /* __SENSOR_H__ */
