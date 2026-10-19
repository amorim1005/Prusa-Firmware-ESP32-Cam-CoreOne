/**
   @file prusalink.h

   @brief Library for reading the printer state from the local PrusaLink API

   The camera reads GET /api/v1/status from the printer and uploads photos often only during printing.
   https://github.com/prusa3d/Prusa-Link-Web/blob/master/spec/openapi.yaml

   @bug: no know bug
*/
#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <MD5Builder.h>
#include <ArduinoJson.h>

#include "mcu_cfg.h"
#include "log.h"
#include "cfg.h"

class Configuration;
class Logs;

class PrusaLink {
private:
  bool Enable;                    ///< enable reading printer state
  String Host;                    ///< printer hostname or IP address
  String User;                    ///< PrusaLink user name
  String Password;                ///< PrusaLink password

  String PrinterState;            ///< last printer state from PrusaLink, for example IDLE, PRINTING
  float JobProgress;              ///< job progress [%]
  bool Reachable;                 ///< last request was successful
  bool LoginError;                ///< wrong PrusaLink user name or password
  bool Printing;                  ///< printer is printing, paused or needs attention
  bool StateChanged;              ///< printing state changed since the last call of ConsumeStateChange
  String Status;                  ///< status message for WEB page
  uint16_t PollCounter;           ///< seconds since the last request
  uint16_t PollInterval;          ///< interval for the next request [s]. Longer when the printer does not respond

  String DigestRealm;             ///< digest auth realm
  String DigestNonce;             ///< digest auth nonce
  String DigestOpaque;            ///< digest auth opaque
  bool DigestQop;                 ///< digest auth with qop=auth
  uint32_t DigestNonceCount;      ///< digest auth nonce count

  Configuration *config;          ///< pointer to configuration object
  Logs *log;                      ///< pointer to logs object

  int RequestStatus(String *);
  String GetDigestAuthorization(String, String);
  bool ParseDigestChallenge(String);
  String GetDigestParam(String, String);
  String Md5(String);
  void ProcessStatus(String);
  void ClearState();

public:
  PrusaLink(Configuration *, Logs *);
  ~PrusaLink(){};

  void LoadCfgFromEeprom();
  void Loop();

  void SetEnable(bool);
  void SetHost(String);
  void SetUser(String);
  void SetPassword(String);

  bool GetEnable();
  String GetHost();
  String GetUser();
  bool GetReachable();
  bool GetPrinting();
  bool ConsumeStateChange();
  String GetPrinterState();
  float GetJobProgress();
  String GetStatus();
  bool GetLoginError();
};

extern PrusaLink SystemPrusaLink; ///< PrusaLink object

/* EOF */
