#if defined(OPENTENS_USE_STM32_ARDUINO) || defined(OPENTENS_USE_STM32_CUBE_LL) || defined(OPENTENS_USE_STM32_LIBOPENCM3) || defined(OPENTENS_USE_STM32_CMSIS)

/*
STM32F103 的每个 GPIO 端口有 16 个引脚，每个引脚在 CRL/CRH 寄存器中占 4 位（CNF[1:0] + MODE[1:0]）

CNF[1:0] (高2位)：控制引脚的 配置模式（如模拟、浮空、推挽、开漏）
- 00：通用推挽输出 (General Purpose Output Push-Pull)
- 10：复用推挽输出 (Alternate Function Output Push-Pull)

MODE[1:0] (低2位)：控制引脚的输出速度
- 00 (0x0)：输入模式
- 01 (0x1)：最大输出速度 10MHz
- 10 (0x2)：最大输出速度 2MHz
- 11 (0x3)：最大输出速度 50MHz

控制区位偏移
- CRL 控制 Pin 0 ~ Pin 7，偏移量为 Pin * 4
- CRH 控制 Pin 8 ~ Pin 15，偏移量为 (Pin - 8) * 4

| 引脚         | 所属寄存器 | 位偏移计算            | 模式 (50MHz)     | 配置值      |
| ------------ | ---------- | --------------------- | ---------------- | ----------- |
| PC13 (LED)   | GPIOC->CRH | (13 - 8) × 4 = 20 位 | 通用推挽 (0x2UL) | 0x2UL << 20 |
| PB0 (PWM)    | GPIOB->CRL | 0 × 4 = 0 位         | 复用推挽 (0xBUL) | 0xBUL << 0  |
| PB1 (IN1_A)  | GPIOB->CRL | 1 × 4 = 4 位         | 通用推挽 (0x3UL) | 0x3UL << 4  |
| PB10 (IN2_A) | GPIOB->CRH | (10 - 8) × 4 = 8 位  | 通用推挽 (0x3UL) | 0x3UL << 8  |
*/

#include <pt.h>

#include "channel.hpp"
#include "stm32_waveform.hpp"

#if defined(OPENTENS_USE_STM32_LIBOPENCM3)
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#elif defined(OPENTENS_USE_STM32_CMSIS)
void stm32PinMode(GPIO_TypeDef *port, uint32_t pin_mask, uint8_t cnf, uint8_t mode);
#endif


constexpr size_t WAVEFORM_COUNT = sizeof(WAVEFORM_LIST) / sizeof(WAVEFORM_LIST[0]);
static uint8_t waveform_index = 0;


constexpr uint32_t MATRIX_KEYPAD_FILTER_TICK = 25;

typedef struct {
#if defined(OPENTENS_USE_STM32_CUBE_LL) || defined(OPENTENS_USE_STM32_LIBOPENCM3) || defined(OPENTENS_USE_STM32_CMSIS)
	const stm32_pin_t *rows;
	const stm32_pin_t *columns;
#else
	const uint8_t *rows;
	const uint8_t *columns;
#endif
	uint32_t start_tick;
	struct pt proto_thread;
	uint8_t row_count;
	uint8_t column_count;
	uint8_t scan_column;
	uint8_t scan_row;
	uint8_t active_row;
} MatrixKeypad_t;
static MatrixKeypad_t matrix_keypad;


constexpr uint32_t LED_PHASE_TICK = 250;

typedef struct {
	uint32_t start_tick;
	struct pt proto_thread;
	uint8_t phases;
} BoardLed_t;
static BoardLed_t board_led;


#if defined(OPENTENS_USE_STM32_CUBE_LL) || defined(OPENTENS_USE_STM32_LIBOPENCM3) || defined(OPENTENS_USE_STM32_CMSIS)
static volatile uint32_t g_system_ticks = 0;
#if defined(OPENTENS_USE_STM32_LIBOPENCM3)
extern "C" void sys_tick_handler(void) {
#else
extern "C" void SysTick_Handler(void) {
#endif
	g_system_ticks = g_system_ticks + 1;
}
#endif

static inline uint32_t getSysTickCount(void) {
#if defined(OPENTENS_USE_STM32_CUBE_LL) || defined(OPENTENS_USE_STM32_LIBOPENCM3) || defined(OPENTENS_USE_STM32_CMSIS)
	return g_system_ticks;
#else
	return millis();
#endif
}


#if defined(OPENTENS_USE_STM32_ARDUINO) || defined(OPENTENS_USE_STM32_CUBE_LL) || defined(OPENTENS_USE_STM32_LIBOPENCM3) || defined(OPENTENS_USE_STM32_CMSIS)
inline __attribute__((always_inline)) bool readPinInput(stm32_pin_t pin) {
#if defined(OPENTENS_USE_STM32_LIBOPENCM3)
	return gpio_get(pin.port, pin.mask) != 0;
#elif defined(OPENTENS_USE_STM32_CMSIS)
	return READ_BIT(pin.port->IDR, pin.mask) != 0;
#else
	return LL_GPIO_IsInputPinSet(pin.port, pin.mask) != 0;
#endif
}
#endif

#if defined(OPENTENS_USE_STM32_ARDUINO) || defined(OPENTENS_USE_ESP32_ARDUINO)
inline __attribute__((always_inline)) bool readPinInput(uint8_t pin) {
	return digitalRead(pin) == HIGH;
}
#elif defined(OPENTENS_USE_ESP32_IDF)
inline __attribute__((always_inline)) bool readPinInput(gpio_num_t pin) {
	return gpio_get_level(pin) != 0;
}
#endif


// 矩阵键盘线性扫描协程
static PT_THREAD(scanMatrixKeypad(MatrixKeypad_t *keypad, uint8_t *press_key, const uint32_t now)) {
	*press_key = 0;
	PT_BEGIN(&keypad->proto_thread);

	while (true) {
		// 选中当前列，等待 1ms 让电平稳定
		updatePinOutput(keypad->columns[keypad->scan_column], false);
		keypad->start_tick = now;
		PT_WAIT_UNTIL(&keypad->proto_thread, now - keypad->start_tick >= 1);

		// 扫描本列的所有行
		keypad->scan_row = 0;
		while (keypad->scan_row < keypad->row_count) {
			// 检测到当前行可能按下
			if (!readPinInput(keypad->rows[keypad->scan_row])) {
				keypad->active_row = keypad->scan_row;
				// 延时 25ms 消抖，不读 GPIO
				keypad->start_tick = now;
				PT_WAIT_UNTIL(&keypad->proto_thread, now - keypad->start_tick >= MATRIX_KEYPAD_FILTER_TICK);
				// 25ms 结束，仅读取一次 GPIO 最终确认
				if (readPinInput(keypad->rows[keypad->active_row])) {
					// 发生按下毛刺则重试
					PT_YIELD(&keypad->proto_thread);
					continue;
				}
				// 确认有效按下，输出键值
				*press_key = static_cast<uint8_t>(keypad->active_row * keypad->column_count + keypad->scan_column + 1);
				PT_YIELD(&keypad->proto_thread);
				do {
					// 降频每隔 10ms 检查一次释放
					do {
						keypad->start_tick = now;
						PT_WAIT_UNTIL(&keypad->proto_thread, now - keypad->start_tick >= 10);
					} while(!readPinInput(keypad->rows[keypad->active_row]));
					// 延时 25ms 消抖，不读 GPIO
					keypad->start_tick = now;
					PT_WAIT_UNTIL(&keypad->proto_thread, now - keypad->start_tick >= MATRIX_KEYPAD_FILTER_TICK);
					// 25ms 结束，仅读取一次 GPIO 最终确认，未释放则继续挂起 10ms
				} while (!readPinInput(keypad->rows[keypad->active_row]));
			}
			// 转向下一行
			++keypad->scan_row;
			if (keypad->scan_row < keypad->row_count) {
				PT_YIELD(&keypad->proto_thread);
			}
		}
		// 本列所有行扫描完毕，拉高列线并切换到下一列
		updatePinOutput(keypad->columns[keypad->scan_column], true);
		keypad->scan_column = (keypad->scan_column + 1) % keypad->column_count;
		PT_YIELD(&keypad->proto_thread);
	}

	PT_END(&keypad->proto_thread);
}


// LED 状态相位闪烁协程
static PT_THREAD(updateBoardLed(BoardLed_t *led, const uint32_t now)) {
	PT_BEGIN(&led->proto_thread);

	while (true) {
		// 挂起直到有闪烁任务（led_phases > 0）
		PT_WAIT_UNTIL(&led->proto_thread, led->phases > 0);

		while (led->phases > 0) {
			PT_WAIT_UNTIL(&led->proto_thread, (now - led->start_tick) >= LED_PHASE_TICK);
			led->start_tick = now;
			--led->phases;
			const bool on = (led->phases > 0) ? (led->phases % 2 == 0) : channelA.running;
			updatePinOutput(LED_PIN, !on);
		}
	}

	PT_END(&led->proto_thread);
}

void setupMatrixKeypad(MatrixKeypad_t *keypad) {
	keypad->rows = MATRIX_KEYPAD_ROWS;
	keypad->columns = MATRIX_KEYPAD_COLUMNS;
	keypad->start_tick = 0;
	keypad->row_count = MATRIX_KEYPAD_ROW_COUNT;
	keypad->column_count = MATRIX_KEYPAD_COLUMN_COUNT;
	keypad->scan_column = 0;
	keypad->scan_row = 0;
	keypad->active_row = 0;
	// 初始化键盘协程与扫描上下文。
	PT_INIT(&keypad->proto_thread);
	// 关闭 JTAG 释放 PA15/PB3/PB4，仍然保留 SWD
#if defined(OPENTENS_USE_STM32_LIBOPENCM3)
	rcc_periph_clock_enable(RCC_AFIO);
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	gpio_primary_remap(AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_ON, 0);
#elif defined(OPENTENS_USE_STM32_CMSIS)
	SET_BIT(RCC->APB2ENR, RCC_APB2ENR_AFIOEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN);
	static_cast<void>(RCC->APB2ENR);
	MODIFY_REG(AFIO->MAPR, AFIO_MAPR_SWJ_CFG, AFIO_MAPR_SWJ_CFG_JTAGDISABLE);
#endif
	for (uint8_t column = 0; column < keypad->column_count; ++column) {
#if defined(OPENTENS_USE_STM32_CUBE_LL)
		// stm32_cube_ll.cpp MX_GPIO_Init() 已经处理
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
		const stm32_pin_t *pin = &keypad->columns[column];
		gpio_set(pin->port, pin->mask);
		gpio_set_mode(pin->port, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_OPENDRAIN, pin->mask);
#elif defined(OPENTENS_USE_STM32_CMSIS)
		const stm32_pin_t *pin = &keypad->columns[column];
		WRITE_REG(pin->port->BSRR, pin->mask);
		stm32PinMode(pin->port, pin->mask, 0b01, 0b10);
#else
		pinMode(keypad->columns[column], OUTPUT_OPEN_DRAIN);
		digitalWrite(keypad->columns[column], HIGH);
#endif
	}
	for (uint8_t row = 0; row < keypad->row_count; ++row) {
#if defined(OPENTENS_USE_STM32_CUBE_LL)
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
		const stm32_pin_t *pin = &keypad->rows[row];
		gpio_set(pin->port, pin->mask);
		gpio_set_mode(pin->port, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, pin->mask);
#elif defined(OPENTENS_USE_STM32_CMSIS)
		const stm32_pin_t *pin = &keypad->rows[row];
		WRITE_REG(pin->port->BSRR, pin->mask);
		stm32PinMode(pin->port, pin->mask, 0b10, 0b00);
#else
		pinMode(keypad->rows[row], INPUT_PULLUP);
#endif
	}
}


static bool handleButtonStart(Channel_t *channel, const uint8_t key_id) {
	if (channel == nullptr || WAVEFORM_COUNT == 0 || key_id == 0 || key_id > 13) {
		return false;
	}
	if (key_id == 13) {
		// start or next
		if (channel->running) {
			waveform_index = (waveform_index + 1) % WAVEFORM_COUNT;
		}
	} else {
		if (key_id > WAVEFORM_COUNT) {
			return false;
		}
		waveform_index = key_id - 1;
	}
	handleStop(channel);
	channel->waveform = *WAVEFORM_LIST[waveform_index];
	return handleStart(channel, nullptr);
}

void setupSTM32(void) {
	setupMatrixKeypad(&matrix_keypad);

	board_led.start_tick = 0;
	board_led.phases = 0;
	PT_INIT(&board_led.proto_thread);
}

void loopSTM32(void) {
	const uint32_t now = getSysTickCount();
	uint8_t key = 0;
	scanMatrixKeypad(&matrix_keypad, &key, now);
	if (key == 15 || key == 16) {
		handleIntensity(&channelA, key == 16);
	} else if (key == 14) {
		handleStop(&channelA);
	} else if (key != 0) {
		handleButtonStart(&channelA, key);
	}

	if (key != 0) {
		board_led.phases = key * 2;
		board_led.start_tick = now;
		updatePinOutput(LED_PIN, false);
		PT_INIT(&board_led.proto_thread);
	}
	updateBoardLed(&board_led, now);
}

#if defined(OPENTENS_USE_STM32_ARDUINO)
namespace std {
	void __throw_bad_function_call() {
		while (1) {
		}
	}
}
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3) || defined(OPENTENS_USE_STM32_CMSIS)
static void systick_setup(void) {
	// 最低优先级
#if defined(OPENTENS_USE_STM32_LIBOPENCM3)
	nvic_set_priority(NVIC_SYSTICK_IRQ, 15 << 4);
#elif defined(OPENTENS_USE_STM32_CMSIS)
	NVIC_SetPriority(SysTick_IRQn, 15);
#endif
	// HCLK = 72MHz，设定 1ms 产生一次 tick：72000000 / 1000 = 72000
#if defined(OPENTENS_USE_STM32_LIBOPENCM3)
	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
	systick_set_reload(72000 - 1);
	systick_counter_enable();
	systick_interrupt_enable();
#elif defined(OPENTENS_USE_STM32_CMSIS)
	WRITE_REG(SysTick->LOAD, 72000 - 1); // 72MHz / 1000 = 72000 个 tick = 1ms
	CLEAR_REG(SysTick->VAL); // 清当前计数值
	WRITE_REG(SysTick->CTRL,
		SysTick_CTRL_CLKSOURCE_Msk // 时钟源 = HCLK
		| SysTick_CTRL_TICKINT_Msk // 计数到 0 时产生中断
		| SysTick_CTRL_ENABLE_Msk); // 启动
#endif
}

extern "C" int main(void) {
	// HSE 外部 8MHz 晶振，x9 倍频至 72MHz
#if defined(OPENTENS_USE_STM32_LIBOPENCM3)
	rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);
#elif defined(OPENTENS_USE_STM32_CMSIS)
	SET_BIT(RCC->CR, RCC_CR_HSEON);
	while (READ_BIT(RCC->CR, RCC_CR_HSERDY) == 0) {
	}

	WRITE_REG(FLASH->ACR, FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2);
	SET_BIT(RCC->CFGR, RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9);
	SET_BIT(RCC->CFGR, RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV1);
	SET_BIT(RCC->CR, RCC_CR_PLLON);
	while (READ_BIT(RCC->CR, RCC_CR_PLLRDY) == 0) {
	}

	SET_BIT(RCC->CFGR, RCC_CFGR_SW_PLL);
	while (READ_BIT(RCC->CFGR, RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
	}

	SystemCoreClockUpdate();
#endif
	systick_setup();

	setupGPIO(nullptr);
	setupGPIO(&channelA);
	setupTimer(&channelA, nullptr);
	setupSTM32();

	while (true) {
		loopSTM32();
	}
	return 0;
}
#endif//OPENTENS_USE_STM32_LIBOPENCM3 || OPENTENS_USE_STM32_CMSIS

#endif//OPENTENS_USE_STM32_*
