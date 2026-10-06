/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "blackpil.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
struct Hx711_t {
	SPI_HandleTypeDef *hspi;  // clock shall not exceed  10 MHz so that 2 clk 1 mosi 0.2us  > min th/tl  for hx711 ck (max is 50us)
	uint8_t FrontBytes; // front byte count to get at leMHz 60us  so SCL high pule is seen as pwr down , reset  then re start  normal mode
	// shall not be used if scaling is in place ? ref by https://github.com/crjeder/hx711_spi
	unsigned RdInProgress : 1;
	unsigned DataRdy      : 1;
	int ErrCnt;
	uint8_t *CkBuf;
	uint8_t *DiBuf;
};



int Hx711Read( struct Hx711_t *dev){
	int rc, i, nBytes;
	if( dev->RdInProgress ){
		return 1;
	}
	memset(dev->CkBuf, 0, dev->FrontBytes);
	for( i=0; i<24/4;i++)
		dev->CkBuf[dev->FrontBytes+i]=0xAA;
	// we have a small idle 1.12us  every 2  and last byte ( dma reload ? last process by irq ?)
	// this can be limited by sending an extra 0x00 o all clk ecge will be same
	//for scaling 1 2 3 extra clk shall be added so one byte with leading/trailing
	//idle state of the mosiis last emited bit adding extra 0x00 or 0xFF => will help forcing start/idle ck state
	//initial/reset can be obatined by sending enought (proch ) 0xFF to fill 60us
	dev->RdInProgress = 1;
	dev->DataRdy = 0;
	nBytes = dev->FrontBytes+i;
	rc = HAL_SPI_TransmitReceive_DMA(dev->hspi, dev->CkBuf, dev->DiBuf, nBytes);
	if( rc != 0){
		dev->RdInProgress = 0;
		dev->ErrCnt++;
	}
	return rc;
}

void Hx711_SPiComplete(struct Hx711_t  *dev){
	dev->RdInProgress = 0;
	dev->DataRdy = 1;
	//todo deserilaize data from read data
}
/**SPI1 GPIO Configuration
PA5     ------> SPI1_SCK
PA6     ------> SPI1_MISO
PA7     ------> SPI1_MOSI
*/

uint8_t CkBuf[64];
uint8_t DiBuf[64];
struct Hx711_t hx711 = { .hspi = &hspi1, .CkBuf=CkBuf, .DiBuf=DiBuf};

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi){
	if( hspi == hx711.hspi){
		Hx711_SPiComplete(&hx711);

	}
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  LedInit();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  ToggleLed();
	  Hx711Read(&hx711);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
