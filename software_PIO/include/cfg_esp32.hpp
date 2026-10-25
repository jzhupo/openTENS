#pragma once

// 通道A

// boost PWM
#if defined(OPENTENS_USE_ESP32_IDF)
constexpr gpio_num_t BOOST_A_PIN = GPIO_NUM_32;
#else
constexpr uint8_t    BOOST_A_PIN = 32;
#endif

// H桥正向
#if defined(OPENTENS_USE_ESP32_IDF)
constexpr gpio_num_t IN1_A_PIN = GPIO_NUM_33; 
#else
constexpr uint8_t    IN1_A_PIN = 33;
#endif

// H桥反向
#if defined(OPENTENS_USE_ESP32_IDF)
constexpr gpio_num_t IN2_A_PIN = GPIO_NUM_25;
#else
constexpr uint8_t    IN2_A_PIN = 25;
#endif


// 通道B

// boost PWM
#if defined(OPENTENS_USE_ESP32_IDF)
constexpr gpio_num_t BOOST_B_PIN = GPIO_NUM_26;
#else
constexpr uint8_t    BOOST_B_PIN = 26;
#endif

// H桥正向
#if defined(OPENTENS_USE_ESP32_IDF)
constexpr gpio_num_t IN1_B_PIN = GPIO_NUM_27;
#else
constexpr uint8_t    IN1_B_PIN = 27;
#endif

// H桥反向
#if defined(OPENTENS_USE_ESP32_IDF)
constexpr gpio_num_t IN2_B_PIN = GPIO_NUM_14;
#else
constexpr uint8_t    IN2_B_PIN = 14;
#endif


// 板载LED
#if defined(OPENTENS_USE_ESP32_IDF)
constexpr gpio_num_t LED_PIN = GPIO_NUM_22;
#else
constexpr uint8_t    LED_PIN = 22;
#endif
