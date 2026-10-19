/**
   @file server.cpp

   @brief Library for WEB server

   @author Miroslav Pivovarsky
   Contact: miroslav.pivovarsky@gmail.com

   @bug: no know bug

*/

#include "WebServer.h"
#include "Certificate.h"

AsyncWebServer server(WEB_SERVER_PORT);

/**
   @brief Load configuration from EEPROM
   @param none
   @return none
*/
void Server_LoadCfg() {
  WebBasicAuth.EnableAuth = SystemConfig.LoadBasicAuthFlag();
  WebBasicAuth.UserName = SystemConfig.LoadBasicAuthUsername();
  WebBasicAuth.Password = SystemConfig.LoadBasicAuthPassword();
}

/**
   @brief Init WEB server
   @param none
   @return none
*/
void Server_InitWebServer() {
  SystemLog.AddEvent(LogLevel_Info, F("Starting init WEB server"));

  /* route for get last capture photo */
  server.on("/saved-photo.jpg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: get photo"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    /* the response sends its own copy of the photo, the camera can capture a new photo during the transfer */
    uint8_t* photo = NULL;
    size_t photo_len = 0;
    if (false == SystemCamera.GetPhotoCopy(&photo, &photo_len)) {
      request->send(404, "text/plain", "Photo not found!");
      return;
    }

    SystemLog.AddEvent(LogLevel_Verbose, "Photo size: " + String(photo_len) + " bytes");
    AsyncBufferResponse* response = new AsyncBufferResponse(photo, photo_len, "image/jpeg");
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });

  /* route to jquery */
  server.on("/jquery-3.7.0.min.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get jquery-3.7.0.min.js"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "application/javascript", jquery_3_7_0_js);
  });

  Server_InitWebServer_JsonData();
  Server_InitWebServer_WebPages();
  Server_InitWebServer_Icons();
  Server_InitWebServer_Actions();
  Server_InitWebServer_Sets();
  Server_InitWebServer_Update();
  Server_InitWebServer_Stream();

  /* route for not found page */
  server.onNotFound(Server_handleNotFound);

  /* start WEB server */
  server.begin();
}

/**
   @brief Init WEB server json data
   @param none
   @return none
*/
void Server_InitWebServer_JsonData() {
  /* route for json with cfg parameters */
  server.on("/json_input", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: get json_input"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/plain"), Server_GetJsonData().c_str());
  });

  /* route for json with wifi networks */
  server.on("/json_wifi", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: get json_wifi"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/plain"), SystemWifiMngt.GetAvailableWifiNetworks().c_str());
  });

  /* route for san wi-fi networks */
  server.on("/wifi_scan", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: scan WI-FI networks"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/html"), MSG_SCANNING);
    SystemWifiMngt.ScanWiFiNetwork();
  });
}

/**
   @brief Init WEB server web pages
   @param none
   @return none
*/
void Server_InitWebServer_WebPages() {
  /* Route for root / web page */
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get index.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", index_html);
  });

  /* Route for styles */
  server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get styles.css"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/css", styles_css);
  });

  /* Route for java scripts */
  server.on("/scripts.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get scripts.js"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    Server_handleCacheRequest(request, "application/javascript", scripts_js);
  });


  /* Route for config web page */
  server.on("/page_config.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get page_config.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", page_config_html);
  });

  /* Route for wifi web page */
  server.on("/page_wifi.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get page_wifi.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", page_wifi_html);
  });

  /* Route for auth web page */
  server.on("/page_auth.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get page_auth.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", page_auth_html);
  });

  /* Route for system web page */
  server.on("/page_system.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get page_system.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", page_system_html);
  });

  /* route for temperature web page */
  server.on("/page_temperature.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get page_temperature.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", page_temperature_html);
  });

  /* route to license page */
  server.on("/license.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get license.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", license_html);
  });

  /* route to gtac page */
  server.on("/gtac.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get gtac.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", gtac_html);
  });

  /* route to privacy policy page */
  server.on("/privacypolicy.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get privacypolicy.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", privacypolicy_html);
  });

  /* route to cookie page */
  server.on("/cookie.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get cookie.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", cookies_html);
  });

  /* route to logs page */
  server.on("/get_logs", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get get_logs.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    if (true == SystemLog.GetCardDetectedStatus()) {
      request->send(SD_MMC, SystemLog.GetFilePath() + SystemLog.GetFileName(), "text/plain");
      //SystemLog.LogOpenFile();
    } else {
      request->send(404, "text/plain", "Micro SD card not found with FAT32 partition!");
    }
  });

  /* route to get temperature */
  server.on("/get_temp", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get get_temp"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    request->send(200, "text/plain", String(ExternalTemperatureSensor.GetTemperature()));
  });

  /* route to get humidity */
  server.on("/get_hum", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get get_hum"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    request->send(200, "text/plain", String(ExternalTemperatureSensor.GetHumidity()));
  });
}

/**
   @brief Init WEB server icons
   @param none
   @return none
*/
void Server_InitWebServer_Icons() {
  /* route to logo */
  server.on("/esp32_cam.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get esp32_cam.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", esp32_cam_logo_svg);
  });

  /* route to github icon */
  server.on("/github-icon.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get github-icon.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", github_icon_svg);
  });

  /* route to light on icon */
  server.on("/light-on-icon.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get light-icon.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", light_icon_on_svg);
  });

  /* route to light off icon */
  server.on("/light-off-icon.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get light-icon.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", light_icon_off_svg);
  });

  /* route to refresh icon */
  server.on("/refresh-icon.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get refresh-icon.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", refresh_icon_svg);
  });

  /* route to reboot icon */
  server.on("/reboot-icon.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get reboot-icon.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", reboot_icon_svg);
  });

  /* route to wifi icon */
  server.on("/wifi-icon-1.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get wifi-icon-1.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", wifi_icon_1_svg);
  });

  /* route to wifi icon */
  server.on("/wifi-icon-2.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get wifi-icon-2.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", wifi_icon_2_svg);
  });

  /* route to wifi icon */
  server.on("/wifi-icon-3.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get wifi-icon-3.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", wifi_icon_3_svg);
  });

  /* route to wifi icon */
  server.on("/wifi-icon-4.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get wifi-icon-4.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", wifi_icon_4_svg);
  });

  /* route to wifi icon */
  server.on("/wifi-icon-0.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get wifi-icon-0.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", wifi_icon_0_svg);
  });

  /* route to eye icon */
  server.on("/eye.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get eye.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", eye_svg);
  });

  /* route to eye-slash icon */
  server.on("/eye-slash.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get eye-slash.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", eye_slash_svg);
  });

  /* route to favicon */
  server.on("/favicon.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get favicon.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", favicon_svg);
  });
}

/**
   @brief Init WEB server actions
   @param none
   @return none
*/
void Server_InitWebServer_Actions() {
  /*route for capture photo */
  server.on("/action_capture", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /action_capture Take photo"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    SystemCamera.CapturePhoto();
    request->send(200, "text/plain", "Take Photo");
  });

  /* route for send photo to prusa backend */
  server.on("/action_send", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /action_send send photo to cloud"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    Connect.SetSendingIntervalExpired();
    request->send(200, "text/plain", "Send Photo");
  });

  /* route for change LED status */
  server.on("/action_led", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /action_led Change LED status"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    SystemCamera.SetFlashStatus(!SystemCamera.GetFlashStatus());
    SystemCamera.SetCameraFlashEnable(false);

    request->send(200, "text/plain", "Change LED status");
  });

  /* route for change LED status */
  server.on("/light", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /light set LED status"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    if (request->hasArg("on")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Turning light ON"));
      SystemCamera.SetFlashStatus(true);
      SystemCamera.SetCameraFlashEnable(false);
      request->send(200, "text/plain", "Light ON");

    } else if (request->hasArg("off")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Turning light OFF"));
      SystemCamera.SetFlashStatus(false);
      SystemCamera.SetCameraFlashEnable(false);
      request->send(200, "text/plain", "Light OFF");

    } else {
      request->send(400, "text/plain", "Invalid request");
    }
  });

  /* route for change FLASH status */
  server.on("/flash", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /flash set flash status"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    if (request->hasArg("on")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Turning flash ON"));
      SystemCamera.SetCameraFlashEnable(true);
      SystemCamera.SetFlashStatus(false);
      request->send(200, "text/plain", "Flash ON");

    } else if (request->hasArg("off")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Turning flash OFF"));
      SystemCamera.SetCameraFlashEnable(false);
      SystemCamera.SetFlashStatus(false);
      request->send(200, "text/plain", "Flash OFF");

    } else {
      request->send(400, "text/plain", "Invalid request");
    }
  });

  /* reboot MCU */
  server.on("/action_reboot", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /action_reboo reboot MCU!"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/html"), MSG_REBOOT_MCU);
    delay(100); /* wait for sending data */
    ESP.restart();
  });

  /* route for change LED status */
  server.on("/action_sderase", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /action_sderase remove files from SD card"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    StartRemoveSdCard = 1;

    request->send(200, F("text/plain"), "Starting remove files from SD card");
  });
}

/**
   @brief Init WEB server sets
   @param none
   @return none
*/
void Server_InitWebServer_Sets() {
  /* route to set integer value */
  server.on("/set_int", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /set_int"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    bool response = false;
    String response_msg = "";

    /* set refresh interval */
    if (request->hasParam("refresh")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set refresh interval"));
      long value = request->getParam("refresh")->value().toInt();
      if ((value > 0) && (value <= UINT16_MAX) && Connect.CheckRefreshInterval((uint16_t)value)) {
        Connect.SetRefreshInterval((uint16_t)value);
        response_msg = MSG_SAVE_OK;
      } else {
        response_msg = "ERROR! Bad value. Allowed values are " + String(REFRESH_INTERVAL_TEN_SEC) + ", " + String(REFRESH_INTERVAL_THIRTY_SEC) + ", " + String(REFRESH_INTERVAL_SIXTY_SEC) + " or " + String(REFRESH_INTERVAL_TEN_MIN) + " second";
      }
      response = true;
    }

    /* set saturation */
    if (request->hasParam("saturation")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set saturation"));
      SystemCamera.SetSaturation(request->getParam("saturation")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set contrast */
    if (request->hasParam("contrast")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set contrast"));
      SystemCamera.SetContrast(request->getParam("contrast")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set brightness */
    if (request->hasParam("brightness")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set brightness"));
      SystemCamera.SetBrightness(request->getParam("brightness")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set frame size */
    if (request->hasParam("framesize")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set framesize"));
      if (SystemCamera.SetFrameSize(request->getParam("framesize")->value().toInt())) {
        Connect.UpdateDeviceInformation();
        response_msg = MSG_SAVE_OK;
      } else {
        response_msg = "ERROR! This resolution is not supported by the camera sensor " + SystemCamera.GetDetectedSensorName();
      }
      response = true;
    }

    /* set photo quality */
    if (request->hasParam("photo_quality")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set photo_quality"));
      SystemCamera.SetPhotoQuality(73 - request->getParam("photo_quality")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set flash time */
    if (request->hasParam("flash_time")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set flash_time"));
      SystemCamera.SetCameraFlashTime(request->getParam("flash_time")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set white balancing mode */
    if (request->hasParam("wb_mode")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set wb_mode"));
      SystemCamera.SetAwbMode(request->getParam("wb_mode")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set auto exposition level */
    if (request->hasParam("ae_level")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set ae_level"));
      SystemCamera.SetAeLevel(request->getParam("ae_level")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set auto exposition controll value */
    if (request->hasParam("aec_value")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set aec_value"));
      SystemCamera.SetAecValue(request->getParam("aec_value")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set auto gain correction value */
    if (request->hasParam("agc_gain")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set agc_gain"));
      SystemCamera.SetAgcGain(request->getParam("agc_gain")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set image exif rotation */
    if (request->hasParam("image_rotation")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set image EXIF rotation"));
      SystemCamera.SetCameraImageRotation(request->getParam("image_rotation")->value().toInt());
      Connect.UpdateDeviceInformation();
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set log level /set_int?log_level=2 */
    if (request->hasParam("log_level")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set log_level"));
      LogLevel_enum level = (LogLevel_enum)request->getParam("log_level")->value().toInt();
      if ((level >= LogLevel_Error) && (level <= LogLevel_Verbose)) {
        SystemConfig.SaveLogLevel(level);
        SystemLog.SetLogLevel(level);
        response_msg = MSG_SAVE_OK;
      } else {
        response_msg = MSG_SAVE_NOTOK;
      }

      response = true;
    }

    /*  set network ip method. 0 - DHCP, 1 - Static */
    if (request->hasParam("ipcfg")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set ipcfg"));
      SystemWifiMngt.SetNetIpMethod(request->getParam("ipcfg")->value().toInt());
      response_msg = MSG_SAVE_OK_REBOOT;

      response = true;
    }

    /* set saturation */
    if (request->hasParam("temp_unit")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set temp_unit"));
      ExternalTemperatureSensor.SetUnit((TemperatureSensorUnit_enum) request->getParam("temp_unit")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    if (true == response) {
      request->send(200, F("text/html"), response_msg.c_str());
    }
  });

  /* route to set bool value */
  server.on("/set_bool", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /set_bool"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    bool response = false;

    /* check cfg for hmirror */
    if (request->hasParam("hmirror")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set hmirror"));
      SystemCamera.SetHMirror(Server_TransfeStringToBool(request->getParam("hmirror")->value()));
      response = true;
    }

    /* set vertical flip */
    if (request->hasParam("vflip")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set hmirror"));
      SystemCamera.SetVFlip(Server_TransfeStringToBool(request->getParam("vflip")->value()));
      response = true;
    }

    /* set lens correction */
    if (request->hasParam("lenc")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set lensc"));
      SystemCamera.SetLensC(Server_TransfeStringToBool(request->getParam("lenc")->value()));
      response = true;
    }

    /* set exposure controll */
    if (request->hasParam("exposure_ctrl")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set exposure ctrl"));
      SystemCamera.SetExposureCtrl(Server_TransfeStringToBool(request->getParam("exposure_ctrl")->value()));
      response = true;
    }

    /* set auto white balancing */
    if (request->hasParam("awb")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set awb"));
      SystemCamera.SetAwb(Server_TransfeStringToBool(request->getParam("awb")->value()));
      response = true;
    }

    /* set auto white balancing gain */
    if (request->hasParam("awb_gain")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set awb_gain"));
      SystemCamera.SetAwbGain(Server_TransfeStringToBool(request->getParam("awb_gain")->value()));
      response = true;
    }

    /* set bad pixel correction */
    if (request->hasParam("bpc")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set bpc"));
      SystemCamera.SetBpc(Server_TransfeStringToBool(request->getParam("bpc")->value()));
      response = true;
    }

    /* set white pixel correction */
    if (request->hasParam("wpc")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set wpc"));
      SystemCamera.SetWpc(Server_TransfeStringToBool(request->getParam("wpc")->value()));
      response = true;
    }

    /* set raw gama correction */
    if (request->hasParam("raw_gama")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set raw_gama"));
      SystemCamera.SetRawGama(Server_TransfeStringToBool(request->getParam("raw_gama")->value()));
      response = true;
    }

    /* set automatic exposure correction */
    if (request->hasParam("aec2")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set aec2"));
      SystemCamera.SetAec2(Server_TransfeStringToBool(request->getParam("aec2")->value()));
      response = true;
    }

    /* set gain controll */
    if (request->hasParam("gain_ctrl")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set gain_ctrl"));
      SystemCamera.SetGainCtrl(Server_TransfeStringToBool(request->getParam("gain_ctrl")->value()));
      response = true;
    }

    /* set flash */
    if (request->hasParam("flash")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set flash"));
      SystemCamera.SetCameraFlashEnable(Server_TransfeStringToBool(request->getParam("flash")->value()));
      SystemCamera.SetFlashStatus(false);
      response = true;
    }

    /* set service AP */
    if (request->hasParam("serviceap_enable")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set service AP enable"));
      SystemWifiMngt.SetEnableServiceAp(Server_TransfeStringToBool(request->getParam("serviceap_enable")->value()));
      response = true;
    }

    /* set timelaps enable */
    if (request->hasParam("timelaps_enable")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set timelaps enable"));
#if (ENABLE_SD_CARD == true)
      bool val = Server_TransfeStringToBool(request->getParam("timelaps_enable")->value());
      if ((true == val) && (SystemLog.GetCardDetectedStatus() == true)) {
        Connect.SetTimeLapsPhotoSaveStatus(val);
      } else {
        Connect.SetTimeLapsPhotoSaveStatus(false);
      }
#else
        Connect.SetTimeLapsPhotoSaveStatus(false);
#endif
      response = true;
    }

    /* set external temperature sensor enable */
    if (request->hasParam("extsens_enable")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set enable ext temperature"));
      ExternalTemperatureSensor.EnableSensor(Server_TransfeStringToBool(request->getParam("extsens_enable")->value()));
      response = true;
    }

    if (true == response) {
      request->send(200, F("text/html"), MSG_SAVE_OK);
    }
  });

  /* route for set token for authentification to prusa backend*/
  server.on("/set_token", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /set_token"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    if (request->hasParam("token")) {
      String token = request->getParam("token")->value();
      token.trim();

      /* Camera Service token: exactly 20 characters A-Z, a-z, 0-9 */
      bool valid = (token.length() == CAMERA_SERVICE_TOKEN_LEN);
      for (size_t i = 0; (i < token.length()) && valid; i++) {
        valid = isalnum(token.charAt(i));
      }

      if (valid) {
        Connect.SetToken(token);
        request->send(200, F("text/html"), F("Save OK. Camera registration starts now, it must be done within 15 minutes after adding the camera in Prusa Connect"));
      } else {
        request->send(200, F("text/html"), F("ERROR! Token must have exactly 20 characters A-Z, a-z, 0-9"));
      }
    } else {
      request->send(200, F("text/html"), MSG_SAVE_NOTOK);
    }
  });

  /* route for PrusaLink cfg /set_prusalink?enable=true&host=192.168.1.10&user=maker&password=xxx */
  server.on("/set_prusalink", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /set_prusalink"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    String msg = MSG_SAVE_OK;
    if (request->hasParam("host")) {
      String host = request->getParam("host")->value();
      if (host.length() < EEPROM_ADDR_PRUSALINK_HOST_LENGTH) {
        SystemPrusaLink.SetHost(host);
      } else {
        msg = "ERROR! Maximum host length is " + String(EEPROM_ADDR_PRUSALINK_HOST_LENGTH - 1);
      }
    }

    if (request->hasParam("user")) {
      String user = request->getParam("user")->value();
      if (user.length() < EEPROM_ADDR_PRUSALINK_USER_LENGTH) {
        SystemPrusaLink.SetUser(user);
      } else {
        msg = "ERROR! Maximum user name length is " + String(EEPROM_ADDR_PRUSALINK_USER_LENGTH - 1);
      }
    }

    /* empty password parameter keeps the saved password, the WEB page never receives the password */
    if (request->hasParam("password") && (request->getParam("password")->value().length() > 0)) {
      String password = request->getParam("password")->value();
      if (password.length() < EEPROM_ADDR_PRUSALINK_PASSWORD_LENGTH) {
        SystemPrusaLink.SetPassword(password);
      } else {
        msg = "ERROR! Maximum password length is " + String(EEPROM_ADDR_PRUSALINK_PASSWORD_LENGTH - 1);
      }
    }

    if (request->hasParam("enable")) {
      SystemPrusaLink.SetEnable(Server_TransfeStringToBool(request->getParam("enable")->value()));
    }

    request->send(200, F("text/html"), msg.c_str());
  });

  /* route for set prusa connect hostname /set_hostname?hostname=*/
  server.on("/set_hostname", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /set_hostname"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/html"), MSG_SAVE_OK);

    if (request->hasParam("hostname")) {
      Connect.SetPrusaConnectHostname(request->getParam("hostname")->value());
    }
  });

  /* route for set WI-FI credentials */
  server.on("/wifi_cfg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: set WI-FI credentials"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    String TmpPassword = "";
    String TmpSsid = "";
    /* get SSID */
    if (request->hasParam("wifi_ssid")) {
      TmpSsid = request->getParam("wifi_ssid")->value();
    }

    /* get password */
    if (request->hasParam("wifi_pass")) {
      TmpPassword = request->getParam("wifi_pass")->value();
    }

    /* check min and max length WI-FI ssid and password */
#if (WIFI_DISABLE_UNENCRYPTED_STA_PASS_CHECK == false)
    if (((TmpPassword.length() > 0) && (TmpSsid.length() > 0)) && ((TmpPassword.length() < EEPROM_ADDR_WIFI_PASSWORD_LENGTH) && (TmpSsid.length() < EEPROM_ADDR_WIFI_SSID_LENGTH))) {
#else
    if ((TmpSsid.length() > 0) && (TmpSsid.length() < EEPROM_ADDR_WIFI_SSID_LENGTH)) {
#endif

      /* send OK response */
      request->send(200, F("text/html"), MSG_SAVE_OK_WIFI);

      /* save ssid and password */
      SystemWifiMngt.SetStaCredentials(TmpSsid, TmpPassword);
      SystemWifiMngt.WiFiStaConnect();

    } else {
      request->send(200, F("text/html"), MSG_SAVE_NOTOK);
    }
  });

  /* route for set WI-FI static IP address */
  server.on("/wifi_net_cfg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: set WI-FI static IP address"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    String tmpIp = "";
    String tmpMask = "";
    String tmpGw = "";
    String tmpDns = "";

    /* get ip */
    if (request->hasParam("ip")) {
      tmpIp = request->getParam("ip")->value();
    }

    /* get mask */
    if (request->hasParam("mask")) {
      tmpMask = request->getParam("mask")->value();
    }

    /* get gw */
    if (request->hasParam("gw")) {
      tmpGw = request->getParam("gw")->value();
    }

    /* get dns */
    if (request->hasParam("dns")) {
      tmpDns = request->getParam("dns")->value();
    }

    /* check min and max length network parameters */
    if (((tmpIp.length() > 0) && (tmpMask.length() > 0) && (tmpGw.length() > 0) && (tmpDns.length() > 0)) && ((tmpIp.length() <= IPV4_ADDR_MAX_LENGTH) && (tmpMask.length() <= IPV4_ADDR_MAX_LENGTH) && (tmpGw.length() <= IPV4_ADDR_MAX_LENGTH) && (tmpDns.length() <= IPV4_ADDR_MAX_LENGTH))) {

      /* save ssid and password */
      SystemWifiMngt.SetNetworkConfig(tmpIp, tmpMask, tmpGw, tmpDns);

      /* send OK response */
      request->send(200, F("text/html"), MSG_SAVE_OK_REBOOT);

    } else {
      request->send(200, F("text/html"), MSG_SAVE_NOTOK);
    }
  });


  /* route for set basic auth */
  server.on("/basicauth_cfg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: set basic auth user name and password"));
    bool ret = false;
    String ret_msg = "";

    if (Server_CheckBasicAuth(request) == false)
      return;

    /* get username */
    if (request->hasParam("auth_username")) {
      WebBasicAuth.UserName = request->getParam("auth_username")->value();

      /* check min and max length  */
      if ((WebBasicAuth.UserName.length() > 0) && (WebBasicAuth.UserName.length() < EEPROM_ADDR_BASIC_AUTH_USERNAME_LENGTH)) {
        SystemConfig.SaveBasicAuthUsername(WebBasicAuth.UserName);
        ret = true;
      } else {
        ret = false;
        ret_msg = "Maximum username length: " + String(EEPROM_ADDR_BASIC_AUTH_USERNAME_LENGTH);
      }
    }

    /* get password */
    if (request->hasParam("auth_password")) {
      WebBasicAuth.Password = request->getParam("auth_password")->value();

      /* check min and max length  */
      if ((WebBasicAuth.Password.length() > 0) && (WebBasicAuth.Password.length() < EEPROM_ADDR_BASIC_AUTH_PASSWORD_LENGTH)) {
        SystemConfig.SaveBasicAuthPassword(WebBasicAuth.Password);
        ret = true;
      } else {
        ret = false;
        ret_msg = "Maximum password length: " + String(EEPROM_ADDR_BASIC_AUTH_PASSWORD_LENGTH);
      }
    }

    /* get enable / disable we bauth */
    if (request->hasParam("basicauth_enable")) {
      WebBasicAuth.EnableAuth = Server_TransfeStringToBool(request->getParam("basicauth_enable")->value());
      SystemConfig.SaveBasicAuthFlag(WebBasicAuth.EnableAuth);
      ret = true;
    }

    /* send OK response */
    if (true == ret) {
      request->send(200, F("text/html"), MSG_SAVE_OK);

    } else {
      String msg = MSG_SAVE_NOTOK;
      msg += " " + ret_msg;
      request->send(200, F("text/html"), msg.c_str());
    }
  });

  /* route for set mDNS record */
  server.on("/set_mdns", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /set_mdns"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    /* check cfg for mdns */
    if (request->hasParam("mdns")) {
      String tmp = request->getParam("mdns")->value();
      if (tmp.length() < EEPROM_ADDR_MDNS_RECORD_LENGTH) {
        request->send(200, F("text/html"), MSG_SAVE_OK_REBOOT);
        SystemWifiMngt.SetMdns(tmp);
        Connect.UpdateDeviceInformation();

      } else {
        String msg = "Error save mDNS. Maximum length: " + String(EEPROM_ADDR_MDNS_RECORD_LENGTH);
        request->send(200, F("text/html"), msg.c_str());
      }
    }
  });
}

/**
   @brief Init WEB firmware upload. The user uploads an application file (.bin) from the computer:
          this firmware or the original Prusa firmware. There is no download from the internet
   @param none
   @return none
*/
void Server_InitWebServer_Update() {
  server.on(
    "/upload_firmware", HTTP_POST, [](AsyncWebServerRequest* request) {
      if (Server_CheckBasicAuth(request) == false)
        return;

      if ((FirmwareUpload.Error.length() == 0) && (true == Update.isFinished())) {
        request->send(200, F("text/plain"), F("Firmware installed. The camera restarts now, the page reloads in 20 s"));
        FirmwareUpload.RestartPending = true;
      } else {
        String msg = (FirmwareUpload.Error.length() > 0) ? FirmwareUpload.Error : String(F("The upload is not complete"));
        request->send(400, F("text/plain"), "Firmware NOT installed: " + msg);
      }
    },
    [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
      if (0 == index) {
        /* an earlier upload was interrupted by the browser */
        if (true == Update.isRunning()) {
          Update.abort();
        }
        FirmwareUpload.Error = "";

        /* no answer from here, the request handler answers after the upload */
        if ((true == WebBasicAuth.EnableAuth) && (!request->authenticate(WebBasicAuth.UserName.c_str(), WebBasicAuth.Password.c_str()))) {
          FirmwareUpload.Error = F("Unauthorized");
        } else {
          FirmwareUpload.Error = Server_CheckFirmwareImage(data, len);
        }

        if (FirmwareUpload.Error.length() == 0) {
          SystemLog.AddEvent(LogLevel_Warning, F("Firmware upload started: "), filename);
          FirmwareUpload.Active = true;
          FirmwareUpload.LastData = millis();
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            FirmwareUpload.Error = "Update start failed: " + String(Update.errorString());
          }
        }

        if (FirmwareUpload.Error.length() > 0) {
          SystemLog.AddEvent(LogLevel_Error, F("Firmware upload rejected: "), FirmwareUpload.Error);
          FirmwareUpload.Active = false;
        }
      }

      /* after an error, the rest of the file is ignored */
      if (FirmwareUpload.Error.length() > 0) {
        return;
      }

      FirmwareUpload.LastData = millis();
      if (Update.write(data, len) != len) {
        FirmwareUpload.Error = "Flash write failed: " + String(Update.errorString());
        SystemLog.AddEvent(LogLevel_Error, FirmwareUpload.Error);
        Update.abort();
        FirmwareUpload.Active = false;
        return;
      }

      if (final) {
        /* checks the image hash and sets the new firmware for the next start */
        if (true == Update.end(true)) {
          SystemLog.AddEvent(LogLevel_Warning, "Firmware upload done: " + String(index + len) + " B");
        } else {
          FirmwareUpload.Error = "Firmware check failed: " + String(Update.errorString());
          SystemLog.AddEvent(LogLevel_Error, FirmwareUpload.Error);
        }
        FirmwareUpload.Active = false;
      }
    });
}

/**
   @brief Check the beginning of an uploaded firmware file, before anything is written to the flash.
          Accepts only an application image for this chip. The bootloader, the partition table and the merged .bin
          (bootloader + partitions + application) are rejected, they would make the MCU unbootable
   @param const uint8_t* - first part of the file
   @param size_t - length
   @return String - error message. Empty = OK
*/
String Server_CheckFirmwareImage(const uint8_t* i_data, size_t i_len) {
  const size_t desc_offset = sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t);
  if (i_len < (desc_offset + offsetof(esp_app_desc_t, time))) {
    return F("The first part of the upload is too small. Try again");
  }

  esp_image_header_t header;
  esp_app_desc_t desc;
  memcpy(&header, i_data, sizeof(header));
  memcpy(&desc, i_data + desc_offset, offsetof(esp_app_desc_t, time));

  if (ESP_IMAGE_HEADER_MAGIC != header.magic) {
    return F("This is not a firmware file");
  }
  if (CONFIG_IDF_FIRMWARE_CHIP_ID != header.chip_id) {
    return F("This firmware is for another chip, for example ESP32 instead of ESP32-S3");
  }
  if (ESP_APP_DESC_MAGIC_WORD != desc.magic_word) {
    return F("This is not an application file. Use ESP32_PrusaConnectCam.ino.bin, not the merged.bin or the bootloader.bin");
  }

  char project[sizeof(desc.project_name) + 1] = { '\0' };
  char version[sizeof(desc.version) + 1] = { '\0' };
  memcpy(project, desc.project_name, sizeof(desc.project_name));
  memcpy(version, desc.version, sizeof(desc.version));
  SystemLog.AddEvent(LogLevel_Warning, "Firmware upload: project " + String(project) + ", version " + String(version));

  return "";
}

/**
   @brief Init WEB server stream
   @param none
   @return none
*/
void Server_InitWebServer_Stream() {
  server.on("/stream.mjpg", HTTP_GET, Server_streamJpg);
}

/**
   @brief Pause WEB server
   @param none
   @return none
*/
void Server_pause() {
  server.end();
  SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: pause"));
}

/**
   @brief Resume WEB server
   @param none
   @return none
*/
void Server_resume() {
  server.begin();
  SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: resume"));
}

/**
 * @brief Handle cache request
 * 
 * @param AsyncWebServerRequest* - http request
 * @param const char* - content type
 * @param const char* - data
 */
void Server_handleCacheRequest(AsyncWebServerRequest* request, const char* contentType, const char* data) {
  /* the data is in the flash memory, the response reads it directly. The length is computed once, not for every chunk */
  AsyncWebServerResponse* response = request->beginResponse(200, contentType, (const uint8_t*)data, strlen(data));

  /* HTML pages must not be cached, otherwise the browser shows the old pages after a firmware update.
     Other files are cached, styles.css is loaded with the firmware version in the URL */
  if (strcmp(contentType, "text/html") == 0) {
    response->addHeader("Cache-Control", "no-cache");
  } else {
    response->addHeader("Cache-Control", "public, max-age=" + String(WEB_CACHE_INTERVAL));
  }
  request->send(response);
}

/**
   @brief if the page was not found on ESP, then print which page is not there
   @param AsyncWebServerRequest* - request
   @return none
*/
void Server_handleNotFound(AsyncWebServerRequest* request) {
  String message = F("URL not Found\n\n");

  message += "URI: " + request->url() + "\nMethod: ";
  message += (request->method() == HTTP_GET) ? F("GET") : F("POST");
  message += "\nArguments: " + String(request->args()) + "\n";

  for (uint8_t i = 0; i < request->args(); i++) {
    message += " " + request->argName(i) + ": " + request->arg(i) + "\n";
  }

  request->send(404, F("text/plain"), message);
}

/**
   @brief make json data for WEB page on the ESP32
   @param none
   @return String - json data
*/
String Server_GetJsonData() {
  String uptime = "";
  String string_json = "";
  JsonDocument doc_json;
  Server_GetModuleUptime(uptime);

  doc_json["token"] = Connect.GetToken();
  doc_json["fingerprint"] = Connect.GetFingerprint();
  doc_json["refreshInterval"] = String(Connect.GetRefreshInterval());
  doc_json["photoquality"] = String(73 - SystemCamera.GetPhotoQuality());
  doc_json["framesize"] = String(SystemCamera.GetFrameSize());
  doc_json["framesize_supported"] = SystemCamera.GetSupportedFrameSizes();
  doc_json["camera_detected"] = SystemCamera.GetDetectedSensorName();
  doc_json["camera_autofocus"] = SystemCamera.GetAutofocusStatus();
  doc_json["brightness"] = String(SystemCamera.GetBrightness());
  doc_json["contrast"] = String(SystemCamera.GetContrast());
  doc_json["saturation"] = String(SystemCamera.GetSaturation());
  doc_json["hmirror"] = Server_TranslateBoolToString(SystemCamera.GetHMirror());
  doc_json["vflip"] = Server_TranslateBoolToString(SystemCamera.GetVFlip());
  doc_json["lensc"] = Server_TranslateBoolToString(SystemCamera.GetLensC());
  doc_json["exposure_ctrl"] = Server_TranslateBoolToString(SystemCamera.GetExposureCtrl());
  doc_json["awb"] = Server_TranslateBoolToString(SystemCamera.GetAwb());
  doc_json["awb_gain"] = Server_TranslateBoolToString(SystemCamera.GetAwbGain());
  doc_json["wb_mode"] = String(SystemCamera.GetAwbMode());
  doc_json["bpc"] = Server_TranslateBoolToString(SystemCamera.GetBpc());
  doc_json["wpc"] = Server_TranslateBoolToString(SystemCamera.GetWpc());
  doc_json["raw_gama"] = Server_TranslateBoolToString(SystemCamera.GetRawGama());
  doc_json["aec2"] = Server_TranslateBoolToString(SystemCamera.GetAec2());
  doc_json["ae_level"] = SystemCamera.GetAeLevel();
  doc_json["aec_value"] = SystemCamera.GetAecValue();
  doc_json["gain_ctrl"] = Server_TranslateBoolToString(SystemCamera.GetGainCtrl());
  doc_json["agc_gain"] = SystemCamera.GetAgcGaint();
  doc_json["led"] = Server_TranslateBoolToString(SystemCamera.GetFlashStatus());
  doc_json["flash"] = Server_TranslateBoolToString(SystemCamera.GetCameraFlashEnable());
  doc_json["flash_time"] = SystemCamera.GetCameraFlashTime();
  doc_json["ssid"] = SystemWifiMngt.GetStaSsid();
  doc_json["bssid"] = SystemWifiMngt.GetStaBssid();
  doc_json["rssi"] = String(WiFi.RSSI());
  doc_json["rssi_percentage"] = String(SystemWifiMngt.Rssi2Percent(WiFi.RSSI()));
  doc_json["tx_power"] = SystemWifiMngt.TranslateTxPower(WiFi.getTxPower());
  doc_json["ip"] = WiFi.localIP().toString();
  doc_json["wifi_mode"] = SystemWifiMngt.GetWiFiMode();
  doc_json["mdns"] = SystemWifiMngt.GetMdns();
  doc_json["service_ap_ssid"] = SystemWifiMngt.GetServiceApSsid();
  doc_json["serviceap"] = Server_TranslateBoolToString(SystemWifiMngt.GetEnableServiceAp());
  doc_json["auth"] = Server_TranslateBoolToString(WebBasicAuth.EnableAuth);
  doc_json["auth_username"] = WebBasicAuth.UserName;
  doc_json["last_upload_status"] = Connect.GetBackendReceivedStatus();
  doc_json["wifi_network_status"] = SystemWifiMngt.GetStaStatus();
  doc_json["log_level"] = String(SystemLog.GetLogLevel());
  doc_json["uptime"] = uptime;
  doc_json["user_name"] = WebBasicAuth.UserName;
  doc_json["hostname"] = Connect.GetPrusaConnectHostname();
  doc_json["ip_cfg"] = SystemWifiMngt.GetNetIpMethod();
  doc_json["net_ip"] = SystemWifiMngt.GetNetStaticIp();
  doc_json["net_mask"] = SystemWifiMngt.GetNetStaticMask();
  doc_json["net_gw"] = SystemWifiMngt.GetNetStaticGateway();
  doc_json["net_dns"] = SystemWifiMngt.GetNetStaticDns();
  doc_json["image_rotation"] = SystemCamera.GetCameraImageRotation();
  doc_json["timelaps"] = Server_TranslateBoolToString(Connect.GetTimeLapsPhotoSaveStatus());
  doc_json["sd_status"] = (SystemLog.GetCardDetectedStatus() == true) ? F("Card detected") : F("No card detected");
  doc_json["sd_total"] = SystemLog.GetCardSizeMB();
  doc_json["sd_free_p"] = SystemLog.GetFreeSpacePercent();
  doc_json["sd_used_p"] = SystemLog.GetUsedSpacePercent();
  doc_json["mcu_temp"] = String(McuTemperature.TemperatureCelsius) + " *C" + ((true == McuTemperature.ThermalThrottling) ? " - TOO HOT, stream disabled, uploads slowed down" : "");
  doc_json["thermal_throttling"] = Server_TranslateBoolToString(McuTemperature.ThermalThrottling);
  doc_json["upload_interval"] = String(System_GetUploadInterval());
  doc_json["health_status"] = (Connect.GetHealthStatus().length() > 0) ? Connect.GetHealthStatus() : String(F("OK"));
  doc_json["upload_ok"] = Connect.GetUploadOkCount();
  doc_json["upload_failed"] = Connect.GetUploadFailedCount();
  doc_json["unplanned_restarts"] = SystemHealth.UnplannedRestarts;
  doc_json["last_restart"] = SystemHealth.LastRestartReason;
  doc_json["mcu_temp_max"] = String(McuTemperature.TemperatureMax) + " *C";
  doc_json["mem_internal"] = String(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024) + " KB free, largest block " + String(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) / 1024) + " KB";
  doc_json["mem_psram"] = String(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024) + " KB free, largest block " + String(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024) + " KB";
  doc_json["backoff_interval"] = String(Connect.GetBackoffInterval());
  doc_json["prusalink_enable"] = Server_TranslateBoolToString(SystemPrusaLink.GetEnable());
  doc_json["prusalink_host"] = SystemPrusaLink.GetHost();
  doc_json["prusalink_user"] = SystemPrusaLink.GetUser();
  doc_json["prusalink_status"] = SystemPrusaLink.GetStatus();
  doc_json["printer_state"] = SystemPrusaLink.GetPrinterState();
  doc_json["job_progress"] = String(SystemPrusaLink.GetJobProgress(), 0);
  doc_json["sw_build"] = SW_BUILD;
  doc_json["sw_ver"] = SW_VERSION;
  doc_json["extsens_stat"] = ExternalTemperatureSensor.GetSensorStatus();
  doc_json["extsen_en"] = ExternalTemperatureSensor.GetUserEnableSensor();
  doc_json["ext_temp"] = ExternalTemperatureSensor.GetTemperatureString();
  doc_json["ext_hum"] = ExternalTemperatureSensor.GetHumidityString();
  doc_json["exttemp_unit"] = ExternalTemperatureSensor.GetTemperatureUnit();

  serializeJson(doc_json, string_json);
  SystemLog.AddEvent(LogLevel_Verbose, string_json);
  return string_json;
}

/**
   @brief If basic auth is enabled, then check if the user is logged in
   @param AsyncWebServerRequest - request
   @return bool - status
*/
bool Server_CheckBasicAuth(AsyncWebServerRequest* request) {
  if ((!request->authenticate(WebBasicAuth.UserName.c_str(), WebBasicAuth.Password.c_str())) && (true == WebBasicAuth.EnableAuth)) {
    SystemLog.AddEvent(LogLevel_Verbose, F("Unauthorized! Sending longin request"));
    request->requestAuthentication();
    return false;
  }

  return true;
}

/**
   @brief Stream JPG image from camera
   @param AsyncWebServerRequest - request
   @return void
*/
void Server_streamJpg(AsyncWebServerRequest* request) {
  SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /stream.mjpg"));
  if (Server_CheckBasicAuth(request) == false)
    return;

  if (true == McuTemperature.ThermalThrottling) {
    request->send(503, "text/plain", "MCU temperature is too high, the stream is disabled until the camera cools down");
    return;
  }

  /* every client gets the latest frame from the stream buffers, the count is limited by the WiFi bandwidth */
  if (SystemCamera.GetStreamClients() >= CAMERA_STREAM_MAX_CLIENTS) {
    request->send(503, "text/plain", "The stream is already open in " + String(CAMERA_STREAM_MAX_CLIENTS) + " windows. Close one of them first");
    return;
  }

  AsyncJpegStreamResponse* response = new AsyncJpegStreamResponse(&SystemCamera, &SystemLog);
  if (!response) {
    request->send(501);
    return;
  }
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

/**
   @brief Get module uptime
   @param String - uptime value
   @return void
*/
void Server_GetModuleUptime(String& readableTime) {
  unsigned long currentMillis;
  unsigned long seconds;
  unsigned long minutes;
  unsigned long hours;
  unsigned long days;

  currentMillis = millis();
  seconds = currentMillis / 1000;
  minutes = seconds / 60;
  hours = minutes / 60;
  days = hours / 24;
  currentMillis %= 1000;
  seconds %= 60;
  minutes %= 60;
  hours %= 24;

  readableTime = String(days) + " days, ";

  if (hours < 10) {
    readableTime += "0";
  }
  readableTime += String(hours) + ":";

  if (minutes < 10) {
    readableTime += "0";
  }
  readableTime += String(minutes) + ":";

  if (seconds < 10) {
    readableTime += "0";
  }
  readableTime += String(seconds);
}

/**
   @brief Convert string bool variable to bool
   @param String - data
   @return bool - status
*/
bool Server_TransfeStringToBool(String data) {
  bool ret = false;
  if (data.indexOf("true") >= 0) {
    ret = true;
  } else if (data.indexOf("false") >= 0) {
    ret = false;
  }

  return ret;
}

String Server_TranslateBoolToString(bool i_data) {
  String ret = "";
  if (true == i_data) {
    ret = "true";
  } else {
    ret = "";
  }

  return ret;
}

/* EOF */
