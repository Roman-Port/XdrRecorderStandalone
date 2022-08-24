/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

extern DMA2D_HandleTypeDef hdma2d;
extern SAI_HandleTypeDef hsai_BlockA1;
extern SAI_HandleTypeDef hsai_BlockB1;
extern I2C_HandleTypeDef hi2c2;
extern SD_HandleTypeDef hsd;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

#define RECORDER_FW_VER "0.1-dev"

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define XdrRds_Pin GPIO_PIN_2
#define XdrRds_GPIO_Port GPIOE
#define XdrBlend_Pin GPIO_PIN_15
#define XdrBlend_GPIO_Port GPIOC
#define XdrReset_Pin GPIO_PIN_3
#define XdrReset_GPIO_Port GPIOA
#define SD_Detect_Pin GPIO_PIN_12
#define SD_Detect_GPIO_Port GPIOA
#define Display_D_C_Pin GPIO_PIN_3
#define Display_D_C_GPIO_Port GPIOD
#define Display_Reset_Pin GPIO_PIN_4
#define Display_Reset_GPIO_Port GPIOD
#define Display_CS_Pin GPIO_PIN_5
#define Display_CS_GPIO_Port GPIOD
#define BtnA_Pin GPIO_PIN_7
#define BtnA_GPIO_Port GPIOD
#define BtnB_Pin GPIO_PIN_9
#define BtnB_GPIO_Port GPIOG
#define BtnC_Pin GPIO_PIN_10
#define BtnC_GPIO_Port GPIOG
#define LedA_Pin GPIO_PIN_11
#define LedA_GPIO_Port GPIOG
#define LedB_Pin GPIO_PIN_12
#define LedB_GPIO_Port GPIOG
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
