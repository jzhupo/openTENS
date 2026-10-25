#pragma once

// 通道A

// boost PWM
#if defined(OPENTENS_USE_ESP32_IDF)
constexpr gpio_num_t BOOST_A_PIN = GPIO_NUM_7;
#else
constexpr uint8_t    BOOST_A_PIN = 7;
#endif

// H桥正向
#if defined(OPENTENS_USE_ESP32_IDF)
constexpr gpio_num_t IN1_A_PIN = GPIO_NUM_6; 
#else
constexpr uint8_t    IN1_A_PIN = 6;
#endif

// H桥反向
#if defined(OPENTENS_USE_ESP32_IDF)
constexpr gpio_num_t IN2_A_PIN = GPIO_NUM_5;
#else
constexpr uint8_t    IN2_A_PIN = 5;
#endif


// 通道B

// boost PWM
#if defined(OPENTENS_USE_ESP32_IDF)
constexpr gpio_num_t BOOST_B_PIN = GPIO_NUM_3;
#else
constexpr uint8_t BOOST_B_PIN = 3;
#endif

// H桥正向
#if defined(OPENTENS_USE_ESP32_IDF)
constexpr gpio_num_t IN1_B_PIN = GPIO_NUM_1;
#else
constexpr uint8_t IN1_B_PIN = 1;
#endif

// H桥反向
#if defined(OPENTENS_USE_ESP32_IDF)
constexpr gpio_num_t IN2_B_PIN = GPIO_NUM_0;
#else
constexpr uint8_t IN2_B_PIN = 0;
#endif


// 板载LED
#if defined(OPENTENS_USE_ESP32_IDF)
constexpr gpio_num_t LED_PIN = GPIO_NUM_8;
#else
constexpr uint8_t    LED_PIN = 8;
#endif
