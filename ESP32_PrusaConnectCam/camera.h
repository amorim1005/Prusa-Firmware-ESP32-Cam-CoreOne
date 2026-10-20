/**
   @file camera.h

   @brief Library for working with a camera

   @author Miroslav Pivovarsky
   Contact: miroslav.pivovarsky@gmail.com

   @bug: no know bug
*/

#pragma once

#include "esp_camera.h"
#include "esp_timer.h"
#include <esp_task_wdt.h>
#include "img_converters.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "micro_sd.h"
#include "log.h"
#include "cfg.h"
#include "exif.h"
#include "ESP32_OV5640_AF.h"
#include "module_XIAO_ESP32-S3-cam.h"
#include "mcu_cfg.h"
#include "var.h"

class Configuration;

struct PhotoExifData_t {
  const uint8_t *header;
  size_t len;
  size_t offset;
};

class Camera {
private:
  uint8_t PhotoQuality;      ///< photo quality
  uint8_t FrameSize;         ///< framesize
  int8_t brightness;         ///< brightness
  int8_t contrast;           ///< contrast
  int8_t saturation;         ///< saturation
  bool awb;                  ///< automatic white balancing
  bool awb_gain;             ///< automatic white balancing gain
  uint8_t wb_mode;           ///< white balancing mode
  bool aec2;                 ///< automatic exposition controll
  int8_t ae_level;           ///< automatic exposition level
  uint16_t aec_value;        ///< automatic exposition time
  bool gain_ctrl;            ///< automatic gain
  uint8_t agc_gain;          ///< automatic gain level
  bool bpc;                  ///< bad pixel correction
  bool wpc;                  ///< white pixel correction
  bool raw_gama;             ///< raw gama correction
  bool hmirror;              ///< horizontal mirror
  bool vflip;                ///< vertical flip
  bool lensc;                ///< lens corection
  bool exposure_ctrl;        ///< exposure control
  framesize_t TFrameSize;    ///< framesize_t type for camera module
  uint8_t imageExifRotation; ///< image rotation. 0 degree: value 1, 90 degree: value 6, 180 degree: value 3, 270 degree: value 8

  bool CameraCaptureSuccess; ///< camera capture success

  /* OV2640 camera module pinout and cfg*/
  camera_config_t CameraConfig;             ///< camera configuration
  camera_fb_t *FrameBuffer;                 ///< frame buffer
  sensor_t* sensor;                         ///< sensor
  bool StreamOnOff;                         ///< stream on/off, true when at least one stream client is connected

  /* live stream: the capture task fills the buffers in turn, the WEB server sends the latest complete frame */
  uint8_t *StreamBuf[CAMERA_STREAM_BUFFERS];          ///< stream frame buffers in PSRAM
  size_t StreamBufLen[CAMERA_STREAM_BUFFERS];         ///< length of the frame in the buffer
  size_t StreamBufCapacity[CAMERA_STREAM_BUFFERS];    ///< allocated size of the buffer
  uint8_t StreamBufReaders[CAMERA_STREAM_BUFFERS];    ///< count of readers of the buffer, a buffer with readers is not overwritten
  int8_t StreamLatest;                      ///< index of the latest complete frame, -1 = none
  bool StreamLatestConsumed;                ///< the latest frame was taken by a reader
  uint32_t StreamLatestTime;                ///< time of the latest frame [ms]
  volatile uint32_t StreamFrameSeq;         ///< sequence number of the latest frame
  volatile uint8_t StreamClients;           ///< count of connected stream clients
  SemaphoreHandle_t streamMutex;            ///< mutex for the stream buffer state (short operations only)
  TaskHandle_t StreamTask;                  ///< stream capture task
  SemaphoreHandle_t frameBufferSemaphore;   ///< semaphore for frame buffer
  float StreamAverageFps;                   ///< stream average fps
  uint16_t StreamAverageSize;               ///< stream average size
  PhotoExifData_t PhotoExifData;            ///< photo exif data
  uint8_t CameraCaptureFailedCounter;       ///< camera capture failed counter
  camera_pid_t CameraType;                  ///< detected camera sensor PID. 0 = not detected
  String CameraName;                        ///< detected camera sensor name
  framesize_t SensorMaxFrameSize;           ///< maximum frame size of the detected sensor
  OV5640 Ov5640Autofocus;                   ///< OV5640 autofocus firmware loader
  String AutofocusStatus;                   ///< autofocus status for the WEB page

  Configuration *config;                    ///< pointer to Configuration object
  Logs *log;                                ///< pointer to Logs object

  void InitCameraModule();
  void InitCameraModule(framesize_t);
  uint32_t GetSensorXclk();
  void CheckFrameSizeLimit();
  void InitAutofocus();
  static void StreamTaskEntry(void *);
  void StreamTaskLoop();
  bool StreamCaptureFrame();
  camera_pid_t TransformCameraSensorToPid(uint8_t);
  uint8_t TransformPidToCameraSensor(camera_pid_t);

public:
  Camera(Configuration*, Logs*);
  ~Camera(){};
  void Init();
  void ApplyCameraCfg();
  void LoadCameraCfgFromEeprom();
  void ReinitCameraModule();
  void GetCameraModel();
  void CapturePhoto();
  void CaptureReturnFrameBuffer();
  void StartStreamTask();
  void SetStreamStatus(bool);
  bool GetStreamStatus();
  uint8_t GetStreamClients();
  bool StreamAcquireFrame(uint32_t, uint8_t **, size_t *, uint8_t *, uint32_t *, uint32_t);
  void StreamReleaseFrame(uint8_t);
  bool GetCameraCaptureSuccess();

  void StreamSetFrameSize(uint16_t);
  void StreamSetFrameFps(float);
  uint16_t StreamGetFrameAverageSize();
  float StreamGetFrameAverageFps();
  void StreamClearFrameData();
  
  bool GetPhotoCopy(uint8_t **, size_t *);
  framesize_t TransformFrameSizeDataType(uint8_t);
  

  void SetPhotoQuality(uint8_t);
  bool SetFrameSize(uint8_t);
  bool CheckFrameSizeSupported(uint8_t);
  String GetSupportedFrameSizes();
  String GetDetectedSensorName();
  String GetAutofocusStatus();
  void SetBrightness(int8_t);
  void SetContrast(int8_t);
  void SetSaturation(int8_t);
  void SetAwb(bool);
  void SetAwbGain(bool);
  void SetAwbMode(uint8_t);
  void SetAec2(bool);
  void SetAeLevel(int8_t);
  void SetAecValue(uint16_t);
  void SetGainCtrl(bool);
  void SetAgcGain(uint8_t);
  void SetBpc(bool);
  void SetWpc(bool);
  void SetRawGama(bool);
  void SetHMirror(bool);
  void SetVFlip(bool);
  void SetLensC(bool);
  void SetExposureCtrl(bool);
  void SetCameraImageRotation(uint8_t);

  uint8_t GetPhotoQuality();
  uint8_t GetFrameSize();
  uint16_t GetFrameSizeWidth();
  uint16_t GetFrameSizeHeight();
  int8_t GetBrightness();
  int8_t GetContrast();
  int8_t GetSaturation();
  bool GetAwb();
  bool GetAwbGain();
  uint8_t GetAwbMode();
  bool GetAec2();
  int8_t GetAeLevel();
  uint16_t GetAecValue();
  bool GetGainCtrl();
  uint8_t GetAgcGaint();
  bool GetBpc();
  bool GetWpc();
  bool GetRawGama();
  bool GetHMirror();
  bool GetVFlip();
  bool GetLensC();
  bool GetExposureCtrl();
  uint8_t GetCameraImageRotation();
};

extern Camera SystemCamera; ///< Camera object

/* EOF */