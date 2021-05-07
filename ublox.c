/*
 * gps.c
 *
 * for Atmega328p
 *  Author: PjV
 *
 * Version: 1.0  
 */ 

#include <avr/io.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "ublox.h"



/***********************************************************************//**
* GPS Configuration Commands.
* Command Packet Structure :

|header     |ids               |Payloads|checksum |
|Sync1|Sync2|Class|ID|Len0|Len1|Payloads|CK_A|CK_B|

* more information u-blox6_ReceiverDescriptionProtocolSpec.pdf
* www.u-blox.com
***************************************************************************/
const unsigned char cmd_header[2] PROGMEM = { 0xB5/*Sync1*/, 0x62/*Sync2*/};

const CMD_ID_t ids PROGMEM = 
{		/*cfg_RATE_period*/	{0x06,		/* Class - Configuration Input Message*/ 
							 0x08,		/* ID - Measurement Rate*/ 
							 0x06, 0x00	/* Len0, Len1 - payload's length=6 bytes*/}, 
		/*cfg_MSG_rate*/	{0x06,		/* Class - Configuration Input Message*/
							 0x01,		/* ID - Message Rate(s)*/  
							 0x08, 0x00,/* payload's length=8*/}, 
		/*cfg_RXM_power*/	{0x06,		/* Class - Configuration Input Message*/
							 0x11,		/* ID - Receiver Manager, Power management*/ 
							 0x02, 0x00,/* payload's length=2*/} 
};

const CMD_PAYLOAD_t payloads PROGMEM = 
{
/*set_period_7s*/	{0x58, 0x1B,/*0x1B58==7000mS=>period 7sec*/ 0x01, 0x00, 0x01, 0x00 },
/*set_period_max*/	{0x60, 0xEA, 0x01, 0x00, 0x01, 0x00 },
	/*disable_GGA*/	{0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	/*disable_GLL*/	{0xF0, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	/*disable_GSA*/	{0xF0, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	/*disable_GSV*/	{0xF0, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	/*enable_RMC*/	{0xF0, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 },/*Enable RMC only in module-uart1*/
	/*disable_RMC*/	{0xF0, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	/*disable_VTG*/ {0xF0, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	/*power_down*/	{0x08, /*Always set to 8*/ 0x01, /*	0: Max. performance mode
														1: Power Save Mode (>= FW 6.00 only)
														4: Eco mode */},
	/*power_eco*/	{0x08, /*Always set to 8*/ 0x04, /*	0: Max. performance mode
														1: Power Save Mode (>= FW 6.00 only)
														4: Eco mode */}
};


/*************************************************************************//**
* static functions
*****************************************************************************/
static void _calc_checksum_p(PGM_P id, PGM_P payload, unsigned char* checksum);

static void _send_cmd_p(PGM_P id, PGM_P payload);


/*************************************************************************//**
* Description: Initialize GPS communications USART1
*****************************************************************************/



/*************************************************************************//**
 * Description: Set only Recommended Minimum data (NMEA-RMC), n sec period.
				More information u-blox6_ReceiverDescriptionProtocolSpec.pdf
				www.u-blox.com
 * \param
 * \returns  
*****************************************************************************/
void GPS_config(void)
{
#if USE_NEO6_GPS_MODULE
	UCSR0B |= (1 << TXEN0);		// Turn on the transmission
	UCSR0B &= ~(1 << RXEN0);	// Turn off the receiving, we dont expect any acks

	_send_cmd_p( ids.cfg_RATE_period, payloads.RATE_set_period_7s );
	//_send_cmd_p( ids.cfg_MSG_rate, payloads.MSG_disable_GGA );
	_send_cmd_p( ids.cfg_MSG_rate, payloads.MSG_disable_GLL );
	_send_cmd_p( ids.cfg_MSG_rate, payloads.MSG_disable_GSA );
	_send_cmd_p( ids.cfg_MSG_rate, payloads.MSG_disable_GSV );
	_send_cmd_p( ids.cfg_MSG_rate, payloads.MSG_enable_RMC );/*Enable only RMC, GPSmodule:uart1*/
	//_send_cmd_p( ids.cfg_MSG_rate, payloads.MSG_disable_VTG );
	
	UCSR0B &= ~(1 << TXEN0);	// Turn off the transmission, we don't need it anymore
	UCSR0B |= (1 << RXEN0);		// Now we are ready receiving data from GPS again
#endif	
}

/*************************************************************************//**
* Description:	Send configuration command to the GPS chip.
				Send command: header - ID - Payload - Checksum.
				Calculates checksums
* Param[in]:	Command's ID (4 bytes)
				Payload (Variable length field)
*****************************************************************************/
static void _send_cmd_p(PGM_P id, PGM_P payload)
{
	unsigned char chksmbts[2] = {0x00, 0x00};	// checksum bytes, calculate
	const unsigned char *ptr_to_header = cmd_header;
	unsigned char pl_len = pgm_read_byte(id+2);//payload's length, third byte in ID
	unsigned char ind;
	
	_calc_checksum_p( id, payload, chksmbts );
	
	/* send start bytes, sync */
	for ( ind=0; ind<2; ind++ ){
		while(!(UCSR0A & (1<<UDRE0)));
		UDR0 = pgm_read_byte(ptr_to_header++);
	}	
	/*send command ID (& payload's length)*/
	for ( ind=0; ind<4; ind++ ){
		while(!(UCSR0A & (1 << UDRE0)));
		UDR0 = pgm_read_byte( id++ );	
	}	
	/*send payload*/
	for ( ind=0; ind<pl_len; ind++ ){
		while(!(UCSR0A & (1 << UDRE0)));
		UDR0 = pgm_read_byte( payload++ );
	}
	/*send checksums*/
	for ( ind=0; ind<2; ind++ ){
		while(!(UCSR0A & (1<<UDRE0)));
		UDR0 = chksmbts[ind];
	}	
	while(!(UCSR0A & (1<<UDRE0)));
}

/*************************************************************************//**
* Description: Calculates checksums for GPS commands.
* Param[in]:		id - Command ID (&payload's length), 4 bytes
* Param[in]:		payload - Commands payload (Variable length field)
* Param[in,out]:	checksum - calculated checksum
*****************************************************************************/
static void _calc_checksum_p(PGM_P id, PGM_P payload, unsigned char* checksum )
{
	unsigned char temp = 0;
	unsigned char pl_len = pgm_read_byte(id+2);//payload's length, third byte in ID

	for( unsigned char i=0; i<4; i++ )
	{
		temp = pgm_read_byte( id++ );
		checksum[0] = checksum[0] + temp;
		checksum[1] = checksum[1] + checksum[0];
	}
	for( unsigned char i=0; i<pl_len; i++ )
	{
		temp = pgm_read_byte( payload++ );
		checksum[0] = checksum[0] + temp;
		checksum[1] = checksum[1] + checksum[0];
	}
}


/*************************************************************************//**
* more information u-blox6_ReceiverDescriptionProtocolSpec.pdf
* www.u-blox.com
*****************************************************************************/
void GPS_EcoPower(void)
{
#if USE_NEO6_GPS_MODULE
	UCSR0B &= ~(1 << RXCIE0);	/* disable GPS_uart RXC-interrupt */
	UCSR0B &= ~(1 << RXEN0);	// Turn off the receiving, we dont expect any acks
	UCSR0B |= (1 << TXEN0);		// Turn on the transmission

	/*Set Power Mode*/
	_send_cmd_p( ids.cfg_RXM_power, payloads.RXM_power_eco );
	
	UCSR0B &= ~(1 << TXEN0);	// Turn off the transmission, we don't need it anymore
	UCSR0B |= (1 << RXCIE0);	// Enable RX complete interrupt.
	UCSR0B |= (1 << RXEN0);		// Enable RX. Now we are ready receiving data from GPS again	
	_delay_ms(1);
#endif	
}

