/**
   @file system.h

   @brief system library

   @author Miroslav Pivovarsky
   Contact: miroslav.pivovarsky@gmail.com

   @bug: no know bug
*/

#pragma once

#include <WiFi.h>
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_wifi.h>
#include <esp_task_wdt.h>

#if defined(CONFIG_IDF_TARGET_ESP32)
    #include "esp32/rom/rtc.h"
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    #include "esp32s3/rom/rtc.h"
#else
    #error "Unsupported chip target"
#endif

#include "mcu_cfg.h"
#include "var.h"
#include "cfg.h"
#include "log.h"
#include "wifi_mngt.h"
#include "connect.h"
#include "prusalink.h"
#include "serial_cfg.h"
#include "sys_led.h"

void System_Init();
void System_LoadCfg();
bool System_CheckIfPsramIsUsed();
void System_Main();
void System_CheckMemory();
String System_GetHealthStatus();
String System_GetRestartReasonText(esp_reset_reason_t, SystemRestartReason_enum);
bool System_IsUnplannedRestart(esp_reset_reason_t, SystemRestartReason_enum);

String System_PrintMcuResetReason(int);
String System_printMcuResetReasonSimple();

void System_TaskWifiManagement(void *);
void System_TaskMain(void *);
void System_TaskCaptureAndSendPhoto(void *);
void System_CheckThermalState();
uint16_t System_GetUploadInterval();
void System_TaskSdCardCheck(void *);
void System_TaskSerialCfg(void *);
void System_TaskSystemTelemetry(void *);
void System_TaskSysLed(void *);
void System_TaskWiFiWatchdog(void *);
void System_TaskSdCardRemove(void *);

/* EOF */