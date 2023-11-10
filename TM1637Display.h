#ifndef __TM1637DISPLAY__
#define __TM1637DISPLAY__

#include <inttypes.h>

#define SEG_A   0b00000001
#define SEG_B   0b00000010
#define SEG_C   0b00000100
#define SEG_D   0b00001000
#define SEG_E   0b00010000
#define SEG_F   0b00100000
#define SEG_G   0b01000000

#define LCDPORT PORTC
#define LCDDDR  DDRC
#define LCDPIN  PINC
#define pinClk  PC3
#define pinDIO  PC4

   void TM1637Display();
   //! @param pos The position from which to start the modification (0 - leftmost, 3 - rightmost)
   void setSegments(const uint8_t segments[], uint8_t length, uint8_t pos);  
   uint8_t encodeDigit(uint8_t digit);
  
   void showNumber(uint16_t number);
   void showAddr(uint8_t addr);
   void showUnderline(uint16_t num, uint8_t pos); //0 = right side, 3 = left side
     
   void start();
   void stop();
   void writeByte(uint8_t b);
   
#endif // __TM1637DISPLAY__
