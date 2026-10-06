# RobotEye attribution

The vector eye geometry, expression set, morphing, blinking, gaze, drift, and boot-animation
model in `robot_eye_display.*` are adapted from
[fonzu/RobotEye](https://github.com/fonzu/RobotEye), version 1.0.0, commit
`8758b199ca853904afaa7dc433cedf64ca7bf6cf`.

The upstream project targets Arduino and U8g2 monochrome OLED displays. This adaptation keeps
the ESP-IDF/LVGL TFT display stack used by XiaoZhi and renders RobotEye geometry into an indexed
LVGL canvas. Optional Irisoled bitmap expressions are not included.

## Upstream license

MIT License

Copyright (c) 2026 fonzu (Modified and extended implementation)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
