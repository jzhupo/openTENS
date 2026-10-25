# ESP32⚡openTENS

## Board

- [ESP32-C3 SuperMini](https://www.espboards.dev/esp32/esp32-c3-super-mini/) or [WeMos LOLIN32 Lite](https://www.espboards.dev/esp32/lolin32-lite/)

- openTENS (交流脉冲版电刺激模块_v2)

## Feature

- [DG-Lab coyote pulse format](https://forum.dg-bbs.com/d/18-344-xin-ban-bo-xing-shu-ju-ge-shi-jie-xi) import

- Control panel over WiFi

- WiFi config in data image

## Plan

- Dual channels with electrical isolation

# STM32⚡openTENS

## Board

- [BluePill F103C6](https://mischianti.org/stm32f103c6t6-high-resolution-pinout-and-specs/)

- openTENS (交流脉冲版电刺激模块_v2)

## Feature

- [DG-Lab coyote pulse format](https://forum.dg-bbs.com/d/18-344-xin-ban-bo-xing-shu-ju-ge-shi-jie-xi) prebuilt

## Plan

- Remote control over [LX2241](http://www.chiptrue.com/product/info/3.html) and [蜂鸟无线 灵-R1A](https://fnrf.cc/product/~E7~81~B5-R1A.html)

# SDK

It tries to drive openTENS to work like DG-Lab coyote output pulse [shape and timing](https://www.reddit.com/r/estim/comments/1d0dw1i/dg_lab_coyote_30_review_by_an_electronics/).
So some logics are re-designed and different from sdk_arduino.

## L-boost

| item | as-is | to-be |
| ---- | ----- | ----- |
| duty | fixed 75% | 2%~70% |
| timing | in the beginning of unit | during entire section & unit |
| level | PWM count | PWM duty |

## H-bridge

| item | as-is | to-be |
| ---- | ----- | ----- |
| positive width | fixed 70us | 0~150us |
| gap width | fixed 1ms | fixed 350us |
| negative width | fixed 70us | 0~150us |
| gap width | fixed 1ms | padding to fixed 1ms period |

## Coding

| item | as-is | to-be |
| ---- | ----- | ----- |
| phase switch | loop | timer interrupt |
| user interface | no | web (ESP32) or key (STM32) |

# Tools

## littlefs-python.py

Arduino default https://github.com/earlephilhower/mklittlefs does not support filename using UTF-8, so https://pypi.org/project/littlefs-python/ is used instead.

Need to install with
```
pip install littlefs-python
```
first. And then set extra_scripts to this py in platformio.ini.

## stm32_waveform.c

To reduce Flash usage in STM32, pre-defined struct is defined and built.

Need to download https://download.savannah.gnu.org/releases/tinycc/ and run with
```
TinyCC\tcc.exe -std=c11 -run stm32_waveform.c ..\data "" > ..\include\stm32_waveform.hpp
```

## STM32CubeMX\openTENS.ioc

STM32CubeMX is used to simplify code for STM32Cube LL driver.

Need to download https://www.st.com/en/development-tools/stm32cubemx.html, and then generate code.

Finally copy functions called SystemClock_Config/MX_TIM2_Init/MX_TIM3_Init/MX_GPIO_Init from STM32CubeMX\Core\Src\main.c to src\stm32_cube_ll.cpp.

