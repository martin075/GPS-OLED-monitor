//-----main.c--------------------
// sylaina/oled-display at github
// 128x64 - 5x6px - 21x8 strings
// ublox NEO6
// atmega328p
//povodne 16MHz
//-------------------------------
#include <stdio.h>
#include <stdint.h>
#include "avr/io.h"
#include <util/delay.h>
#include <avr/interrupt.h>
#include <string.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>

#include "ublox.h"
#include "lcd.h"
#include "uart.h"


#define MAX_STRLEN 75	// minus 2 characters
#define UTC_offset 2

// Global variables  --------------------
unsigned char prijaty_znak;
static volatile unsigned char prijem[MAX_STRLEN];
unsigned char spravaGGA[MAX_STRLEN];
unsigned char spravaRMC[MAX_STRLEN];
unsigned char spravaVTG[MAX_STRLEN];

volatile unsigned char nova_sprava=0;
static unsigned int cnt=0,start=0,end=0,poc=0;
unsigned int RMCnext,GGAnext,VTGnext,indexRMC,indexGGA,indexVTG;

//  Function prototypes ---------------
int timer_tick(int mode);
void port_setup();
void pwm_out();
int porovnaj_retazec(char *str1, char *str2, int pocet_znakov);
int over_spravu(unsigned char *vstup);
int dlzka_spravy(unsigned char *vstup);


ISR(USART_RX_vect){

	prijaty_znak = usart_receive();
	
	if(prijaty_znak =='$'){start=1;spravaRMC[cnt]=prijaty_znak;spravaGGA[cnt]=prijaty_znak;}
	if((start==1)){
		if ( (prijaty_znak != '\r')&&(prijaty_znak != '\n')&&(prijaty_znak != '*')&&(cnt<(MAX_STRLEN-2)) )
			{
			if((prijaty_znak=='G') &&(RMCnext==0)){RMCnext=1;}
			if((prijaty_znak=='P') &&(RMCnext==1)){RMCnext=2;}
			if((prijaty_znak=='R') &&(RMCnext==2)){RMCnext=3;}
			if((prijaty_znak=='M') &&(RMCnext==3)){RMCnext=4;}
			if((prijaty_znak=='C') &&(RMCnext==4)){RMCnext=5;}
			if(RMCnext == 5){spravaRMC[indexRMC]=prijaty_znak;indexRMC++;}
			
			if((prijaty_znak=='G') &&(GGAnext==0)){GGAnext=1;}
			if((prijaty_znak=='P') &&(GGAnext==1)){GGAnext=2;}
			if((prijaty_znak=='G') &&(GGAnext==2)){GGAnext=3;}
			if((prijaty_znak=='G') &&(GGAnext==3)){GGAnext=4;}
			if((prijaty_znak=='A') &&(GGAnext==4)){GGAnext=5;}
			if(GGAnext == 5){spravaGGA[indexGGA]=prijaty_znak;indexGGA++;}
			
			if((prijaty_znak=='G') &&(VTGnext==0)){VTGnext=1;}
			if((prijaty_znak=='P') &&(VTGnext==1)){VTGnext=2;}
			if((prijaty_znak=='V') &&(VTGnext==2)){VTGnext=3;}
			if((prijaty_znak=='T') &&(VTGnext==3)){VTGnext=4;}
			if((prijaty_znak=='G') &&(VTGnext==4)){VTGnext=5;}
			if(VTGnext == 5){spravaVTG[indexVTG]=prijaty_znak;indexVTG++;}
			prijem[cnt]=prijaty_znak;
			cnt++;
			}
		else {	prijem[cnt]= '\0'; // end of message, apend \0
				spravaRMC[cnt]= '\0';spravaGGA[cnt]= '\0';spravaVTG[cnt]= '\0';
				start = 0;cnt=0;end=1;RMCnext=0;GGAnext=0;VTGnext=0;indexRMC=0;indexGGA=0;indexVTG=0;
				nova_sprava=1;poc++;PORTB ^= (1 << PB5); // set toggle
				}
		}
		
		

}

int main(void){
	
	uint8_t i,dlzka=2,hour=0;
	char temp[8];
	unsigned char time[7],date[7],altitude[7],altitude_g[7],lon[15],lat[15],speed[7],sat[5],status='V',gga_stat='0'; 
	unsigned int coma_gga=0,coma_rmc=0,coma_vtg=0;		
	unsigned int j_ag,j_a,j_t,j_d,j_s,j_lon,j_lat,j_sat,j_v;

	
	uartSetup();
	port_setup();
	
	sei();
	GPS_config(); // config u-blox neo6
  
  lcd_init(LCD_DISP_ON);    // init lcd and turn on
  	// 7 rows, 21 columns
  	lcd_charMode(DOUBLESIZE);
	lcd_puts("GPS monitor");  // put string from RAM to display (TEXTMODE) or buffer (GRAPHICMODE)
	//lcd_gotoxy(0,1);
	//lcd_puts("nòl¾tsšzž°C");
  	//lcd_gotoxy(0,2);          // set cursor to first column at line 3
  	//lcd_puts_p(PSTR("String from flash"));  // puts string form flash to display (TEXTMODE) or buffer (GRAPHICMODE)
	//lcd_puts("aáäeéiíuúoóyýcè");
	lcd_gotoxy(0,6);
	lcd_charMode(NORMALSIZE);
	lcd_puts("waiting for signal"); 	// max number of strings
	_delay_ms(250);
		
#if defined GRAPHICMODE
  	lcd_drawCircle(94,32,7,WHITE); // draw circle to buffer white lines
    lcd_display(); // send buffer to display
    lcd_drawLine(104,32,108,45,WHITE);
 	lcd_display(); // send buffer to display
#endif

	lcd_clrscr();
  while(1)
	{
	if(nova_sprava){
		nova_sprava=0;
	coma_rmc=0;coma_gga=0;coma_vtg=0;j_t=0,j_v=0,j_d=0;j_ag=0;j_a=0;j_sat=0;j_s=0;j_lat=0;j_lon=0;
	
	lcd_gotoxy(0,0);lcd_puts("in:");
	for(i=0;i<8;i++)lcd_putc(prijem[i]);
	dlzka = dlzka_spravy(prijem);
	sprintf(temp," l:%d",dlzka);for(i=0;i<8;i++)lcd_putc(temp[i]);
	
	for(i=0;i<(MAX_STRLEN-2);i++)
	{
		if(spravaRMC[i] == ',')coma_rmc++;
		if((coma_rmc == 1) && (j_t < 6) ){time[j_t] = spravaRMC[i+1];j_t++;} // time
		if((coma_rmc == 9) && (j_d < 6) ){date[j_d] = spravaRMC[i+1];j_d++;} // date
		if((coma_rmc == 2) && (j_v < 1) ){status = spravaRMC[i+1];j_v++;} // date
		if((coma_rmc == 3) && (coma_rmc != 4) ){lat[j_lat] = spravaRMC[i+1];j_lat++;} // latitude
		if((coma_rmc == 5) && (coma_rmc != 6) ){lon[j_lon] = spravaRMC[i+1];j_lon++;} // longitude
		if(spravaGGA[i] == ',')coma_gga++;	
		if((coma_gga == 6) && (coma_gga != 7) )gga_stat=spravaGGA[i]; // GGA status - fix to sat
		if((coma_gga == 7) && (coma_gga != 8) ){sat[j_sat] = spravaGGA[i+1];j_sat++;} // amount of sat
		if((coma_gga == 9) && (coma_gga != 10) ){altitude[j_a] = spravaGGA[i+1];j_a++;}
		if((coma_gga == 11) && (coma_gga != 12) ){altitude_g[j_ag] = spravaGGA[i+1];j_ag++;} // height of geoid
		if(spravaVTG[i] == ',')coma_vtg++;
		if((coma_vtg == 7) && (coma_vtg != 8) ){speed[j_s] = spravaVTG[i+1];j_s++;}  // speed

	}

	lcd_gotoxy(0,1);lcd_puts("TIME ");
	hour=(unsigned int)((time[0]-48)*10 )+((unsigned int)((time[1]+UTC_offset)-48) );
	sprintf(temp,"%d",hour);for(i=0;i<2;i++)lcd_putc(temp[i]);
	lcd_puts(":");
	for(i=2;i<4;i++)lcd_putc(time[i]);
	lcd_puts(" DATE");for(i=0;i<6;i++)lcd_putc(date[i]);
	if(status == 'A'){	
		lcd_gotoxy(0,2);lcd_puts("LAT ");
		for(i=0;i<12;i++)lcd_putc(lat[i]);
		lcd_gotoxy(0,3);lcd_puts("LON ");
		for(i=0;i<12;i++)lcd_putc(lon[i]);
		lcd_gotoxy(0,4);lcd_puts("SAT ");for(i=0;i<2;i++)lcd_putc(sat[i]);
		lcd_puts(" Status ");lcd_putc(status);lcd_putc(gga_stat);
		lcd_gotoxy(0,5);
		lcd_puts("alt ");for(i=0;i<5;i++)lcd_putc(altitude[i]);lcd_puts("m");
		lcd_puts(" altG ");for(i=0;i<2;i++)lcd_putc(altitude_g[i]);lcd_puts("m");
		lcd_gotoxy(1,6);
		lcd_puts("speed ");for(i=0;i<5;i++)lcd_putc(speed[i]);lcd_puts(" km/h");
		lcd_gotoxy(0,7);lcd_puts("                    ");
		}//if status
	else {lcd_gotoxy(0,7);lcd_puts("waiting for signal");}
		} //if nova sprava

    	
	} //while loop
  
} // end of main


int over_spravu(unsigned char *vstup)
{
	int vysledok=0,i;

	if(vstup[2]=='i' && vstup[3]=='m'  && vstup[4]=='p'){vysledok = 1;}
	if(vstup[2]=='d' && vstup[3]=='e'  && vstup[4]=='l'){vysledok = 2;} // prescaler
	if((vstup[2]!='i' && vstup[3]!='m'  && vstup[4]!='p') && (vstup[2]!='d' && vstup[3]!='e'  && vstup[4]!='l')) vysledok = 0;
	if(vstup[2]=='h' && vstup[3]=='o'  && vstup[4]=='d')vysledok = 3;	//hours
	if(vstup[2]=='m' && vstup[3]=='i'  && vstup[4]=='n')vysledok = 4;	//minutes
	if(vstup[2]=='d' && vstup[3]=='a'  && vstup[4]=='y')vysledok = 5;	//day date
	if(vstup[2]=='m' && vstup[3]=='o'  && vstup[4]=='n')vysledok = 6;	//month
	if(vstup[2]=='y' && vstup[3]=='e'  && vstup[4]=='a')vysledok = 7;	//year
	if(vstup[2]=='c' && vstup[3]=='d'  && vstup[4]=='a')vysledok = 8; // day of week
	if(vstup[1]=='G'  && vstup[3]=='R' && vstup[4]=='M' && vstup[5]=='C'){vysledok = 9;}	//NMEA RMC
	if(vstup[1]=='G'  && vstup[3]=='G' && vstup[4]=='G' && vstup[5]=='A'){vysledok = 10;
		for(i=0;i<MAX_STRLEN;i++)spravaGGA[i]=vstup[i]; }	//NMEA GGA
	
	
	return vysledok;
}

int dlzka_spravy(unsigned char *vstup)
{ 
	int len=0;

	while ((*vstup++)&&(len<MAX_STRLEN)){
		len++;
		}

	return len;

}

int timer_tick(int mode)
{
	int preddelicka = 2;
	
	TCCR1B = 0; 	//zastavenie citaca
	switch(mode) {
	case 1: {TCCR1B |= (1 << CS11);preddelicka=8; }break;// preddelicka 8
	case 2: {TCCR1B |= (1 << CS12) | (1 << CS10);preddelicka=1024; }break;//preddelièka 1024 (128us) 
	case 3: {TCCR1B |= (1 << CS12);preddelicka=256;}break; // preddelicka 256
	case 4: {TCCR1B |= (1 << CS10)| (1 << CS11);preddelicka=64;}break; // preddelicka 64
	case 5: {TCCR1B |= (1 << CS10);preddelicka=1;}break; // prescaler = 1
	default: {TCCR1B |= (1 << CS10)| (1 << CS11);preddelicka=64;}	
	}
	TIMSK1 |= (1 << TOIE1);  // prerušenie pri preteèení TCNT1     

    //OSCCAL = 0xA1;    // nastavenie kalibracneho bajtu interneho RC oscilatora
	return preddelicka;
}

void port_setup()
{
 DDRB |= (1 << PB1)|(1 << PB2);    //  ako výstupný 
 DDRB |= (1 << PB5);

 	//PORTB |= (1 << PB3);  	// on
	//PORTB &= ~(1 << PB3); 	// off
	//PORTB ^= (1<<PORTB3); 	//toggle
 	//DDRB &= ~(1 << PB1); // PB0 (ICP1) ako vstupny  
 PORTB |= (1 << PB1); //PB0 (ICP1) do log.1   
 PORTB |= (1 << PB5); // set high

}

int porovnaj_retazec(char *str1, char *str2, int pocet_znakov)
{	int i,rovnaky=0;
	
	for(i=0;i<pocet_znakov;i++){
		if(*str1++ == *str2++)rovnaky=0;
			else rovnaky = 1;
	}

return(rovnaky) ;
}

