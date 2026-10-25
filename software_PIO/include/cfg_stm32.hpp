#pragma once

// 通道A

// boost PWM, PB0 (TIM3_CH3)
#if defined(OPENTENS_USE_STM32_ARDUINO)
constexpr uint8_t     BOOST_A_PIN = PB0;
#else
constexpr stm32_pin_t BOOST_A_PIN = {
	.port = GPIOB,
#if defined(OPENTENS_USE_STM32_CUBE_LL)
	.mask = LL_GPIO_PIN_0,
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
	.mask = GPIO0,
#elif defined(OPENTENS_USE_STM32_CMSIS)
	.mask = 1UL << 0,
#endif
};
#endif

// H桥正向 PB1
constexpr stm32_pin_t IN1_A_PIN = {
	.port = GPIOB,
#if defined(OPENTENS_USE_STM32_CUBE_LL)
	.mask = LL_GPIO_PIN_1,
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
	.mask = GPIO1,
#elif defined(OPENTENS_USE_STM32_CMSIS)
	.mask = 1UL << 1,
#endif
#if defined(OPENTENS_USE_STM32_ARDUINO)
	.pin = PB1,
#endif
};

// H桥反向 PB10
constexpr stm32_pin_t IN2_A_PIN = {
	.port = GPIOB,
#if defined(OPENTENS_USE_STM32_CUBE_LL)
	.mask = LL_GPIO_PIN_10,
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
	.mask = GPIO10,
#elif defined(OPENTENS_USE_STM32_CMSIS)
	.mask = 1UL << 10,
#endif
#if defined(OPENTENS_USE_STM32_ARDUINO)
	.pin = PB10,
#endif
};


// LED, PC13
#if defined(OPENTENS_USE_STM32_ARDUINO)
constexpr uint8_t     LED_PIN = PC13;
#else
constexpr stm32_pin_t LED_PIN = {
	.port = GPIOC,
#if defined(OPENTENS_USE_STM32_CUBE_LL)
	.mask = LL_GPIO_PIN_13,
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
	.mask = GPIO13,
#elif defined(OPENTENS_USE_STM32_CMSIS)
	.mask = 1UL << 13,
#endif
};
#endif


// Keypad
#if defined(OPENTENS_USE_STM32_ARDUINO)
constexpr uint8_t     MATRIX_KEYPAD_ROWS[] = {
	PB5, PB4, PB3, PA15,
};
#else
constexpr stm32_pin_t MATRIX_KEYPAD_ROWS[] = {
#if defined(OPENTENS_USE_STM32_CUBE_LL)
	{GPIOB, LL_GPIO_PIN_5}, {GPIOB, LL_GPIO_PIN_4}, {GPIOB, LL_GPIO_PIN_3}, {GPIOA, LL_GPIO_PIN_15},
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
	{GPIOB, GPIO5}, {GPIOB, GPIO4}, {GPIOB, GPIO3}, {GPIOA, GPIO15},
#elif defined(OPENTENS_USE_STM32_CMSIS)
	{GPIOB, 1UL << 5}, {GPIOB, 1UL << 4}, {GPIOB, 1UL << 3}, {GPIOA, 1UL << 15},
#endif
};
#endif
constexpr uint8_t MATRIX_KEYPAD_ROW_COUNT = sizeof(MATRIX_KEYPAD_ROWS) / sizeof(MATRIX_KEYPAD_ROWS[0]);

#if defined(OPENTENS_USE_STM32_ARDUINO)
constexpr uint8_t     MATRIX_KEYPAD_COLUMNS[] = {
	PB6, PB7, PB8, PB9,
};
#else
constexpr stm32_pin_t MATRIX_KEYPAD_COLUMNS[] = {
#if defined(OPENTENS_USE_STM32_CUBE_LL)
	{GPIOB, LL_GPIO_PIN_6}, {GPIOB, LL_GPIO_PIN_7}, {GPIOB, LL_GPIO_PIN_8}, {GPIOB, LL_GPIO_PIN_9},
#elif defined(OPENTENS_USE_STM32_LIBOPENCM3)
	{GPIOB, GPIO6}, {GPIOB, GPIO7}, {GPIOB, GPIO8}, {GPIOB, GPIO9},
#elif defined(OPENTENS_USE_STM32_CMSIS)
	{GPIOB, 1UL << 6}, {GPIOB, 1UL << 7}, {GPIOB, 1UL << 8}, {GPIOB, 1UL << 9},
#endif
};
#endif
constexpr uint8_t MATRIX_KEYPAD_COLUMN_COUNT = sizeof(MATRIX_KEYPAD_COLUMNS) / sizeof(MATRIX_KEYPAD_COLUMNS[0]);
