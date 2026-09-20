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
  uint8_t anim_frame = 0;

  pet_init(&pet);
  ssd1306_init();

  if (!lis3dh_init()) {
    ssd1306_clear();
    ssd1306_draw_string(0, 0, "LIS3DH FAIL", SSD1306_PIXEL_ON);
    ssd1306_update_screen();
    osDelay(2000);
  }

  /* Splash: title, credit, and the bunny hopping in from the left.
   *
   * The vertical offset is a repeating 4-step pattern rather than a real
   * arc — at 32px tall on a 1-bit display the difference is invisible, and
   * a lookup table costs nothing. Runs at the same 8 fps as the main loop
   * so the motion matches the rest of the UI. */
  {
    static const int8_t hop[4] = { 0, -3, -5, -3 };

    for (uint8_t i = 0; i < 28; i++) {
      ssd1306_clear();

      ssd1306_draw_string(28, 6,  "POCKET PET", SSD1306_PIXEL_ON);
      ssd1306_draw_string(16, 18, "by @wiredbysarah", SSD1306_PIXEL_ON);

      /* Hops in from off-screen left, stops at x=48 and settles. */
      int16_t bx = (int16_t)(i * 5) - 32;
      if (bx > 48) {
        bx = 48;
      }

      /* Stop bouncing once it has arrived. */
      int16_t by = 30 + ((bx < 48) ? hop[i % 4] : 0);

      /* Runs while moving, idles once it arrives — a run cycle playing on
       * the spot reads as a glitch rather than a pause. */
      const pet_anim_t *splash_anim = (bx < 48) ? &anim_run : &anim_idle;

      ssd1306_draw_bitmap((uint8_t)bx, (uint8_t)by,
                          splash_anim->frames[i % splash_anim->count],
                          PET_SPRITE_W, PET_SPRITE_H, SSD1306_PIXEL_ON);
      ssd1306_update_screen();
      osDelay(125);
    }

    osDelay(600);
  }

  /* Redraw on a fixed 125 ms cadence rather than waiting on the queue, so
   * animation runs at 8 fps regardless of how often the pet state changes. */
  for(;;)
  {
    /* Timeout 0: take a new state if there is one, carry on if not. */
    osMessageQueueGet(petStateQueueHandle, &pet, NULL, 0);

    pet_mood_t mood = pet_get_mood(&pet);

    ssd1306_clear();

    /* --- Stat bars: labelled, down the left ------------------------------ */
    ssd1306_draw_string(0, 2, "HUNGER", SSD1306_PIXEL_ON);
    ssd1306_draw_rect(38, 1, 30, 7, SSD1306_PIXEL_ON);
    ssd1306_fill_rect(39, 2, (uint8_t)(pet.hunger * 28 / 100), 5, SSD1306_PIXEL_ON);

    ssd1306_draw_string(0, 13, "MOOD", SSD1306_PIXEL_ON);
    ssd1306_draw_rect(38, 12, 30, 7, SSD1306_PIXEL_ON);
    ssd1306_fill_rect(39, 13, (uint8_t)(pet.mood * 28 / 100), 5, SSD1306_PIXEL_ON);

    ssd1306_draw_string(0, 24, "ENERGY", SSD1306_PIXEL_ON);
    ssd1306_draw_rect(38, 23, 30, 7, SSD1306_PIXEL_ON);
    ssd1306_fill_rect(39, 24, (uint8_t)(pet.energy * 28 / 100), 5, SSD1306_PIXEL_ON);

    /* --- The pet, right-hand side ----------------------------------------
     * Adults render at 2x, so growing up is visible rather than just a label
     * change. Both sizes sit their feet on the same baseline, so the
     * creature grows upward in place.
     *
     * The y calculation is done signed and clamped: at 2x the sprite is 64px
     * tall and would underflow a uint8_t subtraction, wrapping to a large
     * value and clipping the top of the sprite. */

    /* A recent event overrides the mood animation, so a button press is
     * visibly acknowledged and the reaction animations get used. Death and
     * distress still win: those are things the player needs to see. */
    const pet_anim_t *anim;

    if (mood == PET_MOOD_DEAD) {
      anim = &anim_dead;
    } else if (mood == PET_MOOD_DISTRESSED) {
      anim = &anim_hurt;
    } else if (pet.reaction_ticks > 0) {
      switch (pet.last_event) {
        case PET_EVENT_FEED:  anim = &anim_attack; break;
        case PET_EVENT_PLAY:  anim = &anim_run;    break;
        case PET_EVENT_SHAKE: anim = &anim_run;    break;
        default:              anim = &anim_idle;   break;
      }
    } else {
      switch (mood) {
        case PET_MOOD_HAPPY:    anim = &anim_idle;    break;
        case PET_MOOD_NEUTRAL:  anim = &anim_sitting; break;
        case PET_MOOD_SAD:      anim = &anim_liedown; break;
        case PET_MOOD_SLEEPING: anim = &anim_sleep;   break;
        default:                anim = &anim_idle;    break;
      }
    }

    uint8_t scale = (pet.stage == PET_STAGE_ADULT) ? 2 : 1;
    int16_t sprite_h = (int16_t)(PET_SPRITE_H * scale);
    int16_t sy = 54 - sprite_h;
    if (sy < 0) {
      sy = 0;
    }

    /* Both sizes share a horizontal centre at x=95, so the creature grows in
     * place rather than shifting sideways when it reaches adulthood. */
    uint8_t sprite_w = (uint8_t)(PET_SPRITE_W * scale);
    uint8_t sx = (uint8_t)(105 - sprite_w / 2);


    ssd1306_draw_bitmap_scaled(sx, (uint8_t)sy,
                               anim->frames[anim_frame % anim->count],
                               PET_SPRITE_W, PET_SPRITE_H, scale,
                               SSD1306_PIXEL_ON);

    /* --- Status line, bottom --------------------------------------------- */
    if (mood == PET_MOOD_DEAD) {
      ssd1306_draw_string(4, 56, "RIP", SSD1306_PIXEL_ON);
    } else if (mood == PET_MOOD_DISTRESSED) {
      snprintf(line, sizeof(line), "HELP! %us",
               (unsigned)(DISTRESS_TICKS - pet.distress_ticks));
      ssd1306_draw_string(4, 56, line, SSD1306_PIXEL_ON);
    } else if (mood == PET_MOOD_SLEEPING) {
      ssd1306_draw_string(4, 56, "ZZZ", SSD1306_PIXEL_ON);
    } else {
      snprintf(line, sizeof(line), "%s %lus",
               (pet.stage == PET_STAGE_ADULT) ? "ADULT" : "BABY",
               pet.age_s);
      ssd1306_draw_string(0, 56, line, SSD1306_PIXEL_ON);
    }
    anim_frame++;
    ssd1306_update_screen();

#ifdef BOARD_NUCLEO
    HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
#endif

    osDelay(125);
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
		  uint16_t btn_b_held = 0;
		  uint8_t  btn_b_longpress_fired = 0;
		  uint8_t accel_divider = 0;

		  /* 50 samples at 20 ms = 1 second. */
		  #define LONGPRESS_SAMPLES 50

		  for(;;)
		  {
		    uint8_t btn_a = (HAL_GPIO_ReadPin(BTN_A_GPIO_Port, BTN_A_Pin) == GPIO_PIN_SET) ? 1 : 0;
		    uint8_t btn_b = (HAL_GPIO_ReadPin(BTN_B_GPIO_Port, BTN_B_Pin) == GPIO_PIN_SET) ? 1 : 0;

		    if (btn_a == 0 && btn_a_prev == 1) {
		      pet_event_t ev = PET_EVENT_FEED;
		      osMessageQueuePut(petEventQueueHandle, &ev, 0, 0);
		    }

		    /* B is press-and-hold: a short press plays, a hold toggles sleep. The
		     * long press fires while still held so the hold has a definite moment,
		     * and a flag stops the release also registering as a play. */
		    if (btn_b == 0) {
		      btn_b_held++;

		      if (btn_b_held >= LONGPRESS_SAMPLES && !btn_b_longpress_fired) {
		        pet_event_t ev = PET_EVENT_SLEEP_TOGGLE;
		        osMessageQueuePut(petEventQueueHandle, &ev, 0, 0);
		        btn_b_longpress_fired = 1;
		      }
		    } else {
		      if (btn_b_prev == 0 && !btn_b_longpress_fired) {
		        pet_event_t ev = PET_EVENT_PLAY;
		        osMessageQueuePut(petEventQueueHandle, &ev, 0, 0);
		      }
		      btn_b_held = 0;
		      btn_b_longpress_fired = 0;
		    }

		    btn_a_prev = btn_a;
		    btn_b_prev = btn_b;

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

