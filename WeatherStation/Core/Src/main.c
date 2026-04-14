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
#include "stdio.h"
#include "bme280.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
// Screen types for OLED display
typedef enum {
	SCREEN_TEMP = 0,    // Temperature screen
	SCREEN_HUM,         // Humidity screen
	SCREEN_PRESS,       // Pressure screen
	SCREEN_CLOCK        // Large clock display
} ScreenMode;

// System operating modes
typedef enum {
	MODE_NORMAL = 0,    // Normal display mode
	MODE_SET_TIME       // Time adjustment mode
} SystemMode;

// Editable fields for RTC
typedef enum {
	EDIT_HOUR = 0, EDIT_MIN, EDIT_SEC
} EditField;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define BME280_ADDR (0x76 << 1)   // SDO = GND
#define BME280_REG_ID     0xD0
#define BME280_REG_RESET  0xE0
#define BME280_REG_CTRL_HUM 0xF2
#define BME280_REG_STATUS 0xF3
#define BME280_REG_CTRL_MEAS 0xF4
#define BME280_REG_CONFIG 0xF5
#define BME280_REG_DATA   0xF7
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

/* USER CODE BEGIN PV */
extern struct bme280_t bme280;
char buffer[30];
volatile ScreenMode currentScreen = SCREEN_TEMP;
volatile SystemMode currentMode = MODE_NORMAL;

volatile uint8_t btn_next = 0;
volatile uint8_t btn_up = 0;
volatile uint8_t btn_down = 0;

uint32_t lastPressTime_next = 0;
uint32_t lastPressTime_up = 0;
uint32_t lastPressTime_down = 0;
uint32_t lastPressTime_mode = 0;
uint8_t systemOn = 1;
uint32_t nextPressStart = 0;
uint8_t nextPressed = 0;
volatile uint8_t btn_long = 0;
volatile uint8_t btn_power = 0;
volatile EditField currentField = EDIT_HOUR;

RTC_TimeTypeDef sTime;
RTC_DateTypeDef sDate;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/*uint8_t BME280_ReadID(void) {
 uint8_t id;
 HAL_I2C_Mem_Read(&hi2c1, BME280_ADDR, BME280_REG_ID, 1, &id, 1, 100);
 return id;
 }*/

/*void BME280_Init(void) {
 uint8_t data;

 // Reset
 data = 0xB6;
 HAL_I2C_Mem_Write(&hi2c1, BME280_ADDR, BME280_REG_RESET, 1, &data, 1, 100);
 HAL_Delay(100);

 // Humidity oversampling x1
 data = 0x01;
 HAL_I2C_Mem_Write(&hi2c1, BME280_ADDR, BME280_REG_CTRL_HUM, 1, &data, 1,
 100);

 // Temp + Pressure oversampling x1, normal mode
 data = 0x27;
 HAL_I2C_Mem_Write(&hi2c1, BME280_ADDR, BME280_REG_CTRL_MEAS, 1, &data, 1,
 100);

 // Standby + filter
 data = 0xA0;
 HAL_I2C_Mem_Write(&hi2c1, BME280_ADDR, BME280_REG_CONFIG, 1, &data, 1, 100);
 }*/

// Reads temperature, pressure and humidity from BME280
void BME280_GetData(float *temp, float *press, float *hum) {

	// Force a new measurement
	bme280_set_power_mode(BME280_FORCED_MODE);
	HAL_Delay(50);

	s32 ut, up, uh;

	// Read raw sensor values
	bme280_read_uncomp_pressure_temperature_humidity(&up, &ut, &uh);

	// Convert raw values to real-world units
	s32 t = bme280_compensate_temperature_int32(ut);
	u32 p = bme280_compensate_pressure_int32(up);
	u32 h = bme280_compensate_humidity_int32(uh);

	*temp = t / 100.0;
	*press = p / 100.0;
	*hum = h / 1024.0;
}

// External interrupt handler for all buttons
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	uint32_t now = HAL_GetTick();

	// ===== NEXT BUTTON (PA5) =====
	// Detect short press and long press
	if (GPIO_Pin == GPIO_PIN_5) {
		if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET) {
			// Button pressed -> start timing
			nextPressStart = HAL_GetTick();
			nextPressed = 1;
		} else {
			// Button released -> calculate press duration
			if (nextPressed) {
				uint32_t pressTime = HAL_GetTick() - nextPressStart;

				if (pressTime > 800)
					btn_long = 1; // Long press -> enter/exit SET TIME
				else
					btn_next = 1; // Short press -> change screen

				nextPressed = 0;
			}
		}
	}

	// ===== UP BUTTON (PA1) =====
	else if (GPIO_Pin == GPIO_PIN_1) {
		if (now - lastPressTime_up > 200
				&& HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET) {
			btn_up = 1; // Increase selected time field
			lastPressTime_up = now;
		}
	}

	// ===== DOWN BUTTON (PB0) =====
	else if (GPIO_Pin == GPIO_PIN_0) {
		if (now - lastPressTime_down > 200
				&& HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_RESET) {
			btn_down = 1; // Decrease selected time field
			lastPressTime_down = now;
		}
	}

	// ===== POWER BUTTON (PA4) =====
	else if (GPIO_Pin == GPIO_PIN_4) {
		if (now - lastPressTime_mode > 200
				&& HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET) {
			btn_power = 1; // Toggle display ON/OFF
			lastPressTime_mode = now;
		}
	}
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();
	SystemClock_Config();

	MX_GPIO_Init();
	MX_I2C1_Init();
	MX_RTC_Init();

	ssd1306_Init();
	ssd1306_Fill(Black);
	ssd1306_UpdateScreen();

	/* USER CODE BEGIN 2 */

	I2C_routine();          // Bosch I2C
	bme280_init(&bme280);  // calibration load
	bme280_set_power_mode(BME280_NORMAL_MODE);
	float temp, press, hum;

	while (1) {
		ssd1306_Fill(Black);

		// ===== RTC READ =====
		HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
		HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

		// ===== SENSOR READ =====
		BME280_GetData(&temp, &press, &hum);

		// ===== DISPLAY CURRENT TIME =====
		if (currentMode == MODE_NORMAL) {
			sprintf(buffer, "%02d:%02d:%02d", sTime.Hours, sTime.Minutes,
					sTime.Seconds);
			ssd1306_SetCursor(70, 0);
			ssd1306_WriteString(buffer, Font_7x10, White);
		}

		// ===== POWER BUTTON HANDLING =====
		if (btn_power) {
			systemOn = !systemOn;

			// Reinitialize display when turning ON
			if (systemOn)
				ssd1306_Init();

			btn_power = 0;
		}

		// ===== SLEEP MODE =====
		if (!systemOn) {
			ssd1306_Fill(Black);
			ssd1306_UpdateScreen();
			HAL_Delay(100);
			continue;
		}

		// ===== LONG PRESS (ENTER/EXIT SET TIME) =====
		if (btn_long) {
			if (currentMode == MODE_NORMAL) {
				currentMode = MODE_SET_TIME;
				currentField = EDIT_HOUR;
			} else {
				currentMode = MODE_NORMAL;
			}

			btn_long = 0;
		}

		// ===== SHORT PRESS (SCREEN CHANGE / FIELD CHANGE) =====
		if (btn_next) {
			if (currentMode == MODE_NORMAL) {
				currentScreen++;

				if (currentScreen > SCREEN_CLOCK)
					currentScreen = SCREEN_TEMP;
			} else {
				currentField++;

				if (currentField > EDIT_SEC)
					currentField = EDIT_HOUR;
			}

			btn_next = 0;
		}

		// ===== UP =====
		if (btn_up && currentMode == MODE_SET_TIME) {
			if (currentField == EDIT_HOUR) {
				sTime.Hours = (sTime.Hours + 1) % 24;
				sTime.Seconds = 0;
			} else if (currentField == EDIT_MIN) {
				sTime.Minutes = (sTime.Minutes + 1) % 60;
				sTime.Seconds = 0;
			} else
				sTime.Seconds = (sTime.Seconds + 1) % 60;

			HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

			btn_up = 0;
		}

		// ===== DOWN =====
		if (btn_down && currentMode == MODE_SET_TIME) {
			if (currentField == EDIT_HOUR) {
				sTime.Hours = (sTime.Hours == 0) ? 23 : sTime.Hours - 1;
				sTime.Seconds = 0;
			} else if (currentField == EDIT_MIN) {
				sTime.Minutes = (sTime.Minutes == 0) ? 59 : sTime.Minutes - 1;
				sTime.Seconds = 0;
			} else
				sTime.Seconds = (sTime.Seconds == 0) ? 59 : sTime.Seconds - 1;

			HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

			btn_down = 0;
		}

		// Generate simple environmental interpretation
		char *comment;

		if (temp > 30)
			comment = "Hot";
		else if (temp < 10)
			comment = "Cold";
		else if (hum > 80)
			comment = "Rain possible";
		else
			comment = "Normal";

		// ===== DISPLAY MODE =====
		if (currentMode == MODE_NORMAL) {
			switch (currentScreen) {
			case SCREEN_TEMP:
				sprintf(buffer, "Temp: %.1f C", temp);
				break;

			case SCREEN_HUM:
				sprintf(buffer, "Hum: %.1f %%", hum);
				break;

			case SCREEN_PRESS:
				sprintf(buffer, "Press: %.1f hPa", press);
				break;

			case SCREEN_CLOCK:
				sprintf(buffer, "%02d:%02d:%02d", sTime.Hours, sTime.Minutes,
						sTime.Seconds);
				ssd1306_SetCursor(10, 25);
				ssd1306_WriteString(buffer, Font_11x18, White);
				goto skip_small_text;
			}

			ssd1306_SetCursor(0, 20);
			ssd1306_WriteString(buffer, Font_7x10, White);

			ssd1306_SetCursor(0, 54);
			ssd1306_WriteString(comment, Font_6x8, White);
		} else {
			ssd1306_SetCursor(0, 0);
			ssd1306_WriteString("SET TIME", Font_7x10, White);

			sprintf(buffer, "%02d:%02d:%02d", sTime.Hours, sTime.Minutes,
					sTime.Seconds);

			ssd1306_SetCursor(20, 20);
			ssd1306_WriteString(buffer, Font_7x10, White);

			if (currentField == EDIT_HOUR)
				ssd1306_SetCursor(20, 30);
			else if (currentField == EDIT_MIN)
				ssd1306_SetCursor(40, 30);
			else
				ssd1306_SetCursor(60, 30);

			ssd1306_WriteString("^", Font_7x10, White);
		}

		skip_small_text:

		ssd1306_UpdateScreen();
		HAL_Delay(50);
	}
}
/* USER CODE END 3 */

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI
			| RCC_OSCILLATORTYPE_LSI;
	RCC_OscInitStruct.LSEState = RCC_LSE_OFF;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.LSIState = RCC_LSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 16;
	RCC_OscInitStruct.PLL.PLLN = 336;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
	RCC_OscInitStruct.PLL.PLLQ = 2;
	RCC_OscInitStruct.PLL.PLLR = 2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

	/* USER CODE BEGIN I2C1_Init 0 */

	/* USER CODE END I2C1_Init 0 */

	/* USER CODE BEGIN I2C1_Init 1 */

	/* USER CODE END I2C1_Init 1 */
	hi2c1.Instance = I2C1;
	hi2c1.Init.ClockSpeed = 100000;
	hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2C1_Init 2 */

	/* USER CODE END I2C1_Init 2 */

}

/**
 * @brief RTC Initialization Function
 * @param None
 * @retval None
 */
static void MX_RTC_Init(void) {

	/* USER CODE BEGIN RTC_Init 0 */

	/* USER CODE END RTC_Init 0 */

	/* USER CODE BEGIN RTC_Init 1 */

	/* USER CODE END RTC_Init 1 */

	/** Initialize RTC Only
	 */
	hrtc.Instance = RTC;
	hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
	hrtc.Init.AsynchPrediv = 127;
	hrtc.Init.SynchPrediv = 255;
	hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
	hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
	hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
	if (HAL_RTC_Init(&hrtc) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN RTC_Init 2 */

	/* USER CODE END RTC_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* USER CODE BEGIN MX_GPIO_Init_1 */

	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pins : PA1 PA4 */
	GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_4;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : PA5 */
	GPIO_InitStruct.Pin = GPIO_PIN_5;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : PB0 */
	GPIO_InitStruct.Pin = GPIO_PIN_0;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI0_IRQn);

	HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI1_IRQn);

	HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI4_IRQn);

	HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

	/* USER CODE BEGIN MX_GPIO_Init_2 */

	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
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
