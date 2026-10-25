#include "channel.hpp"
#if defined(OPENTENS_USE_ESP32_ARDUINO) || defined(OPENTENS_USE_ESP32_IDF)
  #include "dg_lab.hpp"
#endif
#if defined(OPENTENS_USE_ESP32_IDF)
  #include <esp_rom_sys.h>
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
  #include <libopencm3/stm32/rcc.h>
  #include <libopencm3/cm3/nvic.h>
#endif

constexpr uint32_t BOOST_PWM_MAX_DUTY = 70UL * 255UL / 100UL; // 占空比 (0~255*70%)

constexpr uint16_t inter_gap_width_us = 350; // 正负脉冲相间延迟固定为 350us
constexpr uint16_t min_rest_cycle_us  = 350; // 周期末尾最小保护空闲时间为 350us


static void IRAM_ATTR updatePulsePeriod(Channel_t *channel) {
	// 有效性检查放在 parseWaveform
	const Section_t *section_data = &channel->waveform.sections[channel->section_index];
	const uint32_t section_elapsed_ms = channel->section_elapsed_ms;
	uint16_t unit_index = static_cast<uint16_t>(section_elapsed_ms / section_data->unit_time_ms);
	if (unit_index >= section_data->unit_count) {
		unit_index = section_data->unit_count - 1;
	}

	// unit_elapsed_ms ÷ unit_time_ms = pulse_index ÷ pulse_count
	const uint32_t unit_elapsed_ms = section_elapsed_ms % section_data->unit_time_ms;
	uint16_t pulse_index = static_cast<uint16_t>(
		(unit_elapsed_ms * section_data->pulse_count) / section_data->unit_time_ms);
	if (pulse_index >= section_data->pulse_count) {
		pulse_index = section_data->pulse_count - 1;
	}

	channel->pos_pulse_width_us = section_data->pulses[pulse_index].width_us;
	channel->neg_pulse_width_us = channel->pos_pulse_width_us;

	// 插值计算
	int16_t period_delta = 0;
	const int32_t period_diff = static_cast<int32_t>(section_data->period_end_ms)
		- static_cast<int32_t>(section_data->period_start_ms);

	if (period_diff == 0) {
	} else if (section_data->frequency_mode == 2) {
		// 节内渐变
		period_delta = static_cast<int16_t>(period_diff
			* static_cast<int32_t>(section_elapsed_ms)
			/ static_cast<int32_t>(section_data->section_time_ms));
	} else if (section_data->frequency_mode == 3) {
		// 元内渐变
		period_delta = static_cast<int16_t>(period_diff
			* static_cast<int32_t>(unit_elapsed_ms)
			/ static_cast<int32_t>(section_data->unit_time_ms));
	} else if (section_data->frequency_mode == 4) {
		// 元间渐变
		if (section_data->unit_count > 1) {
			period_delta = static_cast<int16_t>(period_diff
				* static_cast<int32_t>(unit_index)
				/ static_cast<int32_t>(section_data->unit_count - 1));
		}
	}

	const int16_t base_period_ms = section_data->period_start_ms + period_delta;
	if (channel->waveform.speed_factor == 0 || channel->waveform.speed_factor == 1) {
		channel->pulse_period_ms = base_period_ms;
	} else {
		channel->pulse_period_ms = base_period_ms / channel->waveform.speed_factor;
	}
	const uint32_t total_active_pulse_us = channel->pos_pulse_width_us
		+ inter_gap_width_us + channel->neg_pulse_width_us + min_rest_cycle_us;
	const uint16_t min_period_ms = static_cast<uint16_t>((total_active_pulse_us + 999UL) / 1000UL);
	if (channel->pulse_period_ms < min_period_ms) {
		channel->pulse_period_ms = min_period_ms;
	}
}

static bool IRAM_ATTR updatePulseCounter(Channel_t *channel) {
	const uint32_t this_section_time_ms = channel->waveform.sections[channel->section_index].section_time_ms;
	channel->section_elapsed_ms = channel->section_elapsed_ms + channel->pulse_period_ms;

	if (channel->section_elapsed_ms < this_section_time_ms) {
		return false;
	}
	// 达到或超过该小节总时间，无缝切入下一小节
	channel->section_elapsed_ms -= this_section_time_ms;
	channel->section_index = channel->section_index + 1;
	if (channel->section_index >= channel->waveform.section_count) {
		channel->section_index = 0;
	}
	const uint32_t next_section_time_ms = channel->waveform.sections[channel->section_index].section_time_ms;
	// 从长小节切到短小节，或异常 frequency_A/B
	if (channel->section_elapsed_ms >= next_section_time_ms) {
		channel->section_elapsed_ms %= next_section_time_ms;
	}

	// 整轮波形是否结束
	return channel->section_index == 0;
}


// GPIO控制
static inline void IRAM_ATTR updatePinBridge(Channel_t *channel, bool pin1, bool pin2) {
#if defined(OPENTENS_USE_STM32_LIBOPENCM3)
	if ((channel->pin_in1.port == channel->pin_in2.port) && (!pin1 && !pin2)) {
		gpio_clear(channel->pin_in1.port, channel->pin_in1.mask | channel->pin_in2.mask);
	} else
#elif defined(OPENTENS_USE_STM32_CMSIS)
	if ((channel->pin_in1.port == channel->pin_in2.port) && (!pin1 && !pin2)) {
		WRITE_REG(channel->pin_in1.port->BRR, channel->pin_in1.mask | channel->pin_in2.mask);
	} else
#endif
	{
		if (!pin1) {
			updatePinOutput(channel->pin_in1, false);
		}
		if (!pin2) {
			updatePinOutput(channel->pin_in2, false);
		}
		if (pin1) {
			updatePinOutput(channel->pin_in1, true);
		}
		if (pin2) {
			updatePinOutput(channel->pin_in2, true);
		}
	}
#if defined(OPENTENS_USE_ESP32_ARDUINO) || defined(OPENTENS_USE_ESP32_IDF)
	if (channel->mirror != nullptr) {
		if (!pin1) {
			updatePinOutput(channel->mirror->pin_in1, false);
		}
		if (!pin2) {
			updatePinOutput(channel->mirror->pin_in2, false);
		}
		if (pin1) {
			updatePinOutput(channel->mirror->pin_in1, true);
		}
		if (pin2) {
			updatePinOutput(channel->mirror->pin_in2, true);
		}
	}
#endif
}

static void IRAM_ATTR updatePinBoost(Channel_t *channel, bool enable) {
	uint32_t final_duty = 0;
	if (enable && channel->running) {
		if (channel->global_intensity >= 100) {
			final_duty = BOOST_PWM_MAX_DUTY;
		} else if (channel->global_intensity > 0) {
			// 二次衰减法，让每一档 out (1~178) 的增加幅度，随着in (1~100) 的升高而减小，
			// 并且还要确保每一档都有物理变化。
			// 理想 out = 178 - 1.01 * (100 - in) - 0.007878 * (100 - in)^2
			// 近似 out = 178 - (100 - in) - (100 - in)^2 / 128
			const uint32_t diff1 = 100UL - channel->global_intensity;
			const uint32_t diff2 = (diff1 * diff1) >> 7UL;
			final_duty = BOOST_PWM_MAX_DUTY - diff1 - diff2;
		}
	}
#if defined(OPENTENS_USE_ESP32_ARDUINO)
	ledcWriteChannel(channel->ledc_boost, final_duty);
	if (channel->mirror != nullptr) {
		ledcWriteChannel(channel->mirror->ledc_boost, final_duty);
	}
#elif defined(OPENTENS_USE_ESP32_IDF)
	ledc_set_duty(LEDC_LOW_SPEED_MODE, channel->ledc_boost, final_duty);
	ledc_update_duty(LEDC_LOW_SPEED_MODE, channel->ledc_boost);
	if (channel->mirror != nullptr) {
		ledc_set_duty(LEDC_LOW_SPEED_MODE, channel->mirror->ledc_boost, final_duty);
		ledc_update_duty(LEDC_LOW_SPEED_MODE, channel->mirror->ledc_boost);
	}
#elif defined(OPENTENS_USE_STM32_CUBE_LL)
	LL_TIM_OC_SetCompareCH3(TIM3, final_duty * 144U / 256U);
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
	timer_set_oc_value(TIM3, TIM_OC3, final_duty * 144U / 256U);
#elif defined(OPENTENS_USE_STM32_CMSIS)
	WRITE_REG(TIM3->CCR3, final_duty * 144U / 256U);
#else
	analogWrite(BOOST_A_PIN, final_duty);
#endif
}

#if defined(OPENTENS_USE_STM32_CMSIS)
void stm32PinMode(GPIO_TypeDef *port, uint32_t pin_mask, uint8_t cnf, uint8_t mode) {
	if (port == nullptr) {
		return;
	}
	if (pin_mask == 0) {
		return;
	}

	// 1. 利用硬件单周期指令，瞬间将掩码转换为引脚序号 0~15
	const uint32_t pin_idx = static_cast<uint32_t>(__builtin_ctz(pin_mask));
	if (pin_idx > 15) {
		// 防御性越界拦截
		return;
	}

	// 2. 计算当前引脚在其对应控制寄存器 (CRL 或 CRH) 内的相对偏移量 (0~28 位)
	const uint32_t shift = (pin_idx & 0x07UL) * 4UL;

	// 3. 自动根据引脚号是 0~7 还是 8~15，指针偏移至 CRL 或 CRH 寄存器
	// pin_idx < 8 (0) 指向 &port->CRL; pin_idx >= 8 (1) 指向 &port->CRH
	volatile uint32_t *cr_reg = (pin_idx < 8) ? &port->CRL : &port->CRH;

	// 4. 将传入的 2位 cnf 和 2位 mode 合并成 4位配置值 (cnf 占高2位，mode 占低2位)
	const uint32_t config_val =
		((static_cast<uint32_t>(cnf) & 0x03UL) << 2UL) | (static_cast<uint32_t>(mode) & 0x03UL);

	// 5. 清除目标 4 位并填入新配置，全程指针安全保护
	CLEAR_BIT(*cr_reg, 0xFUL << shift);
	SET_BIT(*cr_reg, (config_val & 0xFUL) << shift);
}
#endif//OPENTENS_USE_STM32_CMSIS

void setupGPIO(Channel_t *channel) {
	if (channel == nullptr) {

		// 开启时钟
#if defined(OPENTENS_USE_STM32_LIBOPENCM3)
		rcc_periph_clock_enable(RCC_GPIOB);
		rcc_periph_clock_enable(RCC_GPIOC);
		rcc_periph_clock_enable(RCC_TIM3);
		__asm__ volatile("dsb");
#elif defined(OPENTENS_USE_STM32_CMSIS)
		SET_BIT(RCC->APB2ENR, RCC_APB2ENR_IOPBEN);
		SET_BIT(RCC->APB2ENR, RCC_APB2ENR_IOPCEN);
		SET_BIT(RCC->APB1ENR, RCC_APB1ENR_TIM3EN);
		while (READ_BIT(RCC->APB1ENR, RCC_APB1ENR_TIM3EN) == 0) {}
#endif
		// LED
#if defined(OPENTENS_USE_STM32_CUBE_LL)
		// stm32_cube_ll.cpp MX_GPIO_Init() 已经处理
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
		gpio_set(LED_PIN.port, LED_PIN.mask);
		gpio_set_mode(LED_PIN.port, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, LED_PIN.mask);
#elif defined(OPENTENS_USE_STM32_CMSIS)
		// LED PC13 位于CRH，偏移20，CNF=00, MODE=10，2MHz通用推挽输出
		WRITE_REG(LED_PIN.port->BSRR, LED_PIN.mask);
		stm32PinMode(LED_PIN.port, LED_PIN.mask, 0b00, 0b10);
#elif defined(OPENTENS_USE_ESP32_IDF)
		gpio_config_t led_conf = {};
		led_conf.pin_bit_mask = (1ULL << LED_PIN);
		led_conf.mode = GPIO_MODE_OUTPUT;
		ESP_ERROR_CHECK(gpio_config(&led_conf));
		gpio_set_level(LED_PIN, 1);
#else
		pinMode(LED_PIN, OUTPUT);
		digitalWrite(LED_PIN, HIGH);
#endif
		// Boost PWM timer
#if defined(OPENTENS_USE_ESP32_IDF)
		ledc_timer_config_t ledc_timer = {};
		ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE;
		ledc_timer.duty_resolution = BOOST_PWM_RESOLUTION;
		ledc_timer.timer_num = LEDC_TIMER_0;
		ledc_timer.freq_hz = BOOST_PWM_FREQ;
		ledc_timer.clk_cfg = LEDC_AUTO_CLK;
		ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
		// TIM3
		rcc_periph_reset_pulse(RST_TIM3);
		timer_set_mode(TIM3, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
		timer_set_prescaler(TIM3, 19); // 72MHz ÷ (19 + 1) = 3.6MHz
		timer_set_period(TIM3, 143); // 3.6MHz ÷ (143 + 1) = 25kHz
#elif defined(OPENTENS_USE_STM32_CMSIS)
		// TIM3
		SET_BIT(RCC->APB1RSTR, RCC_APB1RSTR_TIM3RST);
		CLEAR_BIT(RCC->APB1RSTR, RCC_APB1RSTR_TIM3RST);
		CLEAR_BIT(TIM3->CR1, (TIM_CR1_CKD | TIM_CR1_CMS | TIM_CR1_DIR));
		WRITE_REG(TIM3->PSC, 19); // 72MHz ÷ (19 + 1) = 3.6MHz
		WRITE_REG(TIM3->ARR, 143); // 3.6MHz ÷ (143 + 1) = 25kHz
#endif
	} else {
		// H桥 GPIO
#if defined(OPENTENS_USE_STM32_ARDUINO)
		pinMode(IN1_A_PIN.pin, OUTPUT);
		digitalWrite(IN1_A_PIN.pin, LOW);
		pinMode(IN2_A_PIN.pin, OUTPUT);
		digitalWrite(IN2_A_PIN.pin, LOW);
#elif defined(OPENTENS_USE_STM32_CUBE_LL)
		// stm32_cube_ll.cpp MX_GPIO_Init() 已经处理
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
		if (channel->pin_in1.port == channel->pin_in2.port) {
			gpio_clear(channel->pin_in1.port, channel->pin_in1.mask | channel->pin_in2.mask);
		} else {
			gpio_clear(channel->pin_in1.port, channel->pin_in1.mask);
			gpio_clear(channel->pin_in2.port, channel->pin_in2.mask);
		}
		gpio_set_mode(channel->pin_in1.port, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, channel->pin_in1.mask);
		gpio_set_mode(channel->pin_in2.port, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, channel->pin_in2.mask);
#elif defined(OPENTENS_USE_STM32_CMSIS)
		// 2个pin同时清0
		if (channel->pin_in1.port == channel->pin_in2.port) {
			WRITE_REG(channel->pin_in1.port->BRR, channel->pin_in1.mask | channel->pin_in2.mask);
		} else {
			WRITE_REG(channel->pin_in1.port->BRR, channel->pin_in1.mask);
			WRITE_REG(channel->pin_in2.port->BRR, channel->pin_in2.mask);
		}
		// PB1（IN1_A）位于CRL，偏移4，CNF=00, MODE=11，50MHz通用推挽输出
		stm32PinMode(channel->pin_in1.port, channel->pin_in1.mask, 0b00, 0b11);
		// PB10（IN2_A）位于CRH，偏移8，CNF=00, MODE=11，50MHz通用推挽输出
		stm32PinMode(channel->pin_in2.port, channel->pin_in2.mask, 0b00, 0b11);
#elif defined(OPENTENS_USE_ESP32_IDF)
		gpio_config_t io_conf = {};
		io_conf.pin_bit_mask = (1ULL << channel->pin_in1) | (1ULL << channel->pin_in2);
		io_conf.mode         = GPIO_MODE_OUTPUT;
		io_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
		io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
		io_conf.intr_type    = GPIO_INTR_DISABLE;
		ESP_ERROR_CHECK(gpio_config(&io_conf));
		gpio_set_level(channel->pin_in1, 0);
		gpio_set_level(channel->pin_in2, 0);
#else
		pinMode(channel->pin_in1, OUTPUT);
		digitalWrite(channel->pin_in1, LOW);
		pinMode(channel->pin_in2, OUTPUT);
		digitalWrite(channel->pin_in2, LOW);
#endif
		// boost PWM
#if defined(OPENTENS_USE_ESP32_ARDUINO)
		ledcAttachChannel(channel->pin_boost, BOOST_PWM_FREQ, BOOST_PWM_RESOLUTION, channel->ledc_boost);
		ledcWriteChannel(channel->ledc_boost, 0);
#elif defined(OPENTENS_USE_ESP32_IDF)
		ledc_channel_config_t ledc_channel = {};
		ledc_channel.gpio_num   = channel->pin_boost;
		ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
		ledc_channel.channel    = channel->ledc_boost;
		ledc_channel.timer_sel  = LEDC_TIMER_0;
		ledc_channel.duty       = 0;
		ledc_channel.hpoint     = 0;
		ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
#elif defined(OPENTENS_USE_STM32_ARDUINO)
		pinMode(BOOST_A_PIN, OUTPUT);
		analogWriteFrequency(BOOST_PWM_FREQ);
		analogWriteResolution(BOOST_PWM_RESOLUTION);
		analogWrite(BOOST_A_PIN, 0);
#elif defined(OPENTENS_USE_STM32_CUBE_LL)
		// stm32_cube_ll.cpp MX_GPIO_Init() 已经处理
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
		// PWM PB0 引脚的默认复用功能就是 TIM3_CH3
		timer_set_oc_value(TIM3, TIM_OC3, 0); // 占空比 0
		timer_set_oc_mode(TIM3, TIM_OC3, TIM_OCM_PWM1);
		timer_enable_oc_preload(TIM3, TIM_OC3);
		// 50MHz复用推挽输出
		gpio_set_mode(BOOST_A_PIN.port, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, BOOST_A_PIN.mask);
		timer_enable_oc_output(TIM3, TIM_OC3);
		timer_enable_break_main_output(TIM3);
		timer_enable_counter(TIM3);
#elif defined(OPENTENS_USE_STM32_CMSIS)
		// PWM PB0 引脚的默认复用功能就是 TIM3_CH3
		WRITE_REG(TIM3->CCR3, 0); // 占空比 0
		MODIFY_REG(TIM3->CCMR2,
			TIM_CCMR2_OC3M | TIM_CCMR2_CC3S,
			TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3PE);
		// PB0（boost PWM）位于CRL，偏移0，CNF=10, MODE=11，50MHz复用推挽输出
		stm32PinMode(BOOST_A_PIN.port, BOOST_A_PIN.mask, 0b10, 0b11);
		// 开启通道输出并使能高级定时器主输出
		SET_BIT(TIM3->CCER, TIM_CCER_CC3E);
		SET_BIT(TIM3->BDTR, TIM_BDTR_MOE);
		// 启动定时器
		SET_BIT(TIM3->CR1, TIM_CR1_CEN);
#endif
	}
}


// 硬件定时器
#if defined(OPENTENS_USE_ESP32_IDF)
static bool IRAM_ATTR gptimer_isr_trampoline(gptimer_handle_t timer,
	const gptimer_alarm_event_data_t *edata, void *user_ctx) {
	static_cast<void>(timer);
	static_cast<void>(edata);
	void (*cb)(void) = reinterpret_cast<void (*)(void)>(user_ctx);
	if (cb != nullptr) {
		cb();
	}
	return false;
}
#elif defined(OPENTENS_USE_STM32_ARDUINO)
static HardwareTimer tim2_instance(TIM2);
#endif

void setupTimer(Channel_t *channel, void (*userFunc)(void)) {
#if defined(OPENTENS_USE_ESP32_ARDUINO)
	channel->timer = timerBegin(1000000);
	if (channel->timer != nullptr) {
		timerAttachInterrupt(channel->timer, userFunc);
	}
#elif defined(OPENTENS_USE_ESP32_IDF)
	gptimer_config_t timer_config = {};
	timer_config.clk_src       = GPTIMER_CLK_SRC_DEFAULT;
	timer_config.direction     = GPTIMER_COUNT_UP;
	timer_config.resolution_hz = 1000000; // 1MHz, 1 tick = 1us
	ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &channel->timer));

	gptimer_event_callbacks_t cbs = {};
	cbs.on_alarm = gptimer_isr_trampoline;
	ESP_ERROR_CHECK(gptimer_register_event_callbacks(channel->timer, &cbs, reinterpret_cast<void *>(userFunc)));
	ESP_ERROR_CHECK(gptimer_enable(channel->timer));
#elif defined(OPENTENS_USE_STM32_ARDUINO)
	channel->timer = &tim2_instance;
	channel->timer->attachInterrupt(userFunc);
	channel->timer->setInterruptPriority(3, 0);
#elif defined(OPENTENS_USE_STM32_CUBE_LL)
	// MX_TIM2_Init() 处理
	static_cast<void>(channel);
	static_cast<void>(userFunc);
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
	rcc_periph_clock_enable(RCC_TIM2);
	rcc_periph_reset_pulse(RST_TIM2);
	__asm__ volatile("dsb");
	//timer_set_mode(TIM2, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
	timer_set_prescaler(TIM2, 71); // 1MHz
	timer_set_period(TIM2, 65535);
	timer_enable_preload(TIM2);
	nvic_set_priority(NVIC_TIM2_IRQ, 3 << 4);
	nvic_enable_irq(NVIC_TIM2_IRQ);
	static_cast<void>(channel);
	static_cast<void>(userFunc);
#elif defined(OPENTENS_USE_STM32_CMSIS)
	SET_BIT(RCC->APB1ENR, RCC_APB1ENR_TIM2EN);
	SET_BIT(RCC->APB1RSTR, RCC_APB1RSTR_TIM2RST);
	CLEAR_BIT(RCC->APB1RSTR, RCC_APB1RSTR_TIM2RST);
	while (READ_BIT(RCC->APB1ENR, RCC_APB1ENR_TIM2EN) == 0) {}
	//CLEAR_BIT(TIM2->CR1, (TIM_CR1_CKD | TIM_CR1_CMS | TIM_CR1_DIR));
	WRITE_REG(TIM2->PSC, 71); // 1MHz
	WRITE_REG(TIM2->ARR, 65535);
	SET_BIT(TIM2->CR1, TIM_CR1_ARPE);
	NVIC_SetPriority(TIM2_IRQn, 3);
	NVIC_EnableIRQ(TIM2_IRQn);
	static_cast<void>(channel);
	static_cast<void>(userFunc);
#endif
}

static inline void IRAM_ATTR setTimerPeriod(Channel_t *channel, uint64_t period_us) {
#if defined(OPENTENS_USE_ESP32_ARDUINO)
	timerWrite(channel->timer, 0);
	timerAlarm(channel->timer, period_us, true, 0);
#elif defined(OPENTENS_USE_ESP32_IDF)
	gptimer_alarm_config_t alarm_cfg = {};
	alarm_cfg.alarm_count = period_us;
	alarm_cfg.reload_count = 0;
	alarm_cfg.flags.auto_reload_on_alarm = true;
	gptimer_set_raw_count(channel->timer, 0);
	gptimer_set_alarm_action(channel->timer, &alarm_cfg);
#elif defined(OPENTENS_USE_STM32_ARDUINO)
	channel->timer->setOverflow(static_cast<uint32_t>(period_us), MICROSEC_FORMAT);
	channel->timer->refresh(); // 立即加载 PSC/ARR，并重新开始计时
	__DSB();
	LL_TIM_ClearFlag_UPDATE(TIM2); // 清除 refresh() 产生的 UIF
#elif defined(OPENTENS_USE_STM32_CUBE_LL) || defined(OPENTENS_USE_STM32_LIBOPENCM3) || defined(OPENTENS_USE_STM32_CMSIS)
	if (period_us > 65535ULL) {
		// 大休眠：PSC = 35999 (每步 500us = 0.5ms)，最大支持 32.7 秒
#if defined(OPENTENS_USE_STM32_CUBE_LL)
		LL_TIM_SetPrescaler(TIM2, 36000 - 1);
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
		timer_set_prescaler(TIM2, 36000 - 1);
#elif defined(OPENTENS_USE_STM32_CMSIS)
		WRITE_REG(TIM2->PSC, 36000 - 1);
#endif
		const uint32_t ticks = static_cast<uint32_t>(period_us / 500ULL);
		const uint32_t reload = (ticks > 0) ? (ticks - 1UL) : 0;
#if defined(OPENTENS_USE_STM32_CUBE_LL)
		LL_TIM_SetAutoReload(TIM2, reload);
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
		timer_set_period(TIM2, reload);
#elif defined(OPENTENS_USE_STM32_CMSIS)
		WRITE_REG(TIM2->ARR, reload);
#endif
	} else {
		// 常规脉冲微秒档：PSC = 71 (每步 1us)，最大支持 65.535ms
		const uint32_t reload = (period_us > 0) ? (static_cast<uint32_t>(period_us) - 1UL) : 0;
#if defined(OPENTENS_USE_STM32_CUBE_LL)
		LL_TIM_SetPrescaler(TIM2, 72 - 1);
		LL_TIM_SetAutoReload(TIM2, reload);
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
		timer_set_prescaler(TIM2, 72 - 1);
		timer_set_period(TIM2, reload);
#elif defined(OPENTENS_USE_STM32_CMSIS)
		WRITE_REG(TIM2->PSC, 72 - 1);
		WRITE_REG(TIM2->ARR, reload);
#endif
	}
	// 触发软件更新事件 (UG)，让新的 PSC 和 ARR 立即加载进硬件影子寄存器并清零计数器 CNT
	// 注意：UG 事件会置位 UIF 中断标志，必须在紧接着的一行立刻 ClearFlag，防止误触发中断！
#if defined(OPENTENS_USE_STM32_CUBE_LL)
	LL_TIM_GenerateEvent_UPDATE(TIM2);
	__DSB();
	LL_TIM_ClearFlag_UPDATE(TIM2);
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
	timer_generate_event(TIM2, TIM_EGR_UG);
	__asm__ volatile("dsb");
	timer_clear_flag(TIM2, TIM_SR_UIF);
#elif defined(OPENTENS_USE_STM32_CMSIS)
	WRITE_REG(TIM2->EGR, TIM_EGR_UG);
	__DSB();
	WRITE_REG(TIM2->SR, ~TIM_SR_UIF);
#endif
	static_cast<void>(channel);
#endif
}

static void IRAM_ATTR handleTimer(Channel_t *channel) {
	ENTER_CRITICAL_LOCK(channel->timer_mux);

	if (!channel->running) {
		updatePinBridge(channel, false, false);
		updatePinBoost(channel, false);
		EXIT_CRITICAL_LOCK(channel->timer_mux);
		return;
	}

	switch (channel->pulse_phase) {
		case PHASE_SLEEP_END:
			updatePinBoost(channel, true);
			[[fallthrough]];

		case PHASE_POS_PULSE:
			if (channel->pos_pulse_width_us > 0) {
				updatePinBridge(channel, true, false);
				channel->pulse_phase = PHASE_INTER_GAP;
				setTimerPeriod(channel, channel->pos_pulse_width_us);
				break;
			}
			[[fallthrough]];

		case PHASE_INTER_GAP:
			updatePinBridge(channel, false, false);
			if (channel->neg_pulse_width_us > 0) {
				channel->pulse_phase = PHASE_NEG_PULSE;
			} else {
				channel->pulse_phase = PHASE_REST_CYCLE;
			}
			setTimerPeriod(channel, inter_gap_width_us);
			break;

		case PHASE_NEG_PULSE:
			if (channel->neg_pulse_width_us > 0) {
				updatePinBridge(channel, false, true);
				channel->pulse_phase = PHASE_REST_CYCLE;
				setTimerPeriod(channel, channel->neg_pulse_width_us);
				break;
			}
			[[fallthrough]];

		case PHASE_REST_CYCLE:
			updatePinBridge(channel, false, false);

			{
				const bool is_round_finished = updatePulseCounter(channel);
				if (is_round_finished && (channel->waveform.pulse_section_pause_time > 0)) {
					channel->pulse_phase = PHASE_SLEEP_START;
				} else {
					channel->pulse_phase = PHASE_POS_PULSE;
				}

				const uint64_t total_cycle_us = static_cast<uint64_t>(channel->pulse_period_ms) * 1000ULL;
				const uint64_t active_h_bridge_us = static_cast<uint64_t>(channel->pos_pulse_width_us)
					+ static_cast<uint64_t>(inter_gap_width_us) + static_cast<uint64_t>(channel->neg_pulse_width_us);

				uint64_t remaining_us = static_cast<uint64_t>(min_rest_cycle_us);
				if (total_cycle_us > active_h_bridge_us + min_rest_cycle_us) {
					remaining_us = total_cycle_us - active_h_bridge_us;
				}
				updatePulsePeriod(channel);
				setTimerPeriod(channel, remaining_us);
			}
			break;

		case PHASE_SLEEP_START:
			channel->pulse_phase = PHASE_SLEEP_END;
			updatePinBoost(channel, false);
			{
				const uint64_t sleep_us = static_cast<uint64_t>(
					channel->waveform.pulse_section_pause_time) * 100000ULL;
				setTimerPeriod(channel, sleep_us);
			}
			break;

		default:
			break;
	}

	EXIT_CRITICAL_LOCK(channel->timer_mux);
}


// 实例化通道A
Channel_t channelA = {
#if defined(OPENTENS_USE_ESP32_ARDUINO) || defined(OPENTENS_USE_ESP32_IDF) || defined(OPENTENS_USE_STM32_ARDUINO)
	.timer = nullptr,
#endif
#if defined(OPENTENS_USE_ESP32_ARDUINO) || defined(OPENTENS_USE_ESP32_IDF)
	.timer_mux = portMUX_INITIALIZER_UNLOCKED,
#endif
	.waveform = {},
	.section_elapsed_ms = 0,
	.pulse_period_ms = 0,
	.pos_pulse_width_us = 0,
	.neg_pulse_width_us = 0,
	.global_intensity = 10,
	.section_index = 0,
	.pulse_phase = PHASE_POS_PULSE,
	.running = false,
#if defined(OPENTENS_USE_ESP32_ARDUINO) || defined(OPENTENS_USE_ESP32_IDF)
	.pin_boost = BOOST_A_PIN,
	.ledc_boost = LEDC_CHANNEL_0,
#endif
	.pin_in1 = IN1_A_PIN,
	.pin_in2 = IN2_A_PIN,
#if defined(OPENTENS_USE_ESP32_ARDUINO) || defined(OPENTENS_USE_ESP32_IDF)
	.raw_waveform = "",
	.mirror = nullptr,
#endif
};

// 通道A 定时器中断入口
#if defined(OPENTENS_USE_ESP32_ARDUINO) || defined(OPENTENS_USE_ESP32_IDF) || defined(OPENTENS_USE_STM32_ARDUINO)
static void IRAM_ATTR onTimerChA() {
	handleTimer(&channelA);
}
#elif defined(OPENTENS_USE_STM32_CUBE_LL)
extern "C" void TIM2_IRQHandler(void) {
	if (LL_TIM_IsActiveFlag_UPDATE(TIM2) != 0) {
		LL_TIM_ClearFlag_UPDATE(TIM2);
		handleTimer(&channelA);
	}
}
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
extern "C" void tim2_isr(void) {
	if (timer_get_flag(TIM2, TIM_SR_UIF)) {
		timer_clear_flag(TIM2, TIM_SR_UIF);
		handleTimer(&channelA);
	}
}
#elif defined(OPENTENS_USE_STM32_CMSIS)
extern "C" void TIM2_IRQHandler(void) {
	if (READ_BIT(TIM2->SR, TIM_SR_UIF) != 0) {
		WRITE_REG(TIM2->SR, ~TIM_SR_UIF);
		handleTimer(&channelA);
	}
}
#endif


#if defined(OPENTENS_USE_ESP32_ARDUINO) || defined(OPENTENS_USE_ESP32_IDF)
// 实例化通道B
Channel_t channelB = {
	.timer = nullptr,
	.timer_mux = portMUX_INITIALIZER_UNLOCKED,
	.waveform = {},
	.section_elapsed_ms = 0,
	.pulse_period_ms = 0,
	.pos_pulse_width_us = 0,
	.neg_pulse_width_us = 0,
	.global_intensity = 10,
	.section_index = 0,
	.pulse_phase = PHASE_POS_PULSE,
	.running = false,
	.pin_boost = BOOST_B_PIN,
	.ledc_boost = LEDC_CHANNEL_1,
	.pin_in1 = IN1_B_PIN,
	.pin_in2 = IN2_B_PIN,
	.raw_waveform = "",
	.mirror = nullptr,
};

// 通道B 定时器中断入口
static void IRAM_ATTR onTimerChB() {
	handleTimer(&channelB);
}

void handleMirror(bool enable) {
	if (channelA.timer == nullptr) {
		return;
	}
	handleStop(&channelA);
	handleStop(&channelB);
	if (enable) {
		if (channelB.timer != nullptr) {
#if defined(OPENTENS_USE_ESP32_ARDUINO)
			timerEnd(channelB.timer);
#elif defined(OPENTENS_USE_ESP32_IDF)
			ESP_ERROR_CHECK(gptimer_disable(channelB.timer));
			ESP_ERROR_CHECK(gptimer_del_timer(channelB.timer));
#endif
			channelB.timer = nullptr;
		}
		channelA.mirror = &channelB;
	} else {
		channelA.mirror = nullptr;
		if (channelB.timer == nullptr) {
			setupTimer(&channelB, &onTimerChB);
		}
	}
}

Channel_t *getChannelByKey(const char *key_id) {
	if (key_id == nullptr) {
		return nullptr;
	}
	if (strcmp(key_id, "A") == 0 || strcmp(key_id, "a") == 0) {
		return &channelA;
	}
	if (strcmp(key_id, "B") == 0 || strcmp(key_id, "b") == 0) {
		return &channelB;
	}
	return nullptr;
}
#endif


static inline bool isChannelTimerReady(const Channel_t *channel) {
	if (channel == nullptr) {
		return false;
	}
#if defined(OPENTENS_USE_ESP32_ARDUINO) || defined(OPENTENS_USE_ESP32_IDF) || defined(OPENTENS_USE_STM32_ARDUINO)
	return (channel->timer != nullptr);
#elif defined(OPENTENS_USE_STM32_CUBE_LL) || defined(OPENTENS_USE_STM32_CMSIS)
	return NVIC_GetEnableIRQ(TIM2_IRQn) != 0;
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
	return nvic_get_irq_enabled(NVIC_TIM2_IRQ) != 0;
#endif
	return true;
}

// 通道独立启停控制
bool handleStart(Channel_t *channel, const char *input_waveform) {
	if (!isChannelTimerReady(channel)) {
		return false;
	}
	// 通道未停止，不允许启动新波形，避免错误输入导致struct解析中断而损坏
	if (channel->running) {
		return false;
	}
#if defined(OPENTENS_USE_ESP32_ARDUINO) || defined(OPENTENS_USE_ESP32_IDF)
	if (!parseWaveform(&channel->waveform, input_waveform)) {
		return false;
	}
	channel->raw_waveform = input_waveform;
#else
	static_cast<void>(input_waveform);
#endif
	if (channel->waveform.section_count == 0) {
		return false;
	}

	ENTER_CRITICAL_NORMAL(channel->timer_mux);
	channel->section_index = 0;
	channel->section_elapsed_ms = 0;
	channel->pulse_phase = PHASE_POS_PULSE;
	updatePulsePeriod(channel);
	// boost PWM
	channel->running = true;
	updatePinBoost(channel, true);
	// 硬件定时器
	setTimerPeriod(channel, inter_gap_width_us);
#if defined(OPENTENS_USE_ESP32_ARDUINO)
	timerStart(channel->timer);
#elif defined(OPENTENS_USE_ESP32_IDF)
	gptimer_start(channel->timer);
#elif defined(OPENTENS_USE_STM32_ARDUINO)
	channel->timer->setCount(0);
	channel->timer->resume();
#elif defined(OPENTENS_USE_STM32_CUBE_LL)
	LL_TIM_SetCounter(TIM2, 0);
	LL_TIM_ClearFlag_UPDATE(TIM2);
	LL_TIM_EnableIT_UPDATE(TIM2);
	LL_TIM_EnableCounter(TIM2);
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
	timer_set_counter(TIM2, 0);
	timer_clear_flag(TIM2, TIM_SR_UIF);
	timer_enable_irq(TIM2, TIM_DIER_UIE);
	timer_enable_counter(TIM2);
#elif defined(OPENTENS_USE_STM32_CMSIS)
	CLEAR_REG(TIM2->CNT);
	WRITE_REG(TIM2->SR, ~TIM_SR_UIF);
	SET_BIT(TIM2->DIER, TIM_DIER_UIE);
	SET_BIT(TIM2->CR1, TIM_CR1_CEN);
#endif
	EXIT_CRITICAL_NORMAL(channel->timer_mux);

	return true;
}

static inline void delayUs(uint32_t us) {
#if defined(OPENTENS_USE_ESP32_IDF)
	esp_rom_delay_us(us);
#elif defined(OPENTENS_USE_STM32_CUBE_LL) || defined(OPENTENS_USE_STM32_LIBOPENCM3) || defined(OPENTENS_USE_STM32_CMSIS)
	if (us == 0) {
		return;
	}
	// 72MHz 下，使用 subs(1周期) + bne(2周期) 汇编循环
	// 每次循环固定消耗 3 个时钟周期。1微秒需要 72/3 = 24 次循环。
	uint32_t loops = us * 24UL;
	__asm__ volatile(
		"1: \n"
		"subs %[loops], #1 \n"
		"bne 1b \n"
		: [loops] "+r"(loops)
		:
		: "cc"
	);
#else
	delayMicroseconds(us);
#endif
}

void handleStop(Channel_t *channel) {
	if (!isChannelTimerReady(channel)) {
		return;
	}
	if (!channel->running) {
		return;
	}

	ENTER_CRITICAL_NORMAL(channel->timer_mux);
	channel->running = false;
	// 关 STM32 硬件定时器
#if defined(OPENTENS_USE_STM32_ARDUINO)
	channel->timer->pause();
#elif defined(OPENTENS_USE_STM32_CUBE_LL)
	LL_TIM_DisableCounter(TIM2);
	LL_TIM_DisableIT_UPDATE(TIM2);
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
	timer_disable_counter(TIM2);
	timer_disable_irq(TIM2, TIM_DIER_UIE);
#elif defined(OPENTENS_USE_STM32_CMSIS)
	CLEAR_BIT(TIM2->CR1, TIM_CR1_CEN);
	CLEAR_BIT(TIM2->DIER, TIM_DIER_UIE);
#endif
	EXIT_CRITICAL_NORMAL(channel->timer_mux);

	// boost PWM 停止充电
	updatePinBoost(channel, false);

	// 关 ESP32 硬件定时器
#if defined(OPENTENS_USE_ESP32_ARDUINO)
	timerStop(channel->timer);
#elif defined(OPENTENS_USE_ESP32_IDF)
	gptimer_stop(channel->timer);
#endif

	// H桥释放残余
	delayUs(1000000UL / BOOST_PWM_FREQ);
	for (int i = 0; i < 100; i++) {
		if (i % 2 == 0) {
			updatePinBridge(channel, true, false);
		} else {
			updatePinBridge(channel, false, true);
		}
		delayUs(2);
	}
	updatePinBridge(channel, false, false);
}

void handleIntensity(Channel_t *channel, bool up) {
	if (!isChannelTimerReady(channel)) {
		return;
	}
	ENTER_CRITICAL_NORMAL(channel->timer_mux);
	if (up && (channel->global_intensity < 100)) {
		channel->global_intensity = channel->global_intensity + 1;
	}
	if (!up && (channel->global_intensity > 0)) {
		channel->global_intensity = channel->global_intensity - 1;
	}
	const bool sleeping = (channel->pulse_phase == PHASE_SLEEP_START) || (channel->pulse_phase == PHASE_SLEEP_END);
	updatePinBoost(channel, channel->running && !sleeping);
	EXIT_CRITICAL_NORMAL(channel->timer_mux);
}


#if defined(OPENTENS_USE_ESP32_ARDUINO) || defined(OPENTENS_USE_ESP32_IDF)
void setupESP32(void);
#elif defined(OPENTENS_USE_STM32_ARDUINO)
void setupSTM32(void);
void loopSTM32(void);
#endif


// 系统初始化与主循环
#if defined(OPENTENS_USE_ESP32_ARDUINO)
void setup() {
	setupGPIO(nullptr);
	setupGPIO(&channelA);
	setupGPIO(&channelB);
	setupTimer(&channelA, &onTimerChA);
	setupTimer(&channelB, &onTimerChB);
	setupESP32();
}
#elif defined(OPENTENS_USE_ESP32_IDF)
extern "C" void app_main(void) {
	setupGPIO(nullptr);
	setupGPIO(&channelA);
	setupGPIO(&channelB);
	setupTimer(&channelA, &onTimerChA);
	setupTimer(&channelB, &onTimerChB);
	setupESP32();
}
#elif defined(OPENTENS_USE_STM32_ARDUINO)
void setup() {
	setupGPIO(nullptr);
	setupGPIO(&channelA);
	setupTimer(&channelA, &onTimerChA);
	setupSTM32();
}
#endif

#if defined(OPENTENS_USE_ESP32_ARDUINO)
void loop() {
	// 主循环保持非阻塞
}
#elif defined(OPENTENS_USE_STM32_ARDUINO)
void loop() {
	loopSTM32();
}
#endif
