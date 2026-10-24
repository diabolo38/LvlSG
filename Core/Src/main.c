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
#include "rtc.h"
#include "spi.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "blackpil.h"
#include <string.h>
#include <stdio.h>
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "hx711.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SYS_STAT(x) (void)0
//#define  LCD_ALWAYS_ON
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

int MeasPeriodSec=60;



/**SPI1 GPIO Configuration
PA5     ------> SPI1_SCK
PA6     ------> SPI1_MISO
PA7     ------> SPI1_MOSI
*/

uint8_t CkBuf[64];
uint8_t DiBuf[64];
struct Hx711_t hx711 = {
		.hspi = &hspi1, .CkBuf=CkBuf, .DiBuf=DiBuf,
		.RawOffset = 0xedb8bc //todo get this from eeprom or calib
		// 0xedba39 next day ref
		// 0xee5fca ~ 40g
};

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi){
	if( hspi == hx711.hspi){
		Hx711_SPiComplete(&hx711);

	}
}


volatile struct Dbg_t  {
	unsigned TogleLed :1;
	unsigned DoRead :1;
	unsigned DoPwrDown :1;
	unsigned StopMode :1; //if not set then we don't go into stop simply use wfi aka debgu stop entering
}Dbg ={ .StopMode = 1 };

struct task_t task;

struct Lcd_t {
	unsigned PwrOn:1;
	unsigned Update:1;

};
struct Lcd_t lcd;

void LcdPwrOn(){
	if( lcd.PwrOn == 0){
	  HAL_GPIO_WritePin(GPIOB, LCD_PWR0_Pin|LCD_PWR1_Pin|LCD_PWR2_Pin, GPIO_PIN_SET);
	  lcd.PwrOn=1;
	  lcd.Update=1;
	  ssd1306_Init();
	}
}

void LcdPwrDown(){
	HAL_GPIO_WritePin(GPIOB, LCD_PWR0_Pin|LCD_PWR1_Pin|LCD_PWR2_Pin, GPIO_PIN_RESET);
#ifndef LCD_ALWAYS_ON
	lcd.PwrOn =0;
#endif
	lcd.Update =0;

}

void LcdUpdate(){
	char s[32];
	int g, dg;
	if( lcd.PwrOn != 0 && lcd.Update){
		ssd1306_Fill(Black);
		ssd1306_SetCursor(32, 0);
		g = (int)hx711.Weight/1000;
		dg = (hx711.Weight - g*1000 )/100;
		sprintf(s,"w %d.%d", g, dg);
		ssd1306_WriteString(s, Font_7x10, White);
		ssd1306_SetCursor(32, 11);
		sprintf(s,"r %x", (unsigned)hx711.Value);
		ssd1306_WriteString(s, Font_7x10, White);
		// current lvl % value raw ?  drawing ?
		// pump on time
		// total run time ?
		ssd1306_UpdateScreen();

	}
	lcd.Update = 0; //clr req even we are off
}



//use pwr stabdy bit ( hse/hsi t reset may to be used if hse useed) in what case clk config whall be set back
int GetStopState(){
	if (__HAL_PWR_GET_FLAG(PWR_CSR_SBF) != RESET) {
	    // The microcontroller exited from Standby mode
	    __HAL_PWR_CLEAR_FLAG(PWR_CSR_SBF); // Clear the flag for future checks
	    return -1;
	}
	return 0;

}

void StopResume(){
	HAL_ResumeTick();
	if( GetStopState() ){ //avoid all clk setup if we'r just call form exti/rtc while not stoped
		// fixme is needed ? is we used hse base
#if 0
		SystemClock_Config ();
#endif
		SYS_STAT(Resume++);
	}

}



void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if(GPIO_Pin == USR_BUT_Pin){
	  StopResume();
	  task.active.user = 1; //safer that ony relying i/o chg is seen on the main loop
  }
}


struct SgTask_t {
#define SG_REP_4_AVG	3
	int RepMeas; // on wake up run  this amount of measure in cont mode then step to single/off

} Task_SG;;

void SgWakeUp(){
	if( task.active.SG ==0   ){
		task.active.SG = 1;
		//shall we take care we lay be in interactive mdoe so already in cont mode and averaged value ?
		Task_SG.RepMeas = SG_REP_4_AVG;
		hx711.ContMode = 1;
	}
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc){
	StopResume();
	SgWakeUp();
	task.active.SG = 1;
	SetLed(0); //put on
}


//in rfmaster proj the hal alarm was somehow not causing stop exit or has issue some extra code set alarm , exti irqhandled was used.
// why ? extra overhead , "dhms" fromat of hal that was not folowed in rf master to get milli second precision ?


void RtcEnableWkUpExtiAlarm(){
#if 0
	//rf master code
	__HAL_RTC_ALARM_EXTI_CLEAR_FLAG();
	RTC_EnableAlarmIT(&hrtc);
	HAL_NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);
	HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);
	HAL_RTC_SetAlarm_IT()
#endif

}

/**
 *
 * @param sTime [in/out] time to add min:sec
 * @param m minute to add  (okf if more than 60)
 * @param s seconf o add (ok if > 60 )
 * overall m +s shall be less than 24 hr multipel day span is nto handle properly (maybe even not by  hal)
 * @return
 */
void  RtcTimeAdd(RTC_TimeTypeDef *sTime, int m, int s){
	int carry_m, carry_h;
	s+= sTime->Seconds;
	carry_m = s /60;
	sTime->Seconds = s %60;
	m +=  carry_m + sTime->Minutes;
	carry_h = m / 60;
	sTime->Minutes = m % 60;
	sTime->Hours = (sTime->Hours + carry_h)% 24;
}

void RtcSetNextWake(){
	RTC_AlarmTypeDef alarm;
	alarm.Alarm = 1;
	HAL_RTC_GetTime(&hrtc, &alarm.AlarmTime, RTC_FORMAT_BIN);
	RtcTimeAdd(&alarm.AlarmTime, 0, MeasPeriodSec);
	HAL_RTC_SetAlarm_IT(&hrtc, &alarm, RTC_FORMAT_BIN);

}

struct UsrTask_t {
	unsigned PrevBut : 1;
	uint32_t TikcChg; //tick last user buttton chg /Activate
	uint32_t OnTime;
	#define	USER_OFF_MSEC	5000 //for debug dev use 5 sec
} UsrTask = { .OnTime= 1000*15 };

struct TUiItem_t ConfMenu[] = {
		[0]= { .first =1, .has_sub =0, .fmt_arg = "stat clear"},


		[0]= { .last =1, .has_sub =0, },
};
void InteractiveEnd(){
	LcdPwrDown();       // turn lcd off
	task.active.user =0;
}

void Task_User(){
	uint32_t now=HAL_GetTick();

	int but = HAL_GPIO_ReadPin(USR_BUT_GPIO_Port, USR_BUT_Pin) != 0;
	if( but != UsrTask.PrevBut ){
		//mark  Activation
		UsrTask.TikcChg = now;

		task.active.user = 1;
		LcdPwrOn(); 	//if lcd was off it will kick the display update/Refresh if on nothing
	}
	else {
		//no chg for more than user on time , shall condition to buton is not pressed ?
		if( task.active.user) {
			if( now - UsrTask.TikcChg > UsrTask.OnTime ) {
				InteractiveEnd();
			}
		}
	}
	UsrTask.PrevBut = but;
}


void EnterStop(){
	//Ensure wake condition  button rtc (if not yet on)
	Hx711PwrDown(&hx711);

	HAL_SuspendTick();
	__HAL_GPIO_EXTI_CLEAR_IT(USR_BUT_Pin);
	HAL_NVIC_ClearPendingIRQ(USR_BUT_IRQN);
	HAL_NVIC_EnableIRQ(USR_BUT_IRQN);

	SetLed(1); // turn led off
	RtcSetNextWake();
	if( Dbg.StopMode ){
		HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
	}
	else {
		__WFI();
	}
}

void OnDataRdy(){
	lcd.Update = 1; // mark lcd to be update
	hx711.DataRdy = 0 ;
	//handle pump activation on/off  timer when lvl is high
	if( Task_SG.RepMeas  ){
	  Task_SG.RepMeas--;
	  // hx711.ContMode = 0; // after this swicth to power down
	  // => this set and measure can race with user change and brake "60us" pwr donw pulse rules if not handle
	  // make the magaement more complex it is much safer/Simpler to be alays in cont mode and force pd when to stop
	  // with limited risk sck high > 60 us if we ahve already set next wake and took long
	}
	if( Task_SG.RepMeas ==0 ){
	  task.active.SG = 0; // out ourself off
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
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */
  LedInit();
  hx711.ContMode = 1;
  Hx711PwrDown(&hx711);
  HAL_Delay(2); //Sensure we is down on  boot

  //strat in user interactive mode led on
  task.active.user = 1 ; // keep mcu not sleeping at boot it can help debuger connect after reset button issue if we enter stop to fast
  LcdPwrOn(); // boot up with lcd
  SetLed(0);   // is revert led ctrl 0= on
  HAL_NVIC_EnableIRQ(USR_BUT_IRQN);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  if( HAL_GPIO_ReadPin(USR_BUT_GPIO_Port, USR_BUT_Pin) != 0){
		  task.active.user=1;
	  }
	  if( Dbg.TogleLed ) {
		  Dbg.TogleLed =0;
		  ToggleLed();
	  }
	  if( Dbg.DoRead ){
		  Dbg.DoRead = 0;
		  Hx711Read(&hx711, 0);

	  }
	  if( Dbg.DoPwrDown ){
		  Dbg.DoPwrDown = 0;
		  Hx711PwrDown(&hx711);
	  }
	  if( hx711.DataRdy ){
		  OnDataRdy();
	  }
	  if( task.active.SG || task.active.user ){
		  Hx711Read(&hx711, 1); //read w/o rate controle
	  }
	  if( task.active.user ){
		  Task_User();
	  }
	  LcdUpdate();
	 //unless we are in user active lcd we don't kick measure before next period and datary +  RdInProgress  is our task bit
	  if( task.active.dw ) {
		  __WFI();
	  }
	  else {
		  //set next rtc time wake up and go stop
		  EnterStop();
	  }
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
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
