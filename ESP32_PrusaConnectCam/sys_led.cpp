/**
   @file sys_led.cpp

   @brief Library for system LED control

   @author Miroslav Pivovarsky
   Contact: miroslav.pivovarsky@gmail.com

   @bug: no know bug
*/

#include "sys_led.h"

sys_led system_led(STATUS_LED_GPIO_NUM, STATUS_LED_ON_DURATION);

/**
 * @brief Construct a new sys led::sys led object
 * 
 * @param uint8_t - pin number for system LED
 * @param uint32_t - duration of LED on
 */
sys_led::sys_led(uint8_t i_pin, uint32_t i_on_duration) {
  pin = i_pin;
  time = 100;
  ledOnDuration = i_on_duration;
  mode = STATUS_LED_MODE_ON;
}

/**
 * @brief Construct a new sys led::sys led object
 * 
 * @param uint8_t - pin number for system LED
 * @param uint32_t - duration of LED on
 * @param Logs * - pointer to log class
 */
sys_led::sys_led(uint8_t i_pin, uint32_t i_on_duration, Logs *i_log) {
  pin = i_pin;
  time = 100;
  log = i_log;
  ledOnDuration = i_on_duration;
  mode = STATUS_LED_MODE_ON;
}

/**
 * @brief Init system LED
 * 
 */
void sys_led::init() {
  pinMode(pin, OUTPUT);
  /* STATUS_LED_OFF_PIN_LEVEL lights the XIAO LED (active low), the inverted level keeps it dark */
  digitalWrite(pin, (STATUS_LED_MODE_OFF == mode) ? !STATUS_LED_OFF_PIN_LEVEL : STATUS_LED_OFF_PIN_LEVEL);
}

/**
 * @brief Toggle system LED
 * 
 */
void sys_led::toggle() {
  if (false == blinkAllowed()) {
    digitalWrite(pin, !STATUS_LED_OFF_PIN_LEVEL);
    return;
  }
  digitalWrite(pin, !digitalRead(pin));
}

/**
 * @brief Set system LED
 * 
 * @param bool - state of LED
 */
void sys_led::set(bool state) {
  digitalWrite(pin, state);
}

/**
 * @brief Get system LED
 * 
 * @return bool - state of LED
 */
bool sys_led::get() {
  return digitalRead(pin);
}

/**
 * @brief Set timer for system LED
 * 
 * @param uint32_t - time in ms
 */
void sys_led::setTimer(uint32_t i_time) {
  time = i_time;
}

/**
 * @brief Get timer for next start task for system LED
 * 
 * @return uint32_t - time in ms
 */
uint32_t sys_led::getTimer() {
  uint32_t tmp = 0;

  if (digitalRead(pin) == STATUS_LED_OFF_PIN_LEVEL) {
    tmp = ledOnDuration;
  } else {
    tmp = time;
  }

  return tmp;
}

/**
 * @brief Set the status LED mode
 * 
 * @param uint8_t - STATUS_LED_MODE_xxx, an unknown value means STATUS_LED_MODE_ON
 */
void sys_led::setMode(uint8_t i_mode) {
  mode = (i_mode > STATUS_LED_MODE_OFF) ? STATUS_LED_MODE_ON : i_mode;
}

/**
 * @brief Get the status LED mode
 * 
 * @return uint8_t - STATUS_LED_MODE_xxx
 */
uint8_t sys_led::getMode() {
  return mode;
}

/**
 * @brief Check if the LED may blink in the current mode and state
 * 
 * @return bool - true when the LED may blink
 */
bool sys_led::blinkAllowed() {
  if (STATUS_LED_MODE_OFF == mode) {
    return false;
  }
  if (STATUS_LED_MODE_ERRORS == mode) {
    /* connected to the Wi-Fi network: nothing to report */
    return (STATUS_LED_STA_CONNECTED != time);
  }
  return true;
}

/* EOF */