/**
   @file camera.cpp

   @brief Library for working with a camera

   @author Miroslav Pivovarsky
   Contact: miroslav.pivovarsky@gmail.com

   https://github.com/espressif/esp32-camera


   @bug: no know bug
*/
#include "camera.h"

Camera SystemCamera(&SystemConfig, &SystemLog, FLASH_GPIO_NUM);

/**
   @brief Init camera constructor
   @param Configuration* - pointer to Configuration class
   @param Logs* - pointer to Logs class
   @param uint8_t - flash pin
   @return none
*/
Camera::Camera(Configuration* i_conf, Logs* i_log, int8_t i_FlashPin) {
  config = i_conf;
  log = i_log;

  CameraFlashPin = i_FlashPin;
  StreamOnOff = false;
  frameBufferSemaphore = xSemaphoreCreateMutex();
  streamMutex = xSemaphoreCreateMutex();

  /* stream buffers are allocated by the capture task, when a stream client connects */
  for (uint8_t i = 0; i < CAMERA_STREAM_BUFFERS; i++) {
    StreamBuf[i] = NULL;
    StreamBufLen[i] = 0;
    StreamBufCapacity[i] = 0;
    StreamBufReaders[i] = 0;
  }
  StreamLatest = -1;
  StreamLatestConsumed = false;
  StreamLatestTime = 0;
  StreamFrameSeq = 0;
  StreamClients = 0;
  StreamTask = NULL;
  FrameBuffer = NULL;
  CameraCaptureSuccess = false;
  sensor = NULL;
  CameraType = (camera_pid_t)0;
  CameraName = F("Unknown");
  SensorMaxFrameSize = CAMERA_UNKNOWN_SENSOR_MAX_FRAMESIZE;
  AutofocusStatus = F("Not supported by the camera sensor");

  PhotoExifData.header = NULL;
  PhotoExifData.len = 0;
  PhotoExifData.offset = 0;
  CameraCaptureFailedCounter = 0;
}

/**
   @brief Init the camera module and set the camera configuration
   @param none
   @return none
*/
void Camera::Init() {
  log->AddEvent(LogLevel_Info, F("Init camera lib"));

#if (true == ENABLE_CAMERA_FLASH)
  log->AddEvent(LogLevel_Info, F("Init GPIO"));
#if (true == CAMERA_FLASH_PWM_CTRL)
  ledcAttach(FLASH_GPIO_NUM, FLASH_PWM_FREQ, FLASH_PWM_RESOLUTION);

#elif (true == CAMERA_FLASH_DIGITAL_CTRL)
  pinMode(FLASH_GPIO_NUM, OUTPUT);
  digitalWrite(FLASH_GPIO_NUM, LOW);
#endif
  SetFlashStatus(false);
#endif

  /* Init with the sensor saved on a previous start (XCLK and frame size limit). The driver reads the sensor ID
     on every init anyway, so the saved sensor is always checked. A frame size above the sensor maximum
     is limited by the driver, a wrong saved sensor cannot cause a failed init */
  camera_pid_t SavedType = CameraType;
  if ((camera_pid_t)0 != SavedType) {
    CheckFrameSizeLimit();
  }
  InitCameraModule(TFrameSize);
  GetCameraModel();
  CheckFrameSizeLimit();

  if (TransformPidToCameraSensor(CameraType) != TransformPidToCameraSensor(SavedType)) {
    /* first start, or the camera module was changed: save the sensor, the next start needs only one init */
    log->AddEvent(LogLevel_Warning, "New camera sensor detected: " + CameraName);
    config->SaveCameraSensor(TransformPidToCameraSensor(CameraType));

    if (GetSensorXclk() != (uint32_t)CameraConfig.xclk_freq_hz) {
      log->AddEvent(LogLevel_Info, F("Init camera module again with the XCLK of the detected sensor"));
      esp_camera_deinit();
      delay(100);
      InitCameraModule(TFrameSize);
    }
  }

  ApplyCameraCfg();
  InitAutofocus();
}

/**
   @brief Load the autofocus firmware to the OV5640 and start continuous autofocus.
          The sensor loses the firmware on every camera init, so it is loaded after every init.
          OV2640 and OV3660 have a fixed focus lens.
   @param none
   @return none
*/
void Camera::InitAutofocus() {
  if ((OV5640_PID != CameraType) || (sensor == NULL)) {
    AutofocusStatus = F("Not supported by the camera sensor");
    return;
  }

  /* the library checks the chip ID, an OV5640 module without the focus motor has the same ID */
  if (false == Ov5640Autofocus.start(sensor)) {
    AutofocusStatus = F("Failed: OV5640 not found");
    log->AddEvent(LogLevel_Error, AutofocusStatus);
    return;
  }

  log->AddEvent(LogLevel_Info, F("Load OV5640 autofocus firmware"));
  uint32_t start = millis();
  uint8_t ret = Ov5640Autofocus.focusInit();
  if (0 != ret) {
    AutofocusStatus = "Failed: autofocus firmware not started (" + String(ret) + ")";
    log->AddEvent(LogLevel_Error, AutofocusStatus);
    return;
  }

  ret = Ov5640Autofocus.autoFocusMode();
  if (0 != ret) {
    AutofocusStatus = "Failed: continuous autofocus not started (" + String(ret) + ")";
    log->AddEvent(LogLevel_Error, AutofocusStatus);
    return;
  }

  AutofocusStatus = F("Continuous autofocus");
  log->AddEvent(LogLevel_Info, "OV5640 continuous autofocus started in " + String(millis() - start) + " ms");
}

/**
   @brief Get autofocus status for the WEB page. Reads the focus state from the OV5640
   @param none
   @return String - status
*/
String Camera::GetAutofocusStatus() {
  if ((OV5640_PID != CameraType) || (false == AutofocusStatus.startsWith(F("Continuous")))) {
    return AutofocusStatus;
  }

  /* the sensor must not be read during a camera reinit. When the camera is busy, return the last status */
  if (!xSemaphoreTake(frameBufferSemaphore, pdMS_TO_TICKS(100))) {
    return AutofocusStatus;
  }

  /* FW_STATUS: 0x10 focused, 0x00-0x0F and 0x80-0x8F focusing, 0x70 idle */
  uint8_t state = Ov5640Autofocus.getFWStatus();
  xSemaphoreGive(frameBufferSemaphore);
  String ret = AutofocusStatus;
  if (FW_STATUS_S_FOCUSED == state) {
    ret += F(" - focused");
  } else if ((state <= 0x0F) || ((state >= 0x80) && (state <= 0x8F))) {
    ret += F(" - focusing");
  } else if (FW_STATUS_S_IDLE == state) {
    ret += F(" - idle");
  }
  return ret;
}

/**
   @brief Init the camera module
   @param none
   @return none
*/
void Camera::InitCameraModule() {
  InitCameraModule(TFrameSize);
}

/**
   @brief Init the camera module with a frame size
   @param framesize_t - frame size
   @return none
*/
void Camera::InitCameraModule(framesize_t i_frameSize) {
  log->AddEvent(LogLevel_Info, F("Init camera module"));
  /* Turn-off the 'brownout detector' */
#if (true == ENABLE_BROWN_OUT_DETECTION)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
#endif
  esp_err_t err;

  CameraConfig.ledc_channel = LEDC_CHANNEL_0;
  CameraConfig.ledc_timer = LEDC_TIMER_0;
  CameraConfig.pin_d0 = Y2_GPIO_NUM;
  CameraConfig.pin_d1 = Y3_GPIO_NUM;
  CameraConfig.pin_d2 = Y4_GPIO_NUM;
  CameraConfig.pin_d3 = Y5_GPIO_NUM;
  CameraConfig.pin_d4 = Y6_GPIO_NUM;
  CameraConfig.pin_d5 = Y7_GPIO_NUM;
  CameraConfig.pin_d6 = Y8_GPIO_NUM;
  CameraConfig.pin_d7 = Y9_GPIO_NUM;
  CameraConfig.pin_xclk = XCLK_GPIO_NUM;
  CameraConfig.pin_pclk = PCLK_GPIO_NUM;
  CameraConfig.pin_vsync = VSYNC_GPIO_NUM;
  CameraConfig.pin_href = HREF_GPIO_NUM;
  CameraConfig.pin_sccb_sda = SIOD_GPIO_NUM;
  CameraConfig.pin_sccb_scl = SIOC_GPIO_NUM;
  CameraConfig.pin_pwdn = PWDN_GPIO_NUM;
  CameraConfig.pin_reset = RESET_GPIO_NUM;
  CameraConfig.xclk_freq_hz = GetSensorXclk();
  CameraConfig.pixel_format = PIXFORMAT_JPEG; /* YUV422,GRAYSCALE,RGB565,JPEG */

  /* OV2640
    FRAMESIZE_QVGA (320 x 240)
    FRAMESIZE_CIF (352 x 288)
    FRAMESIZE_VGA (640 x 480)
    FRAMESIZE_SVGA (800 x 600)
    FRAMESIZE_XGA (1024 x 768)
    FRAMESIZE_SXGA (1280 x 1024)
    FRAMESIZE_UXGA (1600 x 1200)

    CAMERA_GRAB_WHEN_EMPTY - Fills buffers when they are empty. Less resources but first 'fb_count' frames might be old
    CAMERA_GRAB_LATEST     - Except when 1 frame buffer is used, queue will always contain the last 'fb_count' frames
  */

  CameraConfig.frame_size = i_frameSize;            /* FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA|HD|FHD|QXGA|5MP */
  CameraConfig.jpeg_quality = PhotoQuality;         /* 10-63 lower number means higher quality */
  CameraConfig.fb_count = 1;                        /* picture frame buffer alocation */
  CameraConfig.grab_mode = CAMERA_GRAB_LATEST;      /* CAMERA_GRAB_WHEN_EMPTY or CAMERA_GRAB_LATEST */
#if (true == ENABLE_PSRAM)
  CameraConfig.fb_location = CAMERA_FB_IN_PSRAM;    /* CAMERA_FB_IN_PSRAM or CAMERA_FB_IN_DRAM  */
#else
  CameraConfig.fb_location = CAMERA_FB_IN_DRAM;     /* CAMERA_FB_IN_PSRAM or CAMERA_FB_IN_DRAM  */
#endif
  
  if (CameraConfig.fb_location == CAMERA_FB_IN_DRAM) {
    log->AddEvent(LogLevel_Verbose, F("Camera frame buffer location: DRAM"));
  } else if (CameraConfig.fb_location == CAMERA_FB_IN_PSRAM) {
    log->AddEvent(LogLevel_Verbose, F("Camera frame buffer location: PSRAM"));
  } else {
    log->AddEvent(LogLevel_Verbose, F("Camera frame buffer location: Unknown"));
  }

  /* Camera init */
  err = esp_camera_init(&CameraConfig);
  if (err != ESP_OK) {
    log->AddEvent(LogLevel_Warning, F("Camera init failed. Error: "), String(err, HEX));
    log->AddEvent(LogLevel_Warning, F("Reset ESP32-cam!"));
    System_RestartMcu(SystemRestart_CameraInit);
  }
  sensor = esp_camera_sensor_get();
}

/**
   @brief Load camera CFG from EEPROM
   @param none
   @return none
*/
void Camera::LoadCameraCfgFromEeprom() {
  log->AddEvent(LogLevel_Info, F("Load camera CFG from EEPROM"));
  PhotoQuality = config->LoadPhotoQuality();
  FrameSize = config->LoadFrameSize();
  TFrameSize = TransformFrameSizeDataType(config->LoadFrameSize());
  brightness = config->LoadBrightness();
  contrast = config->LoadContrast();
  saturation = config->LoadSaturation();
  awb = config->LoadAwb();
  awb_gain = config->LoadAwbGain();
  wb_mode = config->LoadAwbMode();
  aec2 = config->LoadAec2();
  ae_level = config->LoadAeLevel();
  aec_value = config->LoadAecValue();
  gain_ctrl = config->LoadGainCtrl();
  agc_gain = config->LoadAgcGain();
  bpc = config->LoadBpc();
  wpc = config->LoadWpc();
  raw_gama = config->LoadRawGama();
  hmirror = config->LoadHmirror();
  vflip = config->LoadVflip();
  lensc = config->LoadLensCorrect();
  exposure_ctrl = config->LoadExposureCtrl();
  CameraFlashEnable = config->LoadCameraFlashEnable();
  CameraFlashTime = config->LoadCameraFlashTime();
  imageExifRotation = config->LoadCameraImageExifRotation();

  /* sensor detected on a previous start */
  CameraType = TransformCameraSensorToPid(config->LoadCameraSensor());
  if ((camera_pid_t)0 != CameraType) {
    sensor_id_t id = {};
    id.PID = CameraType;
    camera_sensor_info_t* info = esp_camera_sensor_get_info(&id);
    if (info != NULL) {
      CameraName = info->name;
      SensorMaxFrameSize = info->max_size;
    }
  }
}

/**
   @brief Translate the saved camera sensor code to the sensor PID
   @param uint8_t - CAMERA_SENSOR_xxx
   @return camera_pid_t - PID, 0 when unknown
*/
camera_pid_t Camera::TransformCameraSensorToPid(uint8_t i_data) {
  switch (i_data) {
    case CAMERA_SENSOR_OV2640:
      return OV2640_PID;
    case CAMERA_SENSOR_OV3660:
      return OV3660_PID;
    case CAMERA_SENSOR_OV5640:
      return OV5640_PID;
    default:
      return (camera_pid_t)0;
  }
}

/**
   @brief Translate the sensor PID to the camera sensor code saved in the EEPROM
   @param camera_pid_t - PID
   @return uint8_t - CAMERA_SENSOR_xxx, CAMERA_SENSOR_UNKNOWN for other sensors
*/
uint8_t Camera::TransformPidToCameraSensor(camera_pid_t i_data) {
  switch (i_data) {
    case OV2640_PID:
      return CAMERA_SENSOR_OV2640;
    case OV3660_PID:
      return CAMERA_SENSOR_OV3660;
    case OV5640_PID:
      return CAMERA_SENSOR_OV5640;
    default:
      return CAMERA_SENSOR_UNKNOWN;
  }
}

/**
   @brief Frame sizes selectable on the WEB page. The index is saved in the EEPROM, never change the order, only append.
          The sensor decides which frame sizes are available: OV2640 up to UXGA, OV3660 up to QXGA, OV5640 up to 5MP
*/
static const framesize_t FrameSizeTable[] = {
  FRAMESIZE_QVGA,   /* 0 - 320x240 */
  FRAMESIZE_CIF,    /* 1 - 400x296 */
  FRAMESIZE_VGA,    /* 2 - 640x480 */
  FRAMESIZE_SVGA,   /* 3 - 800x600 */
  FRAMESIZE_XGA,    /* 4 - 1024x768 */
  FRAMESIZE_SXGA,   /* 5 - 1280x1024 */
  FRAMESIZE_UXGA,   /* 6 - 1600x1200 */
  FRAMESIZE_HD,     /* 7 - 1280x720 */
  FRAMESIZE_FHD,    /* 8 - 1920x1080 */
  FRAMESIZE_QXGA,   /* 9 - 2048x1536 */
  FRAMESIZE_5MP,    /* 10 - 2592x1944 */
};
static const uint8_t FrameSizeTableCount = sizeof(FrameSizeTable) / sizeof(FrameSizeTable[0]);

/**
   @brief transform uint8_t from web interface to framesize_t
   @param uint8_t - int value from web
   @return framesize_t
*/
framesize_t Camera::TransformFrameSizeDataType(uint8_t i_data) {
  if (i_data < FrameSizeTableCount) {
    return FrameSizeTable[i_data];
  }

  log->AddEvent(LogLevel_Warning, F("Bad frame size. Set default value. "), String(i_data));
  return FRAMESIZE_QVGA;
}

/**
   @brief Check if the detected sensor supports the frame size
   @param uint8_t - frame size index from web
   @return bool - true if supported
*/
bool Camera::CheckFrameSizeSupported(uint8_t i_data) {
  return ((i_data < FrameSizeTableCount) && (FrameSizeTable[i_data] <= SensorMaxFrameSize));
}

/**
   @brief List of frame size indexes supported by the detected sensor, for the WEB page
   @param none
   @return String - comma separated indexes, for example "0,1,2,3,4,5,6,7"
*/
String Camera::GetSupportedFrameSizes() {
  String ret = "";
  for (uint8_t i = 0; i < FrameSizeTableCount; i++) {
    if (CheckFrameSizeSupported(i)) {
      if (ret.length() > 0) {
        ret += ",";
      }
      ret += String(i);
    }
  }

  return ret;
}

/**
   @brief Limit the saved frame size to the maximum of the detected sensor, for example after a camera module change
   @param none
   @return none
*/
void Camera::CheckFrameSizeLimit() {
  if (CheckFrameSizeSupported(FrameSize)) {
    return;
  }

  /* use the largest supported frame size */
  uint8_t best = 0;
  for (uint8_t i = 0; i < FrameSizeTableCount; i++) {
    if (CheckFrameSizeSupported(i) && (FrameSizeTable[i] > FrameSizeTable[best])) {
      best = i;
    }
  }

  log->AddEvent(LogLevel_Warning, "Frame size " + String(FrameSize) + " is not supported by " + CameraName + ". New frame size: " + String(best));
  FrameSize = best;
  TFrameSize = FrameSizeTable[best];
  config->SaveFrameSize(best);
}

/**
   @brief Get XCLK for the detected camera sensor. Before the detection the OV2640 value is used
   @param none
   @return uint32_t - XCLK [Hz]
*/
uint32_t Camera::GetSensorXclk() {
  switch (CameraType) {
    case OV3660_PID:
      return CAMERA_XCLK_OV3660;
    case OV5640_PID:
      return CAMERA_XCLK_OV5640;
    case OV2640_PID:
    default:
      return CAMERA_XCLK_OV2640;
  }
}

/**
   @brief Get the name of the detected camera sensor
   @param none
   @return String - name, for example OV5640
*/
String Camera::GetDetectedSensorName() {
  return CameraName;
}

/**
   @brief Function set flash status
   @param bool i_data - true = on, false = off
   @return none
*/
void Camera::SetFlashStatus(bool i_data) {
#if (true == ENABLE_CAMERA_FLASH)
/* PWM control of the FLASH */
#if (true == CAMERA_FLASH_PWM_CTRL)
  if (true == i_data) {
    ledcWrite(FLASH_GPIO_NUM, FLASH_ON_STATUS);
  } else if (false == i_data) {
    ledcWrite(FLASH_GPIO_NUM, FLASH_OFF_STATUS);
  }

/* Digital control of the FLASH */
#elif (true == CAMERA_FLASH_DIGITAL_CTRL)
  if (true == i_data) {
    digitalWrite(FLASH_GPIO_NUM, HIGH);
  } else if (false == i_data) {
    digitalWrite(FLASH_GPIO_NUM, LOW);
  }
#endif

/* rgbLedWrite control of the FLASH */
#if (true == CAMERA_FLASH_NEOPIXEL)
  if (true == i_data) {
    rgbLedWrite(FLASH_NEOPIXEL_LED_PIN, RGB_BRIGHTNESS, RGB_BRIGHTNESS, RGB_BRIGHTNESS);
  } else if (false == i_data) {
    rgbLedWrite(FLASH_NEOPIXEL_LED_PIN, 0, 0, 0);
  }
#endif
#endif
}

/**
   @brief Function get flash status
   @param none
   @return bool - true = on, false = off
*/
bool Camera::GetFlashStatus() {
#if (true == ENABLE_CAMERA_FLASH)
#if (true == CAMERA_FLASH_PWM_CTRL)
  if (ledcRead(FLASH_GPIO_NUM) == FLASH_OFF_STATUS) {
    return false;
  } else if (ledcRead(FLASH_GPIO_NUM) == FLASH_ON_STATUS) {
    return true;
  }

#elif (true == CAMERA_FLASH_DIGITAL_CTRL)
  if (digitalRead(FLASH_GPIO_NUM) == LOW) {
    return false;
  } else if (digitalRead(FLASH_GPIO_NUM) == HIGH) {
    return true;
  }
#endif
#endif
  return false;
}

/**
   @brief Function set camer acfg
   @param none
   @return none
*/
void Camera::ApplyCameraCfg() {
  log->AddEvent(LogLevel_Info, F("Set camera CFG"));

  /* sensor configuration */
  sensor = esp_camera_sensor_get();
  if (sensor == NULL) {
    log->AddEvent(LogLevel_Error, F("Camera sensor is NULL"));
    return;
  }

/* not every sensor driver implements every setting, skip the missing ones */
#define CAMERA_SENSOR_SET(function, value) \
  if (sensor->function != NULL) { \
    sensor->function(sensor, value); \
  }

  CAMERA_SENSOR_SET(set_brightness, brightness);         // -2 to 2
  CAMERA_SENSOR_SET(set_contrast, contrast);             // -2 to 2
  CAMERA_SENSOR_SET(set_saturation, saturation);         // -2 to 2
  CAMERA_SENSOR_SET(set_special_effect, 0);              // 0 to 6 (0 - No Effect, 1 - Negative, 2 - Grayscale, 3 - Red Tint, 4 - Green Tint, 5 - Blue Tint, 6 - Sepia)
  CAMERA_SENSOR_SET(set_whitebal, awb);                  // automatic white balancing 0 = disable , 1 = enable
  CAMERA_SENSOR_SET(set_awb_gain, awb_gain);             // automatic white balancing gain 0 = disable , 1 = enable
  CAMERA_SENSOR_SET(set_wb_mode, wb_mode);               // white balancing mode 0 to 4 - if awb_gain enabled (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
  CAMERA_SENSOR_SET(set_exposure_ctrl, exposure_ctrl);   // exposition controll 0 = disable , 1 = enable
  CAMERA_SENSOR_SET(set_aec2, aec2);                     // enable exposition controll 0 = disable , 1 = enable
  CAMERA_SENSOR_SET(set_ae_level, ae_level);             // automatic exposition level -2 to 2
  CAMERA_SENSOR_SET(set_aec_value, aec_value);           // expozition time - 0 to 1200
  CAMERA_SENSOR_SET(set_gain_ctrl, gain_ctrl);           // automatic gain controll 0 = disable , 1 = enable
  CAMERA_SENSOR_SET(set_agc_gain, agc_gain);             // automatic gain controll level 0 to 30
  CAMERA_SENSOR_SET(set_gainceiling, (gainceiling_t)0);  // maximum gain 0 to 6
  CAMERA_SENSOR_SET(set_bpc, bpc);                       // bad pixel correction 0 = disable , 1 = enable
  CAMERA_SENSOR_SET(set_wpc, wpc);                       // white pixel correction 0 = disable , 1 = enable
  CAMERA_SENSOR_SET(set_raw_gma, raw_gama);              // raw gama correction 0 = disable , 1 = enable
  CAMERA_SENSOR_SET(set_lenc, lensc);                    // lens correction 0 = disable , 1 = enable
  CAMERA_SENSOR_SET(set_hmirror, hmirror);               // horizontal mirror 0 = disable , 1 = enable
  CAMERA_SENSOR_SET(set_vflip, vflip);                   // vertical flip 0 = disable , 1 = enable
  CAMERA_SENSOR_SET(set_dcw, 1);                         // 0 = disable , 1 = enable
  CAMERA_SENSOR_SET(set_colorbar, 0);                    // external collor lines, 0 = disable , 1 = enable

#undef CAMERA_SENSOR_SET
}

/**
   @brief Function for reinit camera module
   @param none
   @return none
*/
void Camera::ReinitCameraModule() {
  /* the web server and the photo task can both reinit the camera, never deinit during a capture */
  bool locked = xSemaphoreTake(frameBufferSemaphore, pdMS_TO_TICKS(CAMERA_SEMAPHORE_TIMEOUT));
  if (!locked) {
    log->AddEvent(LogLevel_Warning, F("Camera reinit without frame buffer semaphore"));
  }

  /* deinit frees all frame buffers, the held one must not be returned later */
  FrameBuffer = NULL;
  CameraCaptureSuccess = false;
  esp_err_t err = esp_camera_deinit();
  if (err != ESP_OK) {
    log->AddEvent(LogLevel_Warning, F("Camera error deinit camera module. Error: "), String(err, HEX));
  }
  delay(100);
  InitCameraModule();
  ApplyCameraCfg();
  InitAutofocus();

  if (locked) {
    xSemaphoreGive(frameBufferSemaphore);
  }
}

/**
   @brief Function for get camera model and type
   @param void
   @return none
*/
void Camera::GetCameraModel() {
  log->AddEvent(LogLevel_Info, F("Get camera model and type"));
  if (sensor == NULL) {
    log->AddEvent(LogLevel_Error, F("Camera sensor is NULL"));
    return;
  } 
  
  camera_sensor_info_t *info = esp_camera_sensor_get_info(&sensor->id);
  if (info == NULL) {
    log->AddEvent(LogLevel_Error, F("Camera sensor info is NULL"));
    return;
  }

  CameraType = (camera_pid_t) sensor->id.PID;
  CameraName = info->name;
  SensorMaxFrameSize = info->max_size;
  update_exif_camera_model(info->name);
  log->AddEvent(LogLevel_Info, F("Camera type: "), String(CameraType));
  log->AddEvent(LogLevel_Info, F("Camera name: "), String(CameraName));
  log->AddEvent(LogLevel_Info, F("Camera model: "), String(info->model));
  log->AddEvent(LogLevel_Info, F("Camera PID: "), String(info->pid));
  log->AddEvent(LogLevel_Info, F("Camera MAX framesize: "), String(info->max_size));
  log->AddEvent(LogLevel_Info, F("Camera support jpeg: "), String(info->support_jpeg));
}

/**
   @brief Capture Photo and Save it to string array
   @param none
   @return none
*/
void Camera::CapturePhoto() {
  if (!xSemaphoreTake(frameBufferSemaphore, pdMS_TO_TICKS(CAMERA_SEMAPHORE_TIMEOUT))) {
    log->AddEvent(LogLevel_Error, F("Failed to take frame buffer semaphore"));
    return;
  }

  /* Stream is on: the photo for Prusa Connect is the latest stream frame, the camera is used by the stream capture task */
  if (true == StreamOnOff) {
    xSemaphoreGive(frameBufferSemaphore);

    /* wait for the first frame after the stream start */
    uint32_t start = millis();
    while ((StreamLatest < 0) && ((millis() - start) < CAMERA_STREAM_FRAME_TIMEOUT)) {
      delay(10);
    }
    CameraCaptureSuccess = (StreamLatest >= 0);
    return;
  }

  {
    CameraCaptureSuccess = false;
    /* check flash, and enable FLASH LED */
    if (true == CameraFlashEnable) {
      SetFlashStatus(true);
      delay(CameraFlashTime);
    }

    if (FrameBuffer) {
      esp_camera_fb_return(FrameBuffer);
      FrameBuffer = NULL;
    }

    /* Capturing a training photo. Without this sequence, the camera will not obtain the current photo but photo from the previous cycle. */
    camera_fb_t* TrainingFrame = esp_camera_fb_get();
    if (TrainingFrame) {
      esp_camera_fb_return(TrainingFrame);
      log->AddEvent(LogLevel_Verbose, F("Camera capture training photo"));
    } else {
      log->AddEvent(LogLevel_Error, F("Camera capture failed training photo"));
    }

    const int maxAttempts = 5;
    PhotoExifData.header = NULL;
    for (int attempts = 0; attempts < maxAttempts; attempts++) {
      log->AddEvent(LogLevel_Verbose, F("Taking photo..."));

      FrameBuffer = esp_camera_fb_get();
      if (!FrameBuffer) {
        CameraCaptureFailedCounter++;
        log->AddEvent(LogLevel_Error, F("Camera capture failed! photo. Attempt: "), String(CameraCaptureFailedCounter));
        break;
      }

      char buf[150] = { '\0' };
      uint8_t ControlFlag = (FrameBuffer->len > 15) ? (uint8_t)FrameBuffer->buf[15] : 0xFF;
      sprintf(buf, "The picture has been saved. Size: %d bytes, Photo resolution: %zu x %zu", FrameBuffer->len, FrameBuffer->width, FrameBuffer->height);
      log->AddEvent(LogLevel_Verbose, buf);

      if ((ControlFlag != 0x00) || (FrameBuffer->len <= 100)) {
        log->AddEvent(LogLevel_Error, "Camera capture failed! flag: " + String(ControlFlag, HEX));
        /* return the bad frame, otherwise the next esp_camera_fb_get() waits for a free buffer and times out */
        esp_camera_fb_return(FrameBuffer);
        FrameBuffer = NULL;
        continue;
      }

      log->AddEvent(LogLevel_Verbose, F("Photo OK! "), String(ControlFlag, HEX));
      CameraCaptureFailedCounter = 0;

      /* generate exif header */
      update_exif_from_cfg(imageExifRotation);
      get_exif_header(FrameBuffer, &PhotoExifData.header, &PhotoExifData.len);
      PhotoExifData.offset = get_jpeg_data_offset(FrameBuffer);
      CameraCaptureSuccess = true;

      if (PhotoExifData.header != NULL) {
        log->AddEvent(LogLevel_Verbose, F("Exif header OK! Len: "), String(PhotoExifData.len));
      } else {
        log->AddEvent(LogLevel_Error, F("Exif header failed! "), String(PhotoExifData.len));
      }
      break;
    }

    if (false == CameraCaptureSuccess) {
      log->AddEvent(LogLevel_Error, F("Failed to capture a valid photo"));
    }

    /* Disable flash, also when the capture failed */
    if (true == CameraFlashEnable) {
      SetFlashStatus(false);
    }
  }
  xSemaphoreGive(frameBufferSemaphore);

  /* Reinit camera module if photo capture failed */
  if (CameraCaptureFailedCounter > CAMERA_MAX_FAIL_CAPTURE) { 
    log->AddEvent(LogLevel_Error, F("Camera capture failed! photo max attempts"));
    CameraCaptureFailedCounter = 0;
    ReinitCameraModule();
  }
}

/**
   @brief Start the stream capture task. Call it once, after the camera init
   @param none
   @return none
*/
void Camera::StartStreamTask() {
  if (NULL == StreamTask) {
    /* core 1: the WiFi stack runs on core 0 */
    xTaskCreatePinnedToCore(Camera::StreamTaskEntry, "StreamCapture", CAMERA_STREAM_TASK_STACK, this, 2, &StreamTask, 1);
  }
}

/**
   @brief Stream capture task entry point
   @param void * - pointer to the Camera object
   @return none
*/
void Camera::StreamTaskEntry(void *i_param) {
  static_cast<Camera *>(i_param)->StreamTaskLoop();
}

/**
   @brief Stream capture task. Captures frames only while a stream client is connected
   @param none
   @return none
*/
void Camera::StreamTaskLoop() {
  log->AddEvent(LogLevel_Info, F("Stream capture task. core: "), String(xPortGetCoreID()));
  esp_task_wdt_add(NULL);

  while (true) {
    esp_task_wdt_reset();

    if (0 == StreamClients) {
      /* no client: release the PSRAM of the stream buffers that nobody reads */
      xSemaphoreTake(streamMutex, portMAX_DELAY);
      for (uint8_t i = 0; i < CAMERA_STREAM_BUFFERS; i++) {
        if ((NULL != StreamBuf[i]) && (0 == StreamBufReaders[i])) {
          free(StreamBuf[i]);
          StreamBuf[i] = NULL;
          StreamBufCapacity[i] = 0;
          StreamBufLen[i] = 0;
        }
      }
      StreamLatest = -1;
      xSemaphoreGive(streamMutex);

      /* woken up by SetStreamStatus when a client connects */
      ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
      continue;
    }

    if (false == StreamCaptureFrame()) {
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

/**
   @brief Capture one stream frame into a free stream buffer.
          A new frame is captured when the latest frame was taken by a client, or when it is older than
          CAMERA_STREAM_MAX_FRAME_AGE, so the camera does not work faster than the clients receive the frames
   @param none
   @return bool - false when the capture failed
*/
bool Camera::StreamCaptureFrame() {
  /* find a free buffer: not the latest frame and not being sent */
  int8_t w = -1;
  xSemaphoreTake(streamMutex, portMAX_DELAY);
  bool wait = (StreamLatest >= 0) && (false == StreamLatestConsumed) && ((millis() - StreamLatestTime) < CAMERA_STREAM_MAX_FRAME_AGE);
  if (false == wait) {
    for (uint8_t i = 0; i < CAMERA_STREAM_BUFFERS; i++) {
      if ((i != StreamLatest) && (0 == StreamBufReaders[i])) {
        w = i;
        break;
      }
    }
  }
  xSemaphoreGive(streamMutex);

  if (w < 0) {
    vTaskDelay(pdMS_TO_TICKS(2));
    return true;
  }

  if (!xSemaphoreTake(frameBufferSemaphore, pdMS_TO_TICKS(CAMERA_SEMAPHORE_TIMEOUT))) {
    log->AddEvent(LogLevel_Error, F("Stream: failed to take frame buffer semaphore"));
    return false;
  }

  /* the stream may have ended while waiting for the semaphore */
  if (false == StreamOnOff) {
    xSemaphoreGive(frameBufferSemaphore);
    return true;
  }

  camera_fb_t* fb = esp_camera_fb_get();
  if ((NULL == fb) || (fb->len <= 100)) {
    if (NULL != fb) {
      esp_camera_fb_return(fb);
    }
    xSemaphoreGive(frameBufferSemaphore);
    log->AddEvent(LogLevel_Error, F("Camera capture failed! stream"));
    return false;
  }

  /* optional EXIF header with the image rotation */
  const uint8_t* Header = NULL;
  size_t HeaderLen = 0;
  size_t Offset = 0;
#if (true == CAMERA_EXIF_ROTATION_STREAM)
  if (1 != imageExifRotation) { /* 1 = image rotation 0 degree */
    update_exif_from_cfg(imageExifRotation);
    get_exif_header(fb, &Header, &HeaderLen);
    Offset = get_jpeg_data_offset(fb);
    if ((NULL == Header) || (Offset >= fb->len)) {
      Header = NULL;
      HeaderLen = 0;
      Offset = 0;
    }
  }
#endif

  /* the buffer grows when a frame is larger, it is not allocated for every frame */
  size_t Total = HeaderLen + fb->len - Offset;
  if (StreamBufCapacity[w] < Total) {
    free(StreamBuf[w]);
    size_t Capacity = Total + (Total / 4);
    StreamBuf[w] = (uint8_t*)heap_caps_malloc(Capacity, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    StreamBufCapacity[w] = (NULL != StreamBuf[w]) ? Capacity : 0;
  }

  bool ret = false;
  if (NULL != StreamBuf[w]) {
    if (HeaderLen > 0) {
      memcpy(StreamBuf[w], Header, HeaderLen);
    }
    memcpy(StreamBuf[w] + HeaderLen, fb->buf + Offset, fb->len - Offset);
    ret = true;
  } else {
    log->AddEvent(LogLevel_Error, F("Stream: failed to allocate frame buffer"));
  }

  /* return the frame to the driver now, the next frame is captured while this one is being sent */
  esp_camera_fb_return(fb);
  xSemaphoreGive(frameBufferSemaphore);

  if (true == ret) {
    xSemaphoreTake(streamMutex, portMAX_DELAY);
    StreamBufLen[w] = Total;
    StreamLatest = w;
    StreamLatestConsumed = false;
    StreamLatestTime = millis();
    StreamFrameSeq++;
    xSemaphoreGive(streamMutex);
  }

  return ret;
}

/**
   @brief Take the latest stream frame. The frame is not overwritten until StreamReleaseFrame is called
   @param uint32_t - sequence number of the last frame the caller has, it waits for a newer frame
   @param uint8_t ** - output, frame data
   @param size_t * - output, frame length
   @param uint8_t * - output, buffer index for StreamReleaseFrame
   @param uint32_t * - output, frame sequence number
   @param uint32_t - maximum waiting time [ms]
   @return bool - true when a frame was taken
*/
bool Camera::StreamAcquireFrame(uint32_t i_lastSeq, uint8_t **o_buf, size_t *o_len, uint8_t *o_idx, uint32_t *o_seq, uint32_t i_timeout) {
  uint32_t start = millis();

  while (true) {
    xSemaphoreTake(streamMutex, portMAX_DELAY);
    if ((StreamLatest >= 0) && (StreamFrameSeq != i_lastSeq)) {
      *o_idx = StreamLatest;
      *o_buf = StreamBuf[StreamLatest];
      *o_len = StreamBufLen[StreamLatest];
      *o_seq = StreamFrameSeq;
      StreamBufReaders[StreamLatest]++;
      StreamLatestConsumed = true;
      xSemaphoreGive(streamMutex);
      return true;
    }
    xSemaphoreGive(streamMutex);

    if (((millis() - start) >= i_timeout) || (0 == StreamClients)) {
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

/**
   @brief Release a stream frame taken by StreamAcquireFrame
   @param uint8_t - buffer index
   @return none
*/
void Camera::StreamReleaseFrame(uint8_t i_idx) {
  if (i_idx >= CAMERA_STREAM_BUFFERS) {
    return;
  }

  xSemaphoreTake(streamMutex, portMAX_DELAY);
  if (StreamBufReaders[i_idx] > 0) {
    StreamBufReaders[i_idx]--;
  }
  xSemaphoreGive(streamMutex);
}

/**
   @brief Get count of connected stream clients
   @param none
   @return uint8_t - count
*/
uint8_t Camera::GetStreamClients() {
  return StreamClients;
}

/**
   @brief Capture Return Frame Buffer
   @param none
   @return none
*/
void Camera::CaptureReturnFrameBuffer() {
  if (!xSemaphoreTake(frameBufferSemaphore, pdMS_TO_TICKS(CAMERA_SEMAPHORE_TIMEOUT))) {
    log->AddEvent(LogLevel_Error, F("Return frame buffer: failed to take semaphore"));
    return;
  }

  if (FrameBuffer) {
    esp_camera_fb_return(FrameBuffer);
    FrameBuffer = NULL;
  }
  xSemaphoreGive(frameBufferSemaphore);
}

/**
   @brief Set Stream Status
   @param bool - true = on, false = off
   @return none
*/
void Camera::SetStreamStatus(bool i_status) {
  /* the client count must always be updated, also when the camera is busy */
  bool locked = xSemaphoreTake(frameBufferSemaphore, pdMS_TO_TICKS(CAMERA_SEMAPHORE_TIMEOUT));
  if (!locked) {
    log->AddEvent(LogLevel_Error, F("Set stream status: failed to take semaphore"));
  }

  xSemaphoreTake(streamMutex, portMAX_DELAY);
  if (true == i_status) {
    if (0 == StreamClients) {
      /* new stream session: never send a frame from the previous session */
      StreamLatest = -1;
      StreamLatestConsumed = false;
    }
    StreamClients++;
  } else if (StreamClients > 0) {
    StreamClients--;
  }
  StreamOnOff = (StreamClients > 0);
  xSemaphoreGive(streamMutex);

  if (locked) {
    /* the held photo blocks the single camera frame buffer, return it to the driver for the stream */
    if (FrameBuffer) {
      esp_camera_fb_return(FrameBuffer);
      FrameBuffer = NULL;
    }

    /* a new photo is captured in the next cycle */
    CameraCaptureSuccess = false;
    xSemaphoreGive(frameBufferSemaphore);
  }

  if (NULL != StreamTask) {
    xTaskNotifyGive(StreamTask);
  }
  log->AddEvent(LogLevel_Info, F("Camera video stream clients: "), String(StreamClients));
}

/**
   @brief Copy the last photo to a new buffer. Prusa Connect upload, SD card and WEB server work with this copy,
          so the camera can capture a new photo while the copy is still being sent.
          Without stream: EXIF header + JPEG data of the last photo. With stream: the last stream frame.
   @param uint8_t ** - output, allocated buffer. The caller must release it with free()
   @param size_t * - output, buffer length
   @return bool - true when a photo is available
*/
bool Camera::GetPhotoCopy(uint8_t** o_buf, size_t* o_len) {
  *o_buf = NULL;
  *o_len = 0;

  /* stream is on: copy the latest stream frame, the camera is used by the stream capture task */
  if (true == StreamOnOff) {
    uint8_t* Frame = NULL;
    size_t FrameLen = 0;
    uint8_t Idx = 0;
    uint32_t Seq = 0;
    if (false == StreamAcquireFrame(UINT32_MAX, &Frame, &FrameLen, &Idx, &Seq, CAMERA_STREAM_FRAME_TIMEOUT)) {
      log->AddEvent(LogLevel_Error, F("Photo copy: no stream frame"));
      return false;
    }

    uint8_t* Buf = (uint8_t*)heap_caps_malloc(FrameLen, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (Buf == NULL) {
      Buf = (uint8_t*)malloc(FrameLen);
    }
    if (Buf != NULL) {
      memcpy(Buf, Frame, FrameLen);
      *o_buf = Buf;
      *o_len = FrameLen;
    } else {
      log->AddEvent(LogLevel_Error, F("Photo copy: failed to allocate memory: "), String(FrameLen));
    }
    StreamReleaseFrame(Idx);
    return (Buf != NULL);
  }

  if (!xSemaphoreTake(frameBufferSemaphore, pdMS_TO_TICKS(CAMERA_SEMAPHORE_TIMEOUT))) {
    log->AddEvent(LogLevel_Error, F("Photo copy: failed to take frame buffer semaphore"));
    return false;
  }

  const uint8_t* Header = NULL;
  size_t HeaderLen = 0;
  const uint8_t* Data = NULL;
  size_t DataLen = 0;

  if ((false == StreamOnOff) && (true == CameraCaptureSuccess) && (FrameBuffer != NULL)) {
    if ((PhotoExifData.header != NULL) && (PhotoExifData.offset < FrameBuffer->len)) {
      Header = PhotoExifData.header;
      HeaderLen = PhotoExifData.len;
      Data = FrameBuffer->buf + PhotoExifData.offset;
      DataLen = FrameBuffer->len - PhotoExifData.offset;
    } else {
      Data = FrameBuffer->buf;
      DataLen = FrameBuffer->len;
    }
  }

  bool ret = false;
  if ((Data != NULL) && (DataLen > 0)) {
    size_t TotalLen = HeaderLen + DataLen;
    uint8_t* Buf = (uint8_t*)heap_caps_malloc(TotalLen, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (Buf == NULL) {
      Buf = (uint8_t*)malloc(TotalLen);
    }

    if (Buf != NULL) {
      if (HeaderLen > 0) {
        memcpy(Buf, Header, HeaderLen);
      }
      memcpy(Buf + HeaderLen, Data, DataLen);
      *o_buf = Buf;
      *o_len = TotalLen;
      ret = true;
    } else {
      log->AddEvent(LogLevel_Error, F("Photo copy: failed to allocate memory: "), String(TotalLen));
    }
  }

  xSemaphoreGive(frameBufferSemaphore);
  return ret;
}

/**
   @brief Get Stream Status
   @param none
   @return bool - true = on, false = off
*/
bool Camera::GetStreamStatus() {
  return StreamOnOff;
}

bool Camera::GetCameraCaptureSuccess() {
  return CameraCaptureSuccess;
}

/**
   @brief Set Frame Size
   @param uint16_t - frame size
   @return none
*/
void Camera::StreamSetFrameSize(uint16_t i_data) {
  StreamAverageSize = (StreamAverageSize + i_data) / 2;
}

/**
   @brief Set average Frame Fps
   @param float - frame fps
   @return none
*/
void Camera::StreamSetFrameFps(float i_data) {
  StreamAverageFps = (StreamAverageFps + i_data) / 2.0;
}

/**
   @brief Get average stream Frame Size
   @param none
   @return uint16_t - frame size
*/
uint16_t Camera::StreamGetFrameAverageSize() {
  return StreamAverageSize;
}

/**
   @brief Get average stream Frame Fps
   @param none
   @return float - frame fps
*/
float Camera::StreamGetFrameAverageFps() {
  return StreamAverageFps;
}

/**
   @brief Clear Frame Data
   @param none
   @return none
*/
void Camera::StreamClearFrameData() {
  StreamAverageFps = 0.0;
  StreamAverageSize = 0;
}

/**
   @brief Get Photo
   @param none
   @return String - photo
*/
String Camera::GetPhoto() {
  Photo = "";
  for (size_t i = 0; i < FrameBuffer->len; i++) {
    Photo += (char)FrameBuffer->buf[i];
  }
  return Photo;
}

/**
   @brief Get Photo Frame Buffer
   @param none
   @return camera_fb_t* - photo frame buffer
*/
camera_fb_t* Camera::GetPhotoFb() {
  return FrameBuffer;
}

/**
   @brief Get Photo Exif Data
   @param none
   @return PhotoExifData_t* - photo exif data
*/
PhotoExifData_t* Camera::GetPhotoExifData() {
  return &PhotoExifData;
}

/**
   @brief Copy Photo
   @param camera_fb_t* - pointer to camera_fb_t
   @return none
*/
void Camera::CopyPhoto(camera_fb_t* i_data) {
  *i_data = *FrameBuffer;
}

/**
   @brief Copy Photo
   @param String* - pointer to string
   @return none
*/
void Camera::CopyPhoto(String* i_data) {
  Photo = "";
  for (size_t i = 0; i < FrameBuffer->len; i++) {
    Photo += (char)FrameBuffer->buf[i];
  }
  *i_data = Photo;
}

/**
 * @brief Copy photo from frame buffer to string array with range
 * 
 * @param i_data - pointer to string 
 * @param i_from - start index
 * @param i_to - end index
 */
void Camera::CopyPhoto(String* i_data, int i_from, int i_to) {
  *i_data = "";

  for (int i = i_from; i < i_to; i++) {
    *i_data += (char)FrameBuffer->buf[i];
  }
}

/**
 * @brief Copy photo from frame buffer to char array with range
 * 
 * @param i_data - pointer to char array
 * @param i_from - start index
 * @param i_to - end index
 */
void Camera::CopyPhoto(char* i_data, int i_from, int i_to) {
  int length = i_to - i_from;
  memcpy(i_data, FrameBuffer->buf + i_from, length);
  i_data[length + 1] = '\0';
}

/**
 * @brief Get Photo Size
 * 
 * @return int - photo size
 */
int Camera::GetPhotoSize() {
  log->AddEvent(LogLevel_Verbose, F("Photo size: "), String(FrameBuffer->len));
  return (int)FrameBuffer->len;
}

/**
   @brief Set Photo Quality
   @param uint8_t - photo quality
   @return none
*/
void Camera::SetPhotoQuality(uint8_t i_data) {
  config->SavePhotoQuality(i_data);
  PhotoQuality = i_data;
  ReinitCameraModule();
}

/**
   @brief Set Frame Size
   @param uint8_t - frame size
   @return none
*/
bool Camera::SetFrameSize(uint8_t i_data) {
  if (false == CheckFrameSizeSupported(i_data)) {
    log->AddEvent(LogLevel_Warning, F("Frame size is not supported by the camera sensor: "), String(i_data));
    return false;
  }

  config->SaveFrameSize(i_data);
  FrameSize = i_data;
  TFrameSize = TransformFrameSizeDataType(i_data);
  ReinitCameraModule();
  return true;
}

/**
   @brief Set Brightness
   @param int8_t - brightness
   @return none
*/
void Camera::SetBrightness(int8_t i_data) {
  config->SaveBrightness(i_data);
  brightness = i_data;
  ApplyCameraCfg();
}

/**
   @brief Set Contrast
   @param int8_t - contrast
   @return none
*/
void Camera::SetContrast(int8_t i_data) {
  config->SaveContrast(i_data);
  contrast = i_data;
  ApplyCameraCfg();
}

/**
   @brief Set Saturation
   @param int8_t - saturation
   @return none
*/
void Camera::SetSaturation(int8_t i_data) {
  config->SaveSaturation(i_data);
  saturation = i_data;
  ApplyCameraCfg();
}

/**
   @brief Set automatic white balance
   @param bool - automatic white balance
   @return none
*/
void Camera::SetAwb(bool i_data) {
  config->SaveAwb(i_data);
  awb = i_data;
  ApplyCameraCfg();
}

/**
   @brief Set automatic white balance gain
   @param bool - automatic white balance gain
   @return none
*/
void Camera::SetAwbGain(bool i_data) {
  config->SaveAwbGain(i_data);
  awb_gain = i_data;
  ApplyCameraCfg();
}

/**
   @brief Set automatic white balance mode
   @param uint8_t - automatic white balance mode
   @return none
*/
void Camera::SetAwbMode(uint8_t i_data) {
  config->SaveAwbMode(i_data);
  wb_mode = i_data;
  ApplyCameraCfg();
}

/**
   @brief Set automatic exposure control 2
   @param bool - automatic exposure control 2
   @return none
*/
void Camera::SetAec2(bool i_data) {
  config->SaveAec2(i_data);
  aec2 = i_data;
  ApplyCameraCfg();
}

/**
   @brief Set automatic exposure level
   @param int8_t - automatic exposure level
   @return none
*/
void Camera::SetAeLevel(int8_t i_data) {
  config->SaveAeLevel(i_data);
  ae_level = i_data;
  ApplyCameraCfg();
}

/**
   @brief Set automatic exposition control value
   @param uint16_t - automatic exposure control value
   @return none
*/
void Camera::SetAecValue(uint16_t i_data) {
  config->SaveAecValue(i_data);
  aec_value = i_data;
  ApplyCameraCfg();
}

/**
   @brief Set Gain Ctrl
   @param bool - gain ctrl
   @return none
*/
void Camera::SetGainCtrl(bool i_data) {
  config->SaveGainCtrl(i_data);
  gain_ctrl = i_data;
  ApplyCameraCfg();
}

/**
   @brief Set automatic gain control gain
   @param uint8_t - automatic gain control gain
   @return none
*/
void Camera::SetAgcGain(uint8_t i_data) {
  config->SaveAgcGain(i_data);
  agc_gain = i_data;
  ApplyCameraCfg();
}

/**
   @brief Set bad pixel correction
   @param bool - bad pixel correction
   @return none
*/
void Camera::SetBpc(bool i_data) {
  config->SaveBpc(i_data);
  bpc = i_data;
  ApplyCameraCfg();
}

/**
   @brief Set white pixel correction
   @param bool - white pixel correction
   @return none
*/
void Camera::SetWpc(bool i_data) {
  config->SaveWpc(i_data);
  wpc = i_data;
  ApplyCameraCfg();
}

/**
   @brief Set Raw Gama
   @param bool - raw gama
   @return none
*/
void Camera::SetRawGama(bool i_data) {
  config->SaveRawGama(i_data);
  raw_gama = i_data;
  ApplyCameraCfg();
}

/**
   @brief Set horizontal Mirror
   @param bool - horizontal mirror
   @return none
*/
void Camera::SetHMirror(bool i_data) {
  config->SaveHmirror(i_data);
  hmirror = i_data;
  ApplyCameraCfg();
}

/**
   @brief Set V Flip
   @param bool - vflip
   @return none
*/
void Camera::SetVFlip(bool i_data) {
  config->SaveVflip(i_data);
  vflip = i_data;
  ApplyCameraCfg();
}

/**
   @brief Set Lens correction
   @param bool - lens correction
   @return none
*/
void Camera::SetLensC(bool i_data) {
  config->SaveLensCorrect(i_data);
  lensc = i_data;
  ApplyCameraCfg();
}

/**
   @brief Set Exposure Ctrl
   @param bool - exposure ctrl
   @return none
*/
void Camera::SetExposureCtrl(bool i_data) {
  config->SaveExposureCtrl(i_data);
  exposure_ctrl = i_data;
  ApplyCameraCfg();
}

/**
   @brief Set Camera Flash Enable
   @param bool - flash enable/disable
   @return none
*/
void Camera::SetCameraFlashEnable(bool i_data) {
  config->SaveCameraFlashEnable(i_data);
  CameraFlashEnable = i_data;
}

/**
   @brief Set Camera Flash Time
   @param uint16_t - flash time
   @return none
*/
void Camera::SetCameraFlashTime(uint16_t i_data) {
  config->SaveCameraFlashTime(i_data);
  CameraFlashTime = i_data;
}

void Camera::SetCameraImageRotation(uint8_t i_data) {
  config->SaveCameraImageExifRotation(i_data);
  imageExifRotation = i_data;
}

/**
   @brief Get Photo Quality
   @param none
   @return uint8_t - photo quality
*/
uint8_t Camera::GetPhotoQuality() {
  return PhotoQuality;
}

/**
   @brief Get Frame Size
   @param none
   @return uint8_t - frame size
*/
uint8_t Camera::GetFrameSize() {
  return FrameSize;
}

/**
 * @brief Width of the selected frame size, from the camera driver resolution table
 * 
 * @return uint16_t - width [px]
 */
uint16_t Camera::GetFrameSizeWidth() {
  return resolution[TFrameSize].width;
}

/**
 * @brief Height of the selected frame size, from the camera driver resolution table
 * 
 * @return uint16_t - height [px]
 */
uint16_t Camera::GetFrameSizeHeight() {
  return resolution[TFrameSize].height;
}

/**
   @brief Get Brightness
   @param none
   @return int8_t - brightness
*/
int8_t Camera::GetBrightness() {
  return brightness;
}

/**
   @brief Get Contrast
   @param none
   @return int8_t - contrast
*/
int8_t Camera::GetContrast() {
  return contrast;
}

/**
   @brief Get Saturation
   @param none
   @return int8_t - saturation
*/
int8_t Camera::GetSaturation() {
  return saturation;
}

/**
   @brief Get Auto white balance status
   @param none
   @return bool - Auto white balance status
*/
bool Camera::GetAwb() {
  return awb;
}

/**
   @brief Get Auto white balance gain status
   @param none
   @return bool -  Auto white balance gain status
*/
bool Camera::GetAwbGain() {
  return awb_gain;
}

/**
   @brief Get Auto white balance mode
   @param none
   @return uint8_t - Auto white balance mode
*/
uint8_t Camera::GetAwbMode() {
  return wb_mode;
}

/**
   @brief Get automatic exposure control 2 status
   @param none
   @return bool - Automatic exposure control 2 status
*/
bool Camera::GetAec2() {
  return aec2;
}

/**
   @brief Get automatic exposure level
   @param none
   @return int8_t - Automatic exposure level
*/
int8_t Camera::GetAeLevel() {
  return ae_level;
}

/**
   @brief Get automatic exposure control value
   @param none
   @return uint16_t - Automatic exposure control value
*/
uint16_t Camera::GetAecValue() {
  return aec_value;
}

/**
   @brief Get Gain Ctrl status
   @param none
   @return bool - Gain control status
*/
bool Camera::GetGainCtrl() {
  return gain_ctrl;
}

/**
   @brief Get Agc Gaint
   @param none
   @return uint8_t - agc gain
*/
uint8_t Camera::GetAgcGaint() {
  return agc_gain;
}

/**
   @brief Get Bpc
   @param none
   @return bool - bpc status
*/
bool Camera::GetBpc() {
  return bpc;
}

/**
   @brief Get Wpc
   @param none
   @return bool - wpc status
*/
bool Camera::GetWpc() {
  return wpc;
}

/**
   @brief Get Raw Gama value
   @param none
   @return bool - raw gamma value
*/
bool Camera::GetRawGama() {
  return raw_gama;
}

/**
   @brief Get horizontal Mirror status
   @param none
   @return bool - horizontal mirror status
*/
bool Camera::GetHMirror() {
  return hmirror;
}

/**
   @brief Get vertical Flip status
   @param none
   @return bool - vertical flip status
*/
bool Camera::GetVFlip() {
  return vflip;
}

/**
   @brief Get Lens correction status
   @param none
   @return bool - lens correction status
*/
bool Camera::GetLensC() {
  return lensc;
}

/**
   @brief Get exposure control status
   @param none
   @return bool - exposure control status
*/
bool Camera::GetExposureCtrl() {
  return exposure_ctrl;
}

/**
   @brief Get Camera Flash Enable status
   @param none
   @return bool - camera flash enable status
*/
bool Camera::GetCameraFlashEnable() {
  return CameraFlashEnable;
}

/**
 * @brief Get camera flash time
 * @param none
 * @return uint16_t - camera flash time
 */
uint16_t Camera::GetCameraFlashTime() {
  return CameraFlashTime;
}

uint8_t Camera::GetCameraImageRotation() {
  return imageExifRotation;
}

/* EOF */