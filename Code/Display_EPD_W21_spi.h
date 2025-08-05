#ifndef _DISPLAY_EPD_W21_SPI_
#define _DISPLAY_EPD_W21_SPI_
#include "Arduino.h"

//#define  Arduino_UNO
#ifndef ESP8266
#define ESP8266
#endif

#ifdef ESP8266
//IO settings
//HSCLK---D5
//HMOSI--D7
#define isEPD_W21_BUSY digitalRead(2) 
#define EPD_W21_RST_0 digitalWrite(14,LOW)
#define EPD_W21_RST_1 digitalWrite(14,HIGH)
#define EPD_W21_DC_0  digitalWrite(6,LOW)
#define EPD_W21_DC_1  digitalWrite(6,HIGH)
#define EPD_W21_CS_0 digitalWrite(1,LOW)
#define EPD_W21_CS_1 digitalWrite(1,HIGH)

void Sys_run(void);
void LED_run(void);    
#endif 

#ifdef Arduino_UNO
//IO settings
//HSCLK---13
//HMOSI--11
#define isEPD_W21_BUSY digitalRead(4) 
#define EPD_W21_RST_0 digitalWrite(5,LOW)
#define EPD_W21_RST_1 digitalWrite(5,HIGH)
#define EPD_W21_DC_0  digitalWrite(6,LOW)
#define EPD_W21_DC_1  digitalWrite(6,HIGH)
#define EPD_W21_CS_0 digitalWrite(7,LOW)
#define EPD_W21_CS_1 digitalWrite(7,HIGH)
#endif 

void SPI_Write(unsigned char value);
void EPD_W21_WriteDATA(unsigned char datas);
void EPD_W21_WriteCMD(unsigned char command);

#endif 
