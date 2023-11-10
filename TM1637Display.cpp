#include <util/delay.h>
#include <avr/io.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "TM1637Display.h"

#define TM1637_I2C_COMM1    0x40
#define TM1637_I2C_COMM2    0xC0
#define TM1637_I2C_COMM3    0x80

#define LOW  0
#define HIGH 1
#define OUTPUT 0
#define INPUT  1

#define PINMODE_INPUT(pin) LCDDDR&=(~(1 << pin));
#define PINMODE_OUTPUT(pin) LCDDDR |= (1 << pin);
#define PINWRITE_HI(pin) LCDPORT |= (1 << pin);
#define PINWRITE_LO(pin) LCDPORT &= (~(1 << pin));
#define BITDELAY _delay_us(11);

//
//      A
//     ---
//  F |   | B
//     -G-
//  E |   | C
//     ---
//      D
const uint8_t digitToSegment[] = {
 // XGFEDCBA
  0b00111111,    // 0
  0b00000110,    // 1
  0b01011011,    // 2
  0b01001111,    // 3
  0b01100110,    // 4
  0b01101101,    // 5
  0b01111101,    // 6
  0b00000111,    // 7
  0b01111111,    // 8
  0b01101111,    // 9
  0b01110111,    // A
  0b01111100,    // B
  0b00111001,    // C
  0b01000111,    // D
  0b01111001,    // E
  0b01110001     // F
  };

void TM1637Display()
{
    PINMODE_INPUT(pinClk)
    PINMODE_INPUT(pinDIO)
	PINWRITE_LO(pinClk)
	PINWRITE_LO(pinDIO)
}

void setSegments(const uint8_t segments[], uint8_t length, uint8_t pos)
{
    // Write COMM1
	start();
	writeByte(TM1637_I2C_COMM1);
	stop();
	
	// Write COMM2 + first digit address
	start();
	writeByte(TM1637_I2C_COMM2 + (pos & 0x03));
	
	// Write the data bytes
	for (uint8_t k=0; k < length; k++) 
	  writeByte(segments[k]);
	  
	stop();

	// Write COMM3 + brightness
	start();
	writeByte(TM1637_I2C_COMM3 + 8); //minimal brightness
	//writeByte(TM1637_I2C_COMM3 + (m_brightness & 0x0f));
	stop();
}

void showNumber(uint16_t number)
{
    uint16_t R = number;
    uint8_t digit;
    uint8_t prenos = 0;  //this is for sign, that there was previously some digit set, so we have to put 0 instead of spaces
    uint8_t cisla[4] = {0, 0, 0, 0};  //fill with spaces
    
    if (R >= 1000)
	{
        digit = R / 1000;
		cisla[0] = digitToSegment[digit];
		R = R - (digit * 1000);
        prenos = 1;
	}
	
	if (R >= 100)
	{
        digit = R / 100;
		cisla[1] = digitToSegment[digit];
		R = R - (digit * 100);
        prenos = 1;
	}
    else if (prenos == 1)
    {
        cisla[1] = 0b00111111;  //digitToSegment[0];    
    }
	
	if (R >= 10)
	{
        digit = R / 10;
		cisla[2] = digitToSegment[digit];
		R = R - (digit * 10);
	}
    else if (prenos == 1)
    {
        cisla[2] = 0b00111111;  //digitToSegment[0];    
    }

	cisla[3] = digitToSegment[R];
    
    setSegments(cisla,4,0);
}

void showAddr(uint8_t addr)
{
    uint8_t cisla[4] = {0b01110111, 0, 0, 0};  //A,space,space,space
    uint8_t prenos = 0;
    
	if (addr >= 100)
	{
		cisla[1] = 0b00000110; //digitToSegment[1];
		addr -= 100;
        prenos = 1;
	}
    
	if (addr >= 10)
	{
        prenos = addr / 10;
		cisla[2] = digitToSegment[prenos];
		addr = addr - (prenos * 10);
	}
    else if (prenos == 1)
    {
        cisla[2] = 0b00111111;  //digitToSegment[0];    
    }    
    
    cisla[3] = digitToSegment[addr];
    
    setSegments(cisla,4,0);
}

void showUnderline(uint16_t num, uint8_t pos)
{
	 showNumber(num);
	 //only D segment lit up on POS digit
     uint8_t underline = 0b00001000;
     setSegments(&underline,1,3-pos);
}

void start(void) 
{ 
	PINMODE_OUTPUT(pinDIO)
	BITDELAY
} 
  
void stop(void) 
{ 
	PINMODE_OUTPUT(pinDIO)
	BITDELAY
	PINMODE_INPUT(pinClk)
	BITDELAY
	PINMODE_INPUT(pinDIO)
	BITDELAY
} 

void writeByte(uint8_t b)
{
	uint8_t bb = b;
	
    for(uint8_t i = 0; i < 8; i++) 
    { 
        PINMODE_OUTPUT(pinClk)
        BITDELAY
        if ( (bb & 0x01) == 1) {PINMODE_INPUT(pinDIO);} else {PINMODE_OUTPUT(pinDIO);}
        BITDELAY 
        PINMODE_INPUT(pinClk) 
    	BITDELAY
    	bb = (bb >> 1);
    } 
  
    // wait for ACK 
    PINMODE_OUTPUT(pinClk)
    PINMODE_INPUT(pinDIO)
    BITDELAY 
  
    PINMODE_INPUT(pinClk) 
    BITDELAY
  
    uint8_t ack = (LCDPIN & pinDIO) == 0; 
    if (ack == 0) PINMODE_OUTPUT(pinDIO) 
 	BITDELAY
    PINMODE_OUTPUT(pinClk)
    BITDELAY
}  

inline uint8_t encodeDigit(uint8_t digit)
{
	return digitToSegment[digit & 0x0f];
}
   

