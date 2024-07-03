/**
  ******************************************************************************
  * @file    BSP/Src/audio.c
  * @author  MCD Application Team
  * @brief   This example code shows how to use the audio feature in the
  *          stm32746g_discovery driver
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>


/** @addtogroup STM32F7xx_HAL_Examples
  * @{
  */

/** @addtogroup BSP
  * @{
  */

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/*Since SysTick is set to 1ms (unless to set it quicker) */ 
/* to run up to 48khz, a buffer around 1000 (or more) is requested*/
/* to run up to 96khz, a buffer around 2000 (or more) is requested*/
#define AUDIO_BUFFER_SIZE       2048
#define AUDIO_DEFAULT_VOLUME    70

/* Audio file size and start address are defined here since the audio file is
   stored in Flash memory as a constant table of 16-bit data */
#define AUDIO_FILE_SIZE               524288
#define AUDIO_START_OFFSET_ADDRESS    0            /* Offset relative to audio file header size */
#define AUDIO_FILE_ADDRESS            0x08080000   /* Audio file address */

/* Private typedef -----------------------------------------------------------*/
typedef enum {
  AUDIO_STATE_IDLE = 0,
  AUDIO_STATE_INIT,
  AUDIO_STATE_PLAYING,
}AUDIO_PLAYBACK_StateTypeDef;

typedef enum {
  BUFFER_OFFSET_NONE = 0,  
  BUFFER_OFFSET_HALF,  
  BUFFER_OFFSET_FULL,     
}BUFFER_StateTypeDef;

typedef struct {
  uint8_t buff[AUDIO_BUFFER_SIZE];
  uint32_t fptr;  
  BUFFER_StateTypeDef state;
}AUDIO_BufferTypeDef;

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
ALIGN_32BYTES (static AUDIO_BufferTypeDef  buffer_ctl);
static AUDIO_PLAYBACK_StateTypeDef  audio_state;
static uint32_t  AudioStartAddress;
static uint32_t  AudioFileSize;
__IO uint32_t uwVolume = 20;
__IO uint32_t uwPauseEnabledStatus = 0;

static uint32_t AudioFreq[9] = {8000 ,11025, 16000, 22050, 32000, 44100, 48000, 96000, 192000};

/* Private function prototypes -----------------------------------------------*/
static uint32_t GetData(void *pdata, uint32_t offset, uint8_t *pbuf, uint32_t NbrOfData);
AUDIO_ErrorTypeDef AUDIO_Start(uint32_t audio_start_address, uint32_t audio_file_size);



/**
  * @brief  Starts Audio streaming.    
  * @param  None
  * @retval Audio error
  */ 
AUDIO_ErrorTypeDef AUDIO_Start(uint32_t audio_start_address, uint32_t audio_file_size)
{
  uint32_t bytesread;
  
  buffer_ctl.state = BUFFER_OFFSET_NONE;
  AudioStartAddress = audio_start_address;
  AudioFileSize = audio_file_size;
  bytesread = GetData( (void *)AudioStartAddress,
                      0,
                      &buffer_ctl.buff[0],
                      AUDIO_BUFFER_SIZE);
  if(bytesread > 0)
  {
    /* Clean Data Cache to update the content of the SRAM */
    SCB_CleanDCache_by_Addr((uint32_t*)&buffer_ctl.buff[0], AUDIO_BUFFER_SIZE/2);
        
    BSP_AUDIO_OUT_Play((uint16_t*)&buffer_ctl.buff[0], AUDIO_BUFFER_SIZE);
    audio_state = AUDIO_STATE_PLAYING;      
    buffer_ctl.fptr = bytesread;
    return AUDIO_ERROR_NONE;
  }
  return AUDIO_ERROR_IO;
}

/**
  * @brief  Manages Audio process. 
  * @param  None
  * @retval Audio error
  */
uint8_t AUDIO_Process(void)
{
  uint32_t bytesread;
  AUDIO_ErrorTypeDef error_state = AUDIO_ERROR_NONE;  
  
  switch(audio_state)
  {
  case AUDIO_STATE_PLAYING:
    
    if(buffer_ctl.fptr >= AudioFileSize)
    {
      /* Play audio sample again ... */
      buffer_ctl.fptr = 0; 
      error_state = AUDIO_ERROR_EOF;
    }

    /* 1st half buffer played; so fill it and continue playing from bottom*/
    if(buffer_ctl.state == BUFFER_OFFSET_HALF)
    {
      bytesread = GetData((void *)AudioStartAddress,
                          buffer_ctl.fptr,
                          &buffer_ctl.buff[0],
                          AUDIO_BUFFER_SIZE /2);
      
      if( bytesread >0)
      { 
        buffer_ctl.state = BUFFER_OFFSET_NONE;
        buffer_ctl.fptr += bytesread; 
        
        /* Clean Data Cache to update the content of the SRAM */
        SCB_CleanDCache_by_Addr((uint32_t*)&buffer_ctl.buff[0], AUDIO_BUFFER_SIZE/2);
      }
    }
    
    /* 2nd half buffer played; so fill it and continue playing from top */    
    if(buffer_ctl.state == BUFFER_OFFSET_FULL)
    {
      bytesread = GetData((void *)AudioStartAddress,
                          buffer_ctl.fptr, 
                          &buffer_ctl.buff[AUDIO_BUFFER_SIZE /2],
                          AUDIO_BUFFER_SIZE /2);
      if( bytesread > 0)
      {
        buffer_ctl.state = BUFFER_OFFSET_NONE;
        buffer_ctl.fptr += bytesread;
        
        /* Clean Data Cache to update the content of the SRAM */
        SCB_CleanDCache_by_Addr((uint32_t*)&buffer_ctl.buff[AUDIO_BUFFER_SIZE/2], AUDIO_BUFFER_SIZE/2);
      }
    }
    break;
    
  default:
    error_state = AUDIO_ERROR_NOTREADY;
    break;
  }
  
  return (uint8_t) error_state;
}

/**
  * @brief  Gets Data from storage unit.
  * @param  None
  * @retval None
  */
static uint32_t GetData(void *pdata, uint32_t offset, uint8_t *pbuf, uint32_t NbrOfData)
{
  uint8_t *lptr = pdata;
  uint32_t ReadDataNbr;

  ReadDataNbr = 0;
  while(((offset + ReadDataNbr) < AudioFileSize) && (ReadDataNbr < NbrOfData))
  {
    pbuf[ReadDataNbr]= lptr [offset + ReadDataNbr];
    ReadDataNbr++;
  }
  return ReadDataNbr;
}

/*------------------------------------------------------------------------------
       Callbacks implementation:
           the callbacks API are defined __weak in the stm32746g_discovery_audio.c file
           and their implementation should be done the user code if they are needed.
           Below some examples of callback implementations.
  ----------------------------------------------------------------------------*/
/**
  * @brief  Manages the full Transfer complete event.
  * @param  None
  * @retval None
  */
void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
  if(audio_state == AUDIO_STATE_PLAYING)
  {
    /* allows AUDIO_Process() to refill 2nd part of the buffer  */
    buffer_ctl.state = BUFFER_OFFSET_FULL;
  }
}

/**
  * @brief  Manages the DMA Half Transfer complete event.
  * @param  None
  * @retval None
  */
void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{
  if(audio_state == AUDIO_STATE_PLAYING)
  {
    /* allows AUDIO_Process() to refill 1st part of the buffer  */
    buffer_ctl.state = BUFFER_OFFSET_HALF;
  }
}

/**
  * @brief  Manages the DMA FIFO error event.
  * @param  None
  * @retval None
  */
void BSP_AUDIO_OUT_Error_CallBack(void)
{
  /* Display message on the LCD screen */
  BSP_LCD_SetBackColor(LCD_COLOR_RED);
  BSP_LCD_DisplayStringAtLine(14, (uint8_t *)"       DMA  ERROR     ");

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

