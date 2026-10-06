/*
 * bsp.c
 *
 *  Created on: Nov 25, 2023
 *      Author: sanchesm
 */

#include "main.h"

#define LED_PORT GPIOB
#define LED_PIN	GPIO_PIN_12
#define LED_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE();


void ToggleLed(){
	HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
}
void SetLed(int x){
	HAL_GPIO_WritePin(LED_PORT, LED_PIN, x);
}

void LedInit() {

	GPIO_InitTypeDef GPIO_InitStruct = {0};
	LED_CLK_ENABLE();
	GPIO_InitStruct.Pin = LED_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);
}


