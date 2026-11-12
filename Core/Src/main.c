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

#ifdef DO_DBG
void DbgIOSet(int x){
	if(  x>= 0 )
		HAL_GPIO_WritePin(DBG_IO_GPIO_Port,DBG_IO_Pin,x&1);
	else {
		HAL_GPIO_TogglePin(DBG_IO_GPIO_Port,DBG_IO_Pin);
	}
}
#else
#	define DbgIOSet(x) (void)0
#endif

int MeasPeriodSec=60;//shall be 1min ode debuhing issue bad value set 5


/**SPI1 GPIO Configuration
PA5     ------> SPI1_SCK
PA6     ------> SPI1_MISO
PA7     ------> SPI1_MOSI
*/

uint8_t CkBuf[64];
uint8_t DiBuf[64];
struct Hx711_t hx711 = {
		.hspi = &hspi1, .CkBuf=CkBuf, .DiBuf=DiBuf,
		.RawOffset = 0 //todo get this from eeprom or calib
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
  if(GPIO_Pin == R_BUT_Pin || GPIO_Pin == L_BUT_Pin){
	  StopResume();
	  task.active.user = 1; //safer that ony relying i/o chg is seen on the main loop
  }
}
#define MIN_CAL_PERCENT 20
int CalPercent=MIN_CAL_PERCENT; //not less than min
int32_t W0=15651106;        //sg unit aprox 1mg , 1m diam 8 320g
int32_t W100=15579225-10*1000; // 50g less when fully under water

struct SgTask_t {
#define SG_REP_4_AVG	3
	int RepMeas; // on wake up run  this amount of measure in cont mode then step to single/off
	int Sum;
	int Avg;
	int n;
	int skiped;
} Task_SG;;

void SgWakeUp(){
	if( task.active.SG ==0   ){
		task.active.SG = 1;
		//shall we take care we lay be in interactive mdoe so already in cont mode and averaged value ?
#if 0
		Task_SG.Sum=0; // reset average:
		Task_SG.n = 0;
#endif
		Task_SG.RepMeas = SG_REP_4_AVG; //wait to acquire new avg but keep old average (almost same if we reset)
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


const char *_MotnthStr[12]={
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul","Aug", "Sep", "Oct", "Nov", "Dec"
};
const char * MonthName(uint8_t m){
	if( m< 1) m= 1;
	else if( m > 12 ) m= 12;
	return _MotnthStr[m-1];
}

void dumb(){

}
#include "tui.h"

uint8_t CurPannel;
enum PanIndex_e {
	MainPannel = 0,
	CfglPannel,
	CalPannel,
	TimePannel,

};

void MainRLong() __attribute__((alias("dumb")));
void MainLShort() __attribute__((alias("dumb")));


void CalShow(){
	char s[32];
	ssd1306_Fill(Black);
	ssd1306_SetCursor(10, 10);
	sprintf(s,"W0 %d", (int)W0);
	ssd1306_WriteString(s,Font_7x10, White);
	ssd1306_SetCursor(10, 25);
	sprintf(s,"Wf %d", (int)W100);
	ssd1306_WriteString(s,Font_7x10, White);
	ssd1306_SetCursor(10, 45);
	sprintf(s,"Dif %d", (int)(W0-W100));
	ssd1306_WriteString(s,Font_7x10, White);
	ssd1306_UpdateScreen();
	HAL_Delay(1000);
	lcd.Update = 1;
}

void CalLMinus(){
	CalPercent = CalPercent<=MIN_CAL_PERCENT ? MIN_CAL_PERCENT : CalPercent-5;
	lcd.Update = 1;
}

void CalExit(){
	//we may set value saved when we enter
	CurPannel = CfglPannel;
	lcd.Update = 1;
}
void CalLAdd(){
	CalPercent = CalPercent>=95 ? 100 : CalPercent+5;
	lcd.Update = 1;
}
void CalSet(){
	// todo set calibraton value
	if( Task_SG.RepMeas == 0){
		// ensure we have averaged enough
		// point 100% fm val assuming cuurent 0 is valid
		W100=(Task_SG.Avg - W0)*100 / CalPercent+W0;
		//TODO store in ee
		CalShow();
	}
	CurPannel = CfglPannel;
}

void CalDraw(){
	char s[32];
	ssd1306_SetCursor(10, 10);
	sprintf(s,"SG %d", Task_SG.Avg);
	ssd1306_WriteString(s,Font_7x10, White);
	ssd1306_SetCursor(10, 30);
	sprintf(s,"Set %03d", CalPercent);
	ssd1306_WriteString(s,Font_7x10, White);
	ssd1306_SetCursor(10, 40);
	//kill the average ?
}


void DoCal0(){
	W0=Task_SG.Avg;

	CalShow();
}
enum CfgIndex_e {
	CfgOnTime = 0,
	CfgDate,
	CfgCal0,
	CfgCalX,
	CfgWrap = CfgCalX+1,
};

const char *CfgItems[]= {
	[CfgOnTime]= "On time",
	[CfgDate]  = "Date&Time",
	[CfgCal0]  = "Cal 0%",
	[CfgCalX]  = "Cal x%",
};

struct  {
	uint8_t CurIndex;
}Cfg;

void TimeEnter();

void CfgPrev(){
	Cfg.CurIndex = Cfg.CurIndex > 0 ?  Cfg.CurIndex-1 : CfgWrap-1;
	lcd.Update = 1;
}
void CfgExit(){
	CurPannel = MainPannel;
	lcd.Update = 1;
}

void CfgNext(){
	Cfg.CurIndex = (Cfg.CurIndex+1) % CfgWrap;
	lcd.Update = 1;
}

void CfgSet(){
	//todo base on index set next pannel or direct action );
	switch ( Cfg.CurIndex ){
	case CfgOnTime : break;
	case CfgCal0:  DoCal0(); break;
	case CfgCalX:  CurPannel= CalPannel; break;
	case CfgDate:  CurPannel= TimePannel, TimeEnter(); break;
//TODO
	}
	lcd.Update = 1;
}

void CfgDraw(){
	int Top=0, color, i;
	//todo if we have more than 5 line we'll need to handle scroll via top top+5 not to go more than last
	for( i=0; i<MIN(5,CfgWrap); i++){
		ssd1306_SetCursor(2, i*10);
		color = Cfg.CurIndex == i+Top ? Black : White;
		//align/fill based on longest string to sel ?
		ssd1306_WriteString((char*)CfgItems[Top+i], Font_7x10, color);
	}
}

uint8_t DTSel;
enum DTSel_e  {
	DT_Day = 0,
	DT_Month,
	DT_Year,
	DT_Hour,
	DT_Min,
	DT_Wrap = DT_Min+1
};

struct DTEdit_t {
	uint8_t d,mo,y,h,min,sec;
}DTEdit;
struct DTVal_t {
	uint8_t  L,H;
	uint8_t *p;
};
struct DTVal_t  DTv[]={
	[DT_Day]  = { .p= &DTEdit.d, .L=1, .H=31, }, //clip must be adjust on every month set and  28/29 for feb */ },
	[DT_Month]= { .p= &DTEdit.mo, .L=1, .H=12, },
	[DT_Year]=  { .p= &DTEdit.y, . L=26, .H=99, },
	[DT_Hour]=  { .p= &DTEdit.h, . L=0, .H=23, },
	[DT_Min ]=  { .p= &DTEdit.min,. L=0,.H=59, },
};

void DTMonthAdj(){
	DTv[DT_Day].H = DTEdit.mo==2 ? 29 : DTEdit.mo&1 ? 31:30;

}
void TimeMinus(){
	*DTv[DTSel].p = *DTv[DTSel].p <=  DTv[DTSel].L ?  DTv[DTSel].H:  *DTv[DTSel].p-1;
	if(DTSel==DT_Month){
		DTMonthAdj();
	}

}
void TimeNext(){
	DTSel = (DTSel+1)%DT_Wrap;
}

void TimeMore(){
	*DTv[DTSel].p = *DTv[DTSel].p >=  DTv[DTSel].H ?  DTv[DTSel].L:  *DTv[DTSel].p+1;
	if(DTSel==DT_Month){
		DTMonthAdj();
	}
}

void TimeEnter(){
	RTC_DateTypeDef Date;
	RTC_TimeTypeDef Time;
	HAL_RTC_GetDate(&hrtc,&Date,  RTC_FORMAT_BIN);
	DTEdit.d=Date.Date;
	DTEdit.mo=Date.Month;
	DTEdit.y=Date.Year;
	HAL_RTC_GetTime(&hrtc, &Time, RTC_FORMAT_BIN);
	DTEdit.h=Time.Hours;
	DTEdit.min=Time.Minutes;
	DTMonthAdj();
	DTSel=DT_Day;
}

void TimeSave(){
	RTC_DateTypeDef Date;
	RTC_TimeTypeDef Time;

	Date.Date= DTEdit.d;
	Date.Month= DTEdit.mo;
	Date.Year= DTEdit.y;
	//set the current value and set cfg pannel
	HAL_RTC_SetDate(&hrtc,&Date,  RTC_FORMAT_BIN);
	Time.Hours=DTEdit.h;
	Time.Minutes=DTEdit.min;

	HAL_RTC_SetTime(&hrtc, &Time, RTC_FORMAT_BIN);
	CurPannel=CfglPannel;
}

void TimeDraw(){
	char s[10];
	ssd1306_SetCursor(16, 16);
	sprintf(s,"%2d", DTEdit.d);
	ssd1306_WriteString(s,Font_7x10, DTSel== DT_Day ? Black: White);
	ssd1306_WriteString("-",Font_7x10, White);
	sprintf(s,"%3s", MonthName(DTEdit.mo));
	ssd1306_WriteString(s,Font_7x10, DTSel== DT_Month ? Black: White);
	ssd1306_WriteString("-",Font_7x10, White);
	sprintf(s,"20%02d", DTEdit.y);
	ssd1306_WriteString(s, Font_7x10, DTSel== DT_Year ? Black: White);

	ssd1306_SetCursor(16, 32);
	sprintf(s,"%2d", DTEdit.h);
	ssd1306_WriteString(s,Font_7x10, DTSel== DT_Hour ? Black: White);
	ssd1306_WriteString(":",Font_7x10, White);
	sprintf(s,"%02d", DTEdit.min);
	ssd1306_WriteString(s,Font_7x10, DTSel== DT_Min ? Black: White);

}

struct UsrTask_t {
	unsigned ButChg:1;
	unsigned WakeUp :1 ;

	uint32_t TikcChg;

	uint32_t OnTime; //time to stay one w/o button chg
	#define	USER_OFF_MSEC	5000 //for debug dev use 5 sec
} UsrTask = { .OnTime= 1000*5 };


struct TUiItem_t ConfMenu[] = {
		[0]= { .first =1, .has_sub =0, .fmt_arg = "stat clear"},


		[0]= { .last =1, .has_sub =0, },
};

void InteractiveEnd(){
	LcdPwrDown();       // turn lcd off
	task.active.user =0;
	CurPannel=MainPannel;
}

struct ButCtrl_t ButL={ .GpioPin= L_BUT_Pin, .Port=L_BUT_GPIO_Port};
struct ButCtrl_t ButR={ .GpioPin= R_BUT_Pin, .Port=R_BUT_GPIO_Port}; //todo new button

uint8_t l0x= 16; // x0 of text
uint8_t l0y = 2; //y0 of text
uint8_t Lvl0x= 0 , Lvl0y=0 , LvlW=6, LvlH=64-12;


void MainOn(){
	//todo set on
}
void MainLLong(){
	CurPannel=CfglPannel;
}
int GetLevel100(){
	int Lvl100,w;
	if( Task_SG.RepMeas ){
		return -1;
	}
	w = Task_SG.Avg <= 0 ? 0 : Task_SG.Avg;
	//if on prevent pwr off and show left time ?
	Lvl100=100*(w-W0)/(W100-W0);
	if( Lvl100<=0 )
		Lvl100=0;
	else if( Lvl100>100)
		Lvl100=100;
	return Lvl100;
}
int lvl_err;
void MainDraw(){
	char s[32];
	RTC_TimeTypeDef time;
	RTC_DateTypeDef date;
	int Lvl100;

	HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN);
	HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN);
	sprintf(s,"%2d-%3s %2d:%02d", date.Date, MonthName(date.Month),
			time.Hours, time.Minutes);
	ssd1306_SetCursor(l0x, l0y);
	ssd1306_WriteString(s,Font_7x10, White);
	Lvl100 = GetLevel100();
	if( Lvl100>= 0 ){
		ssd1306_DrawRectangle(Lvl0x, Lvl0y                      , Lvl0x+LvlW, Lvl0y+LvlH, White);
		ssd1306_FillRectangle(Lvl0x, Lvl0y+(100-Lvl100)*LvlH/100, Lvl0x+LvlW, Lvl0y+LvlH, White);
		ssd1306_SetCursor(l0x, l0y+10);
		sprintf(s,"Niv %3d%%", Lvl100);
		ssd1306_WriteString(s,Font_7x10, White);
	}
	else {
		lvl_err++;
	}

}

const struct Panel2B_t Pannels[] = {
	[MainPannel] = {.Draw=MainDraw, .LftA={.Short=MainOn   , .SName="On"  , .Long=MainLLong, .LName="Cfg"} , .RgtA={.Short=NULL, .SName ="Graph",  /* no long */} },
	[CfglPannel] = {.Draw=CfgDraw , .LftA={.Short= CfgPrev , .SName="Prev", .Long=CfgExit  , .LName="Exit"}, .RgtA={.Short= CfgNext  , .SName="Next" ,  .Long = CfgSet, .LName="Set" } },
	[CalPannel]  = {.Draw= CalDraw, .LftA={.Short=CalLMinus, .SName="-",    .Long=CalExit,   .LName="Exit"}, .RgtA={.Short=CalLAdd  ,  .SName="+",      .Long=CalSet  , .LName="Set"  } },
	[TimePannel] = {.Draw= TimeDraw,.LftA={.Short=TimeMinus, .SName="-",    .Long=TimeNext,  .LName="Next"}, .RgtA={.Short=TimeMore ,  .SName="+",      .Long=TimeSave, .LName="Save"  } }
};

void Task_User(){
	uint32_t now=HAL_GetTick();
	const struct Panel2B_t *pannel;
	if( UsrTask.WakeUp ){
		UsrTask.ButChg =1;
		if( ButGet(&ButL) == 0 && ButGet(&ButR)==0 ){
			//force sleep for 2 ms avoid bounce
			HAL_Delay(2);
			UsrTask.WakeUp = 0;
		}
		//skip button action on pwd down exit  til button get back idddle
	}
	else {
		//do menu text
		pannel = Pannels+CurPannel;
		UsrTask.ButChg = ButProc(&ButL, &pannel->LftA);
		UsrTask.ButChg|= ButProc(&ButR, &pannel->RgtA); //ony if ni chg yet avoid pannel chg by b1 and b2
	}
	if( UsrTask.ButChg ){
		task.active.user = 1;
		UsrTask.TikcChg = now;
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
}

void LcdUpdate(){
	const struct Panel2B_t *pannel;

	if( lcd.PwrOn != 0 && lcd.Update){
		ssd1306_Fill(Black);

		pannel= Pannels+CurPannel; //todo select active
		Pannel2B_DoHelp(pannel);
		if( pannel->Draw){
			pannel->Draw();
		}
#if 0
		ssd1306_SetCursor(32, 0);
		g = (int)hx711.Weight/1000;
		dg = (hx711.Weight - g*1000 )/100;
		sprintf(s,"w %d.%d", g, dg);
		ssd1306_WriteString(s, Font_7x10, White);
		ssd1306_SetCursor(32, 11);
		sprintf(s,"r %x", (unsigned)hx711.Value);
		ssd1306_WriteString(s, Font_7x10, White);
#endif
		// current lvl % value raw ?  drawing ?
		// pump on time
		// total run time ?
		ssd1306_UpdateScreen();

	}
	lcd.Update = 0; //clr req even we are off
}

void EnterStop(){
	//Ensure wake condition  button rtc (if not yet on)
	Hx711PwrDown(&hx711);

	HAL_SuspendTick();
#if 0 // needed only if irq is disabled at pwr up exit
	__HAL_GPIO_EXTI_CLEAR_IT(R_BUT_Pin);
	HAL_NVIC_ClearPendingIRQ(R_BUT_IRQN);
	HAL_NVIC_EnableIRQ(R_BUT_IRQN);

	__HAL_GPIO_EXTI_CLEAR_IT(L_BUT_Pin);
	HAL_NVIC_ClearPendingIRQ(L_BUT_IRQN);
	HAL_NVIC_EnableIRQ(L_BUT_IRQN);
#endif
	SetLed(1); // turn led off
	UsrTask.WakeUp = 1;
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
	if(  hx711.Weight == 0xFFFFFF ){
		//smthg wrong we read all 1
		Task_SG.skiped++;
		DbgIOSet(-1);
		return;
	}
	//handle pump activation on/off  timer when lvl is high
	if( Task_SG.n < SG_REP_4_AVG ){
		Task_SG.n++;
	}
	else {
		Task_SG.Sum = Task_SG.Sum*(SG_REP_4_AVG-1)/SG_REP_4_AVG;
	}
	Task_SG.Sum +=  hx711.Weight;
	Task_SG.Avg = Task_SG.Sum /  Task_SG.n;


	if( Task_SG.RepMeas  ){
	  Task_SG.RepMeas--;
	  // hx711.ContMode = 0; // after this swicth to power down
	  // => this set and measure can race with user change and brake "60us" pwr donw pulse rules if not handle
	  // make the magaement more complex it is much safer/Simpler to be always in cont mode and force pd when to stop
	  // with limited risk sck high > 60 us if we have already set next wake and took long
	}
	if( Task_SG.RepMeas ==0 ){
	  task.active.SG = 0; //put ourself off
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
  SgWakeUp(); //init restart measure avg
  task.active.user = 1 ; // keep mcu not sleeping at boot it can help debuger connect after reset button issue if we enter stop to fast
  LcdPwrOn(); // boot up with lcd
  SetLed(0);   // is revert led ctrl 0= on
  HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
