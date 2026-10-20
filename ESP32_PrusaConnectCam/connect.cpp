/**
   @file connnect.cpp

   @brief library for communication with prusa connect backend

   @author Miroslav Pivovarsky
   Contact: miroslav.pivovarsky@gmail.com

   @bug: no know bug
*/

#include "connect.h"

PrusaConnect Connect(&SystemConfig, &SystemLog, &SystemCamera, &SystemWifiMngt);

/**
 * @brief Constructor for PrusaConnect class
 *
 * @param Configuration*  - pointer to Configuration class
 * @param Logs*           - pointer to Logs class
 * @param Camera*         - pointer to Camera class
 */
PrusaConnect::PrusaConnect(Configuration *i_conf, Logs *i_log, Camera *i_camera, WiFiMngt *i_wifi) {
  config = i_conf;
  log = i_log;
  camera = i_camera;
  wifi = i_wifi;
  BackendAvailability = WaitForFirstConnection;
  SendDeviceInformationToBackend = true;
  SendingIntervalCounter = 0;
  BackoffInterval = 0;
  ConnectFailCounter = 0;
  UploadOkCount = 0;
  LastPhotoSize = 0;
  LastPhotoFrameSize = 0;
  LastPhotoQuality = 0;
  UploadFailedCount = 0;
  CaptureFailed = false;
  HealthStatus = "";
}

/**
 * @brief init library PrusaConnect
 *
 * @param none
 * @return none
 */
void PrusaConnect::Init() {
  log->AddEvent(LogLevel_Info, F("Init PrusaConnect lib"));
  BackendReceivedStatus = F("Wait for first connection");
}

/**
 * @brief Load configuration from EEPROM
 *
 * @param none
 * @return none
 */
void PrusaConnect::LoadCfgFromEeprom() {
  log->AddEvent(LogLevel_Info, F("Load PrusaConnect CFG from EEPROM"));
  Token = config->LoadToken();
  Fingerprint = config->LoadFingerprint();
  RefreshInterval = config->LoadRefreshInterval();
  PrusaConnectHostname = config->LoadPrusaConnectHostname();
  EnableTimelapsPhotoSave = config->LoadTimeLapseFunctionStatus();

  /* the old Prusa Connect camera API was moved to the Camera Service. Migrate the old default hostname */
  if ((PrusaConnectHostname.length() == 0) || (PrusaConnectHostname == LEGACY_PRUSA_CONNECT_HOSTNAME)) {
    log->AddEvent(LogLevel_Warning, F("Migrate backend hostname to: "), String(FACTORY_CFG_HOSTNAME));
    SetPrusaConnectHostname(FACTORY_CFG_HOSTNAME);
  }

  if ((Fingerprint.length() < CAMERA_SERVICE_FINGERPRINT_MIN_LEN) || (Fingerprint.length() > CAMERA_SERVICE_FINGERPRINT_MAX_LEN)) {
    log->AddEvent(LogLevel_Error, F("Fingerprint length is out of range 16-64: "), String(Fingerprint.length()));
  }
}

/**
 * @brief take picture
 *
 * @param none
 * @return none
 */
void PrusaConnect::TakePicture() {
  camera->CapturePhoto();
}

/**
 * @brief Open a new HTTPS connection to the backend
 * 
 * @return true - connected
 * @return false - connection failed, BackendReceivedStatus contains the error
 */
bool PrusaConnect::BackendConnect() {
  BackendClose();
  BackendClient.setCACert(root_CAs);
  BackendClient.setTimeout(BACKEND_SOCKET_TIMEOUT);
  BackendClient.setHandshakeTimeout(TLS_HANDSHAKE_TIMEOUT);
  BackendClient.setNoDelay(true);

  log->AddEvent(LogLevel_Verbose, F("Connecting to server..."));
  esp_task_wdt_reset();

  if (!BackendClient.connect(PrusaConnectHostname.c_str(), 443)) {
    char err_buf[200];
    int last_error = BackendClient.lastError(err_buf, sizeof(err_buf));
    if (BackendAvailability != WaitForFirstConnection) {
      BackendAvailability = BackendUnavailable;
    }
    ConnectFailCounter++;
    BackendClose();

    BackendReceivedStatus = "Connection failed to domain! Error: " + String(last_error) + " - " + String(err_buf);
    log->AddEvent(LogLevel_Info, BackendReceivedStatus + " ,BA:" + CovertBackendAvailabilitStatusToString(BackendAvailability));
    return false;
  }

  ConnectFailCounter = 0;
  BackendClientHost = PrusaConnectHostname;
  log->AddEvent(LogLevel_Verbose, F("Connected to server!"));
  return true;
}

/**
 * @brief Close the HTTPS connection to the backend and release its memory
 */
void PrusaConnect::BackendClose() {
  BackendClient.stop();
  BackendClientHost = "";
}

/**
 * @brief Send one HTTP request on the open connection and read the complete response
 * 
 * @param i_data - request body
 * @param i_data_length - body length
 * @param i_content_type - body content type
 * @param i_url_path - url path
 * @param i_keepAlive - ask the server to keep the connection open
 * @param o_response - output, beginning of the response for the log
 * @param o_close - output, true when the connection cannot be used for the next request
 * @return int - HTTP response code. 0 = no response
 */
int PrusaConnect::BackendRequest(const uint8_t *i_data, size_t i_data_length, String i_content_type, String i_url_path, bool i_keepAlive, String *o_response, bool *o_close) {
  *o_close = true;

  /* send the request header in one write, every write is one TLS record */
  String header = "PUT " + i_url_path + " HTTP/1.1\r\n";
  header += "Host: " + PrusaConnectHostname + "\r\n";
  header += "User-Agent: " BACKEND_USER_AGENT "\r\n";
  header += (true == i_keepAlive) ? "Connection: keep-alive\r\n" : "Connection: close\r\n";
  header += "Content-Type: " + i_content_type + "\r\n";
  header += "Fingerprint: " + Fingerprint + "\r\n";
  header += "Token: " + Token + "\r\n";
  header += "Content-Length: " + String(i_data_length) + "\r\n\r\n";
  if (BackendClient.print(header) != header.length()) {
    return 0;
  }

  /* send body. write() can send less data than requested */
  esp_task_wdt_reset();
  size_t sent_data = 0;
  while (sent_data < i_data_length) {
    size_t chunk = min((size_t)PHOTO_FRAGMENT_SIZE, i_data_length - sent_data);
    size_t written = BackendClient.write(i_data + sent_data, chunk);
    if (written == 0) {
      break;
    }
    sent_data += written;
    esp_task_wdt_reset();
  }
  BackendClient.flush();
  log->AddEvent(LogLevel_Verbose, "Send done: " + String(i_data_length) + "/" + String(sent_data) + " bytes");

  if (i_data_length != sent_data) {
    log->AddEvent(LogLevel_Error, F("ERROR SEND DATA TO SERVER! INCORRECT DATA LENGTH!"));
    return 0;
  }

  /* read the status line and the headers. Never wait longer than BACKEND_RESPONSE_TIMEOUT */
  int httpCode = 0;
  long contentLength = -1;
  bool headersDone = false;
  bool serverClose = false;
  unsigned long start = millis();
  while ((millis() - start) < BACKEND_RESPONSE_TIMEOUT) {
    if (!BackendClient.available()) {
      if (!BackendClient.connected()) {
        break;
      }
      delay(5);
      esp_task_wdt_reset();
      continue;
    }

    String line = BackendClient.readStringUntil('\n');
    line.trim();
    log->AddEvent(LogLevel_Verbose, line.c_str());

    if (0 == httpCode) {
      if (line.startsWith("HTTP/1.")) {
        httpCode = line.substring(9, 12).toInt();
        *o_response += line + " ";
      }
      continue;
    }

    if (line.length() == 0) {
      headersDone = true;
      break;
    }

    String lower = line;
    lower.toLowerCase();
    if (lower.startsWith("content-length:")) {
      contentLength = lower.substring(15).toInt();
    } else if (lower.startsWith("connection:") && (lower.indexOf("close") >= 0)) {
      serverClose = true;
    }
  }

  if (false == headersDone) {
    log->AddEvent(LogLevel_Warning, F("Timeout or incomplete response from server"));
    return httpCode;
  }

  /* read the body completely, otherwise the next request on this connection reads the rest of this one.
     Keep the beginning for the log, the error body is a short JSON */
  bool bodyDone = false;
  if (contentLength >= 0) {
    long received = 0;
    uint8_t buf[256];
    while ((received < contentLength) && ((millis() - start) < BACKEND_RESPONSE_TIMEOUT)) {
      int n = BackendClient.read(buf, min((long)sizeof(buf), contentLength - received));
      if (n > 0) {
        if (o_response->length() < 512) {
          o_response->concat((const char *)buf, n);
        }
        received += n;
      } else if (!BackendClient.connected()) {
        break;
      } else {
        delay(5);
        esp_task_wdt_reset();
      }
    }
    bodyDone = (received == contentLength);
  }

  /* the connection is reusable only after a complete response with Content-Length */
  *o_close = (false == i_keepAlive) || serverClose || (false == bodyDone);
  return httpCode;
}

/**
 * @brief Sending data to the Camera Service backend. The HTTPS connection is kept open between uploads
 *        when the upload interval is short, a new TLS handshake costs about 1 s of CPU time
 * 
 * @param i_data  - data to send
 * @param i_data_length - data length
 * @param i_content_type - data content type
 * @param i_type - type of data for log message
 * @param i_url_path - url path for backend
 * @return int - HTTP response code. 0 = no response (connection failed, timeout, incomplete upload)
 */
int PrusaConnect::SendDataToBackend(const uint8_t *i_data, size_t i_data_length, String i_content_type, String i_type, String i_url_path) {
  log->AddEvent(LogLevel_Verbose, "Sending " + i_type + " to PrusaConnect, " + String(i_data_length) + " bytes");

  /* check fingerprint and token length */
  if ((Fingerprint.length() == 0) || (Token.length() == 0)) {
    log->AddEvent(LogLevel_Verbose, F("ERROR SEND DATA TO SERVER! INVALID DATA!"));
    BackendReceivedStatus = (Fingerprint.length() == 0) ? F("Missing fingerprint") : F("Missing token");
    return -1;
  }

  bool keepAlive = (GetEffectiveInterval() <= BACKEND_KEEPALIVE_MAX_INTERVAL);
  int httpCode = 0;
  String fullResponse = "";

  /* second attempt only when a reused connection was closed by the server in the meantime */
  for (uint8_t attempt = 0; attempt < 2; attempt++) {
    bool reused = BackendClient.connected() && (BackendClientHost == PrusaConnectHostname);
    if (reused) {
      log->AddEvent(LogLevel_Verbose, F("Reusing connection to server"));
    } else if (false == BackendConnect()) {
      return 0;
    }

    bool closeConnection = true;
    fullResponse = "";
    httpCode = BackendRequest(i_data, i_data_length, i_content_type, i_url_path, keepAlive, &fullResponse, &closeConnection);
    if (closeConnection) {
      BackendClose();
    }

    if ((0 == httpCode) && reused && (0 == attempt)) {
      log->AddEvent(LogLevel_Verbose, F("Reused connection was closed by the server, connecting again"));
      continue;
    }
    break;
  }

  log->AddEvent(LogLevel_Verbose, "Full response: " + fullResponse);

  if (0 == httpCode) {
    BackendReceivedStatus = i_type + F(": no response from server");
  } else {
    BackendReceivedStatus = i_type + ": " + ProcessHttpResponseCode(httpCode);
    if (BackendResultOk != ProcessHttpResponseCodeClass(httpCode)) {
      log->AddEvent(LogLevel_Warning, "Backend error response: " + fullResponse);
    }
  }
  BackendAvailability = BackendAvailable;

  log->AddEvent(LogLevel_Info, "Upload done: " + String(i_data_length) + " bytes. Response code: " + BackendReceivedStatus + " ,BA:" + CovertBackendAvailabilitStatusToString(BackendAvailability));
  return httpCode;
}

/**
 * @brief Update the backoff interval according to the backend response
 *
 * @param int - http code from SendDataToBackend
 * @param SendDataToBackendType - photo or info
 * @return none
 */
void PrusaConnect::ProcessBackendResult(int i_code, SendDataToBackendType i_type) {
  if (i_code < 0) {
    /* missing token or fingerprint, nothing was sent */
    return;
  }

  BackendResultClass result = ProcessHttpResponseCodeClass(i_code);

  /* camera info rejected (4xx): the same data gives the same answer. Send it again only after a configuration change.
     The photo upload has its own result, a wrong token is detected there and slows down the uploads */
  if ((SendInfo == i_type) && (BackendResultTransient != result)) {
    SendDeviceInformationToBackend = false;
    if (BackendResultOk != result) {
      log->AddEvent(LogLevel_Error, F("Camera info rejected by the backend, code: "), String(i_code));
    }
    return;
  }

  switch (result) {
    case BackendResultOk:
      if (BackoffInterval != 0) {
        log->AddEvent(LogLevel_Info, F("Backend OK, backoff cleared"));
      }
      BackoffInterval = 0;
      break;

    case BackendResultTransient:
      /* connection problem, rate limit or server error: double the waiting time up to BACKEND_BACKOFF_MAX */
      BackoffInterval = (BackoffInterval == 0) ? RefreshInterval : min((uint16_t)(BackoffInterval * 2), (uint16_t)BACKEND_BACKOFF_MAX);
      log->AddEvent(LogLevel_Warning, F("Backend temporary error, next attempt in [s]: "), String(BackoffInterval));
      break;

    case BackendResultClientError:
      /* bad token, fingerprint or request. Retry rarely until the user changes the configuration */
      BackoffInterval = BACKEND_BACKOFF_MAX;
      log->AddEvent(LogLevel_Error, F("Backend rejected the request. Check the token. Next attempt in [s]: "), String(BackoffInterval));
      break;

    case BackendResultTooLarge:
      log->AddEvent(LogLevel_Error, F("Photo is larger than 1 MB. Lower the resolution or the image quality!"));
      break;
  }
}

/**
 * @brief Send photo to prusa connect backend
 *
 * @param const uint8_t * - photo data, JPEG with EXIF header
 * @param size_t - photo length
 * @return bool - true if the photo was accepted
 */
bool PrusaConnect::SendPhotoToBackend(const uint8_t *i_data, size_t i_len) {
  log->AddEvent(LogLevel_Verbose, F("Start sending photo to prusaconnect"));

  if (i_len > CAMERA_SERVICE_SNAPSHOT_MAX_SIZE) {
    BackendReceivedStatus = "Photo: too large " + String(i_len) + " B, maximum is 1 MB. Lower the resolution or quality";
    log->AddEvent(LogLevel_Error, BackendReceivedStatus);
    return false;
  }

  int code = SendDataToBackend(i_data, i_len, F("image/jpeg"), F("Photo"), HOST_URL_CAM_PATH);
  ProcessBackendResult(code, SendPhoto);

  return (BackendResultOk == ProcessHttpResponseCodeClass(code));
}

/**
 * @brief seding device info to prusaconnect backend
 * 
 * @return bool - false when the info was not accepted because of a temporary problem (connection, server)
 */
bool PrusaConnect::SendInfoToBackend() {
  if (false == SendDeviceInformationToBackend) {
    return true;
  }

  log->AddEvent(LogLevel_Info, F("Start sending device information to prusaconnect"));

  /* https://camera-service.prusa3d.com/docs/guide/camera-info. Unknown keys are rejected, config is replaced as a whole */
  JsonDocument json_data;
  String json_string = "";

  String name = wifi->GetMdns();
  if (name.length() == 0) {
    name = F("ESP32 camera");
  }

  /* the Camera Service has no status field: a camera problem is shown as a part of the name, for example "camera [HOT]" */
  String suffix = (HealthStatus.length() > 0) ? (" [" + HealthStatus + "]") : "";

  JsonObject config = json_data["config"].to<JsonObject>();
  config["name"] = name.substring(0, CAMERA_SERVICE_NAME_MAX_LEN - suffix.length()) + suffix;
  config["trigger_scheme"] = TranslateRefreshIntervalToTriggerScheme(RefreshInterval);
  config["rotation"] = TranslateExifRotationToDegree(camera->GetCameraImageRotation());

  JsonObject resolution = config["resolution"].to<JsonObject>();
  resolution["width"] = camera->GetFrameSizeWidth();
  resolution["height"] = camera->GetFrameSizeHeight();

  JsonObject network_info = config["network_info"].to<JsonObject>();
  network_info["wifi_mac"] = wifi->GetWifiMac();
  network_info["wifi_ipv4"] = wifi->GetStaIp();
  network_info["wifi_ssid"] = wifi->GetStaSsid();

  JsonArray features = json_data["features"].to<JsonArray>();
  features.add("snapshot");

  JsonObject device = json_data["device"].to<JsonObject>();
  device["type"] = F("ESP32");
  device["model"] = String(BOARD_NAME) + " " + camera->GetDetectedSensorName();
  device["firmware_version"] = String(SW_VERSION).substring(0, CAMERA_SERVICE_FW_VERSION_MAX_LEN);
  device["hardware_version"] = String(BOARD_NAME).substring(0, CAMERA_SERVICE_HW_VERSION_MAX_LEN);

  serializeJson(json_data, json_string);
  log->AddEvent(LogLevel_Info, "Data: " + json_string);
  int code = SendDataToBackend((const uint8_t *)json_string.c_str(), json_string.length(), F("application/json"), F("Info"), HOST_URL_INFO_PATH);
  ProcessBackendResult(code, SendInfo);

  return (BackendResultTransient != ProcessHttpResponseCodeClass(code));
}

/**
 * @brief Take picture and send to backend
 *
 * @param none
 * @return none
 */
void PrusaConnect::TakePictureAndSendToBackend() {
  camera->CapturePhoto();

  /* check if photo was captured */
  CaptureFailed = (camera->GetCameraCaptureSuccess() == false);
  if (true == CaptureFailed) {
    log->AddEvent(LogLevel_Error, F("Error capturing photo. Stop sending to backend!"));
    BackendReceivedStatus = F("Photo: camera capture failed");
    UploadFailedCount++;
    return;
  }

  /* the upload works with its own copy of the photo, the camera can capture a new one in the meantime */
  uint8_t *photo = NULL;
  size_t photo_len = 0;
  if (false == camera->GetPhotoCopy(&photo, &photo_len)) {
    log->AddEvent(LogLevel_Error, F("Error copying photo. Stop sending to backend!"));
    UploadFailedCount++;
    return;
  }

  /* real size of a photo with these settings, for the photo size estimate on the WEB page */
  LastPhotoSize = photo_len;
  LastPhotoFrameSize = camera->GetFrameSize();
  LastPhotoQuality = camera->GetPhotoQuality();

  if (true == SendPhotoToBackend(photo, photo_len)) {
    UploadOkCount++;
  } else {
    UploadFailedCount++;
  }

  /* save photo to SD card */
  if (false == camera->GetStreamStatus()) {
    SavePhotoToSdCard(photo, photo_len);
  }

  free(photo);
}

/**
   @brief Function for processing http response code from prusa backend
   @param int - http response code
   @return none
*/
String PrusaConnect::ProcessHttpResponseCode(int code) {
  String ret = "";
  switch (code) {
    case 200:
      ret = F("200 - OK");
      break;
    case 201:
      ret = F("201 - OK entry created");
      break;
    case 204:
      ret = F("204 - Upload OK");
      break;
    case 304:
      ret = F("304 - Response has not been modified");
      break;
    case 400:
      ret = F("400 - Some data received is not valid");
      break;
    case 401:
      ret = F("401 - Missing token or fingerprint");
      break;
    case 403:
      ret = F("403 - Token is not valid, fingerprint mismatch, or the camera was not registered within 15 minutes");
      break;
    case 404:
      ret = F("404 - Entity not found or invalid auth token");
      break;
    case 409:
      ret = F("409 - Conflict with the state of target resource (user error)");
      break;
    case 411:
      ret = F("411 - Missing Content-Length");
      break;
    case 413:
      ret = F("413 - Photo is larger than 1 MB. Lower the resolution or quality");
      break;
    case 415:
      ret = F("415 - Unsupported content type");
      break;
    case 422:
      ret = F("422 - Validation error. Token must have 20 characters");
      break;
    case 429:
      ret = F("429 - Too many uploads. Waiting before next attempt");
      break;
    case 500:
      ret = F("500 - Server error. Try again later");
      break;
    case 503:
      ret = F("503 - Service is unavailable or rate limited. Try again later");
      break;
    default:
      ret = String(code);
      ret += F(" - unknown error code");
      break;
  }

  return ret;
}

/**
 * @brief Classify http response code, the Camera Service documentation says what to do with each code
 * 
 * @param code - http response code. 0 = no response
 * @return BackendResultClass - result class
 */
BackendResultClass PrusaConnect::ProcessHttpResponseCodeClass(int code) {
  if ((code >= 200) && (code < 300)) {
    return BackendResultOk;
  } else if (413 == code) {
    return BackendResultTooLarge;
  } else if ((code <= 0) || (408 == code) || (429 == code) || (code >= 500)) {
    return BackendResultTransient;
  }

  return BackendResultClientError;
}

/**
 * @brief Translate refresh interval to Camera Service trigger scheme
 *
 * @param uint16_t - interval [s]
 * @return String - trigger scheme
 */
String PrusaConnect::TranslateRefreshIntervalToTriggerScheme(uint16_t i_interval) {
  switch (i_interval) {
    case REFRESH_INTERVAL_TEN_SEC:
      return F("TEN_SEC");
    case REFRESH_INTERVAL_SIXTY_SEC:
      return F("SIXTY_SEC");
    case REFRESH_INTERVAL_TEN_MIN:
      return F("TEN_MIN");
    case REFRESH_INTERVAL_THIRTY_SEC:
    default:
      return F("THIRTY_SEC");
  }
}

/**
 * @brief Translate EXIF orientation to physical rotation in degrees
 *
 * @param uint8_t - EXIF orientation. 1 - 0°, 6 - 90°, 3 - 180°, 8 - 270°
 * @return int - rotation in degrees
 */
int PrusaConnect::TranslateExifRotationToDegree(uint8_t i_rotation) {
  switch (i_rotation) {
    case 6:
      return 90;
    case 3:
      return 180;
    case 8:
      return 270;
    default:
      return 0;
  }
}

/**
 * @brief Update device information. Camera info is sent again before the next photo
 *
 * @param none
 * @return none
 */
void PrusaConnect::UpdateDeviceInformation() {
  SendDeviceInformationToBackend = true;
}

/**
 * @brief Check if the refresh interval is one of the Camera Service trigger schemes
 *
 * @param uint16_t i_data - refresh interval [s]
 * @return bool - true if the interval is valid
 */
bool PrusaConnect::CheckRefreshInterval(uint16_t i_data) {
  return ((REFRESH_INTERVAL_TEN_SEC == i_data) || (REFRESH_INTERVAL_THIRTY_SEC == i_data) || (REFRESH_INTERVAL_SIXTY_SEC == i_data) || (REFRESH_INTERVAL_TEN_MIN == i_data));
}

/**
 * @brief Set refresh interval
 *
 * @param uint16_t i_data - refresh interval [s]
 * @return none
 */
void PrusaConnect::SetRefreshInterval(uint16_t i_data) {
  RefreshInterval = i_data;
  config->SaveRefreshInterval(RefreshInterval);
  UpdateDeviceInformation();
}

/**
 * @brief Set token
 *
 * @param String i_data - token
 * @return none
 */
void PrusaConnect::SetToken(String i_data) {
  Token = i_data;
  config->SaveToken(Token);

  /* new token: register the camera immediately, the first call must arrive within 15 minutes */
  BackoffInterval = 0;
  UpdateDeviceInformation();
  SetSendingIntervalExpired();
}

/**
 * @brief Set backend availability status
 *
 * @param BackendAvailabilitStatus - backend status
 * @return none
 */
void PrusaConnect::SetBackendAvailabilitStatus(BackendAvailabilitStatus i_data) {
  BackendAvailability = i_data;
}

/**
 * @brief set prusa connect hostname
 * 
 * @param String i_data - hostname
 */
void PrusaConnect::SetPrusaConnectHostname(String i_data) {
  PrusaConnectHostname = i_data;
  config->SavePrusaConnectHostname(PrusaConnectHostname);
  BackoffInterval = 0;
  UpdateDeviceInformation();
}

/**
 * @brief Set time laps photo save status
 * 
 * @param bool - status
 */
void PrusaConnect::SetTimeLapsPhotoSaveStatus(bool i_data) {
  EnableTimelapsPhotoSave = i_data;
  config->SaveTimeLapseFunctionStatus(EnableTimelapsPhotoSave);
}

/**
   @brief Function for saving photo to SD card
   @param uint8_t * - photo data with EXIF header
   @param size_t - photo length
   @return none
*/
void PrusaConnect::SavePhotoToSdCard(uint8_t *i_data, size_t i_len) {
#if (ENABLE_SD_CARD == true)
  /* check if time laps photo save is enabled */
  if (EnableTimelapsPhotoSave == true) {
    log->AddEvent(LogLevel_Info, F("Save TimeLaps photo to SD card"));
    
    /* check if SD card is detected */
    if (log->GetCardDetectedStatus() == false) {
      log->AddEvent(LogLevel_Error, F("SD card not detected!"));
      return;
    }

    /* check if folder for time laps photos exists */
    if (false == log->CheckDir(SD_MMC, TIMELAPS_PHOTO_FOLDER)) {
      log->AddEvent(LogLevel_Info, F("Create folder for TimeLaps photos"));
      log->CreateDir(SD_MMC, TIMELAPS_PHOTO_FOLDER);
    }

    /* create file name */
    String FileName = String(TIMELAPS_PHOTO_FOLDER) + "/" + String(TIMELAPS_PHOTO_PREFIX) + "_";
    FileName += log->GetSystemTime();
    FileName += TIMELAPS_PHOTO_SUFFIX;
    log->AddEvent(LogLevel_Verbose, F("Saving file: "), FileName);

    /* save photo to SD card */
    if (log->WritePicture(FileName, i_data, i_len) == true) {
      log->AddEvent(LogLevel_Info, F("Photo saved to SD card"));
    } else {
      log->AddEvent(LogLevel_Error, F("Error saving photo to SD card"));
    }
  }
#endif
}

/**
 * @brief Get refresh interval
 *
 * @param none
 * @return uint8_t - refresh interval
 */
uint16_t PrusaConnect::GetRefreshInterval() {
  return RefreshInterval;
}

/**
 * @brief Get the interval for the next upload. Backoff interval after an error, otherwise refresh interval
 *
 * @return uint16_t - interval [s]
 */
uint16_t PrusaConnect::GetEffectiveInterval() {
  return (BackoffInterval > RefreshInterval) ? BackoffInterval : RefreshInterval;
}

/**
 * @brief Get backoff interval
 *
 * @return uint16_t - backoff interval [s]. 0 = no backoff
 */
uint16_t PrusaConnect::GetBackoffInterval() {
  return BackoffInterval;
}

/**
 * @brief Get count of consecutive failed connections to the backend
 *
 * @return uint16_t - count
 */
uint16_t PrusaConnect::GetConnectFailCounter() {
  return ConnectFailCounter;
}

/**
 * @brief Clear count of consecutive failed connections to the backend
 */
void PrusaConnect::ClearConnectFailCounter() {
  ConnectFailCounter = 0;
}

/**
 * @brief get backend received status
 *
 * @param none
 * @return String - backend received status
 */
String PrusaConnect::GetBackendReceivedStatus() {
  return BackendReceivedStatus;
}

/**
 * @brief get token
 *
 * @param none
 * @return String - token
 */
String PrusaConnect::GetToken() {
  return Token;
}

/**
 * @brief get fingerprint
 *
 * @param none
 * @return String - fingerprint
 */
String PrusaConnect::GetFingerprint() {
  return Fingerprint;
}

/**
 * @brief get prusa connect hostname
 * 
 * @return String - hostanme
 */
String PrusaConnect::GetPrusaConnectHostname() {
  return PrusaConnectHostname;
}

/**
 * @brief Get backend availability status
 *
 * @param none
 * @return BackendAvailabilitStatus - backend status
 */
BackendAvailabilitStatus PrusaConnect::GetBackendAvailabilitStatus() {
  return BackendAvailability;
}

/** 
 * @brief Convert backend availability status to string
 * @param BackendAvailabilitStatus - backend status
 * @return String - backend status as string
*/
String PrusaConnect::CovertBackendAvailabilitStatusToString(BackendAvailabilitStatus i_data) {
  String ret = "";
  switch (i_data) {
    case WaitForFirstConnection:
      ret = F("Wait for first connection");
      break;
    case BackendAvailable:
      ret = F("Backend available");
      break;
    case BackendUnavailable:
      ret = F("Backend unavailable");
      break;
    default:
      ret = F("Unknown");
      break;
  }

  return ret;
}

bool PrusaConnect::GetTimeLapsPhotoSaveStatus() {
  return EnableTimelapsPhotoSave;
}

/**
 * @brief Set the camera problem shown in Prusa Connect. The camera information is sent again with the next photo
 *
 * @param String - short problem description, empty when everything is OK
 * @return none
 */
void PrusaConnect::SetHealthStatus(String i_data) {
  if (i_data != HealthStatus) {
    log->AddEvent(LogLevel_Warning, F("Camera health for Prusa Connect: "), (i_data.length() > 0) ? i_data : String(F("OK")));
    HealthStatus = i_data;
    SendDeviceInformationToBackend = true;
  }
}

uint32_t PrusaConnect::GetUploadOkCount() {
  return UploadOkCount;
}

uint32_t PrusaConnect::GetLastPhotoSize() {
  return LastPhotoSize;
}

uint8_t PrusaConnect::GetLastPhotoFrameSize() {
  return LastPhotoFrameSize;
}

uint8_t PrusaConnect::GetLastPhotoQuality() {
  return LastPhotoQuality;
}

uint32_t PrusaConnect::GetUploadFailedCount() {
  return UploadFailedCount;
}

bool PrusaConnect::GetCaptureFailed() {
  return CaptureFailed;
}

String PrusaConnect::GetHealthStatus() {
  return HealthStatus;
}

/**
 * @brief Increase sending interval counter
 *
 * @param none
 * @return none
 */
void PrusaConnect::IncreaseSendingIntervalCounter() {
  if (SendingIntervalCounter < UINT16_MAX) {
    SendingIntervalCounter++;
  }
}

/**
 * @brief Set sending interval counter
 *
 * @param uint16_t i_data - counter
 * @return none
 */
void PrusaConnect::SetSendingIntervalCounter(uint16_t i_data) {
  SendingIntervalCounter = i_data;
}

void PrusaConnect::SetSendingIntervalExpired() {
  SendingIntervalCounter = UINT16_MAX;
}

/**
 * @brief Get sending interval counter
 * 
 * @return uint16_t - counter
 */
uint16_t PrusaConnect::GetSendingIntervalCounter() {
  return SendingIntervalCounter;
}

/**
 * @brief Check if sending interval is expired. and can I send the data to the backend. [seconds]
 * 
 * @param uint16_t - interval [s]
 * @return true 
 * @return false 
 */
bool PrusaConnect::CheckSendingIntervalExpired(uint16_t i_interval) {
  bool ret = false;
  if (SendingIntervalCounter >= i_interval) {
    ret = true;
  }

  return ret;
}

/* EOF */