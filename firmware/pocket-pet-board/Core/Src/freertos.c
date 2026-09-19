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
#include "lis3dh.h"
#include "pet.h"
#include "pocket_pet_pins.h"
#include <stdio.h>
#include "pet_sprites.h"
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
      .stack_size = 256 * 4,
      .priority = (osPriority_t) osPriorityNormal,
    };
    osThreadId_t petTaskHandle;


    osMessageQueueId_t petEventQueueHandle;

    const osThreadAttr_t inputTask_attributes = {
      .name = "inputTask",
      .stack_size = 256 * 4,
      .priority = (osPriority_t) osPriorityNormal,
    };
    osThreadId_t inputTaskHandle;


/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 256 * 4,
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
			    if (osMessageQueueGet(petStateQueueHandle, &pet, NULL, osWaitForever) == osOK)
			    {
			      ssd1306_clear();

			      pet_mood_t mood = pet_get_mood(&pet);

			      /* Sprite on the left, stats on the right. */
			      const uint8_t *sprite = (mood == PET_MOOD_HAPPY) ? pet_happy : pet_sad;
			      ssd1306_draw_bitmap(4, 8, sprite, PET_SPRITE_W, PET_SPRITE_H,
			                          SSD1306_PIXEL_ON);

			      /* Three stat bars, labelled and scaled to 60 px. */
			      snprintf(line, sizeof(line), "HUN");
			      ssd1306_draw_string(40, 4, line, SSD1306_PIXEL_ON);
			      ssd1306_draw_rect(62, 3, 62, 9, SSD1306_PIXEL_ON);
			      ssd1306_fill_rect(63, 4, (uint8_t)(pet.hunger * 60 / 100), 7,
			                        SSD1306_PIXEL_ON);

			      snprintf(line, sizeof(line), "MOO");
			      ssd1306_draw_string(40, 18, line, SSD1306_PIXEL_ON);
			      ssd1306_draw_rect(62, 17, 62, 9, SSD1306_PIXEL_ON);
			      ssd1306_fill_rect(63, 18, (uint8_t)(pet.mood * 60 / 100), 7,
			                        SSD1306_PIXEL_ON);

			      snprintf(line, sizeof(line), "NRG");
			      ssd1306_draw_string(40, 32, line, SSD1306_PIXEL_ON);
			      ssd1306_draw_rect(62, 31, 62, 9, SSD1306_PIXEL_ON);
			      ssd1306_fill_rect(63, 32, (uint8_t)(pet.energy * 60 / 100), 7,
			                        SSD1306_PIXEL_ON);

			      /* Status line: whichever of these is most urgent. */
			      if (mood == PET_MOOD_DEAD) {
			        ssd1306_draw_string(4, 50, "RIP", SSD1306_PIXEL_ON);
			      } else if (mood == PET_MOOD_DISTRESSED) {
			        snprintf(line, sizeof(line), "HELP! %us",
			                 (unsigned)((DISTRESS_TICKS - pet.distress_ticks)));
			        ssd1306_draw_string(4, 50, line, SSD1306_PIXEL_ON);
			      } else if (mood == PET_MOOD_SLEEPING) {
			        ssd1306_draw_string(4, 50, "ZZZ", SSD1306_PIXEL_ON);
			      } else {
			        snprintf(line, sizeof(line), "%s  %lus",
			                 (pet.stage == PET_STAGE_ADULT) ? "ADULT" : "BABY",
			                 pet.age_s);
			        ssd1306_draw_string(4, 50, line, SSD1306_PIXEL_ON);
			      }

			      ssd1306_update_screen();
			    }

			#ifdef BOARD_NUCLEO
			    HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
			#endif
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
		  uint8_t btn_a_prev = 1;
		  uint8_t btn_b_prev = 1;
		  uint8_t accel_divider = 0;

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

		    /* Buttons need 20 ms sampling to debounce cleanly; the accelerometer
		     * does not, and polling I2C that often wastes bus time and power. Every
		     * fifth pass gives 100 ms, which matches the cooldown assumption in
		     * lis3dh_check_shake(). */
		    if (++accel_divider >= 5) {
		      accel_divider = 0;

		      if (lis3dh_check_shake()) {
		        pet_event_t ev = PET_EVENT_SHAKE;
		        osMessageQueuePut(petEventQueueHandle, &ev, 0, 0);
		      }
		    }

		    osDelay(20);
		  }
	}

	/* USER CODE END Application */

