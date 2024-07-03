#include "main.h"
#include <stdio.h>
#include "string.h"
#include <stdbool.h>


typedef enum
{
  BUFFER_OFFSET_NONE = 0,
  BUFFER_OFFSET_HALF = 1,
  BUFFER_OFFSET_FULL = 2,
} BUFFER_StateTypeDef;

extern AUDIO_ErrorTypeDef AUDIO_Start(uint32_t audio_start_address, uint32_t audio_file_size);

#define AUDIO_BLOCK_SIZE   ((uint32_t)0xFFFE)
#define AUDIO_NB_BLOCKS    ((uint32_t)4)
#define AUDIO_BUFFER_SIZE  (AUDIO_BLOCK_SIZE * AUDIO_NB_BLOCKS * 2)
#define DELAY_BUFFER_SIZE 2048

#define NOISE_THRESHOLD 1000
#define GATE_ATTACK_TIME 100
#define GATE_RELEASE_TIME 200

#define NUM_TAPS 5

static uint16_t  internal_buffer[AUDIO_BLOCK_SIZE];
uint32_t  audio_rec_buffer_state;
static void AudioRec_SetHint(void);


void AudioRec_demo (void)
{
  Setup();
  Start_Audio();
}


void AudioRec_vibrato (void)
{
  Setup();

  #define VIBRATO_FREQUENCY 8
  #define VIBRATO_DEPTH 1.5

  uint32_t i;
  uint16_t *src_ptr;
  uint16_t *dst_ptr;
  float vibrato;
  int32_t vibrato_offset;

  // First loop: Apply vibrato effect
  src_ptr = (uint16_t *)(AUDIO_REC_START_ADDR);
  dst_ptr = (uint16_t *)(AUDIO_REC_START_ADDR);

  for (i = 0; i < (AUDIO_BLOCK_SIZE * AUDIO_NB_BLOCKS); i++) {
    vibrato = sin(2.0 * 3.14159265358979323846 * VIBRATO_FREQUENCY * i / DEFAULT_AUDIO_IN_FREQ) * VIBRATO_DEPTH;
    vibrato_offset = (int32_t)vibrato;

    if ((i + vibrato_offset) < (AUDIO_BLOCK_SIZE * AUDIO_NB_BLOCKS) && (i + vibrato_offset) >= 0) {
      *dst_ptr = *(src_ptr + vibrato_offset);
    } else {
      *dst_ptr = *(src_ptr - vibrato_offset);
    }

    src_ptr++;
    dst_ptr++;
  }

  Start_Audio();
}


void AudioRec_nightcore (void)
{
  Setup();

  ////  pitch up
  uint32_t i;
  uint16_t *src_ptr, *dst_ptr;

  // First loop: Copy every second sample to the original buffer
  src_ptr = (uint16_t *)AUDIO_REC_START_ADDR;
  dst_ptr = (uint16_t *)AUDIO_REC_START_ADDR;

  for (i = 0; i < (AUDIO_BLOCK_SIZE * AUDIO_NB_BLOCKS) / 2; i++) {
	  *dst_ptr = *(src_ptr + i * 2);
	  dst_ptr++;
  }

  // Second loop: Copy the processed samples to a new location
  src_ptr = (uint16_t *)AUDIO_REC_START_ADDR;
  dst_ptr = (uint16_t *)(AUDIO_REC_START_ADDR + AUDIO_BLOCK_SIZE * AUDIO_NB_BLOCKS);

  for (i = 0; i < (AUDIO_BLOCK_SIZE * AUDIO_NB_BLOCKS) / 2; i++) {
	  *(dst_ptr + i) = *(src_ptr + i);
  }

  Start_Audio();
}


void AudioRec_reverb (void)
{
  Setup();

  float tap_delay[NUM_TAPS] = {0.1, 0.2, 0.3, 0.4, 0.5};
  float tap_gain[NUM_TAPS] = {0.7, 0.5, 0.3, 0.2, 0.1};

  uint32_t i, j;
  uint32_t delay_samples[NUM_TAPS];
  uint16_t *audio_ptr = (uint16_t *)AUDIO_REC_START_ADDR;
  uint16_t *delay_buffer = (uint16_t *)AUDIO_REC_START_ADDR; // Overwrite in-place

  // Calculate delay samples for each tap
  for (i = 0; i < NUM_TAPS; i++) {
      delay_samples[i] = (uint32_t)(tap_delay[i] * DEFAULT_AUDIO_IN_FREQ);
  }

  // Apply reverb effect
  for (i = 0; i < AUDIO_BUFFER_SIZE / 2; i++) {
      float out_sample = audio_ptr[i];

      for (j = 0; j < NUM_TAPS; j++) {
          uint32_t delay_index = (i >= delay_samples[j]) ? (i - delay_samples[j]) : (AUDIO_BUFFER_SIZE / 2 + i - delay_samples[j]);
          out_sample += tap_gain[j] * delay_buffer[delay_index];
      }

      // Clip the output sample to fit into 16-bit range
      if (out_sample > 32767) {
          out_sample = 32767;
      } else if (out_sample < -32768) {
          out_sample = -32768;
      }

      delay_buffer[i] = (uint16_t)out_sample;
  }

  Start_Audio();
}


void AudioRec_distortion (void)
{
  Setup();

  uint32_t i;
  uint16_t *src_ptr;

  src_ptr = (uint16_t *)(AUDIO_REC_START_ADDR);

  for (i = 0; i < (AUDIO_BLOCK_SIZE * AUDIO_NB_BLOCKS); i++) {
    // Apply distortion by clipping the signal
    int32_t sample = (int32_t)(*src_ptr) - 32768; // Convert to signed 16-bit
    if (sample > 10000) sample = 10000;
    else if (sample < -10000) sample = -10000;
    *src_ptr = (uint16_t)(sample + 32768); // Convert back to unsigned 16-bit
    src_ptr++;
  }

  Start_Audio();
}


void Setup (void) {
	uint32_t  block_number;

	  AudioRec_SetHint();

	  /* Initialize Audio Recorder */
	  if (BSP_AUDIO_IN_Init(DEFAULT_AUDIO_IN_FREQ, DEFAULT_AUDIO_IN_BIT_RESOLUTION, DEFAULT_AUDIO_IN_CHANNEL_NBR) == AUDIO_OK)
	  {
	    BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
	    BSP_LCD_SetTextColor(LCD_COLOR_GREEN);
	    BSP_LCD_DisplayStringAt(0, BSP_LCD_GetYSize() - 95, (uint8_t *)"  AUDIO RECORD INIT OK  ", CENTER_MODE);
	  }
	  else
	  {
	    BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
	    BSP_LCD_SetTextColor(LCD_COLOR_RED);
	    BSP_LCD_DisplayStringAt(0, BSP_LCD_GetYSize() - 95, (uint8_t *)"  AUDIO RECORD INIT FAIL", CENTER_MODE);
	    BSP_LCD_DisplayStringAt(0, BSP_LCD_GetYSize() - 80, (uint8_t *)" Try to reset board ", CENTER_MODE);
	  }

	  audio_rec_buffer_state = BUFFER_OFFSET_NONE;

	  /* Display the state on the screen */
	  BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
	  BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	  BSP_LCD_DisplayStringAt(0, BSP_LCD_GetYSize() - 80, (uint8_t *)"       RECORDING...     ", CENTER_MODE);

	  /* Start Recording */
	  BSP_AUDIO_IN_Record(internal_buffer, AUDIO_BLOCK_SIZE);

	  for (block_number = 0; block_number < AUDIO_NB_BLOCKS; block_number++)
	  {
	    /* Wait end of half block recording */
	    while(audio_rec_buffer_state != BUFFER_OFFSET_HALF)
	    {
	      if (CheckForUserInput() > 0)
	      {
	        /* Stop Player before close Test */
	        BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
	        return;
	      }
	    }
	    audio_rec_buffer_state = BUFFER_OFFSET_NONE;
	    /* Copy recorded 1st half block in SDRAM */
	    memcpy((uint32_t *)(AUDIO_REC_START_ADDR + (block_number * AUDIO_BLOCK_SIZE * 2)),
	           internal_buffer,
	           AUDIO_BLOCK_SIZE);

	    /* Wait end of one block recording */
	    while(audio_rec_buffer_state != BUFFER_OFFSET_FULL)
	    {
	      if (CheckForUserInput() > 0)
	      {
	        /* Stop Player before close Test */
	        BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
	        return;
	      }
	    }
	    audio_rec_buffer_state = BUFFER_OFFSET_NONE;
	    /* Copy recorded 2nd half block in SDRAM */
	    memcpy((uint32_t *)(AUDIO_REC_START_ADDR + (block_number * AUDIO_BLOCK_SIZE * 2) + (AUDIO_BLOCK_SIZE)),
	           (uint16_t *)(&internal_buffer[AUDIO_BLOCK_SIZE/2]),
	           AUDIO_BLOCK_SIZE);
	  }

	  /* Stop recorder */
	  BSP_AUDIO_IN_Stop(CODEC_PDWN_SW);

	  BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
	  BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	  BSP_LCD_DisplayStringAt(0, BSP_LCD_GetYSize() - 65, (uint8_t *)"RECORDING DONE, START PLAYBACK...", CENTER_MODE);

	  /* -----------Start Playback -------------- */
	  /* Initialize audio IN at REC_FREQ*/
	  BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_HEADPHONE, 80, DEFAULT_AUDIO_IN_FREQ);
	  BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);
}

void Start_Audio (void) {
  /* Play the recorded buffer*/
  AUDIO_Start(AUDIO_REC_START_ADDR, AUDIO_BLOCK_SIZE * AUDIO_NB_BLOCKS * 2);  /* Use Audio play demo to playback sound */

  BSP_LCD_DisplayStringAt(0, BSP_LCD_GetYSize() - 40, (uint8_t *)"PLAYBACK DONE", CENTER_MODE);

  while (1)
  {
	AUDIO_Process();

	if (CheckForUserInput() > 0)
	{
	  /* Stop Player before close Test */
	  BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
	  return;
	}
  }
}

static void AudioRec_SetHint(void)
{
  /* Clear the LCD */
  BSP_LCD_Clear(LCD_COLOR_WHITE);

  /* Set Audio Demo description */
  BSP_LCD_SetTextColor(LCD_COLOR_DARKGREEN);
  BSP_LCD_FillRect(0, 0, BSP_LCD_GetXSize(), 90);
  BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
  BSP_LCD_SetBackColor(LCD_COLOR_DARKGREEN);
  BSP_LCD_SetFont(&Font24);
  BSP_LCD_DisplayStringAt(0, 0, (uint8_t *)"AUDIO RECORD", CENTER_MODE);
  BSP_LCD_SetFont(&Font12);
  BSP_LCD_DisplayStringAt(0, 30, (uint8_t *)"Press User button for next effect", CENTER_MODE);

  /* Set the LCD Text Color */
  BSP_LCD_SetTextColor(LCD_COLOR_MAGENTA);
  BSP_LCD_DrawRect(10, 100, BSP_LCD_GetXSize() - 20, BSP_LCD_GetYSize() - 110);
  BSP_LCD_DrawRect(11, 101, BSP_LCD_GetXSize() - 22, BSP_LCD_GetYSize() - 112);

}

/*------------------------------------------------------------------------------
       Callbacks implementation:
           the callbacks API are defined __weak in the stm32746g_discovery_audio.c file
           and their implementation should be done the user code if they are needed.
           Below some examples of callback implementations.
  ----------------------------------------------------------------------------*/
/**
  * @brief Manages the DMA Transfer complete interrupt.
  * @param None
  * @retval None
  */
void BSP_AUDIO_IN_TransferComplete_CallBack(void)
{
  audio_rec_buffer_state = BUFFER_OFFSET_FULL;
  return;
}

/**
  * @brief  Manages the DMA Half Transfer complete interrupt.
  * @param  None
  * @retval None
  */
void BSP_AUDIO_IN_HalfTransfer_CallBack(void)
{
  audio_rec_buffer_state = BUFFER_OFFSET_HALF;
  return;
}

/**
  * @brief  Audio IN Error callback function.
  * @param  None
  * @retval None
  */
void BSP_AUDIO_IN_Error_CallBack(void)
{
  /* This function is called when an Interrupt due to transfer error on or peripheral
     error occurs. */
  /* Display message on the LCD screen */
  BSP_LCD_SetBackColor(LCD_COLOR_RED);
  BSP_LCD_DisplayStringAt(0, LINE(14), (uint8_t *)"       DMA  ERROR     ", CENTER_MODE);

  /* Stop the program with an infinite loop */
  while (BSP_PB_GetState(BUTTON_KEY) != RESET)
  {
    return;
  }
  /* could also generate a system reset to recover from the error */
  /* .... */
}



/**
  * @}
  */

/**
  * @}
  */

