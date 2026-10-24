/*
 * ee_emul.c
 *
 *  Created on: Oct 12, 2026
 *      Author: diabo
 */

#include "ee_emul.h"
#include <string.h>

/* Symbols defined in the linker script to mark the non volatile "eeprom" flash area start and end
 * we'll use this to save settings such as light intensity level, brake with no speed time max  */
extern struct EeData_t  _eedata_start;
extern struct EeData_t  _eedata_end;

/**
 * flashs may  be lock after so write ca be done w/o unlock again
 */
void EeFullErase(){
	int rc;
	FLASH_EraseInitTypeDef Erase = {
			.TypeErase = FLASH_TYPEERASE_PAGES,
			.Banks= FLASH_BANK_1,
			.NbPages = ((void*)&_eedata_end - (void*)&_eedata_start )/FLASH_PAGE_SIZE,
			.PageAddress = (uint32_t)&_eedata_start,
	};

	rc = HAL_FLASH_Unlock();
	if( rc == 0 ){
		uint32_t Err;
		rc = HAL_FLASHEx_Erase(&Erase, &Err);
		HAL_FLASH_Lock();
	}
}

uint32_t SumSerie(uint8_t *Data, int n){
	uint32_t sum;
	int i;
	for( i=0, sum=0; i<n; i++)
		sum+=Data[i];
	return sum;
}
// ee start end in address sense , bottom of flash area but end of the nvm
// using stack like top to bottom is prefered vs bot top . It enable extension/grow on f/W upgrade
//  (bot top don't enbale it) yet shrink space is not direct. The eedata if to be shrink nned to be move on top
// before flashing the new f/w.
//if not and ee data content is robust the trashed  nvm entry (now code) shall be detect bad and f/w will restart
//clean default , basiclay ok but previous stored settings lost.

/**
 *
 * @return NULL if none required erase
 */
struct EeData_t * EeFirstFree(){
	struct EeData_t *pEE;
	pEE = &_eedata_end-1;//topmost last/first entry
	while( pEE > &_eedata_start  && pEE->EeCtrl != 0xFF  ){
		pEE--;
	}
	// we could check the full entry if to be oversafeQ
	if( pEE->EeCtrl !=  0xFF ){
		//reach end w/o free entry need to full erase the area and start again
		pEE=NULL;
	}
	return pEE;
}

const struct EeData_t * EeActiveEntry(){
	struct EeData_t *pEE;
	pEE = &_eedata_end-1;//topmost last/first entry
	while( pEE > &_eedata_start  && pEE->EeCtrl != EE_CTRL_VALID  && pEE->EeCtrl!=0xFF  ){
		pEE--;
	}
	// we could check the full entry if to be oversafeQ
	if( pEE->EeCtrl !=  EE_CTRL_VALID ){
		//reach end w/o free entry need to full erase the area and start again
		pEE=NULL;
	}
	return pEE;
}
/**
 * @return true if valid
 */
int EeIsValidEntry(const struct EeData_t *pEe){
	return (SumSerie((void*)pEe, sizeof(*pEe)-1)&0xFF) == pEe->Sum;
}
/**
 *
 * @param pSet [in] all value [out]  ctrl+checksum udpated
 * @return
 */
int EeSetEntry(struct EeData_t *pSet){
	struct EeData_t *pFree;
	struct EeData_t *pPrev=NULL;
	uint16_t  *psw, *pdw; //Src dest word
	int rc,left;

	pSet->EeCtrl = 0xA5;
	pSet->Sum=SumSerie((void*)pSet, sizeof(*pSet)-1);

	pFree = EeFirstFree();
	if( pFree == NULL ){
		Log("Eerase all\n");
		EeFullErase();
		pFree=(&_eedata_end)-1;
	}
	if( pFree+1 < &_eedata_end ){
		pPrev=pFree+1;
	}
	rc = HAL_FLASH_Unlock();
	for( left = sizeof(*pSet), psw=(void*)pSet, pdw=(void*)pFree;
			left >0;
			left-=2, psw++, pdw++ ){

		rc = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, (uint32_t)pdw, *psw);
		if( rc ){
			Log("EeW %d\n", left);
			goto done_l;
		}
	}
	//invbaldiate orevious now we set new (woudl be find last anyway in seek first using free)
	if( pPrev ){
		rc = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, (uint32_t)pPrev, 0);
		Log("ERel %p\n", pPrev);
		//what to do ? can it happen
	}
done_l:
	HAL_FLASH_Lock();
	return rc;

}
//if run twice a full erase shall occur
void ee_test(){
	const struct EeData_t * Activ=NULL;
	struct EeData_t Ref;
	uint8_t *b=(void*)&Ref;
	int i,rc, ok;
	//setup ref data with adress pattern
	for( i=1; b<&Ref.Sum; b++,i++ )
		*b=i;

	Activ = EeFirstFree();
	Log("ee test org free %p\n",Activ);

	i=0;
	do{
		rc = EeSetEntry(&Ref);
		Activ=EeActiveEntry();
		ok=memcmp(Activ, &Ref, sizeof(Ref));
		Log("Set %3d rc %d cmp %d\n",i,rc,ok);

		i++;
		memset(&Ref.ee_data, i, sizeof(Ref.ee_data)); //some chg in pattern
	}while( EeFirstFree()!=NULL );
	(void)ok;
	(void)rc;

}

void ee_check(){
	volatile int x=0;
	if( x )
		ee_test();
}

