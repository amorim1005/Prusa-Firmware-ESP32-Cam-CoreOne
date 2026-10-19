/**
   @file variable.cpp

   @brief Library with global variables

   @author Miroslav Pivovarsky
   Contact: miroslav.pivovarsky@gmail.com

   @bug: no know bug
*/

#include "var.h"

WebBasicAuth_struct WebBasicAuth = { false, "", "" };
struct SystemHealth_struct SystemHealth = { false, 0, 0, "N/A" };
struct FirmwareUpload_struct FirmwareUpload = { false, 0, false, "" };
struct McuTemperature_struct McuTemperature = {0.0, false, 0.0};

TaskHandle_t Task_CapturePhotoAndSend;
TaskHandle_t Task_WiFiManagement;
TaskHandle_t Task_SystemMain;
TaskHandle_t Task_SdCardCheck;
TaskHandle_t Task_SerialCfg;
TaskHandle_t Task_SystemTelemetry;
TaskHandle_t Task_SysLed;
TaskHandle_t Task_WiFiWatchdog;
//TaskHandle_t Task_SdCardFileRemove;

uint8_t StartRemoveSdCard = 0;
uint32_t SdCardRemoveTime = 0;

/* the RTC memory keeps its content after a software restart, it is not cleared by the startup code */
#define SYSTEM_RESTART_MAGIC  0x52455354  ///< "REST": the restart reason below is valid
RTC_NOINIT_ATTR uint32_t SystemRestartMagic;
RTC_NOINIT_ATTR uint8_t SystemRestartReason;

/**
 * @brief Restart the MCU after a problem. The reason is shown on the System page after the restart
 *
 * @param SystemRestartReason_enum - reason
 * @return none
 */
void System_RestartMcu(SystemRestartReason_enum i_reason) {
  SystemRestartReason = (uint8_t)i_reason;
  SystemRestartMagic = SYSTEM_RESTART_MAGIC;
  delay(100);
  ESP.restart();
}

/**
 * @brief Read the reason of an automatic restart. Call it once after the start
 *
 * @return SystemRestartReason_enum - reason. SystemRestart_None after a power on or a restart by the user
 */
SystemRestartReason_enum System_ConsumeRestartReason() {
  SystemRestartReason_enum ret = SystemRestart_None;
  if ((SYSTEM_RESTART_MAGIC == SystemRestartMagic) && (ESP_RST_SW == esp_reset_reason())) {
    ret = (SystemRestartReason_enum)SystemRestartReason;
  }
  SystemRestartMagic = 0;
  return ret;
}

/**
 * @brief Check if a firmware upload is in progress. An upload without new data for 30 s was interrupted by the browser
 *
 * @return bool - true when the upload is in progress
 */
bool System_FirmwareUploadActive() {
  return (true == FirmwareUpload.Active) && ((millis() - FirmwareUpload.LastData) < 30000);
}

/* EOF */