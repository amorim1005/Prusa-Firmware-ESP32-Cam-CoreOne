/**
   @file system.cpp

   @brief system library

   @author Miroslav Pivovarsky
   Contact: miroslav.pivovarsky@gmail.com

   @bug: no know bug
*/

#include "system.h"

/**
   @brief Function for init system library
   @param none
   @return none
*/
void System_Init() {
  SystemLog.AddEvent(LogLevel_Info, F("Init system lib"));
  SystemLog.AddEvent(LogLevel_Info, "SW Version: " + String(SW_VERSION) + " Build: " + String(SW_BUILD));
  SystemLog.AddEvent(LogLevel_Info, "Board name: " + String(BOARD_NAME));

  /* show last reset status */
  String reason_simple = System_printMcuResetReasonSimple();
  SystemLog.AddEvent(LogLevel_Warning, "CPU reset reason: " + reason_simple);

  String reason_core0 = System_PrintMcuResetReason(rtc_get_reset_reason(0));
  String reason_core1 = System_PrintMcuResetReason(rtc_get_reset_reason(1));
  SystemLog.AddEvent(LogLevel_Warning, "CPU0 reset reason: " + reason_core0);
  SystemLog.AddEvent(LogLevel_Warning, "CPU1 reset reason: " + reason_core1);

  SystemLog.AddEvent(LogLevel_Info, "MCU Temperature: " + String(temperatureRead()) + " *C");
  SystemLog.AddEvent(LogLevel_Info, "Internal Total heap: " + String(ESP.getHeapSize()) + " B, Internal Free Heap: " + String(ESP.getFreeHeap()));
  SystemLog.AddEvent(LogLevel_Info, "PSRAM Total heap: " + String(ESP.getPsramSize()) + " B, PSRAM Free Heap: " + String(ESP.getFreePsram()));
  SystemLog.AddEvent(LogLevel_Info, "Chip model: " + String(ESP.getChipModel()) + ", ChipRevision: " + String(ESP.getChipRevision()) + ", Cpu Freq: " + String(ESP.getCpuFreqMHz()));
  SystemLog.AddEvent(LogLevel_Info, "SDK Version: " + String(ESP.getSdkVersion()) + ", Core Version: " + String(ESP.getCoreVersion()));
  SystemLog.AddEvent(LogLevel_Info, "Flash Size: " + String(ESP.getFlashChipSize()) + ", Flash Speed " + String(ESP.getFlashChipSpeed()) + ", Flash Mode: " + String(ESP.getFlashChipMode()));
  
  System_CheckIfPsramIsUsed();
}

/**
   @brief Function for load configuration from EEPROM
   @param none
   @return none
*/
void System_LoadCfg() {
  /* restart statistics */
  SystemRestartReason_enum automatic = System_ConsumeRestartReason();
  esp_reset_reason_t reason = esp_reset_reason();
  SystemHealth.LastRestartReason = System_GetRestartReasonText(reason, automatic);
  SystemHealth.UnplannedRestarts = SystemConfig.LoadUnplannedRestarts();

  if (true == System_IsUnplannedRestart(reason, automatic)) {
    SystemHealth.UnplannedRestarts++;
    SystemConfig.SaveUnplannedRestarts(SystemHealth.UnplannedRestarts);
  }
  SystemLog.AddEvent(LogLevel_Warning, "Last restart: " + SystemHealth.LastRestartReason + ". Unplanned restarts: " + String(SystemHealth.UnplannedRestarts));
}

/**
   @brief Function for check if PSRAM is used
   @param none
   @return none
*/
bool System_CheckIfPsramIsUsed() {
  bool ret = false;
  if (psramFound()) {
    SystemLog.AddEvent(LogLevel_Info, F("PSRAM is used."));
    ret = true;
    void *ptr = malloc(100);

    if (ptr != NULL) {
      if (esp_ptr_external_ram(ptr)) {
        SystemLog.AddEvent(LogLevel_Info, F("malloc/new is using SPIRAM"));
      } else {
        SystemLog.AddEvent(LogLevel_Info, F("malloc/new is not using SPIRAM"));
      }
      free(ptr);
    } else {
      SystemLog.AddEvent(LogLevel_Info, F("Failed to allocate memory"));
    }
  } else {
    SystemLog.AddEvent(LogLevel_Info, F("PSRAM is not used."));
  }

  return ret;
}

/**
   @brief Main function for system lib. Call it once per second
   @param none
   @return none
*/
void System_Main() {
  static uint16_t HealthCheckCounter = 0;

  /* restart after a firmware upload from the WEB page. Wait one cycle, the WEB server sends the answer to the browser */
  static bool RestartWait = false;
  if (true == FirmwareUpload.RestartPending) {
    if (true == RestartWait) {
      SystemLog.AddEvent(LogLevel_Warning, F("Restart MCU with the new firmware"));
      SystemLog.LogFlushFile();
      ESP.restart();
    }
    RestartWait = true;
  }

  HealthCheckCounter++;
  if (HealthCheckCounter >= HEALTH_CHECK_INTERVAL) {
    HealthCheckCounter = 0;
    System_CheckMemory();
  }
}

/**
   @brief Check the free memory. After a long time the memory can be fragmented: the big blocks for photos and TLS
          are not available any more and the uploads fail. Restart the MCU when the memory stays low, when it does not matter
   @param none
   @return none
*/
void System_CheckMemory() {
  size_t internal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  size_t psram = psramFound() ? heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) : SIZE_MAX;
  bool low = (internal < HEALTH_LOW_MEMORY_INTERNAL) || (psram < HEALTH_LOW_MEMORY_PSRAM);

  if (false == low) {
    if (true == SystemHealth.LowMemory) {
      SystemLog.AddEvent(LogLevel_Info, F("Free memory is OK again"));
    }
    SystemHealth.LowMemory = false;
    SystemHealth.LowMemoryTime = 0;
    return;
  }

  if (false == SystemHealth.LowMemory) {
    SystemLog.AddEvent(LogLevel_Warning, "Low memory! Largest free block, internal: " + String(internal) + " B, PSRAM: " + String(psram) + " B");
  }
  SystemHealth.LowMemory = true;
  SystemHealth.LowMemoryTime += HEALTH_CHECK_INTERVAL;

  /* restart when nobody watches the stream and the printer is not printing. While printing, restart only when the memory does not get better */
  bool printing = (true == SystemPrusaLink.GetEnable()) && (true == SystemPrusaLink.GetReachable()) && (true == SystemPrusaLink.GetPrinting());
  bool idle = (0 == SystemCamera.GetStreamClients()) && (false == printing) && (false == System_FirmwareUploadActive());

  if (((true == idle) && (SystemHealth.LowMemoryTime >= HEALTH_LOW_MEMORY_RESTART)) || (SystemHealth.LowMemoryTime >= HEALTH_LOW_MEMORY_FORCE)) {
    SystemLog.AddEvent(LogLevel_Error, F("Memory is low for a long time. Restart MCU!"));
    SystemLog.LogFlushFile();
    System_RestartMcu(SystemRestart_LowMemory);
  }
}

/**
   @brief Camera health for Prusa Connect. The most important problem wins
   @param none
   @return String - short problem description, empty when everything is OK
*/
String System_GetHealthStatus() {
  if (true == McuTemperature.ThermalThrottling) {
    return F("HOT");
  }
  if (true == SystemHealth.LowMemory) {
    return F("LOW MEMORY");
  }
  if (true == Connect.GetCaptureFailed()) {
    return F("CAMERA ERROR");
  }
  if (true == SystemPrusaLink.GetLoginError()) {
    return F("PRUSALINK LOGIN");
  }
  return "";
}

/**
   @brief Short restart reason for the WEB page
   @param esp_reset_reason_t - reset reason from the chip
   @param SystemRestartReason_enum - reason of an automatic restart by the firmware
   @return String - reason
*/
String System_GetRestartReasonText(esp_reset_reason_t i_reason, SystemRestartReason_enum i_automatic) {
  switch (i_automatic) {
    case SystemRestart_LowMemory:
      return F("Automatic restart: low memory");
    case SystemRestart_WiFiLost:
      return F("Automatic restart: WiFi connection lost");
    case SystemRestart_CameraInit:
      return F("Automatic restart: camera init failed");
    default:
      break;
  }

  switch (i_reason) {
    case ESP_RST_POWERON:
      return F("Power on");
    case ESP_RST_SW:
      return F("Restart from the web page, settings or serial port");
    case ESP_RST_USB:
      return F("Restart by USB (flashing or serial monitor)");
    case ESP_RST_PANIC:
    case ESP_RST_CPU_LOCKUP:
      return F("Crash (exception)");
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
      return F("Watchdog: a task was stuck");
    case ESP_RST_BROWNOUT:
    case ESP_RST_PWR_GLITCH:
      return F("Power drop (brownout)");
    default:
      return "Other (" + String((int)i_reason) + ")";
  }
}

/**
   @brief Check if the restart was not requested by the user: crash, watchdog, power drop or automatic restart
   @param esp_reset_reason_t - reset reason from the chip
   @param SystemRestartReason_enum - reason of an automatic restart by the firmware
   @return bool - true for an unplanned restart
*/
bool System_IsUnplannedRestart(esp_reset_reason_t i_reason, SystemRestartReason_enum i_automatic) {
  return (SystemRestart_None != i_automatic) || (ESP_RST_PANIC == i_reason) || (ESP_RST_INT_WDT == i_reason) ||
         (ESP_RST_TASK_WDT == i_reason) || (ESP_RST_WDT == i_reason) || (ESP_RST_BROWNOUT == i_reason) ||
         (ESP_RST_PWR_GLITCH == i_reason) || (ESP_RST_CPU_LOCKUP == i_reason);
}

/**
   @brief Function for print reset reason
   @param int - reset reason
   @return String - reset reason
*/
String System_PrintMcuResetReason(int reason) {
  String ret = "";
  switch (reason) {
    case 1: /**<1,  Vbat power on reset*/
      ret = F("POWERON_RESET");
      break;
    case 3: /**<3,  Software reset digital core*/
      ret = F("SW_RESET");
      break;
    case 4: /**<4,  Legacy watch dog reset digital core*/
      ret = F("OWDT_RESET");
      break;
    case 5: /**<5,  Deep Sleep reset digital core*/
      ret = F("DEEPSLEEP_RESET");
      break;
    case 6: /**<6,  Reset by SLC module, reset digital core*/
      ret = F("SDIO_RESET");
      break;
    case 7: /**<7,  Timer Group0 Watch dog reset digital core*/
      ret = F("TG0WDT_SYS_RESET");
      break;
    case 8: /**<8,  Timer Group1 Watch dog reset digital core*/
      ret = F("TG1WDT_SYS_RESET");
      break;
    case 9: /**<9,  RTC Watch dog Reset digital core*/
      ret = F("RTCWDT_SYS_RESET");
      break;
    case 10: /**<10, Instrusion tested to reset CPU*/
      ret = F("INTRUSION_RESET");
      break;
    case 11: /**<11, Time Group reset CPU*/
      ret = F("TGWDT_CPU_RESET");
      break;
    case 12: /**<12, Software reset CPU*/
      ret = F("SW_CPU_RESET");
      break;
    case 13: /**<13, RTC Watch dog Reset CPU*/
      ret = F("RTCWDT_CPU_RESET");
      break;
    case 14: /**<14, for APP CPU, reseted by PRO CPU*/
      ret = F("EXT_CPU_RESET");
      break;
    case 15: /**<15, Reset when the vdd voltage is not stable*/
      ret = F("RTCWDT_BROWN_OUT_RESET");
      break;
    case 16: /**<16, RTC Watch dog reset digital core and rtc module*/
      ret = F("RTCWDT_RTC_RESET");
      break;
    default:
      ret = F("NO_MEAN");
  }

  return ret;
}

/**
   @brief Function for print reset reason
   @param none
   @return String - reset reason
*/
String System_printMcuResetReasonSimple() {
  String ret = "";
  esp_reset_reason_t reason = esp_reset_reason();

  switch (reason) {
    case ESP_RST_UNKNOWN:
      ret = F("Reset reason can not be determined");
      break;
    case ESP_RST_POWERON:
      ret = F("Reset due to power-on event");
      break;
    case ESP_RST_EXT:
      ret = F("Reset by external pin (not applicable for ESP32)");
      break;
    case ESP_RST_SW:
      ret = F("Software reset via esp_restart");
      break;
    case ESP_RST_PANIC:
      ret = F("Software reset due to exception/panic");
      break;
    case ESP_RST_INT_WDT:
      ret = F("Reset (software or hardware) due to interrupt watchdog");
      break;
    case ESP_RST_TASK_WDT:
      ret = F("Reset due to task watchdog");
      break;
    case ESP_RST_WDT:
      ret = F("Reset due to other watchdogs");
      break;
    case ESP_RST_DEEPSLEEP:
      ret = F("Reset after exiting deep sleep mode");
      break;
    case ESP_RST_BROWNOUT:
      ret = F("Brownout reset (software or hardware)");
      break;
    case ESP_RST_SDIO:
      ret = F("Reset over SDIO");
      break;
    default:
      ret = F("N/A");
      break;
  }

  return ret;
}

/**
   @brief Function for WiFi management system task
   @param void *pvParameters
   @return none
*/
void System_TaskWifiManagement(void *pvParameters) {
  SystemLog.AddEvent(LogLevel_Info, F("Task Wifi Management. core: "), String(xPortGetCoreID()));
  TickType_t xLastWakeTime = xTaskGetTickCount();

  while (1) {
    /* wifi management. Enable/disable AP_STA mode and STA mode*/
    SystemWifiMngt.WifiManagement();
    McuTemperature.TemperatureCelsius = temperatureRead();

    /* wifi reconnect after signal lost */
    SystemWifiMngt.WiFiReconnect();
    /* build the verbose message only when it is logged */
    if (SystemLog.GetLogLevel() >= LogLevel_Verbose) {
      SystemLog.AddEvent(LogLevel_Verbose, F("WiFiManagement task. Stack free size: "), String(uxTaskGetStackHighWaterMark(NULL)) + "B");
      SystemLog.AddEvent(LogLevel_Verbose, F("WiFi status: "), String(WiFi.status()));
    }

    /* reset wdg */
    esp_task_wdt_reset();

    /* next start task */
    vTaskDelayUntil(&xLastWakeTime, TASK_WIFI / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Function for main system task
 * 
 * @param void *pvParameters
 * @return none
 */
void System_TaskMain(void *pvParameters) {
  SystemLog.AddEvent(LogLevel_Info, F("System task. core: "), String(xPortGetCoreID()));
  TickType_t xLastWakeTime = xTaskGetTickCount();

  while (1) {
    esp_task_wdt_reset();
    System_Main();
    /* build the verbose message only when it is logged */
    if (SystemLog.GetLogLevel() >= LogLevel_Verbose) {
      SystemLog.AddEvent(LogLevel_Verbose, F("System task. Stack free size: "), String(uxTaskGetStackHighWaterMark(NULL)) + "B");
    }

    /* reset wdg */
    esp_task_wdt_reset();

    /* next start task */
    vTaskDelayUntil(&xLastWakeTime, TASK_SYSTEM / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Read MCU temperature and update the thermal throttling state, with hysteresis.
 *        The XIAO ESP32-S3 inside the heated chamber of a printer can overheat
 * 
 * @param none
 * @return none
 */
void System_CheckThermalState() {
  float temp = temperatureRead();
  McuTemperature.TemperatureCelsius = temp;
  if (temp > McuTemperature.TemperatureMax) {
    McuTemperature.TemperatureMax = temp;
  }

  if ((false == McuTemperature.ThermalThrottling) && (temp >= THERMAL_THROTTLE_TEMP)) {
    McuTemperature.ThermalThrottling = true;
    SystemLog.AddEvent(LogLevel_Warning, "MCU temperature is too high: " + String(temp) + " *C. Stream stopped, uploads slowed down");
  } else if ((true == McuTemperature.ThermalThrottling) && (temp <= THERMAL_RESUME_TEMP)) {
    McuTemperature.ThermalThrottling = false;
    SystemLog.AddEvent(LogLevel_Info, "MCU temperature is OK: " + String(temp) + " *C. Normal operation");
  }
}

/**
 * @brief Get interval for the next photo upload. The longest interval wins:
 *        - refresh interval, or backoff interval after a failed upload
 *        - PrusaLink: PRUSALINK_IDLE_INTERVAL when the printer is reachable and is not printing
 *        - thermal throttling: THERMAL_THROTTLE_INTERVAL
 * 
 * @param none
 * @return uint16_t - interval [s]
 */
uint16_t System_GetUploadInterval() {
  uint16_t interval = Connect.GetEffectiveInterval();

  if ((true == SystemPrusaLink.GetEnable()) && (true == SystemPrusaLink.GetReachable()) && (false == SystemPrusaLink.GetPrinting())) {
    interval = max(interval, (uint16_t)PRUSALINK_IDLE_INTERVAL);
  }

  if (true == McuTemperature.ThermalThrottling) {
    interval = max(interval, (uint16_t)THERMAL_THROTTLE_INTERVAL);
  }

  return interval;
}

/**
 * @brief Function for capture and send photo task
 * 
 * @param void *pvParameters
 * @return none
 */
void System_TaskCaptureAndSendPhoto(void *pvParameters) {
  SystemLog.AddEvent(LogLevel_Info, F("Task photo processing. core: "), String(xPortGetCoreID()));
  TickType_t xLastWakeTime = xTaskGetTickCount();

  while (1) {
    System_CheckThermalState();

    /* read printer state from PrusaLink. Upload immediately when the printer starts or finishes printing */
    SystemPrusaLink.Loop();
    if ((true == SystemPrusaLink.ConsumeStateChange()) && (0 == Connect.GetBackoffInterval())) {
      SystemLog.AddEvent(LogLevel_Info, F("Printer state changed, send photo now"));
      Connect.SetSendingIntervalExpired();
    }

    /* camera problems are shown in Prusa Connect */
    Connect.SetHealthStatus(System_GetHealthStatus());

    if (Connect.CheckSendingIntervalExpired(System_GetUploadInterval())) {
      Connect.SetSendingIntervalCounter(0);
      bool InfoOk = true;

      /* send camera information to backend */
      if ((WL_CONNECTED == WiFi.status()) && (false == System_FirmwareUploadActive())) {
        SystemLog.AddEvent(LogLevel_Verbose, F("Task photo processing. Start sending info"));
        esp_task_wdt_reset();
        InfoOk = Connect.SendInfoToBackend();
      }

      /* send photo to backend. Skip it when the backend is not reachable */
      if ((WL_CONNECTED == WiFi.status()) && (false == System_FirmwareUploadActive()) && (true == InfoOk)) {
        SystemLog.AddEvent(LogLevel_Verbose, F("Task photo processing. Start sending photo"));
        esp_task_wdt_reset();
        Connect.TakePictureAndSendToBackend();
      }

    } else {
      /* update counter */
      Connect.IncreaseSendingIntervalCounter();
    }
    
    /* build the verbose message only when it is logged */
    if (SystemLog.GetLogLevel() >= LogLevel_Verbose) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Photo processing task. Stack free size: "), String(uxTaskGetStackHighWaterMark(NULL)) + "B");
    }

    /* reset wdg */
    esp_task_wdt_reset();

    /* next start task */
    vTaskDelayUntil(&xLastWakeTime, TASK_PHOTO_SEND / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Function for micro SD card check task
 * 
 * @param void *pvParameters
 * @return none
 */
void System_TaskSdCardCheck(void *pvParameters) {
  SystemLog.AddEvent(LogLevel_Info, F("MicroSdCard check task. core: "), String(xPortGetCoreID()));
  TickType_t xLastWakeTime = xTaskGetTickCount();

  while (1) {
    esp_task_wdt_reset();
    /* write buffered log messages to the card */
    SystemLog.LogFlushFile();

    /* check micro SD card */
    if ((true == SystemLog.GetCardDetectAfterBoot()) && (false == SystemLog.GetCardDetectedStatus())) {
      SystemLog.LogCloseFile();
      SystemLog.ReinitCard();
      SystemLog.LogOpenFile();
      SystemLog.AddEvent(LogLevel_Warning, F("Reinit micro SD card done!"));
    }

    /* check card free space */
    if (true == SystemLog.GetCardDetectedStatus()) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Check card free space"));
      SystemLog.CheckCardSpace();
    }

    /* check maximum log file size */
    if (true == SystemLog.GetCardDetectedStatus()) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Check maximum log file size"));
      SystemLog.CheckMaxLogFileSize();
    }

    /* check if log file is opened */
    if (true == SystemLog.GetCardDetectedStatus()) {
      SystemLog.LogCheckOpenedFile();
      if (false == SystemLog.GetLogFileOpened()) {
        SystemLog.LogOpenFile();
        SystemLog.AddEvent(LogLevel_Warning, F("Log file is not opened!"));
      }
    }

    SystemLog.AddEvent(LogLevel_Info, "CardStatus: " + String(SystemLog.GetCardDetectedStatus()) + " FileStatus: " + String(SystemLog.GetLogFileOpened()));
    /* build the verbose message only when it is logged */
    if (SystemLog.GetLogLevel() >= LogLevel_Verbose) {
      SystemLog.AddEvent(LogLevel_Verbose, F("MicroSdCard task. Stack free size: "), String(uxTaskGetStackHighWaterMark(NULL)) + "B");
    }

    /* reset wdg */
    esp_task_wdt_reset();

    /* next start task */
    vTaskDelayUntil(&xLastWakeTime, TASK_SDCARD / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Function for serial configuration task
 * 
 * @param void *pvParameters
 * @return none
 */
void System_TaskSerialCfg(void *pvParameters) {
  SystemLog.AddEvent(LogLevel_Info, F("SerialCg task. core: "), String(xPortGetCoreID()));
  TickType_t xLastWakeTime = xTaskGetTickCount();

  while (1) {
    esp_task_wdt_reset();
    SystemSerialCfg.ProcessIncommingData();
    /* build the verbose message only when it is logged */
    if (SystemLog.GetLogLevel() >= LogLevel_Verbose) {
      SystemLog.AddEvent(LogLevel_Verbose, F("SerialCfg task. Stack free size: "), String(uxTaskGetStackHighWaterMark(NULL)) + "B");
    }

    /* reset wdg */
    esp_task_wdt_reset();

    /* next start task */
    vTaskDelayUntil(&xLastWakeTime, TASK_SERIAL_CFG / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Function for system telemetry task
 * 
 * @param void *pvParameters
 * @return none
 */
void System_TaskSystemTelemetry(void *pvParameters) {
  SystemLog.AddEvent(LogLevel_Info, F("SystemTelemetry task. core: "), String(xPortGetCoreID()));
  TickType_t xLastWakeTime = xTaskGetTickCount();

  while (1) {
    esp_task_wdt_reset();
    /* build the verbose message only when it is logged */
    if (SystemLog.GetLogLevel() >= LogLevel_Verbose) {
      SystemLog.AddEvent(LogLevel_Verbose, F("SystemTelemetry task. Stack free size: "), String(uxTaskGetStackHighWaterMark(NULL)) + "B");
    }
    if (SystemCamera.GetStreamStatus()) {
      char buf[80] = { '\0' };
      sprintf(buf, "Stream, average data in %dsec. FPS: %.1f, Size: %uKB", (TASK_SYSTEM_TELEMETRY / SECOND_TO_MILISECOND), SystemCamera.StreamGetFrameAverageFps(), SystemCamera.StreamGetFrameAverageSize());
      SystemLog.AddEvent(LogLevel_Info, buf);
      SystemCamera.StreamClearFrameData();
    }

    SystemLog.AddEvent(LogLevel_Info, "Free RAM: " + String(ESP.getFreeHeap()) + " B" + ", Min: " + String(ESP.getMinFreeHeap()) + ", Largest block: " + String(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
    SystemLog.AddEvent(LogLevel_Info, "Free PSRAM: " + String(ESP.getFreePsram()) + " B" + ", Min: " + String(ESP.getMinFreePsram()) + ", Largest block: " + String(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
    SystemLog.AddEvent(LogLevel_Info, "MCU Temperature: " + String(McuTemperature.TemperatureCelsius) + " *C");

    ExternalTemperatureSensor.ReadSensorData();

    /* reset wdg */
    esp_task_wdt_reset();

    /* next start task */
    vTaskDelayUntil(&xLastWakeTime, TASK_SYSTEM_TELEMETRY / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Function for system led task
 * 
 * @param void *pvParameters
 * @return none
 */
void System_TaskSysLed(void *pvParameters) {
  SystemLog.AddEvent(LogLevel_Info, F("SystemLed task. core: "), String(xPortGetCoreID()));
  TickType_t xLastWakeTime = xTaskGetTickCount();

  while (1) {
    system_led.toggle();
    /* reset wdg */
    esp_task_wdt_reset();
    /* build the verbose message only when it is logged */
    if (SystemLog.GetLogLevel() >= LogLevel_Verbose) {
      SystemLog.AddEvent(LogLevel_Verbose, F("SystemLed task. Stack free size: "), String(uxTaskGetStackHighWaterMark(NULL)) + "B");
    }

    /* next start task */
    vTaskDelayUntil(&xLastWakeTime, system_led.getTimer() / portTICK_PERIOD_MS);
  }
} 

/**
 * @brief Function for WiFi watchdog task
 * 
 * @param void *pvParameters
 * @return none
 */
void System_TaskWiFiWatchdog(void *pvParameters) {
  SystemLog.AddEvent(LogLevel_Info, F("WiFiWatchdog task. core: "), String(xPortGetCoreID()));
  TickType_t xLastWakeTime = xTaskGetTickCount();

  while (1) {
    esp_task_wdt_reset();
    SystemWifiMngt.WiFiWatchdog();
    /* build the verbose message only when it is logged */
    if (SystemLog.GetLogLevel() >= LogLevel_Verbose) {
      SystemLog.AddEvent(LogLevel_Verbose, F("WiFiWatchdog task. Stack free size: "), String(uxTaskGetStackHighWaterMark(NULL)) + "B");
    }

    /* reset wdg */
    esp_task_wdt_reset();

    /* next start task */
    vTaskDelayUntil(&xLastWakeTime, TASK_WIFI_WATCHDOG / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Function for micro SD card remove files task
 * 
 * @param void *pvParameters
 * @return none
 */
void System_TaskSdCardRemove(void *pvParameters) {
  SystemLog.AddEvent(LogLevel_Info, F("TaskSdCardRemove. core: "), String(xPortGetCoreID()));
  TickType_t xLastWakeTime = xTaskGetTickCount();
  SdCardRemoveTime = TASK_SDCARD_FILE_REMOVE;

    while (1) {
      esp_task_wdt_reset();
      if (0 != StartRemoveSdCard) {
        if (1 == StartRemoveSdCard) {
          SdCardRemoveTime = 5000;
          SystemLog.AddEvent(LogLevel_Info, F("Start remove timelaps photo"));
          uint16_t file_count = SystemLog.CountFilesInDir(SD_MMC, TIMELAPS_PHOTO_FOLDER);
          SystemLog.AddEvent(LogLevel_Info, F("Files in dir: "), String(file_count));
          esp_task_wdt_reset();
          StartRemoveSdCard = 2;
        }

        if ( false == SystemLog.RemoveFilesInDir(SD_MMC, TIMELAPS_PHOTO_FOLDER, FILE_REMOVE_MAX_COUNT)) {
          SystemLog.AddEvent(LogLevel_Info, F("Remove files in dir done"));
          StartRemoveSdCard = 0;
          SdCardRemoveTime = TASK_SDCARD_FILE_REMOVE;
        }
      }
    
    /* build the verbose message only when it is logged */
    if (SystemLog.GetLogLevel() >= LogLevel_Verbose) {
      SystemLog.AddEvent(LogLevel_Verbose, F("MicroSdCard task. Stack free size: "), String(uxTaskGetStackHighWaterMark(NULL)) + "B");
    }

    /* reset wdg */
    esp_task_wdt_reset();

    /* next start task */
    vTaskDelayUntil(&xLastWakeTime, SdCardRemoveTime / portTICK_PERIOD_MS);
  }
}

/* EOF */