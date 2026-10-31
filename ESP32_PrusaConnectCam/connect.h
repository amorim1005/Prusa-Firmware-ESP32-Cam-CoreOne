/**
   @file connnect.h

   @brief library for communication with prusa connect backend

   @author Miroslav Pivovarsky
   Contact: miroslav.pivovarsky@gmail.com

   @bug: no know bug
*/
#pragma once

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_task_wdt.h>
#include <Arduino.h>
#include <ArduinoJson.h>

#include "wifi_mngt.h"
#include "mcu_cfg.h"
#include "var.h"
#include "log.h"
#include "camera.h"
#include "cfg.h"
#include "Certificate.h"
#include "WebServer.h"
#include "connect_types.h"

class WiFiMngt;
class Configuration;
class Camera;

class PrusaConnect {
private:
  uint16_t RefreshInterval;                       ///< interval for sending photo to backend [s]. One of the Camera Service trigger schemes
  String BackendReceivedStatus;                   ///< status of backend response
  BackendAvailabilitStatus BackendAvailability;   ///< status of backend availability
  bool SendDeviceInformationToBackend;            ///< flag for sending device information to backend
  uint16_t SendingIntervalCounter;                ///< counter for sending interval, represents seconds
  uint16_t BackoffInterval;                       ///< interval after a failed upload [s]. 0 = no backoff
  uint16_t ConnectFailCounter;                    ///< count of consecutive failed connections to the backend server
  bool EnableTimelapsPhotoSave;                   ///< flag for saving photo to SD card
  uint32_t UploadOkCount;                         ///< photos accepted by the backend since the start
  uint32_t UploadFailedCount;                     ///< photos not uploaded since the start: capture or upload failed
  bool CaptureFailed;                             ///< the last photo capture failed
  String HealthStatus;                            ///< camera problem shown in Prusa Connect as a part of the camera name. Empty = OK

  String Token;                                   ///< token for backend communication
  String Fingerprint;                             ///< fingerprint for backend communication 
  String PrusaConnectHostname;                    ///< hostname of prusa connect backend          

  WiFiClientSecure BackendClient;                 ///< HTTPS connection to the backend, kept open between uploads
  String BackendClientHost;                       ///< hostname of the open connection

  Configuration *config;                          ///< pointer to configuration object
  Logs *log;                                      ///< pointer to logs object
  Camera *camera;                                 ///< pointer to camera object
  WiFiMngt *wifi;                                 ///< pointer to wifi object

  int SendDataToBackend(const uint8_t *, size_t, String, String, String);
  bool BackendConnect();
  void BackendClose();
  int BackendRequest(const uint8_t *, size_t, String, String, bool, String *, bool *);
  void ProcessBackendResult(int, SendDataToBackendType);
  String TranslateRefreshIntervalToTriggerScheme(uint16_t);
  int TranslateExifRotationToDegree(uint8_t);

public:
  PrusaConnect(Configuration*, Logs*, Camera*, WiFiMngt*);
  ~PrusaConnect(){};

  void Init();
  void LoadCfgFromEeprom();

  void TakePicture();
  bool SendPhotoToBackend(const uint8_t *, size_t);
  bool SendInfoToBackend();
  void TakePictureAndSendToBackend();
  String ProcessHttpResponseCode(int);
  BackendResultClass ProcessHttpResponseCodeClass(int);
  void UpdateDeviceInformation();

  void SetRefreshInterval(uint16_t);
  bool CheckRefreshInterval(uint16_t);
  void SetToken(String);
  void SetBackendAvailabilitStatus(BackendAvailabilitStatus);
  void SetPrusaConnectHostname(String);
  void SetTimeLapsPhotoSaveStatus(bool);
  void SetHealthStatus(String);

  void SavePhotoToSdCard(uint8_t *, size_t);

  uint16_t GetRefreshInterval();
  uint16_t GetEffectiveInterval();
  uint16_t GetBackoffInterval();
  uint16_t GetConnectFailCounter();
  void ClearConnectFailCounter();
  String GetBackendReceivedStatus();
  String GetToken();
  String GetFingerprint();
  String GetPrusaConnectHostname();
  BackendAvailabilitStatus GetBackendAvailabilitStatus();
  String CovertBackendAvailabilitStatusToString(BackendAvailabilitStatus);
  bool GetTimeLapsPhotoSaveStatus();
  uint32_t GetUploadOkCount();
  uint32_t GetUploadFailedCount();
  bool GetCaptureFailed();
  String GetHealthStatus();

  void IncreaseSendingIntervalCounter();
  void SetSendingIntervalCounter(uint16_t);
  void SetSendingIntervalExpired();
  uint16_t GetSendingIntervalCounter();
  bool CheckSendingIntervalExpired(uint16_t);
};

extern PrusaConnect Connect;  ///< PrusaConnect object

/* EOF */
