#pragma once

#include <driver/gpio.h>
#include <sdkconfig.h>

#include "audio_config.h"
#include "motion_control_config.h"
#include "robot_hardware_config.h"

#ifdef CONFIG_BREAD_COMPACT_WIFI_LCD_FC03
// Reserve GPIO48 for the right encoder; do not drive the onboard LED on this pin.
#define BUILTIN_LED_GPIO GPIO_NUM_NC
#else
#define BUILTIN_LED_GPIO GPIO_NUM_48
#endif
#define BOOT_BUTTON_GPIO GPIO_NUM_0
#define TOUCH_BUTTON_GPIO GPIO_NUM_NC
#define VOLUME_UP_BUTTON_GPIO GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

// No lamp is connected; this pin remains unavailable to the car.
#define LAMP_GPIO GPIO_NUM_NC
