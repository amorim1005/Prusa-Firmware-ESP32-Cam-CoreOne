/**
   @file variable.h

   @brief Library with global variables

   @author Miroslav Pivovarsky
   Contact: miroslav.pivovarsky@gmail.com

   @bug: no know bug
*/

#pragma once

#include <Arduino.h>
#include "mcu_cfg.h"

struct WebBasicAuth_struct {
  bool EnableAuth;                        ///< user definition for enable/disable basic auth
  String UserName;                        ///< login name for basic auth
  String Password;                        ///< password for basic auth
};

/* automatic restart reasons, saved in the RTC memory before the restart */
enum SystemRestartReason_enum {
  SystemRestart_None = 0,                 ///< no automatic restart
  SystemRestart_LowMemory = 1,            ///< free memory was too low
  SystemRestart_WiFiLost = 2,             ///< WiFi connection was lost for a long time
  SystemRestart_CameraInit = 3,           ///< camera init failed
};

struct SystemHealth_struct {
  volatile bool LowMemory;                ///< free memory is below the limit
  uint16_t LowMemoryTime;                 ///< how long the free memory is below the limit [s]
  uint16_t UnplannedRestarts;             ///< restarts after a crash, watchdog, power drop or automatic restart. Saved in EEPROM
  String LastRestartReason;               ///< reason of the last restart
};

struct McuTemperature_struct {
  float TemperatureCelsius;               ///< MCU temperature
  volatile bool ThermalThrottling;        ///< MCU is too hot: stream is stopped and uploads are slowed down
  float TemperatureMax;                   ///< highest MCU temperature since the start
};

extern struct WebBasicAuth_struct WebBasicAuth;      ///< structure with configuration for basic auth
extern struct SystemHealth_struct SystemHealth;      ///< memory and restart statistics
extern struct McuTemperature_struct McuTemperature;  ///< MCU temperature

extern TaskHandle_t Task_CapturePhotoAndSend;        ///< task handle for capture photo and send
extern TaskHandle_t Task_WiFiManagement;             ///< task handle for wifi management
extern TaskHandle_t Task_SystemMain;                 ///< task handle for system main
extern TaskHandle_t Task_SdCardCheck;                ///< task handle for sd card check  
extern TaskHandle_t Task_SerialCfg;                  ///< task handle for serial configuration
extern TaskHandle_t Task_SystemTelemetry;            ///< task handle for system telemetry
extern TaskHandle_t Task_SysLed;                     ///< task handle for system led
extern TaskHandle_t Task_WiFiWatchdog;               ///< task handle for wifi watchdog
//extern TaskHandle_t Task_SdCardFileRemove;           ///< task handle for remove file from sd card  

extern uint8_t StartRemoveSdCard;
extern uint32_t SdCardRemoveTime;

void System_RestartMcu(SystemRestartReason_enum);
SystemRestartReason_enum System_ConsumeRestartReason();

/* EOF */