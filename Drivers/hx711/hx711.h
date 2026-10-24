/**
 * @file hx711.h
 * @brief hx711 strain gauge ic spi mode driver stm32 + hal + dma
 *
 * konw issue todo
 *
 * right now continuous/single mode measure to measure rate not fully robust missing delay check and control
 *  aka PD (power down) state may no be entered if user don't grant is wan't call new measure in more than 60us
 *
 * int Hx711PwrDown(struct Hx711_t  *dev) is not safe robust not checking measure ngoing
 *
 *  Created on: Oct 24, 2026
 *      Author: diabo
 */

#ifndef HX711_HX711_H_
#define HX711_HX711_H_

#include "main.h"

enum Hx711State_e {
	Hx711_On      = 0,
	Hx711_PwrDown = 1,

};

struct Hx711_t {
	SPI_HandleTypeDef *hspi;  // clock shall not exceed  10 MHz so that 2 clk 1 mosi 0.2us  > min th/tl  for hx711 ck (max is 50us)
	uint8_t FrontBytes; // front 0x00 byte  help ensuring 60us before 1st CLk high pulse ==> ensure pwr down , reset  then re start  normal mode
	// shall not be used if scaling is in place ? ref by https://github.com/crjeder/hx711_spi
	unsigned RdInProgress : 1;
	unsigned DataRdy      : 1;
	unsigned ContMode     : 1;
	// when not set no extra 0x0 sent after measure, stm32f103 seam to keep sck high when not full 0 last send
	//   what leikeyh to cause hx to enter pd if next don't come fast  < 60 us
	// when set hx an extra 0x00 is sent so sck stay low to ensure hx711 don't enter pd  and perform next measure continusloy

	unsigned WairRdyErr   : 1; // Sticky set if wait for rdy occur
	unsigned DoMeasErr    : 1; // Sticky set if error in rxtx for meas


	int ErrCnt;
	int32_t Value;	// raw read

	int32_t Weight;

	uint32_t LastTick;
	uint8_t *CkBuf;
	uint8_t *DiBuf;

	int state;

	uint32_t RawOffset; //value for no weight

};

#ifndef Hx711SetTaskActive
/** user wrapper/hook for  rtos/power down control app =>  hx711 is curenlty running an xfer
 * and waiting for assynch completion of spi transfer mcu must not got into stop/standby step (sleep wfi is ok)
 */
#	define Hx711SetTaskActive(...) (void)0
#	pragma message "no standy disable task control defined none set as default"
#endif

/**
 *  perform next measure
 *
 *  minimal rate hal tcik based is done, but may be overwaiting in case the tick get suspended
 *     resteting dev->LastTick can be a work arround
 * @param dev
 * @param RateCtrl
 * @return
 */
int Hx711Read( struct Hx711_t *dev, int RateCtrl);

/**
 * @brief : put/truy to ensure PD by ensuring sck high (sent all 1 on spi)
 * @warning : not safe robust
 *   will try to  force pd and use spi wahetever is the current state , may not work , not do what expected
 *   igf spi is already busy measure ongoing
 *
 * @param dev
 * @return
 */
int Hx711PwrDown(struct Hx711_t  *dev);

/**
 * spi complete call back to be called by user on it's top spi completed handler when hdspi source it for dev
 *
 * @param dev
 */
void Hx711_SPiComplete(struct Hx711_t  *dev);



#endif /* HX711_HX711_H_ */
