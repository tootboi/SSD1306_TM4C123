// SSD1306.c
// Code for TM4C123GH6PM

#include "SSD1306.h"
#include "ASCII.h"

//variables
unsigned char slaveAddr = 0x3C;
unsigned char ssd1306_buffer[1024];	//128*64/8. every pixel is a bit, and all fit in memory
#define stop_bit 	0x00000004
#define start_bit 	0x00000002
#define run_bit		0x00000001

// initialise the oled display
void ssd1306_init() {
	delayms(100);		//delay for ssd1306 power up
	ssd1306_command(SSD1306_DISPLAYOFF);			// 0xAE
	ssd1306_command(SSD1306_SETDISPLAYCLOCKDIV);	// 0xD5
	ssd1306_command(0x80);							//suggested divide ratio
	ssd1306_command(SSD1306_SETMULTIPLEX);			// 0xA8
	
	ssd1306_command(0x3F); // height-1 = 31
	
	ssd1306_command(SSD1306_SETDISPLAYOFFSET);	// 0xD3
	ssd1306_command(0x0);						// no offset
	ssd1306_command(SSD1306_SETSTARTLINE);		// 0x40; line #0.
	ssd1306_command(SSD1306_CHARGEPUMP);		// 0x8D
	
	ssd1306_command(0x14);
	
	ssd1306_command(SSD1306_MEMORYMODE);		// 0x20
	ssd1306_command(0x00);						//
	ssd1306_command(SSD1306_SEGREMAP | 0x1);	//
	ssd1306_command(SSD1306_COMSCANDEC);		//
	
	ssd1306_command(SSD1306_SETCOMPINS);
	ssd1306_command(0x12);
	
	ssd1306_command(SSD1306_SETCONTRAST);
	ssd1306_command(0xCF);
	
	ssd1306_command(SSD1306_SETPRECHARGE);	// 0xD9
	ssd1306_command(0xF1);
	
	ssd1306_command(SSD1306_SETVCOMDETECT);	// 0xDB
	ssd1306_command(0x40);
	ssd1306_command(SSD1306_DISPLAYON);	// Main screen turn on
	ssd1306_clear();
	ssd1306_update();
}

// send command to slave
void ssd1306_command(unsigned char c) {
	setSlaveAddr(slaveAddr, 0);
	writeByte(0x00, start_bit | run_bit);
	writeByte(c, stop_bit | run_bit);
}

// update all pixels to display
void ssd1306_update() {
	ssd1306_command(SSD1306_PAGEADDR);
	ssd1306_command(0);
	ssd1306_command(0xFF);
	ssd1306_command(SSD1306_COLUMNADDR);
	ssd1306_command(0);
	ssd1306_command(128 - 1); // Width
	
	unsigned short count = 1024-1;
	unsigned char *ptr = ssd1306_buffer;
	setSlaveAddr(slaveAddr, 0);
	writeByte(0x40, start_bit | run_bit);
	while(count--) {
		writeByte(*ptr++, run_bit);
	}
	writeByte(*ptr++, stop_bit | run_bit);
}

// set pixel in buffer. Call ssd1306_update() to push to display.
void ssd1306_drawPixel(unsigned char x, unsigned char y, unsigned char color) {
	if((x>=128)||(y>=64)) {
		return;
	}
	if(color == 1) {
		ssd1306_buffer[x+(y/8)*128] |= (1 << (y&7));
	} else {
		ssd1306_buffer[x+(y/8)*128] &= ~(1 << (y&7));
	}
}

// clear buffer
void ssd1306_clear() {
   for(unsigned long i=0; i<sizeof ssd1306_buffer; ++i)
		ssd1306_buffer[i]=0;
}

// saves a byte of data to buffer
void ssd1306_drawByte(unsigned char x, unsigned char y, unsigned char data) {
	unsigned char mask;
	unsigned char yOffset = 7;
	for(mask=0x80;mask!=0;mask>>=1) {		// loop through every bit. starting from MSB
		if(data & mask) {	// bit is 1
			ssd1306_drawPixel(x, y+yOffset, 1);
		} else {	// bit is 0
			ssd1306_drawPixel(x, y+yOffset, 0);
		}
		yOffset--;
	}
}

// puts a character in buffer
void ssd1306_drawChar(unsigned char x, unsigned char y, unsigned char data) {
	unsigned char i;
	ssd1306_drawByte(x, y, 0x00); // padding column
	for(i=0; i<5; i++) {
		ssd1306_drawByte(x+i+1, y, ASCII[data-0x20][i]);
	}
	ssd1306_drawByte(x+6, y, 0x00); // padding column
}

// puts a string in buffer
void ssd1306_drawString(unsigned char x, unsigned char y, char *ptr) {
	unsigned char newX = x;
	unsigned char newY = y;
	while(*ptr) {
		ssd1306_drawChar(newX,newY,(unsigned char)*ptr++);
		newX+=7;
		if(newX > 127-7) {	// wraps text when it reaches end of display (x-axis)
			newY += 9;
			newX = 0;
		}
	}
}

// puts remaining data of bmp in buffer
void ssd1306_drawNonByteBitmap(unsigned char x, unsigned char y, unsigned char remainder, unsigned char data) {
	unsigned char mask;
	unsigned char yOffset = 0;
	unsigned char rowCount = 0;
	for(mask=0x01;mask<0x80;mask<<=1) {	// loop through byte. starting with LSB
		if(rowCount<remainder) {	// only draw necessary rows
			if(data & mask) {	// bit is 1
				ssd1306_drawPixel(x,y+yOffset,1);
			} else {	// bit is 0
				ssd1306_drawPixel(x,y+yOffset,0);
			}
			yOffset++;
			rowCount++;
		}
	}
}

// puts bitmap in buffer
// when creating a bitmap, make sure the height is a multiple of 8
// leave the unused rows blank, and specify the height wanted
void ssd1306_drawBitmap(unsigned char x, unsigned char y, unsigned char bitmap[], unsigned char bmpWidth, unsigned char bmpHeight) {
	unsigned char *ptr = bitmap;
	unsigned short count = (bmpWidth * bmpHeight) / 8;	// bytes in bmp
	unsigned short ogCount = count;
	unsigned char newX = x;
	unsigned char newY = y;
	unsigned char remainderH=0;
	if(bmpHeight%8!=0) {	// check if height of bmp is NOT multiple of byte (8 bits)
		remainderH = bmpHeight%8;
		count = (bmpWidth*(bmpHeight-remainderH)) / 8;
		ogCount = count;
	}
	while(count--) {
		if((count+1)!=ogCount && (count+1)%bmpWidth==0) {	// wraps image reaches end of image width
			newY += 8;
			newX = x;
		}
		ssd1306_drawByte(newX++,newY,(unsigned char)*ptr++);
	}
	if(remainderH) {	// draw rows that do not fill a byte
		newX = x;
		newY += 8;
		unsigned remainerCount = bmpWidth;	// number of columns
		while(remainerCount--) {
			if((count+1)!=ogCount && (count+1)%bmpWidth==0) {	// wraps image reaches end of image width
				newY += 8;
				newX = x;
			}
			ssd1306_drawNonByteBitmap(newX++,newY,remainderH,(unsigned char)*ptr++);
		}
	}
}

// delay in ms. Not terribly accurate.
void delayms(unsigned long ms){
	unsigned long count;
	while(ms > 0 ) {
		count = 2500;
		while (count > 0) { 
			count--;
		}
		ms--;
	}
}
