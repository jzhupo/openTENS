#pragma once

// 硬件引脚分配
#if defined(OPENTENS_USE_ESP32_ARDUINO)
  #include <Arduino.h>
#elif defined(OPENTENS_USE_ESP32_IDF)
  #include <string>
  #include "freertos/FreeRTOS.h"
  #include "driver/gpio.h"
  #include "driver/ledc.h"
  #include "driver/gptimer.h"
#elif defined(OPENTENS_USE_STM32_ARDUINO)
  #include <Arduino.h>
  #include <HardwareTimer.h>
  #include <stm32f1xx_ll_gpio.h>
#elif defined(OPENTENS_USE_STM32_CUBE_LL)
  #include <stm32f1xx_ll_gpio.h>
  #include <stm32f1xx_ll_tim.h>
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
  #include <stddef.h>
  #include <libopencm3/cm3/cortex.h>
  #include <libopencm3/stm32/gpio.h>
  #include <libopencm3/stm32/timer.h>
#elif defined(OPENTENS_USE_STM32_CMSIS)
  #include <stddef.h>
  #include <stm32f1xx.h>
#elif defined(__TINYC__)
  #include <stddef.h>
  #include <stdint.h>
  #ifndef __cplusplus
    #include <stdbool.h>
    #define nullptr NULL
    #define constexpr static const
  #endif
#else
  #error "Unsupported framework"
#endif

#if defined(OPENTENS_USE_STM32_ARDUINO)
struct stm32_pin_t {
	GPIO_TypeDef *port;
	uint32_t      mask;
	uint8_t       pin;
};
#elif defined(OPENTENS_USE_STM32_CUBE_LL) || defined(OPENTENS_USE_STM32_CMSIS)
struct stm32_pin_t {
	GPIO_TypeDef *port;
	uint32_t      mask;
};
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
struct stm32_pin_t {
	uint32_t port;
	uint16_t mask;
};
#endif

#if defined(ARDUINO_LOLIN_C3_MINI)
  // ESP32-C3 (RISC-V架构)
  #include "cfg_esp32c3.hpp"
#elif defined(ARDUINO_LOLIN32_LITE)
  // ESP32 (Xtensa架构)
  #include "cfg_esp32.hpp"
#elif defined(ARDUINO_BLUEPILL_F103C6)
  // STM32 (ARM架构)
  #include "cfg_stm32.hpp"
#elif defined(__TINYC__)
  // Tiny C Compiler in Windows (x86_64架构)
#else
  #error "Unsupported board"
#endif

#if defined(OPENTENS_USE_ESP32_ARDUINO) || defined(OPENTENS_USE_ESP32_IDF)
  #define ENTER_CRITICAL_LOCK(mux)   portENTER_CRITICAL_ISR(&(mux))
  #define EXIT_CRITICAL_LOCK(mux)    portEXIT_CRITICAL_ISR(&(mux))
  #define ENTER_CRITICAL_NORMAL(mux) portENTER_CRITICAL(&(mux))
  #define EXIT_CRITICAL_NORMAL(mux)  portEXIT_CRITICAL(&(mux))
#elif defined(OPENTENS_USE_STM32_ARDUINO) || defined(OPENTENS_USE_STM32_CUBE_LL) || defined(OPENTENS_USE_STM32_CMSIS)
  #define ENTER_CRITICAL_LOCK(mux)   __disable_irq()
  #define EXIT_CRITICAL_LOCK(mux)    __enable_irq()
  #define ENTER_CRITICAL_NORMAL(mux) __disable_irq()
  #define EXIT_CRITICAL_NORMAL(mux)  __enable_irq()
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
  #define ENTER_CRITICAL_LOCK(mux)   cm_disable_interrupts()
  #define EXIT_CRITICAL_LOCK(mux)    cm_enable_interrupts()
  #define ENTER_CRITICAL_NORMAL(mux) cm_disable_interrupts()
  #define EXIT_CRITICAL_NORMAL(mux)  cm_enable_interrupts()
#endif
#if defined(OPENTENS_USE_ESP32_ARDUINO) || defined(OPENTENS_USE_ESP32_IDF)
#else
  #define IRAM_ATTR
#endif

#define WAVEFORM_MAX_SECTION_COUNT 10
// 加速揉搓.pulse
#define WAVEFORM_MAX_PULSE_COUNT 700
#define WAVEFORM_MAX_STRING_LENGTH 6144

#if !defined(__TINYC__)
// PWM 载波频率 25kHz
constexpr uint32_t BOOST_PWM_FREQ = 25000;
// 8 位分辨率 (0~255)
#if defined(OPENTENS_USE_ESP32_IDF)
constexpr ledc_timer_bit_t BOOST_PWM_RESOLUTION = LEDC_TIMER_8_BIT;
#else
constexpr uint8_t          BOOST_PWM_RESOLUTION = 8;
#endif
#endif

#ifdef __cplusplus
typedef enum : uint8_t {
#else
typedef enum {
#endif
	PHASE_POS_PULSE,   // 正向脉冲 (IN1=1, IN2=0)
	PHASE_INTER_GAP,   // 相间去极化延迟 (IN1=0, IN2=0)
	PHASE_NEG_PULSE,   // 反向脉冲 (IN1=0, IN2=1)
	PHASE_REST_CYCLE,  // 脉冲周期空闲等待 (IN1=0, IN2=0)
	PHASE_SLEEP_START, // 整轮小节完成后的单次大休眠阶段 (IN1=0, IN2=0, PWM=0)
	PHASE_SLEEP_END,   // 休眠结束唤醒阶段 (恢复PWM并瞬时fall-through进正向脉冲)
} pulse_phase_t;

typedef struct {
	uint16_t width_us; // 脉冲宽度: 1.5us * intensity
} Pulse_t;

typedef struct {
	const Pulse_t *pulses;    // 脉冲元列表，动态或静态指向 pulses_pool
	uint32_t section_time_ms; // 小节目标总时长 (ms)
	uint32_t unit_time_ms;    // 单组脉冲元时长 (ms)
	uint16_t period_start_ms; // 起始周期 (ms)
	uint16_t period_end_ms;   // 终止周期 (ms)
	uint16_t unit_count;      // 小节内脉冲元的循环轮数 (向上取整)
	uint16_t pulse_count;     // 每组脉冲元内的脉冲个数
	uint8_t  frequency_mode;  // 频率模式
} Section_t;

typedef struct {
#if defined(OPENTENS_USE_STM32_ARDUINO) || defined(OPENTENS_USE_STM32_CUBE_LL) || defined(OPENTENS_USE_STM32_LIBOPENCM3) || defined(OPENTENS_USE_STM32_CMSIS)
	const Section_t *sections;
#else
	Section_t sections[WAVEFORM_MAX_SECTION_COUNT];  // 小节列表
	Pulse_t   pulses_pool[WAVEFORM_MAX_PULSE_COUNT]; // 统一的扁平化脉冲数组
#endif
	uint8_t pulse_section_pause_time; // 休息时长
	uint8_t speed_factor;             // 播放速度因子
	uint8_t frequency_balance;        // 通道平衡参数
	uint8_t section_count;            // 实际有效小节数量
} Waveform_t;

typedef struct Channel {
#if defined(OPENTENS_USE_ESP32_ARDUINO)
	hw_timer_t      *timer;
	portMUX_TYPE     timer_mux;
#elif defined(OPENTENS_USE_ESP32_IDF)
	gptimer_handle_t timer;
	portMUX_TYPE     timer_mux;
#elif defined(OPENTENS_USE_STM32_ARDUINO)
	HardwareTimer   *timer;
#endif
	Waveform_t waveform; // 当前正在执行的活动波形

	volatile uint32_t section_elapsed_ms; // 实时累加当前小节已经播放的绝对物理时长 (ms)
	volatile uint16_t pulse_period_ms;    // 当前脉冲总周期 (ms)
	volatile uint16_t pos_pulse_width_us; // 正向脉冲宽度 (us)
	volatile uint16_t neg_pulse_width_us; // 反向脉冲宽度 (us)
	volatile uint8_t global_intensity;    // 全局输出刺激强度
	volatile uint8_t section_index;       // 当前正在执行的小节索引
	volatile pulse_phase_t pulse_phase;   // 状态机枚举
	volatile bool running;                // 通道输出使能开关

#if defined(OPENTENS_USE_ESP32_ARDUINO)
	uint8_t        pin_boost;  // boost PWM引脚
	uint8_t        ledc_boost; // boost PWM通道
#elif defined(OPENTENS_USE_ESP32_IDF)
	gpio_num_t     pin_boost;
	ledc_channel_t ledc_boost;
#endif
#if defined(OPENTENS_USE_STM32_ARDUINO) || defined(OPENTENS_USE_STM32_CUBE_LL) || defined(OPENTENS_USE_STM32_LIBOPENCM3) || defined(OPENTENS_USE_STM32_CMSIS)
	stm32_pin_t pin_in1;
	stm32_pin_t pin_in2;
#elif defined(OPENTENS_USE_ESP32_IDF)
	gpio_num_t  pin_in1;
	gpio_num_t  pin_in2;
#else
	uint8_t     pin_in1; // H桥正向驱动引脚
	uint8_t     pin_in2; // H桥反向驱动引脚
#endif

#if defined(OPENTENS_USE_ESP32_ARDUINO)
	String      raw_waveform; // 网页端波形配置镜像字符串
	struct Channel *mirror;
#elif defined(OPENTENS_USE_ESP32_IDF)
	std::string raw_waveform;
	struct Channel *mirror;
#endif
} Channel_t;


#if defined(OPENTENS_USE_STM32_ARDUINO) || defined(OPENTENS_USE_STM32_CUBE_LL) || defined(OPENTENS_USE_STM32_LIBOPENCM3) || defined(OPENTENS_USE_STM32_CMSIS)
inline __attribute__((always_inline)) void updatePinOutput(const stm32_pin_t pin, const bool high) {
#if defined(OPENTENS_USE_STM32_LIBOPENCM3)
	if (high) {
		gpio_set(pin.port, pin.mask);
	} else {
		gpio_clear(pin.port, pin.mask);
	}
#elif defined(OPENTENS_USE_STM32_CMSIS)
	if (high) {
		WRITE_REG(pin.port->BSRR, pin.mask);
	} else {
		WRITE_REG(pin.port->BRR, pin.mask);
	}
#else
	if (high) {
		LL_GPIO_SetOutputPin(pin.port, pin.mask);
	} else {
		LL_GPIO_ResetOutputPin(pin.port, pin.mask);
	}
#endif
}
#endif

#if defined(OPENTENS_USE_ESP32_ARDUINO) || defined(OPENTENS_USE_STM32_ARDUINO)
inline __attribute__((always_inline)) void IRAM_ATTR updatePinOutput(const uint8_t pin, const bool high) {
	digitalWrite(pin, high ? HIGH : LOW);
}
#elif defined(OPENTENS_USE_ESP32_IDF)
inline __attribute__((always_inline)) void IRAM_ATTR updatePinOutput(const gpio_num_t pin, const bool high) {
	gpio_set_level(pin, high ? 1 : 0);
}
#endif


extern Channel_t channelA;
#if defined(OPENTENS_USE_ESP32_ARDUINO) || defined(OPENTENS_USE_ESP32_IDF)
extern Channel_t channelB;
#endif

#if defined(OPENTENS_USE_ESP32_ARDUINO) || defined(OPENTENS_USE_ESP32_IDF)
Channel_t *getChannelByKey(const char *key_id);
void handleMirror(bool enable);
#endif
void setupTimer(Channel_t *channel, void (*userFunc)(void));
void setupGPIO(Channel_t *channel);
bool handleStart(Channel_t *channel, const char *input_waveform);
void handleStop(Channel_t *channel);
void handleIntensity(Channel_t *channel, bool up);
