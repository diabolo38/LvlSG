/*
 * hx711.c
 *
 *  Created on: Oct 24, 2026
 *      Author: diabo
 */
#include "hx711.h"
#include <string.h>

int Hx711Read( struct Hx711_t *dev, int RateCtrl){
	int rc, i, nBytes;
	uint32_t now;
	if( dev->RdInProgress ){
		return 1;
	}
	// quick dirty test measure rate 1 per sec
	now = HAL_GetTick();
	if( RateCtrl && now- dev->LastTick   < 200 )
		return 1;
	dev->LastTick = now;
	Hx711SetTaskActive(*dev);



	//if dev in pwr down send 0 lower sck and wait for rdy
	if( dev->state == Hx711_PwrDown ){
		//fall scl byt writing 0
		dev->CkBuf[0]=0;
		rc = HAL_SPI_Transmit(dev->hspi, dev->CkBuf, 1, 2);
		//todo handle err ?
		dev->state = Hx711_On;
	}
	//Wait data rdy that is spi rd shall be all 0 or at least b0 = 0 fall on last bit while sending 00  to keep active
	//f103 emilt what is teh rd buffer but otehr mcu lay differ
	do{
		dev->CkBuf[0]=0;
		rc = HAL_SPI_TransmitReceive(dev->hspi, dev->CkBuf, dev->DiBuf, 1, 2);
			//todo handle err ?
	}while( dev->DiBuf[0]&1 && rc == 0);
	// rc != 0 mean error in spi ?
	//fixe we shall rise sck for non cont mode
	if( rc ){
		dev->WairRdyErr=1;
		dev->ErrCnt++;
		return -1;
	}

	memset(dev->CkBuf, 0, dev->FrontBytes);
	for( i=0; i<24/4;i++)
		dev->CkBuf[dev->FrontBytes+i]=0xAA;
	// we have a small idle 1.12us  every 2  and last byte ( dma reload ? last process by irq ?)
	// this can be limited by sending an extra 0x00 o all clk ecge will be same
	//for scaling 1 2 3 extra clk shall be added so one byte with leading/trailing
	//idle state of the mosi is last emited bit adding extra 0x00 or 0xFF => will help forcing start/idle ck state
	//initial/reset can be obtained by sending enough (proch ) 0xFF to fill 60us
	// conv ready an be check by sending a 0x00 aka no sck and reading data
	// if any bit is 0 so whal be bit 8 conv is rdy

	if( dev->ContMode ){
		//add extra 0x00 to keep active else it will stay high
		dev->CkBuf[dev->FrontBytes+i]=0x00;
		i++;
	}

	dev->state = Hx711_On;
	dev->RdInProgress = 1;
	dev->DataRdy = 0;
	nBytes = dev->FrontBytes+i;
	rc = HAL_SPI_TransmitReceive_DMA(dev->hspi, dev->CkBuf, dev->DiBuf, nBytes);
	if( rc != 0){
		dev->RdInProgress = 0;
		dev->ErrCnt++;
		//fixme pwr off on ? may be hard to say we don't know what do is forcing pd may be a better option if ! cont mode
	}
	return rc;
}

int Hx711PwrDown(struct Hx711_t  *dev){
	int rc;
	//todo check  we're done on spi / dev state to ensure sae to change state and use spi to rise sck
	uint8_t d=0xFF;
	rc = HAL_SPI_Transmit(dev->hspi, &d, 1, 2);
	HAL_Delay(2);
	//fixme decide if to used dma  many 0xFF or other means to ensure delay or not
	dev->state  = Hx711_PwrDown;
	return rc;
}

void Hx711_SPiComplete(struct Hx711_t  *dev){
	int i;
	dev->RdInProgress = 0;

	int c, bpos, bit, bitm;
	uint32_t v;
	// de-serialize data from spi read buffer
	for(v=0,  i=0; i < 24; i++ ){
		v<<=1;
		bpos= dev->FrontBytes + i / 4; // one byte for 4 bits
		c = dev->DiBuf[bpos];
		// bit pos is (i %4) * 2 , we send 1 0 1 0 so wed read on 0101  bit 6 4 2 0   hx send msb first
		bit = 6-(i % 4)*2;
		bitm = 1 << bit;
		if( c&bitm )
			v|= 1;
	}
	dev->Value = v;
	dev->Weight = dev->Value - dev ->RawOffset;

	if( !dev->ContMode ){
		dev->state= Hx711_PwrDown;
		//fixme to pd before pd handlin ?ng
	}
	else {
		dev->state= Hx711_On;
	}
	dev->DataRdy = 1;

}
