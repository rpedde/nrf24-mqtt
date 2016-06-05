/*
 * nrf24-recv.h Copyright (C) 2015 Ron Pedde <ron@pedde.com>
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

#ifndef _NRF24_RECV_H_
#define _NRF24_RECV_H_

#include <stdbool.h>

extern bool nrf24_recv_init(void);
extern bool nrf24_recv_deinit(void);

#endif /* _NRF24_RECV_H_ */
