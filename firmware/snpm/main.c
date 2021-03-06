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

#include "main.h"

//debug
int cykly = 0;
char cykly_str[10];

int frame_counter = 0;

/* For semihosting on newlib */
//extern void initialise_monitor_handles(void);

//Global variables for burst register reading, bme280
int32_t t_fine;
uint8_t comp_data[26];	//used for readings of compensation data
uint8_t burst_read_data[8] = {0};	//in loop measured data readed into this

// gps global variables
char gps_rx_buffer[255] = {0};
uint8_t gps_rx_buffer_pointer = 0;
char gps_utc_time[10] = {0};
char gps_latitude[9] = {0};
char gps_latitude_ns[1] = {0};
char gps_longitude[10] = {0};
char gps_longitude_ew[1] = {0};
char gps_quality_indicator[1] = {0};
char gps_altitude[8] = {0};
float wgs_latitude = 0.0;
float wgs_longitude = 0.0;
float altitude = 0.0;

// measured values
float temp = 0;
float press = 0;
float hum = 0;
float pm1 = 0;
float pm2_5 = 0;
float pm10 = 0;

//nbiot BC-95 signal data //signal strength, bit error rate
int csq[2] = {0}; //signal strength, bit error rate
int nuestats[14] = {0}; //signal power, total power, tx power, tx time, rx time, cell ID, DL MCS, UL MCS, DCI MCS, ECL, SNR, EARFCN, PCI, RSRQ


// Global variables for compensation functions, bme280:
// temperature
uint16_t dig_T1;
int16_t dig_T2, dig_T3;
// pressure
uint16_t dig_P1;
int16_t dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
// humidity
uint8_t dig_H1, dig_H3;
int16_t dig_H2, dig_H4, dig_H5;
int8_t dig_H6;

// dev_id
char ID[11] = {DEV_ID};
char id_decoded[23]={0};

// Global variables for burst register reading, for OPC-N2
uint8_t histogram_buffer[62];	//whole dataset of opc readed into this
uint8_t pm_values_buffer[12] = {0};	//only pm data





/********************************************************************************************
 *
 * MAIN
 *
 ********************************************************************************************/


int main(void)
{
  clock_setup();
  //systick_setup(250);//250ms tick = 500ms period = 2Hz
//  moje_iwdg_setup();
  gpio_setup();
//  usart_setup();
  i2c_setup();

while(1) {
led_flash(3, 3, 20000);
  //BME280_init();
  atecWake();
  atec();
  wait(2000000);
}
  moje_iwdg_reset();

  wait(SEC*3);
  gpio_set(GPIOD, GPIO2); //SS Log 1
  gpio_clear(GPIOD, GPIO2); //SS Log 0
  gpio_set(GPIOD, GPIO2); //SS Log 1

  debug_usart_send("Welcome to SensoricNet particlemeter");
  #if DEVICE_TYPE == NBIOT
    debug_usart_send("device type is NBIoT");
  #endif
  moje_iwdg_reset();
  particlemeter_power_cycle();
  spi_setup();
  debug_usart_send("tady");
  #if BME280 == 1
    BME280_init();
  #endif
  led_flash(1, 3, 20000);
  nbiot_reset();
  led_flash(1, 3, 200000);
	// semihosting - stdio po debug konzoli, inicializace
	/*
	#if defined(ENABLE_SEMIHOSTING) && (ENABLE_SEMIHOSTING)
		initialise_monitor_handles();
		setbuf(stdout, NULL);
	#endif
	*/
  struct CayenneLPP *lpp;
  unsigned char *buf;
  int size;

  usart_disable_rx_interrupt(USART2);
  usart_disable(USART2);

  //Connect to nbiot network
  #if DEVICE_TYPE == NBIOT
    wait(SEC*1); //until quectel wakes up
    //usartSend("test uvnitr\r\n", 2);
    debug_usart_send("NBIoT site connect");
    moje_iwdg_reset();
    nbiot_connect();
    led_flash(1, 3, 20000);
    moje_iwdg_reset();
    send_reboot_variable();
  #endif

  //Connect to lora network
  #if DEVICE_TYPE == LORAWAN
    debug_usart_send("rn2483 reset");
    lorawan_reset();
    debug_usart_send("lora connect");
    lorawan_connect();
    debug_usart_send("lora connected");
  #endif

  #if PARTICLEMETER == 1
    moje_iwdg_reset();
    debug_usart_send("PM switching on");
    particlemeter_ON();
    particlemeter_set_fan(FAN_SPEED);
    wait(SEC * 1);
    debug_usart_send("PM switched on");
    read_pm_values();
    wait(SEC *5);
    debug_usart_send("PM cleared");
  #endif
  moje_iwdg_reset();

	// init cayenne lpp
  lpp = CayenneLPP__create(255);

  //flash(3, 100000);
  while (1) {
    debug_usart_send("New loop");
    moje_iwdg_reset();

    //vycitani statistik site a ukladani do poli csq a nuestats
    while(nbiot_csq())
      wait(SEC*2);
    while(nbiot_nuestats())
      wait(SEC*2);

    // readout particlemeter data
    #if PARTICLEMETER == 1
      read_pm_values();
    #endif

    #if BME280 == 1
      // readout BME data
      BME280_data_readout(burst_read_data);
    #endif
    // debug gps
    char tmp_string[200] = {0};
    int w;

    for (w=0; w<100; w++) {
      if (gps_rx_buffer[w] != '\n') {
        tmp_string[w] = gps_rx_buffer[w];
      }
    }
    debug_usart_send(tmp_string);

    char pointer_debug_string[80] = {0};
    sprintf(pointer_debug_string, "pointer %d", gps_rx_buffer_pointer);
    debug_usart_send(pointer_debug_string);

    sprintf(pointer_debug_string, "time: %s, latitude: %s, longitude: %s, altitude: %s, fix: %s", gps_utc_time, gps_latitude, gps_longitude, gps_altitude, gps_quality_indicator);
    debug_usart_send(pointer_debug_string);

    // testovaci hodnoty natvrdo, jinak gps_latitude, gps_longitude
//			wgs_latitude = convert_gps_to_wgs84_latitude("5133.82");
//			wgs_longitude = convert_gps_to_wgs84_longitude("00042.24");
    wgs_latitude = convert_gps_to_wgs84_latitude(gps_latitude);
    wgs_longitude = convert_gps_to_wgs84_longitude(gps_longitude);
    // atof neni bezpecne, TODO
    altitude = atof(gps_altitude);

    if (gps_quality_indicator[0] != '0') {
      sprintf(pointer_debug_string, "wgs latitude: %f, wgs longitude: %f, altitude: %f", wgs_latitude, wgs_longitude, altitude);
      debug_usart_send(pointer_debug_string);
    }

    temp = BME280_temp();
    press = BME280_press();
    hum = BME280_hum();
    #if PARTICLEMETER == 1
      pm1 = particlemeter_pm1();
      pm2_5 = particlemeter_pm2_5();
      pm10 = particlemeter_pm10();
    #endif

    debug_usart_send("Encode values");
    char debug_data_string[150] = {0};
    sprintf(debug_data_string, "hum: %.2f, temp: %.2f, press: %.2f, pm1: %.2f, pm2_5: %.2f, pm10: %.2f", hum, temp, press, pm1, pm2_5, pm10);
    debug_usart_send(debug_data_string);
		
    CayenneLPP__addTemperature(lpp, 1, temp-2.1); //FIXME PRASARNA
    CayenneLPP__addBarometricPressure(lpp, 2, press);
    CayenneLPP__addRelativeHumidity(lpp, 3, hum);
    #if PARTICLEMETER == 1
      CayenneLPP__addAnalogInput(lpp, 4, pm1);
      CayenneLPP__addAnalogInput(lpp, 5, pm2_5);
      CayenneLPP__addAnalogInput(lpp, 6, pm10);
    #endif

		// jsou udaje gps validni, pripadne zmenily se od minula? TODO
    if (gps_quality_indicator[0] != '0') {
      CayenneLPP__addGPS(lpp, 7, wgs_latitude, wgs_longitude, altitude);
    }
		
		//statistiky site

    CayenneLPP__addDigitalInput(lpp, 8, csq[0]);//signal strength - jeste se da prepocitat podle datasheetu
    CayenneLPP__addDigitalInput(lpp, 9, csq[1]);//channel bit error rate
    CayenneLPP__addAnalogInput(lpp, 10, nuestats[0]);//signal power
    CayenneLPP__addAnalogInput(lpp, 11, nuestats[1]);//total power
    CayenneLPP__addAnalogInput(lpp, 12, nuestats[2]);//tx power
    CayenneLPP__addAnalogInput(lpp, 13, nuestats[3]);//tx time
    CayenneLPP__addAnalogInput(lpp, 14, nuestats[4]);//rx time
								//cell id
    CayenneLPP__addAnalogInput(lpp, 11, nuestats[6]);//DL MCS
    CayenneLPP__addAnalogInput(lpp, 11, nuestats[7]);//UL MCS
    CayenneLPP__addAnalogInput(lpp, 11, nuestats[8]);//DCI MCS
    CayenneLPP__addAnalogInput(lpp, 11, nuestats[9]);//ECL
    CayenneLPP__addAnalogInput(lpp, 11, nuestats[10]);//SNR
    CayenneLPP__addAnalogInput(lpp, 11, nuestats[11]);//EARFCN
    CayenneLPP__addAnalogInput(lpp, 11, nuestats[12]);//PCI
    CayenneLPP__addAnalogInput(lpp, 11, nuestats[13]);//RSRQ

    buf=CayenneLPP__getBuffer(lpp);
    size=CayenneLPP__getSize(lpp);

		// Send it off
		//sendData(CayenneLPP__getBuffer(lpp), CayenneLPP__getSize(lpp));
		//printf("Encoded data size: %i\n", size);

    char hex_string[400] = {0};		//encoded string here
    char send_string[400] = {0};	//string to send here
		
		//ascii-hex encoding is happening here:
    int j = 0;
    char znak[3];
		
    while(j<size){
      charToHex(buf[j], znak);
      strcat(hex_string, znak);
      j++;
    }
		

    #if DEVICE_TYPE == LORAWAN
    //sestaveni stringu pro LORAWAN
    strcat(send_string, "mac tx uncnf 1 ");
    strcat(send_string, hex_string);

    //odeslani stringu, checkuje "ok", pokud nedostane ok tak to zkusi za chvili znova
    while(lorawan_sendCommand(send_string, "ok", 1)) {
      // TODO odstupnovat, nekolik pokusu, reset (nebo aspon connect)
      wait(SEC*3);
    }

    frame_counter++;

		// jednou za 8 frejmu uloz frame counter
    if ((frame_counter & 0x07) == 0) {
      lorawan_mac_save();
    }
    #endif

		
    #if DEVICE_TYPE == NBIOT
    //tady se konvertuje na string a ulozi delka payloadu (int)

    char nbiot_data_length[10];
    sprintf (nbiot_data_length, "%d", size+(11));

    //sestaveni stringu pro Nb-IOT
    strcat(send_string, "AT+NSOST=0,193.84.207.60,9999,");
    strcat(send_string, nbiot_data_length);
    strcat(send_string, ",");

    ////experimental, eeprom string decoding
    j=0;
    while(j<10){
      charToHex(ID[j], znak);
      strcat(id_decoded, znak);
      j++;
    }
    id_decoded[20] = '0';
    id_decoded[21] = '0';
    id_decoded[22] = 0x00; //=NULL

    usartSend(id_decoded, 2);

    strcat(send_string, id_decoded);	//nbiot-0001
    //strcat(send_string, "00");	//nbiot-0001
    strcat(send_string, hex_string);
    strcat(send_string, "\r\n");
		
    //socket opening
    while (nbiot_sendCommand("AT+NSOCR=DGRAM,17,9999,1\r\n", "OK\r\n", 4))
      wait(SEC*1);
    //Sending datagram
    usartSend(send_string, 2);
    while (nbiot_sendCommand(send_string, "OK", 4))
      wait(SEC*3);
    //Closing socket
    while (nbiot_sendCommand("AT+NSOCL=0\r\n", "OK", 2))
      wait(SEC*1);
    #endif

		
    CayenneLPP__reset(lpp);
    //lpp->cursor = NULL;
		

    char loop_debug_string[20] = {0};
    sprintf(loop_debug_string, "Loop %d done", cykly);
    debug_usart_send(loop_debug_string);

    cykly++;

    flash(2, 100000);

		/*
//pm zeros restart check here?
		*/
    moje_iwdg_reset();
    usart_enable_rx_interrupt(USART2);
    usart_enable(USART2);
		
    wait(SEC *14);
    usart_disable_rx_interrupt(USART2);
    usart_disable(USART2);
		
    }
  return 0;
}


