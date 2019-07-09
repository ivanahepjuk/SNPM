/*
 * This file is part of the SenoricNet project, https://sensoricnet.cz
 *
 * Copyright (C) 2017 Pavel Polach <ivanahepjuk@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BME280_H
#define BME_280_H

#include <stdint.h>

#define BME 0x76
#define ATEC 0xC0

void atecWake(void);
void atec(void);


extern int32_t t_fine;
extern uint8_t comp_data[26]; //compensation data readed into this
extern uint8_t burst_read_data[8]; //burst read data readed into this

//compensation constants
//temperature
extern uint16_t dig_T1;
extern int16_t dig_T2, dig_T3;
//pressure
extern uint16_t dig_P1;
extern int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
//humidity
extern uint8_t dig_H1, dig_H3;
extern int16_t dig_H2, dig_H4, dig_H5;
extern int8_t dig_H6;

//Functions
void  BME280_init(void);
float BME280_press(void);
float BME280_hum(void);
float BME280_temp(void);
void  BME280_data_readout(uint8_t array[]);
void  BME280_compensation_data_readout(uint8_t arrayy[]);


#endif
