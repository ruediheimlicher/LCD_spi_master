/**************************************************************
Es soll alle halbe Sekunde im Wechsel 0 bzw. 1 gesendet werden.
Am korrespondierenden Slave soll zur Indikation jeweils die 
LEDs an bzw. aus gehen
Verdrahtung:	MISO(Master) --> MISO(Slave)
				MOSI(Master) --> MOSI(Slave)
				SCK(Master)  --> SCK(Slave)
				PB0(Master)	 --> SS(Slave)
**************************************************************/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/signal.h>


unsigned char status = 0;
volatile unsigned char count;


volatile uint8_t			spistatus=0;

#define TEENSY_SEND  7
#define TEENSY_RECV  6
#define WHILEMAX 0xFFFF // Wartezeit in while-Schleife : 5 ms

#define SPI_DELAY 1

//http://www.ermicro.com/blog/?p=1050
// MCP23S17 Registers Definition for BANK=0 (default)
#define IODIRA 0x00
#define IODIRB 0x01
#define IOCONA 0x0A
#define GPPUA  0x0C
#define GPPUB  0x0D
#define GPIOA  0x12
#define GPIOB  0x13


uint16_t spiwaitcounter = WHILEMAX; // 5 ms

extern volatile uint8_t arraypos;


void timer1 (void);
void master_init (void);
void master_transmit (unsigned char data);

ISR (SPI_STC_vect)
{
	return;
}

/*
 
 Funktion zur Umwandlung einer vorzeichenlosen 32 Bit Zahl in einen String
 
 */

void r_uitoa(uint32_t zahl, char* string) {
   int8_t i;                             // schleifenzähler
   
   string[10]='\0';                       // String Terminator
   for(i=9; i>=0; i--) {
      string[i]=(zahl % 10) +'0';         // Modulo rechnen, dann den ASCII-Code von '0' addieren
      zahl /= 10;
   }
}


/*
 
 Funktion zur Umwandlung einer vorzeichenbehafteten 32 Bit Zahl in einen String
 
 */

void r_itoa(int32_t zahl, char* string)
{
   uint8_t i;
   
   string[11]='\0';                  // String Terminator
   if( zahl < 0 ) {                  // ist die Zahl negativ?
      string[0] = '-';
      zahl = -zahl;
   }
   else string[0] = ' ';             // Zahl ist positiv
   
   for(i=10; i>=1; i--) {
      string[i]=(zahl % 10) +'0';     // Modulo rechnen, dann den ASCII-Code von '0' addieren
      zahl /= 10;
   }
}

void spi_r_itoa16(int16_t zahl, char* string)
{
   uint8_t i;
   int16_t original=zahl;
   string[7]='\0';                  // String Terminator
   if( zahl < 0 ) {                  // ist die Zahl negativ?
      string[0] = '-';
      zahl = -zahl;
   }
   else string[0] = ' ';             // Zahl ist positiv
   
   for(i=6; i>=1; i--)
   {
      string[i]=(zahl % 10) +'0';     // Modulo rechnen, dann den ASCII-Code von '0' addieren
      zahl /= 10;
   }
   if (abs(original) < 1000)
   {
      string[1]= ' ';
   }
   if (abs(original) < 100)
   {
      string[2]= ' ';
   }
   if (abs(original) < 10)
   {
      string[3]= ' ';
   }
   
}



void spi_master_init (void)
{
   /*
    // in defines.h:
    #define SPI_PORT     PORTB
    #define SPI_PIN      PINB
    #define SPI_DDR      DDRB
    
    
    #define SPI_SS       4
    
    #define SPI_MOSI     5
    #define SPI_MISO     6
    #define SPI_SCK      7
    

    */
   
	SPI_DDR = (1<<SPI_SS) | (1<<SPI_MOSI) | (1<<SPI_SCK);		// setze SCK,MOSI,PB0 (SS) als Ausgang
	SPI_DDR &= ~(1<<SPI_MISO);							// setze MISO als Eingang
	//SPI_PORT = (1<<SPI_SCK) | (1<<SPI_SS);				// SCK und PB0 high (ist mit SS am Slave verbunden)
	//SPCR = (1<<SPE) | (1<<MSTR) | (1<<SPR0);	//Aktivierung des SPI, Master, Taktrate fck/16
	
   
   SPI_PORT |= (1<<SPI_SS); // HI,
   
   //SPI_PORT |= (1<<SPI_CS); // HI, ohne Optokoppler
   
   SPCR |= (1<<MSTR);// Set as Master
   
 //  SPCR0 |= (1<<CPOL0)|(1<<CPHA0);
   
   SPI_PORT &= ~(1<<SPI_MISO); // LO
   /*
    SPI2X 	SPR1 	SPR0     SCK Frequency
    0       0        0     fosc/4
    0       0        1     fosc/16
    0       1        0     fosc/64
    0       1        1     fosc/128
    1       0        0     fosc/2
    1       0        1     fosc/8
    1       1        0     fosc/32
    1       1        1     fosc/64
    */
   
   //SPCR |= (1<<SPR0);               // div 16 SPI2X: div 8
   SPCR |= (1<<SPR1);               // div 64 SPI2X: div 32
   //SPCR |= (1<<SPR1) | (1<<SPR0);   // div 128 SPI2X: div 64
   //SPCR |= (1<<SPI2X);
   
   SPCR |= (1<<SPE); // Enable SPI
   status = SPSR;								//Status loeschen
   
}

void spi_master_restore(void)
{
   //SPCR0=0;
   SPCR &= ~(1<<CPOL);
   SPCR &= ~(1<<CPHA);

}

/*
void setSPI_Teensy(void)
{
   uint8_t spidelay = 5;
   uint8_t spiwaitdelay = 4;

   uint8_t outindex=0;
   SPI_PORT &=  ~(1<<SPI_CS); // CS LO, Start, Slave soll erstes Byte laden
   _delay_us(spidelay);
   
   //SPI_PORT &= ~(1<<SPI_SS); // SS LO, Start, Slave soll erstes Byte laden
   //_delay_us(1);
   
   //PORTB &= ~(1<<0);
   
   
   
   for (outindex=0;outindex < SPI_BUFFERSIZE;outindex++)
      //for (outindex=0;outindex < 4;outindex++)
   {
      //OSZI_A_LO;
      // _delay_us(spidelay);
      SPI_PORT &= ~(1<<SPI_SS); // SS LO, Start, Slave soll erstes Byte laden
      _delay_us(spidelay);
      
      SPDR = spi_txbuffer[outindex];
      
      while(!(SPSR & (1<<SPIF0)) && spiwaitcounter < WHILEMAX)
      {
          spiwaitcounter++;
      }
      spiwaitcounter=0;
      //_delay_us(spidelay);
      //uint8_t incoming = SPDR0;
      
      if (outindex == 0) // slave warten lassen, um code zu laden
      {
         uint8_t incoming = SPDR;
         
         _delay_us(spiwaitdelay);
      }
      else if (outindex ==1) // code lesen, spistatus steuern
      {
         spi_rxbuffer[0] = SPDR;
         
         if (spi_rxbuffer[0] & (1<<TEENSY_SEND))
         {
            spistatus |= (1<< TEENSY_SEND);
            _delay_us(spiwaitdelay);
         }
         else
         {
            spistatus &= ~(1<< TEENSY_SEND);
            spistatus &= ~(1<< TEENSY_RECV);
         }
      }
      else if (spistatus & (1<< TEENSY_SEND))
      {
         if (spi_rxbuffer[0] & 0x7F)
         {
         spi_rxbuffer[outindex-1] = SPDR0; // erster durchgang liest dummy
         _delay_us(spiwaitdelay);
         }
         //spi_rxbuffer[outindex] = incoming;
      }

      
      _delay_us(spidelay);
      SPI_PORT |= (1<<SPI_SS); // SS HI End, Slave soll  Byte-Zähler zurücksetzen
      //OSZI_A_HI;
      
      
   }
   //PORTB |=(1<<0);
   arraypos++;
   arraypos &= 0x07;
   //spi_rxbuffer[outindex] = '\0';
   //outbuffer[outindex] = '\0';
   //char rest = SPDR;
   
   // wichtig
   _delay_us(10);
   
   //SPI_PORT |= (1<<SPI_SS); // SS HI End, Slave soll  Byte-Zähler zurücksetzen
   //_delay_us(1);
   
   SPI_PORT |= (1<<SPI_CS); // CS HI End, Slave soll  Byte-Zähler zurücksetzen
   //OSZI_A_HI;
   //_delay_us(10);

}
*/
unsigned char SPI_get_put_char(uint8_t cData)
{
   /* Start transmission */
   SPDR = cData;
   /* Wait for transmission complete */
  // while(!(SPSR & (1<<SPIF)))
      while(!(SPSR & (1<<SPIF)) && spiwaitcounter < WHILEMAX)
      {
         spiwaitcounter++;
      }
      ;
   /* Return data register */
   return SPDR;
}

uint8_t  get_SR(uint8_t outData)
{
 //  SWITCH_CS_LO;
   uint8_t in = 0;
   LCD_SS_HI;
   _delay_us(1);
 //  SWITCH_LOAD_LO; // Data lesen start
   _delay_us(1);
 //  SWITCH_LOAD_HI;
   _delay_us(1);
   LCD_SS_LO;
   //return 0;
   //SPCR0 |= (1<<CPOL0);
   SPDR = outData;
   while(!(SPSR & (1<<SPIF)) && spiwaitcounter < WHILEMAX)
   {
      spiwaitcounter++;
   }
   
   //SPCR0 &= ~(1<<CPOL0);
   //in = SPDR0;
   
 //  SWITCH_CS_HI;
   return SPDR0;
   
   
   
   
}



uint8_t set_LCD(uint8_t outData)
{
   LCD_SS_LO;
   _delay_us(SPI_DELAY);
  // _delay_ms(2);
   SPDR = outData;
   spiwaitcounter=0;
   while(!(SPSR & (1<<SPIF)) )//&& spiwaitcounter < WHILEMAX)
   {
      spiwaitcounter++;
   }
   _delay_us(SPI_DELAY);
   LCD_SS_HI;
   return SPDR;
}

uint8_t set_LCD_first(uint8_t outData) // erstes Byte, SS LO
{
   LCD_SS_LO;
   _delay_us(SPI_DELAY);
   // _delay_ms(2);
   SPDR = outData;
   spiwaitcounter=0;
   while(!(SPSR & (1<<SPIF)) )//&& spiwaitcounter < WHILEMAX)
   {
      spiwaitcounter++;
   }
   _delay_us(SPI_DELAY);
   //LCD_SS_HI;
   return SPDR;
}

uint8_t set_LCD_inner(uint8_t outData) // inneres Byte, SS nicht veraendert
{
   //LCD_SS_LO;
   _delay_us(SPI_DELAY);
   // _delay_ms(2);
   SPDR = outData;
   spiwaitcounter=0;
   while(!(SPSR & (1<<SPIF)) )//&& spiwaitcounter < WHILEMAX)
   {
      spiwaitcounter++;
   }
   _delay_us(SPI_DELAY);
   //LCD_SS_HI;
   return SPDR;
}


uint8_t set_LCD_last(uint8_t outData) // letztes Byte, SS HI
{
   //LCD_SS_LO;
   _delay_us(SPI_DELAY);
   // _delay_ms(2);
   SPDR = outData;
   spiwaitcounter=0;
   while(!(SPSR & (1<<SPIF)) )//&& spiwaitcounter < WHILEMAX)
   {
      spiwaitcounter++;
   }
   _delay_us(SPI_DELAY);
   LCD_SS_HI;
   return SPDR;
}



void set_LCD_data(uint8_t outData) // 180us
{
   //OSZITOG;
   
    // http://stackoverflow.com/questions/4085612/converting-an-int-to-a-2-byte-hex-value-in-c
   //char array[2]={};
   //array[0] = outData >> 8;
   //array[1] = outData & 0xff;

   char buffer[5]={};
   itoa(outData, buffer,16);
   //_delay_us(SPI_SEND_DELAY);
   set_LCD(buffer[0]);
   //_delay_us(SPI_SEND_DELAY);
   set_LCD(buffer[1]);
   //_delay_us(SPI_SEND_DELAY);
   
}

void set_LCD_task(uint8_t outTask) // 80us
{
   _delay_us(SPI_SEND_DELAY);
   set_LCD(outTask);
   _delay_us(SPI_SEND_DELAY);
}


void set_LCD_string(char* outString)
{
   uint8_t stringpos=0;
   //set_LCD(strlen(outString));
   _delay_us(SPI_SEND_DELAY);

   //while (!(outString[stringpos]=='\0'))
   while ((outString[stringpos]))
   {
      _delay_us(SPI_SEND_DELAY);
      set_LCD(outString[stringpos++]);
      _delay_us(SPI_SEND_DELAY);
   }
}

void set_LCD_string2(char* outString)
{
   uint8_t stringpos=0;
   //set_LCD(strlen(outString));
   _delay_us(SPI_SEND_DELAY);
   
   //while (!(outString[stringpos]=='\0'))
   set_LCD_first(outString[stringpos++]);
   _delay_us(SPI_SEND_DELAY);

   while ((outString[stringpos]))
   {
      _delay_us(SPI_SEND_DELAY);
      set_LCD_inner(outString[stringpos++]);
      _delay_us(SPI_SEND_DELAY);
   }
   set_LCD_last(0);
}

void spi_lcd_puts(char* outString)
{
   uint8_t l = strlen(outString);
   set_LCD(START_TASK);// CR //
   _delay_us(SPI_SEND_DELAY);
   
   set_LCD_task(STRING_TASK); //
   
   _delay_us(SPI_SEND_DELAY);
   
   set_LCD_data(l); // strlen senden
   
   set_LCD_string2(outString);
   
   set_LCD(0);
   

}

void spi_lcd_putc(uint8_t outChar) // 400us
{
   set_LCD(START_TASK);// CR //
   _delay_us(SPI_SEND_DELAY);
    set_LCD_task(CHAR_TASK); //
   //_delay_us(SPI_SEND_DELAY);
   set_LCD_data(outChar);
   //_delay_us(SPI_SEND_DELAY);
   
   //_delay_us(SPI_SEND_DELAY);
   set_LCD(0);
   //_delay_us(SPI_SEND_DELAY);
}




void spi_lcd_gotoxy2(uint8_t x, uint8_t y)
{
   uint8_t goto_pos = (x <<3) | (y & 0x07); // 5 bit col, 3 bit line
   
   set_LCD(START_TASK);// CR //
   _delay_us(SPI_SEND_DELAY);
   set_LCD_task(NEW_TASK); //
   //_delay_us(SPI_SEND_DELAY);
   
   // Numerische Werte muessen als char gesendet werden
   set_LCD_data(goto_pos+'0');
   //_delay_us(SPI_SEND_DELAY);
   // ende paket goto
   //_delay_us(SPI_SEND_DELAY);
   set_LCD(0);
   //_delay_us(SPI_SEND_DELAY);
   
}//lcd_gotoxy

// neue Version mit 4 bytes
void spi_lcd_gotoxy(uint8_t x, uint8_t y)
{
   
   set_LCD(START_TASK);// CR //
   _delay_us(SPI_SEND_DELAY);
   set_LCD_task(GOTO_TASK); //
   //_delay_us(SPI_SEND_DELAY);
   
   // Numerische Werte muessen als char gesendet werden
   
   set_LCD_data(x+'0');
   _delay_us(SPI_SEND_DELAY);
   
   set_LCD_data(y+'0');
   // ende paket goto
   //_delay_us(SPI_SEND_DELAY);
   set_LCD(0);
   //_delay_us(SPI_SEND_DELAY);
   
}/* lcd_gotoxy */

void spi_lcd_putint(uint8_t zahl)
{
   char string[4];
   int8_t i;                             // schleifenzähler
   
   string[3]='\0';                       // String Terminator
   for(i=2; i>=0; i--)
   {
      string[i]=(zahl % 10) +'0';         // Modulo rechnen, dann den ASCII-Code von '0' addieren
      zahl /= 10;
   }
   spi_lcd_puts(string);
}

void spi_lcd_putint2(uint8_t zahl)	//zweistellige Zahl
{
   char string[3];
   int8_t i;								// Schleifenzähler
   zahl%=100;								// 2 hintere Stelle
   //  string[4]='\0';                     // String Terminator
   string[2]='\0';							// String Terminator
   for(i=1; i>=0; i--) {
      string[i]=(zahl % 10) +'0';         // Modulo rechnen, dann den ASCII-Code von '0' addieren
      zahl /= 10;
   }
   spi_lcd_puts(string);
}

void spi_lcd_puthex(uint8_t zahl)
{
   //char string[5];
   char string[3];
   uint8_t l,h;                          // schleifenzähler
   
   string[2]='\0';                       // String Terminator
   l=(zahl % 16);
   if (l<10)
      string[1]=l +'0';
   else
   {
      l%=10;
      string[1]=l + 'A';
      
   }
   zahl /=16;
   h= zahl % 16;
   if (h<10)
      string[0]=h +'0';
   else
   {
      h%=10;
      string[0]=h + 'A'; 
   }
   
   
   spi_lcd_puts(string);
}

void spi_lcd_putint1(uint8_t zahl)	//einstellige Zahl
{
   //char string[5];
   char string[2];
   zahl%=10;								//  hinterste Stelle
   string[1]='\0';							// String Terminator
   string[0]=zahl +'0';         // Modulo rechnen, dann den ASCII-Code von '0' addieren
   spi_lcd_puts(string);
}

void spi_lcd_putsignedint(int8_t zahl)
{
   char string[5];
   int8_t i;                             // schleifenzähler
   
   string[4]='\0';                       // String Terminator
   
   if (zahl<0)
   {
      string[0] = '-';
      zahl *= (-1);
   }
   else
   {
      string[0] = '+';
   }
   zahl &= 0x7F;
   for(i=3; i>0; i--)
   {
      string[i]=(zahl % 10) +'0';         // Modulo rechnen, dann den ASCII-Code von '0' addieren
      zahl /= 10;
   }
   spi_lcd_puts(string);
}



void spi_lcd_putint12(uint16_t zahl)
{
   char string[5];
   int8_t i;                             // schleifenzähler
   
   string[4]='\0';                       // String Terminator
   for(i=3; i>=0; i--)
   {
      string[i]=(zahl % 10) +'0';         // Modulo rechnen, dann den ASCII-Code von '0' addieren
      zahl /= 10;
   }
   spi_lcd_puts(string);
}


void spi_lcd_put_frac(char* string, uint8_t start, uint8_t komma, uint8_t frac)
{
   
   uint8_t i;            // Zähler
   uint8_t flag=0;       // Merker für führende Nullen
   
   // Vorzeichen ausgeben
   if (string[0]=='-') lcd_putc('-'); else lcd_putc(' ');
   
   // Vorkommastellen ohne führende Nullen ausgeben
   for(i=start; i;i--) {
      if (flag==1 || string[i]!='0') {
         lcd_putc(string[i]);
         flag = 1;
      }
      else lcd_putc(' ');         // Leerzeichen
   }
   
   lcd_putc('.');                // Komma ausgeben
   
   // Nachkommastellen ausgeben
   for(; i<(komma+frac); i++) lcd_putc(string[i]);
   
}

void spi_lcd_put_zeit(uint8_t minuten, uint8_t stunden)
{
   //							13:15
   int8_t i;
   if (stunden< 10)
   {
      //	lcd_putc(' ');
   }
   char zeitString[6]={};
   zeitString[5]='\0';
   
   //	Minuten einsetzen
   zeitString[4]=(minuten % 10) +'0';	//hinterste Stelle
   if (minuten>9)
   {
      minuten/=10;
      zeitString[3]=(minuten % 10) +'0';
   }
   else
   {
      zeitString[3]='0';
   }
   
   zeitString[2]=':';
   
   //	Stunden einsetzen
   zeitString[1]=(stunden % 10) +'0';
   if (stunden>9)
   {
      stunden/=10;
      zeitString[0]=(stunden % 10) +'0';
   }
   else
   {
      zeitString[0]='0';
   }
   spi_lcd_puts(zeitString);
}

void spi_lcd_put_wochentag(uint8_t wd)
{
   char* wochentag[] = {"MO","DI","MI","DO","FR","SA","SO"};
   
   spi_lcd_puts(wochentag[wd-1]);	// Array wochentag ist null-basiert
}

void spi_lcd_put_temperatur(uint16_t temperatur)
{
   char buffer[8]={};
   //uint16_t temp=(temperatur-127)*5;
   uint16_t temp=temperatur*5;
   //		uint16_t temp=temperatur;
   //lcd_gotoxy(0,1);
   //lcd_putint16(temp);

   //		itoa(temp, buffer,10);
   r_itoa16(temp,buffer);
   //		lcd_putc(' * ');
   
   char outstring[8]={};
   
   outstring[7]='\0';
   outstring[6]=0xDF;
   outstring[5]=buffer[6];
   outstring[4]='.';
   outstring[3]=buffer[5];
   if (abs(temp)<100)
   {
      outstring[2]=' ';
      outstring[1]=' ';
   }
   else if (abs(temp)<1000)
   {
      outstring[2]=buffer[4];
      outstring[1]=' ';
      
   }
   else
   {
      outstring[2]=buffer[4];
      outstring[1]=buffer[3];
      
   }
   
   outstring[0]=buffer[0];
   /*
    if (temp<100)
    {
    lcd_putc(' ');
    }
    if (temp<10)
    {
    lcd_putc(' ');
    }
    */
   spi_lcd_puts(outstring);
   
}


void spi_lcd_put_tempbis99(uint16_t temperatur)
{

   char buffer[7]={};//' ',' ',' ',' ',' ',' ',' '};
   //uint16_t temp=(temperatur-127)*5;
   //lcd_gotoxy(0,1);
   //lcd_puts("t:\0");
   //lcd_putint((uint8_t) temperatur);
   uint16_t temp=(temperatur)*5;
   char temperaturstring[7]={};
   
   //lcd_puts("T:\0");
   //spi_lcd_putint12(temp);
   
   //		uint16_t temp=temperatur;
   //temp = 1334;
   
   //spi_lcd_putint12(temp);
   //spi_lcd_putc('*');

   //itoa(temp, buffer,10);
   itoa(temp, temperaturstring,10);
   
   //spi_lcd_puts(temperaturstring);
   //spi_lcd_putc('*');

   
   uint8_t l=strlen(temperaturstring);
   
//   spi_lcd_putint2(l);
   //spi_lcd_putc('l');
   char last[2];
   last[0] = temperaturstring[l-1];
   last[1] = '\0';
   //spi_lcd_puts(last);

   
   char vorkommateil[7];
   memset(vorkommateil, '\0', sizeof(vorkommateil));
   
   strncpy(vorkommateil,temperaturstring,l-1);
   strcat(vorkommateil,".\0");
   strcat(vorkommateil,last);
   //strupr(buffer);
   //spi_lcd_putc('v');
   spi_lcd_puts(vorkommateil);
//spi_lcd_putc('+');
   return;
   
   
   
   
  
   //temperaturstring[5]=0xDF;
   temperaturstring[5]='C';
   temperaturstring[4]=buffer[6];
   
   temperaturstring[3]='.';
   temperaturstring[2]=buffer[5];
   /*
  if (abs(temp)<100)
   {
      temperaturstring[1]=' ';
      
   }
   else 
   {
      temperaturstring[1]=buffer[4];
      
   }		
   */
   temperaturstring[1]=buffer[4];
   temperaturstring[0]=buffer[0];
   //temperaturstring[6]='\0';
   /*
   outstring[5]='x';
   outstring[4]=buffer[3];
   outstring[3]='.';
   outstring[2]=buffer[2];
   outstring[1]=buffer[1];
   outstring[0]=buffer[0];
*/
   //spi_lcd_puts(buffer);
   spi_lcd_putc('*');
   //strcpy(temperaturstring,"asdfghjkl\0");
   spi_lcd_puts(temperaturstring);
   //spi_lcd_puts("asdfghjkl");
}

/*
uint8_t set_SR_595(uint8_t outData)
{
   SRB_CS_LO;
   _delay_us(1);
   SPDR0 = outData;
   spiwaitcounter=0;
   while(!(SPSR0 & (1<<SPIF0)) )//&& spiwaitcounter < WHILEMAX)
   {
      spiwaitcounter++;
   }
   
   //SPCR0 &= ~(1<<CPOL0);
   //uint8_t in = SPDR0;
   
   SRB_CS_HI;
   SRB_CS_LO;
   SRB_CS_HI;
   return SPDR0;
   
   
}
*/

/*
void SPI_Write(unsigned char addr,unsigned char data)
{
   // http://www.ermicro.com/blog/?p=1050
   // Activate the CS pin
   SRA_CS_LO;
   // Start MCP23S17 OpCode transmission
   SPDR0 = SPI_SLAVE_ID | ((SPI_SLAVE_ADDR << 1) & 0x0E)| SPI_SLAVE_WRITE;
   // Wait for transmission complete
   while(!(SPSR0 & (1<<SPIF0)));
   // Start MCP23S17 Register Address transmission
   SPDR0 = addr;
   // Wait for transmission complete
   while(!(SPSR0 & (1<<SPIF0)));
   
   // Start Data transmission
   SPDR0 = data;
   // Wait for transmission complete
   while(!(SPSR0 & (1<<SPIF0)));
   // CS pin is not active
   SRA_CS_HI;
}
*/

/*
void init_SR_23S17(void)
{
   
   uint8_t init_opcode = 0x40; // 0x40 & write
   uint8_t init_adresseA = 0x00; // SPI_IODRA PIN-OUT/IN
   uint8_t init_adresseB = 0x01; // SPI_IODRB PIN-OUT/IN
   uint8_t init_dataA = 0xE0; // bit 0-4 output bit 5-7 input (Drehschalter)
   uint8_t init_dataB = 0x03; // bit 0-1 input (Imax, Umax) bit 5-7 output (Relais)
   SRA_CS_LO;
   _delay_us(1);
   
   SPI_Write(IODIRA,init_dataA);   // GPIOA As Input/Output
    _delay_us(1);
   SPI_Write(IODIRB,init_dataB);
   
    _delay_us(1);
   
   SPI_Write(IOCONA,0x28);   // I/O Control Register: BANK=0, SEQOP=1, HAEN=1 (Enable Addressing)
   //SPI_Write(IODIRA,0x00);   // GPIOA As Output
   //SPI_Write(IODIRB,0x00);   // GPIOB As Input
   //SPI_Write(GPPUB,0x00);    // Enable Pull-up Resistor on GPIOB
   SPI_Write(GPIOB,0x03);    // Reset Output on GPIOB
   

  // SPI_Write(GPPUA,0xF0);    // Enable Pull-up Resistor on GPIOA
   
   SPI_Write(GPPUB,0x03);    // Enable Pull-up Resistor on GPIOB
   
   SRA_CS_HI;
   
}
*/
/*
uint8_t set_SR_23S17_A(uint8_t outData)
{
   uint8_t write_opcode = 0x40; // 0x40 & write
   uint8_t write_adresse = 0x12; // GPIOA PIN-OUT/IN
   //uint8_t write_adresse = 0x14; // OLATA PIN-OUT/IN
   uint8_t write_dataA = outData; // alle output
   SRA_CS_LO;
   _delay_us(1);
   spiwaitcounter=0;
   SPDR0 = write_opcode;
   while(!(SPSR0 & (1<<SPIF0)) )//&& spiwaitcounter < WHILEMAX)
   {
      spiwaitcounter++;
   }
   SPDR0 = write_adresse;
   while(!(SPSR0 & (1<<SPIF0)) )//&& spiwaitcounter < WHILEMAX)
   {
      spiwaitcounter++;
   }
   SPDR0 = write_dataA;
   while(!(SPSR0 & (1<<SPIF0)) )//&& spiwaitcounter < WHILEMAX)
   {
      spiwaitcounter++;
   }
   SRA_CS_HI;
   
   return SPDR0;
   
   
}
*/
/*
uint8_t set_SR_23S17_B(uint8_t outData)
{
   uint8_t write_opcode = 0x40; // 0x40 & write
   uint8_t write_adresse = 0x13; // GPIOB PIN-OUT/IN
   //uint8_t write_adresse = 0x15; // OLATB PIN-OUT/IN
   uint8_t write_dataA = outData; // alle output
   SRA_CS_LO;
   _delay_us(1);
   spiwaitcounter=0;
   SPDR0 = write_opcode;
   while(!(SPSR0 & (1<<SPIF0)) )//&& spiwaitcounter < WHILEMAX)
   {
      spiwaitcounter++;
   }
   SPDR0 = write_adresse;
   while(!(SPSR0 & (1<<SPIF0)) )//&& spiwaitcounter < WHILEMAX)
   {
      spiwaitcounter++;
   }
   SPDR0 = write_dataA;
   while(!(SPSR0 & (1<<SPIF0)) )//&& spiwaitcounter < WHILEMAX)
   {
      spiwaitcounter++;
   }
   SRA_CS_HI;
   
   return SPDR0;
   
   
}
*/
/*
uint8_t set_SR_23S17(uint8_t addr,uint8_t outData)
{
   uint8_t write_opcode = 0x40; // 0x40 & write
   uint8_t write_adresse = 0x13; // GPIOB PIN-OUT/IN
   //uint8_t write_adresse = 0x15; // OLATB PIN-OUT/IN
   uint8_t write_dataA = outData; // alle output
   SRA_CS_LO;
   _delay_us(1);
   spiwaitcounter=0;
   SPDR0 = write_opcode;
   while(!(SPSR0 & (1<<SPIF0)) )//&& spiwaitcounter < WHILEMAX)
   {
      spiwaitcounter++;
   }
   SPDR0 = addr;
   while(!(SPSR0 & (1<<SPIF0)) )//&& spiwaitcounter < WHILEMAX)
   {
      spiwaitcounter++;
   }
   SPDR0 = write_dataA;
   while(!(SPSR0 & (1<<SPIF0)) )//&& spiwaitcounter < WHILEMAX)
   {
      spiwaitcounter++;
   }
   SRA_CS_HI;
   
   return SPDR0;
   
   
}
*/
/*
uint8_t get_SR_23S17(uint8_t addr)
{
   uint8_t read_opcode = 0x41; // 0x40 & read
   uint8_t read_adresse = 0x13; // GPIOB PIN-OUT/IN

   // Activate the CS pin
   SRA_CS_LO;
   // Start MCP23S17 OpCode transmission
   //SPDR = SPI_SLAVE_ID | ((SPI_SLAVE_ADDR << 1) & 0x0E)| SPI_SLAVE_READ;
   _delay_us(1);
   spiwaitcounter=0;
   SPDR0 = read_opcode;

   // Wait for transmission complete
   while(!(SPSR0 & (1<<SPIF0)));
   _delay_us(1);
   // Start MCP23S17 Address transmission
   SPDR0 = addr;
   // Wait for transmission complete
   while(!(SPSR0 & (1<<SPIF0)));
   
   _delay_us(1);
   // Send Dummy transmission for reading the data
   SPDR0 = 0x00;
   // Wait for transmission complete
   while(!(SPSR0 & (1<<SPIF0)));
   
   // CS pin is not active
   SRA_CS_HI;
  
   return(SPDR0);
}
*/
/*
uint8_t io_SR_23S17(uint8_t addr, uint8_t data)
{
   uint8_t read_opcode = 0x41; // 0x40 & read
   uint8_t read_adresse = 0x13; // GPIOB PIN-OUT/IN
   
   // Activate the CS pin
   SRA_CS_LO;
   // Start MCP23S17 OpCode transmission
   //SPDR = SPI_SLAVE_ID | ((SPI_SLAVE_ADDR << 1) & 0x0E)| SPI_SLAVE_READ;
   _delay_us(1);
   spiwaitcounter=0;
   SPDR0 = read_opcode;
   
   // Wait for transmission complete
   while(!(SPSR0 & (1<<SPIF0)));
   _delay_us(1);
   // Start MCP23S17 Address transmission
   SPDR0 = addr;
   // Wait for transmission complete
   while(!(SPSR0 & (1<<SPIF0)));
   
   _delay_us(1);
   // Send Dummy transmission for reading the data
   SPDR0 = data;
   // Wait for transmission complete
   while(!(SPSR0 & (1<<SPIF0)));
   
   // CS pin is not active
   SRA_CS_HI;
   return(SPDR0);
}
*/

/*


void setMCP4821_U(uint16_t data)
{
   //OSZI_A_LO;
   
   MCP_U_CS_LO;
   //MCP_PORT &= ~(1<<MCP_DAC_U_CS);
   _delay_us(1);
   //OSZI_A_HI;
   uint8_t hbyte = (((data & 0xFF00)>>8) & 0x0F); // bit 8-11 von data als bit 0-3
   
   //hbyte |= 0x30; // Gain 1
   hbyte |= 0x10; // Gain 2
   //hbyte = 0b00010000;
   SPDR0 = (hbyte);
   while(!(SPSR0 & (1<<SPIF0)) )//&& spiwaitcounter < WHILEMAX)
   {
      spiwaitcounter++;
   }

   SPDR0 = (data & 0x00FF);
   while(!(SPSR0 & (1<<SPIF0)) )//&& spiwaitcounter < WHILEMAX)
   {
      spiwaitcounter++;
   }
   _delay_us(1);
   
   //
   MCP_U_CS_HI;
   //MCP_PORT &= (1<<MCP_DAC_U_CS);
   _delay_us(1);
   
   // Daten laden
   //MCP_LOAD_LO;
   _delay_us(1);
   //MCP_LOAD_HI;
   
   
}
*/
/*
void setMCP4821_I(uint16_t data)
{
   MCP_I_CS_LO;
   //MCP_PORT &= ~(1<<MCP_DAC_U_CS);
   _delay_us(1);
   uint8_t hbyte = (((data & 0xFF00)>>8) & 0x0F); // bit 8-11 von data als bit 0-3
   
   //hbyte |= 0x30; // Gain 1
   hbyte |= 0x10; // Gain 2
   //hbyte = 0b00010000;
   SPDR0 = (hbyte);
   while(!(SPSR0 & (1<<SPIF0)) )//&& spiwaitcounter < WHILEMAX)
   {
      spiwaitcounter++;
   }
   
   SPDR0 = (data & 0x00FF);
   while(!(SPSR0 & (1<<SPIF0)) )//&& spiwaitcounter < WHILEMAX)
   {
      spiwaitcounter++;
   }
   _delay_us(1);
   
   //
   MCP_I_CS_HI;
   //MCP_PORT &= (1<<MCP_DAC_U_CS);
   _delay_us(1);
   
   // Daten laden
   //MCP_LOAD_LO;
  // _delay_us(1);
   //MCP_LOAD_HI;
   
   
}
*/
