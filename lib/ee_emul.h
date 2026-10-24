/**
 * @file ee_emul.h
 *
 * @date: Oct 12, 2026 ( taken fro edklight project)
 * @author michel  diabolo38
 */

#ifndef EE_EMUL_H_
#define EE_EMUL_H_

#include "ee_usr.h" //user/project/ macro & wrapper
//user code to define EE_ENTRY_SIZE  a mulitple of at least 2 (4) and full size 'ee sapve in fhasl)  is multiple too
#ifndef EE_ENTRY_SIZE
#	pragma message "using default 16 (14 user) byte per ee entry"
#	define EE_ENTRY_SIZE 16
#endif

/**
 * the EeData_t  struct must include all item to save by batch in the ee
 *
 * repeating the the ee data struct in 1+ flahs block/page mananing an erase engtri control flags
 * help not  reaching Mcu flash  limited erase/write cycle count  limits
 *
 * struct sized to be multiple of 2 or 4  (  stm32 flash  may  write/Erase by 2 byte or page )
 * the flash memory size constrain:
 *   - best be a multiple of the struct size ( simpler bound check no need to check start +size  < end  is enough
 *   - must be an FLASHP_PAGE_SIZE multiple  for erase
 *
 * the ld shall provide the sylbol to locate the ee area and  teh ee sectiion + mem
 *
 _eedata_end  = ORIGIN(EEPROM) + LENGTH(EEPROM);
_eedata_start  = ORIGIN(EEPROM);

MEMORY
{
 FLASH    (rx)    : ORIGIN = 0x8000000,   LENGTH = 63K <= notice 64k reduce by 1K below
 EEPROM   (rx)    : ORIGIN =  ORIGIN(FLASH) + LENGTH(FLASH),   LENGTH = 1K
 ...
}

SECTIONS
{
 ....
  .eedata  :
  {
    *(.eedata)
    *(.eedata*)
  } > EEPROM

  ....
}
*/
/**
 * keep EeCtrl first
 *
 * add  any  items in between
 * adjust/Del res[sz]  to get full size  2 or 4 byte  align ee size mutiple
 *
 * keep Sum last
 *
 */
struct EeData_t {
#define EE_CTRL_VALID	0xA5
#define EE_CTRL_DEL		0x00
	// user data below
	struct usr_data_t ee_data;
	//update SetConfFromEe and SaveSetting when adding or modifying here

	//!<  adjust/Del to overall struct   size  2 or 4 byte  + aligned and an ee size multiple
	uint8_t res[EE_ENTRY_SIZE-2-sizeof(struct usr_data_t)];
	uint8_t Sum;  //!< keep this last use for entry validity  check ( mcu issue pwr lost during entry write)
	uint8_t EeCtrl; //!< keep this first  /for free/use next management 0 deleted not use anymore 0xFF free (erased value) 0x5 vald dentry

};

int EeSetEntry(struct EeData_t *pSet);
const struct EeData_t * EeActiveEntry();
int EeIsValidEntry(const struct EeData_t *pEe);
void EeFullErase();

#ifndef Log
#define Log(...) (void)0
#endif

#endif /* EE_EMUL_H_ */
