/*
 * ui.c
 *
 *  Created on: Oct 12, 2026
 *      Author: diabo
 */


#include "tui.h"

/**
 *
 * @param str str to ill shall have enougght space for 8 char
 * @return len of final help str
 */
int ButHelpSTtr(char *s, const struct ButAction_t *Act){
	int len, len2, MaxCp;
	if( Act == NULL ){
		strcpy(s,"./.");
		return 3;
	}
	if( Act->SName){
		len = MIN(4,strlen( Act->SName));
		strncpy(s,  Act->SName , len) ;
	}else {
		len =1;
		s[0]='.';
	}
	s[len]='/';
	MaxCp=8-len;
	len++;
	if(  Act->LName){
		len2 = MIN(MaxCp, strlen( Act->LName));
		strncpy(s+len,  Act->LName , len2) ;
	}else {
		len2 = 1;
		s[len]='.';
	}
	s[len+len2]=0;
	return len+len2;
}

#define  Pan2B_HelpXR(p) 0
#define  Pan2B_HelpY(p) (64-10)
#define  Pan2B_HelpXL(p) 127


void Pannel2B_DoHelp( const struct Panel2B_t *pannel){
	char s[32];
	int len;
	len = ButHelpSTtr(s, &pannel->LftA);
	ssd1306_SetCursor(Pan2B_HelpXR(pannel),Pan2B_HelpY(pannel));
	ssd1306_WriteString(s, Font_7x10, White);
	//todo clear area to delate prev or junk ?

	len = ButHelpSTtr(s, &pannel->RgtA);
	ssd1306_SetCursor(Pan2B_HelpXL(pannel)-len*7,Pan2B_HelpY(pannel));
	ssd1306_WriteString(s, Font_7x10, White);
}

int ButGet(struct ButCtrl_t *but){
	return HAL_GPIO_ReadPin(but->Port, but->GpioPin)!= 0;
}

int  ButProc( struct ButCtrl_t *but, const struct ButAction_t  *act ){
	int v, chg;
	uint32_t now=HAL_GetTick();
	v = HAL_GPIO_ReadPin(but->Port, but->GpioPin)!= 0;
	chg = v != but->val;
	if( chg && now -  but->TimActive < 2 ){
		//ignore/debounce wait it get stable (yet the initial chg get captured)
		return 1;
	}

	if( chg ){
		if( v == 0 && ! but->LongDone ){
			//release short
			 if( act!=NULL && act->Short)
				 act->Short();
		}
		but->LongDone = 0;
		but->TimActive = now;
	}
	else {
		if( v  && but->val ){
			//stayu press long
			if( but->LongDone == 0 &&  now - but->TimActive > BUT_LONG_TIME(but)){
				//long
				 but->LongDone = 1;
				 if( act!= NULL && act->Long)
					  act->Long();
			}
		}
	}

	but->val = v;
	return chg;
}
