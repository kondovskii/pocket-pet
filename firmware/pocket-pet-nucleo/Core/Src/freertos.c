/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1306.h"
#include "pet.h"
#include <stdio.h>
#include "i2c.h"
#include "lis3dh.h"
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
/* USER CODE BEGIN Variables */
osMessageQueueId_t petStateQueueHandle;

const osThreadAttr_t petTask_attributes = {
  .name = "petTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
osThreadId_t petTaskHandle;


osMessageQueueId_t petEventQueueHandle;

const osThreadAttr_t inputTask_attributes = {
  .name = "inputTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
osThreadId_t inputTaskHandle;

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartPetTask(void *argument);
void StartInputTask(void *argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
	  /* Depth 1: the display only ever cares about the newest state. A deeper
	   * queue would just mean rendering stale frames if the display fell behind. */
	  petStateQueueHandle = osMessageQueueNew(1, sizeof(pet_state_t), NULL);

	  /* Depth 4: button presses must not be dropped the way stale display frames
	   * can be. A few can buffer if the pet task is mid-tick. */
	  petEventQueueHandle = osMessageQueueNew(4, sizeof(pet_event_t), NULL);
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  petTaskHandle = osThreadNew(StartPetTask, NULL, &petTask_attributes);
  inputTaskHandle = osThreadNew(StartInputTask, NULL, &inputTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
	  pet_state_t pet;
	  char line[24];

	  ssd1306_init();
	  {
	    char l[24];

	    ssd1306_clear();
	    if (lis3dh_init()) {
	      ssd1306_draw_string(0, 0, "LIS3DH OK", SSD1306_PIXEL_ON);
	    } else {
	      ssd1306_draw_string(0, 0, "LIS3DH FAIL", SSD1306_PIXEL_ON);
	    }
	    ssd1306_update_screen();
	    osDelay(1500);

	    /* Live axis readout for 15 seconds, so the wiring can be sanity-checked
	     * by tilting the board before any of this is wired into the game. */
	    for (int i = 0; i < 150; i++) {
	      lis3dh_accel_t a;

	      if (lis3dh_read_accel(&a)) {
	        ssd1306_clear();
	        snprintf(l, sizeof(l), "X %6d", a.x);
	        ssd1306_draw_string(0, 0, l, SSD1306_PIXEL_ON);
	        snprintf(l, sizeof(l), "Y %6d", a.y);
	        ssd1306_draw_string(0, 16, l, SSD1306_PIXEL_ON);
	        snprintf(l, sizeof(l), "Z %6d", a.z);
	        ssd1306_draw_string(0, 32, l, SSD1306_PIXEL_ON);
	        ssd1306_update_screen();
	      }
	      osDelay(100);
	    }
	  }
	  ssd1306_clear();
	  ssd1306_draw_string(0, 0, "POCKET PET", SSD1306_PIXEL_ON);
	  ssd1306_draw_string(0, 10, "v1.0", SSD1306_PIXEL_ON);
	  ssd1306_update_screen();
	  osDelay(1500);

	  for(;;)
	  {
	    /* Block until the pet task publishes a new state. osWaitForever means
	     * this task uses no CPU at all between updates. */
	    if (osMessageQueueGet(petStateQueueHandle, &pet, NULL, osWaitForever) == osOK)
	    {
	      ssd1306_clear();

	      snprintf(line, sizeof(line), "HUNGER %3u", pet.hunger);
	      ssd1306_draw_string(0, 0, line, SSD1306_PIXEL_ON);
	      ssd1306_fill_rect(0, 10, (uint8_t)(pet.hunger * 128 / 100), 6,
	                        SSD1306_PIXEL_ON);

	      snprintf(line, sizeof(line), "MOOD   %3u", pet.mood);
	      ssd1306_draw_string(0, 22, line, SSD1306_PIXEL_ON);
	      ssd1306_fill_rect(0, 32, (uint8_t)(pet.mood * 128 / 100), 6,
	                        SSD1306_PIXEL_ON);

	      snprintf(line, sizeof(line), "AGE %lus", pet.age_s);
	      ssd1306_draw_string(0, 44, line, SSD1306_PIXEL_ON);

	      ssd1306_update_screen();
	    }

	    HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
	  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/*
 * Owns the pet state. Nothing else writes to it.
 *
 * Publishes a copy to the queue after each tick; the display task renders
 * whatever it receives. Neither task touches the other's memory, which is
 * what makes this safe without a mutex.
 */
void StartPetTask(void *argument)
{
  pet_state_t pet;
  pet_init(&pet);

  for(;;)
  {
    pet_event_t ev;

    /* Drain any events that arrived since the last tick. Timeout 0 means
     * this returns immediately when the queue is empty. */
    while (osMessageQueueGet(petEventQueueHandle, &ev, NULL, 0) == osOK) {
      pet_apply_event(&pet, ev);
    }

    pet_tick(&pet);
    osMessageQueuePut(petStateQueueHandle, &pet, 0, 0);

    osDelay(1000);
  }
  {
    pet_tick(&pet);

    /* Overwrite semantics: if the display has not consumed the previous
     * state yet, replace it. Timeout 0 so this task never blocks on a
     * slow renderer. */
    osMessageQueuePut(petStateQueueHandle, &pet, 0, 0);

    osDelay(1000);
  }
}


/*
 * Polls the two buttons and publishes press events.
 *
 * Pins are pull-up, so a press reads LOW. Debouncing is done by sampling
 * every 20 ms and only acting on a transition from released to pressed:
 * mechanical contacts (and a hand-held wire even more so) bounce for a few
 * milliseconds, and without this one press registers as a dozen.
 *
 * Edge detection, not level: holding the wire down feeds once, not
 * continuously. Feeding on level would let you max out the pet by leaving
 * a wire touching.
 */
void StartInputTask(void *argument)
{
  uint8_t btn_a_prev = 1;   /* 1 = released, matches the pull-up idle state */
  uint8_t btn_b_prev = 1;

  for(;;)
  {
    uint8_t btn_a = (HAL_GPIO_ReadPin(BTN_A_GPIO_Port, BTN_A_Pin) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t btn_b = (HAL_GPIO_ReadPin(BTN_B_GPIO_Port, BTN_B_Pin) == GPIO_PIN_SET) ? 1 : 0;

    if (btn_a == 0 && btn_a_prev == 1) {
      pet_event_t ev = PET_EVENT_FEED;
      osMessageQueuePut(petEventQueueHandle, &ev, 0, 0);
    }

    if (btn_b == 0 && btn_b_prev == 1) {
      pet_event_t ev = PET_EVENT_PLAY;
      osMessageQueuePut(petEventQueueHandle, &ev, 0, 0);
    }

    btn_a_prev = btn_a;
    btn_b_prev = btn_b;

    osDelay(20);
  }
}

/* USER CODE END Application */

