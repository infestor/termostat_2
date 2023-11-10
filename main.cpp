#include <avr/io.h>
#include <avr/interrupt.h> 
#include <stdint.h>
#include <inttypes.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Mirf.h"
#include "Mirf_nRF24L01.h"
#include "TM1637Display.h"
#include "onewire.h"
#include "ds18x20.h"

//DEVICE definition
#define DEV_ADDR 5 //1 is master, so it is not possible
#define NUM_SENSORS 3
#define SENSOR_0_TYPE 4 //dallas 18b20 temp sensor (podlaha)
#define SENSOR_1_TYPE 4 //dallas 18b20 temp sensor (vzduch)
#define SENSOR_2_TYPE 0 //on-off output topeni
#define SENSOR_3_TYPE 1 //teplota pozadovana

mirfPacket volatile inPacket;
mirfPacket volatile outPacket;

#define ZNAK_PODLAHA 0b00011000
#define ZNAK_SETPOINT 0b00100001
#define ZNAK_VZDUCH 0b00001001

#define NOP __asm__("nop\n\t");
#define REF_VCC_INPUT_INTERNAL _BV(REFS0)  | (_BV(MUX3) | _BV(MUX2) | _BV(MUX1))
#define OW_ARGS_PODLAHA &PINC, &PORTC, &DDRC, PC2
#define OW_ARGS_VZDUCH &PINC, &PORTC, &DDRC, PC5

#define LED_OVERHEAT PD0
#define LED_TOPENI PD1
#define SWITCH_LED(pin) PORTD ^= (1 << pin);
#define LED_ON(pin) PORTD &= ~(1 << pin);
#define LED_OFF(pin) PORTD |= (1 << pin);

volatile uint8_t timer10msTriggered;
volatile uint16_t longTimer;

typedef union {
	uint16_t uint;
	struct {
		uint8_t lsb;
		uint8_t msb;
	};
} IntUnion;

#define TEPLOTA_OVERHEAT 270 //v desetinach stupne
#define HISTEREZE 5
volatile IntUnion *selected_ow_data;
volatile IntUnion teplota_podlaha;
volatile IntUnion teplota_vzduch;
volatile uint8_t vybrana_sbernice_teplota;
volatile uint8_t pozadovana_teplota = 250;

volatile uint8_t pozadavek_topeni;

enum SBERNICE_TEPLOTY {PODLAHA = 0, VZDUCH = 1};
enum STAVY_TOPENI {STAV_OFF = 0, STAV_ON = 1, STAV_OVERHEAT = 2};
enum STAVY_MERENI_TEPLOT {NEMERI_SE = 0, ZAHAJIT_MERENI_PODLAHA = 1, ZAHAJIT_MERENI_VZDUCH = 2, MERENI_PROBIHA = 3};

struct main_loop_prikazy_t {
	uint8_t prepocitat_topeni : 1;
	uint8_t mereni_teploty : 2; //STAVY_MERENI_TEPLOT, maximalni hodnota 3
	uint8_t prekreslit_display : 1;
	uint8_t display_ukazuje : 1; //SBERNICE_TEPLOTY (jen 1 nebo 0)

	//inicializacni konstruktor nakonec neni potreba u definice staci dat {}
	//main_loop_prikazy_t() : prepocitat_topeni(0), mereni_teploty(0), prekreslit_display(0), display_ukazuje(0) {};
};

void main() __attribute__ ((noreturn));


//-----------------------------------------------------------
ISR(TIMER0_COMPA_vect) {
	timer10msTriggered++;
	longTimer++;
}

ISR(BADISR_vect) { //just for case
	NOP
}

//-----------------------------------------------------------
void PrepniOwPodlaha(void)
{
	selected_ow_data = &teplota_podlaha;
	vybrana_sbernice_teplota = (SBERNICE_TEPLOTY)PODLAHA;
	ow_set_bus(OW_ARGS_PODLAHA); //OW PODLAHA, reset uz je uvnitr
}

void PrepniOwVzduch(void)
{
	selected_ow_data = &teplota_vzduch;
	vybrana_sbernice_teplota = (SBERNICE_TEPLOTY)VZDUCH;
	ow_set_bus(OW_ARGS_VZDUCH); //OW PODLAHA, reset uz je uvnitr
}

inline void DS1820StartConversion(void)
{
	DS18X20_start_meas( DS18X20_POWER_EXTERN, NULL );
}

inline void DS1820WaitForEndConversion_loop(void)
{
	while (DS18X20_conversion_in_progress() == DS18X20_CONVERTING) NOP
}

void DS1820ReadConversionResult(void)
{
	ow_command_with_parasite_enable( DS18X20_READ, NULL );

	//read 16bit value into uint16
	selected_ow_data->lsb = ow_byte_rd();
	selected_ow_data->msb = ow_byte_rd();

	//do not read rest of bytes from sensor, just reset the line
	ow_reset();

	//konvertujeme na cislo bez desetinne tecky s rozlisenim na desetiny (treba 22.25 bude 222)
	//predpokladame hodnotu teploty vetsi nez 0C
	selected_ow_data->uint = (((selected_ow_data->uint >> 2) * 25) / 10);
}

void ReadDS1820(void)
{
	//read temperature from DS1820 and store it to memory
	DS1820StartConversion();
	DS1820WaitForEndConversion_loop();
	DS1820ReadConversionResult();
}
//-----------------------------------------------------------

//-----------------------------------------------------------

void init(void)
{
	TM1637Display();
	const uint8_t buf[] = {0b01110111, 0b01110110, 0b00111111, 0b00011110}; //AHOJ
	setSegments(buf, 4, 0);

	//timer0 10ms period, interrupt enable
	//prescaler 1024, count to 156
	OCR0A = 156;
	OCR0B = 170;
	TCCR0A = 2;
	TCCR0B = 5;
	TIMSK0 = 2;

	//musime vypnout UART aby se daly ovladat jeho diody
	//!! Ale musi se to vypnout uz tady. kdyz se to udela nize tak to nejak nefunguje...
	UCSR0B &= ~((1 << RXEN0) | (1 << TXEN0));

	//set ADC to read temp from internal sensor, 1.1V reference, prescaler 128
	//ADMUX = REF_VCC_INPUT_INTERNAL;
	//ADCSRA = (_BV(ADEN) | _BV(ADIE) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0) );  // enable the ADC

	//disable unused peripherials
	ACSR |= _BV(ACD); //disable comparator
	PRR = ( _BV(PRTWI) | _BV(PRTIM1) | _BV(PRTIM2) | _BV(PRUSART0) ) ;

	//start Radio
	Mirf.init();
	Mirf.config();
	Mirf.setDevAddr(DEV_ADDR);
	Mirf.powerUpRx();

	//set resolution 0.25C for DS18B20 (write just to scratchpad, not to eeprom)
	PrepniOwVzduch();
	ow_command(DS18X20_WRITE, NULL);
	ow_byte_wr(0xFF); //1st byte - unused (register Tl)
	ow_byte_wr(0xFF); //2nd byte - unused (register Th)
	ow_byte_wr(0x3F); //3rd byte - resolution 10bits
	ow_reset();
	ReadDS1820();

	//set resolution 0.25C for DS18B20 (write just to scratchpad, not to eeprom)
	PrepniOwPodlaha();
	ow_command(DS18X20_WRITE, NULL);
	ow_byte_wr(0xFF); //1st byte - unused (register Tl)
	ow_byte_wr(0xFF); //2nd byte - unused (register Th)
	ow_byte_wr(0x3F); //3rd byte - resolution 10bits
	ow_reset();
	ReadDS1820();

	//diody od UARTu jako signalizace -> OUTPUT
	//cervena = overheat
	//zelena = topeni ON
	DDRD |= (1 << LED_OVERHEAT) | (1 << LED_TOPENI);
	PORTD &= ~((1 << LED_OVERHEAT) | (1 << LED_TOPENI)); //obracena logika - 1 je vypne

}

//-----------------------------------------------------------
void main(void)
{
	wdt_disable();
	init();
	sei();

	memset((void*)&outPacket, 0, sizeof(mirfPacket) );

	uint16_t tmout = longTimer + 200; //delay(2000)
	volatile main_loop_prikazy_t prikazy{};

	//Hlavni LOOP
	for(;;)
	{
	//======================================================================================================
		// --- TIMER interrupt ------------------------
		if (longTimer == tmout)
		{
			static uint8_t x = 0;

			tmout = longTimer + 500; //delay(5000)

			x++;
			if (x == 2)
			{
				prikazy.mereni_teploty = (STAVY_MERENI_TEPLOT)ZAHAJIT_MERENI_PODLAHA; //prvni se dycky meri podlaha
				x = 0;
			}

			prikazy.prekreslit_display = 1;
		}

		// ---- NACITANI ONEWIRE DS18B20 teplot --------
		if (prikazy.mereni_teploty > 0) // != (STAVY_MERENI_TEPLOT)NEMERI_SE
		{
			if (prikazy.mereni_teploty == (STAVY_MERENI_TEPLOT)ZAHAJIT_MERENI_PODLAHA)
			{
				PrepniOwPodlaha();
				DS1820StartConversion();
				prikazy.mereni_teploty = (STAVY_MERENI_TEPLOT)MERENI_PROBIHA;
			}

			if (prikazy.mereni_teploty == (STAVY_MERENI_TEPLOT)ZAHAJIT_MERENI_VZDUCH)
			{
				PrepniOwVzduch();
				DS1820StartConversion();
				prikazy.mereni_teploty = (STAVY_MERENI_TEPLOT)MERENI_PROBIHA;
			}

			if (prikazy.mereni_teploty == (STAVY_MERENI_TEPLOT)MERENI_PROBIHA)
			{

				if (DS18X20_conversion_in_progress() == DS18X20_CONVERSION_DONE) //konverze je hotova
				{

					DS1820ReadConversionResult(); //precte data a ulozi je do bufferu pro aktualne vybranou sbernici

					if (vybrana_sbernice_teplota == (SBERNICE_TEPLOTY)PODLAHA) //prvni se dycky meri podlaha
					{
						prikazy.mereni_teploty = (STAVY_MERENI_TEPLOT)ZAHAJIT_MERENI_VZDUCH;
					}

					if (vybrana_sbernice_teplota == (SBERNICE_TEPLOTY)VZDUCH)
					{
						prikazy.mereni_teploty = (STAVY_MERENI_TEPLOT)NEMERI_SE;
					}

					prikazy.prepocitat_topeni = 1;

				}
			}
		}

		// ---- DISPLAY --------------------------------
		if (prikazy.prekreslit_display)
		{
			prikazy.prekreslit_display = 0;

			uint8_t znak_teploty;
			volatile uint16_t *p_teplota;

			if (prikazy.display_ukazuje == (SBERNICE_TEPLOTY)PODLAHA) //prepneme na vzduch
			{
				znak_teploty = ZNAK_VZDUCH;
				prikazy.display_ukazuje = (SBERNICE_TEPLOTY)VZDUCH;
				p_teplota = &(teplota_vzduch.uint);
			}
			else //prepneme na podlahu
			{
				znak_teploty = ZNAK_PODLAHA;
				prikazy.display_ukazuje = (SBERNICE_TEPLOTY)PODLAHA;
				p_teplota = &(teplota_podlaha.uint);
			}

			showNumber(*p_teplota);
			setSegments(&znak_teploty, 1, 0);
		}

		//---- TOPENI ----------------------------------
		if (prikazy.prepocitat_topeni)
		{
			prikazy.prepocitat_topeni = 0;

			if (((pozadavek_topeni != (STAVY_TOPENI)STAV_OFF) && (teplota_vzduch.uint < pozadovana_teplota)) ||
				((pozadavek_topeni == (STAVY_TOPENI)STAV_OFF) && (teplota_vzduch.uint < (uint8_t)(pozadovana_teplota - HISTEREZE))))
			{
				if (((pozadavek_topeni != (STAVY_TOPENI)STAV_OVERHEAT) && (teplota_podlaha.uint < TEPLOTA_OVERHEAT)) || //mame topit a neni overheat
					((pozadavek_topeni == (STAVY_TOPENI)STAV_OVERHEAT) && (teplota_podlaha.uint < (TEPLOTA_OVERHEAT - HISTEREZE))))
				{
					pozadavek_topeni = (STAVY_TOPENI)STAV_ON;
					LED_ON(LED_TOPENI)
					LED_OFF(LED_OVERHEAT)
				}
				else //mame topit ale je overheat
				{
					pozadavek_topeni = (STAVY_TOPENI)STAV_OVERHEAT;
					LED_OFF(LED_TOPENI)
					LED_ON(LED_OVERHEAT)
				}
			}
			else //teplota je dostatecna, nepotrebujeme topit
			{
				pozadavek_topeni = (STAVY_TOPENI)STAV_OFF;
				LED_OFF(LED_TOPENI)
				LED_OFF(LED_OVERHEAT)
			}
		}


		if (timer10msTriggered)
		{
			timer10msTriggered = 0;
			Mirf.handleRxLoop();
			Mirf.handleTxLoop();
		}

		//zpracovat prichozi packet
		if (Mirf.inPacketReady)
		{
			Mirf.readPacket((mirfPacket*)&inPacket);

			if ( (PACKET_TYPE)inPacket.type == PRESENTATION_REQUEST )
			{
				outPacket.type = PRESENTATION_RESPONSE;
				payloadPresentationStruct *res = (payloadPresentationStruct*)&outPacket.payload;

				res->num_sensors = NUM_SENSORS;
				res->sensor_type[0] = SENSOR_0_TYPE;
				res->sensor_type[1] = SENSOR_1_TYPE;
				res->sensor_type[2] = SENSOR_2_TYPE;
				#if NUM_SENSORS == 4
				res->sensor_type[3] = SENSOR_3_TYPE;
				#endif

				Mirf.sendPacket((mirfPacket*)&outPacket);
			}
		}

		//poslat paket po zpracovani
		if (Mirf.sendingStatus == IN_FIFO)
		{
			Mirf.handleTxLoop();
		}




	//======================================================================================================
	}
}
