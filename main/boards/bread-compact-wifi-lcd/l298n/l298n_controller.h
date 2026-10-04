#pragma once

// Board-lifetime controller; call once after the LCD backlight is initialized.
void InitializeL298nController();

// Called only after initialization. Records display events; motion runs on the motor timer.
void UpdateL298nEmotion(const char* emotion, bool speaking);
