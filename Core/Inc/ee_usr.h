/*
 * ee_usr.h
 *
 *  Created on: Oct 14, 2026
 *      Author: diabo
 */

#ifndef INC_EE_USR_H_
#define INC_EE_USR_H_

#include "main.h"
#include "stm32f1xx_hal_flash.h"

struct usr_data_t {
	uint32_t CalZero, CalFull;
	uint16_t OnTimeSec;
};
#define EE_ENTRY_SIZE 16
#endif /* INC_EE_USR_H_ */
