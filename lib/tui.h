/*
 * tui.h
 *
 *  Created on: Oct 24, 2026
 *      Author: diabo
 */

#ifndef TUI_H_
#define TUI_H_

#include <stdint.h>
#include <string.h>
#include "main.h"
#include "ssd1306.h"
#ifndef MIN
#	define MIN(a,b) ((a)<(b)?(a):(b))
#endif

struct TUiItem_t {
	unsigned first:1;  // first of list
	unsigned last:1;   // last of list
	unsigned has_sub:1; // has sube menu => how to interpret p
//few extra bit could be usefull for status (if not rom/const ro)

	int (*fmt)(struct TUiItem_t *ui); // text rendering on null printf wit
	void * fmt_arg;
	/**
	 *  terminnal item the value
	 *  sub: the sub menu first entry
	 */
	void *sub;

};

struct ButCtrl_t {
	// overall size may be reduced using byte if we have < 4 items on end
	unsigned val:1;
	unsigned LongDone:1;
	uint32_t TimActive;
#define BUT_LONG_TIME(but) 1500

	uint16_t GpioPin;
	//if it stay on too long it may retain task on and use  power
	// some extra flags could help to manage this aka ignore next rel acquire prev state
	GPIO_TypeDef *Port;

};

struct ButAction_t {
	void (*const Short)();
	void (*const Long)();
	const char *SName, *LName; // null is accepected
};




int ButGet(struct ButCtrl_t *but);

/**
 *
 * @param but
 * @param act action fn for short long press
 * @return non 0 if state chnaged
 */
int ButProc( struct ButCtrl_t *but, const struct ButAction_t  *act );


struct Panel2B_t {

	struct ButAction_t LftA,RgtA;
	void (*Draw)(void);

};

void  Pannel2B_DoHelp(const struct Panel2B_t *pannel);

#endif /* TUI_H_ */
