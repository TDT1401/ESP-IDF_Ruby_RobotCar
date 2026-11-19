#pragma once

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE 16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// Set to 1 to log microphone levels every 2 seconds, then rebuild and flash.
// Startup configuration and I2S read errors are logged even when this is 0.
#define MIC_DEBUG 0

#define AUDIO_I2S_MIC_GPIO_WS GPIO_NUM_1
#define AUDIO_I2S_MIC_GPIO_SCK GPIO_NUM_2
#define AUDIO_I2S_MIC_GPIO_DIN GPIO_NUM_17
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16
