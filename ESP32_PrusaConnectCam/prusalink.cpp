/**
   @file prusalink.cpp

   @brief Library for reading the printer state from the local PrusaLink API

   PrusaLink uses HTTP digest authentication. User name is "maker" and the password is
   shown on the printer in Settings -> Network -> PrusaLink.

   @bug: no know bug
*/

#include "prusalink.h"

PrusaLink SystemPrusaLink(&SystemConfig, &SystemLog);

/**
 * @brief Constructor for PrusaLink class
 *
 * @param Configuration* - pointer to Configuration class
 * @param Logs* - pointer to Logs class
 */
PrusaLink::PrusaLink(Configuration *i_conf, Logs *i_log) {
  config = i_conf;
  log = i_log;
  Enable = false;
  PollCounter = PRUSALINK_POLL_INTERVAL;
  PollInterval = PRUSALINK_POLL_INTERVAL;
  DigestQop = false;
  DigestNonceCount = 0;
  ResetDigest = false;
  CfgMutex = xSemaphoreCreateMutex();
  ClearState();
}

/**
 * @brief Load configuration from EEPROM
 *
 * @param none
 * @return none
 */
void PrusaLink::LoadCfgFromEeprom() {
  log->AddEvent(LogLevel_Info, F("Load PrusaLink CFG from EEPROM"));
  Enable = config->LoadPrusaLinkEnable();
  Host = config->LoadPrusaLinkHost();
  User = config->LoadPrusaLinkUser();
  Password = config->LoadPrusaLinkPassword();
}

/**
 * @brief Clear printer state. Without a printer state the camera uploads with the normal interval
 *
 * @param none
 * @return none
 */
void PrusaLink::ClearState() {
  SetPrinterState(F("N/A"));
  JobProgress = 0.0;
  Reachable = false;
  LoginError = false;
  Printing = false;
  StateChanged = false;
  SetStatus(Enable ? F("Waiting for the first request") : F("Disabled"));
}

/**
 * @brief Set the status message. The WEB server task reads it, both tasks write it
 *
 * @param String - status
 * @return none
 */
void PrusaLink::SetStatus(String i_data) {
  xSemaphoreTake(CfgMutex, portMAX_DELAY);
  Status = i_data;
  xSemaphoreGive(CfgMutex);
}

/**
 * @brief Set the printer state. The WEB server task reads it, both tasks write it
 *
 * @param String - printer state
 * @return none
 */
void PrusaLink::SetPrinterState(String i_data) {
  xSemaphoreTake(CfgMutex, portMAX_DELAY);
  PrinterState = i_data;
  xSemaphoreGive(CfgMutex);
}

/**
 * @brief Read printer state every PRUSALINK_POLL_INTERVAL. Call it once per second
 *
 * @param none
 * @return none
 */
void PrusaLink::Loop() {
  /* copy of the settings, the WEB server task can change them during the request */
  xSemaphoreTake(CfgMutex, portMAX_DELAY);
  String host = Host;
  String user = User;
  String password = Password;
  xSemaphoreGive(CfgMutex);

  if ((false == Enable) || (host.length() == 0)) {
    return;
  }

  /* new credentials: the old digest nonce is not valid for them */
  if (true == ResetDigest) {
    ResetDigest = false;
    DigestNonce = "";
  }

  PollCounter++;
  if (PollCounter < PollInterval) {
    return;
  }
  PollCounter = 0;

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  String body = "";
  int code = RequestStatus(host, user, password, &body);

  /* a printer that is switched off would block the photo task for the connect timeout on every request */
  PollInterval = (200 == code) ? PRUSALINK_POLL_INTERVAL : PRUSALINK_RETRY_INTERVAL;
  LoginError = (401 == code);

  if (200 == code) {
    ProcessStatus(body);
  } else {
    if (Reachable) {
      log->AddEvent(LogLevel_Warning, F("PrusaLink: printer is not reachable. Code: "), String(code));
    }
    Reachable = false;
    Printing = false;
    SetPrinterState(F("N/A"));
    if (401 == code) {
      SetStatus(F("Wrong PrusaLink user name or password"));
    } else if (code <= 0) {
      SetStatus(F("Printer is not reachable"));
    } else {
      SetStatus("PrusaLink error: " + String(code));
    }
  }
}

/**
 * @brief Send GET /api/v1/status, with digest authentication
 *
 * @param String - printer hostname or IP address
 * @param String - user name
 * @param String - password
 * @param String * - output, response body
 * @return int - HTTP code. Value <= 0 is a connection error
 */
int PrusaLink::RequestStatus(String i_host, String i_user, String i_password, String *o_body) {
  const char *uri = "/api/v1/status";
  const char *headerKeys[] = { "WWW-Authenticate" };
  int code = 0;

  /* first attempt with the cached nonce, second attempt with a new nonce after 401 */
  for (uint8_t attempt = 0; attempt < 2; attempt++) {
    WiFiClient client;
    HTTPClient http;
    http.setConnectTimeout(PRUSALINK_HTTP_TIMEOUT);
    http.setTimeout(PRUSALINK_HTTP_TIMEOUT);
    http.setReuse(false);
    http.collectHeaders(headerKeys, 1);

    if (!http.begin(client, i_host, 80, uri)) {
      return -1;
    }
    http.addHeader(F("Accept"), F("application/json"));
    if (DigestNonce.length() > 0) {
      http.addHeader(F("Authorization"), GetDigestAuthorization(F("GET"), uri, i_user, i_password));
    }

    code = http.GET();
    if (200 == code) {
      *o_body = http.getString();
      http.end();
      return code;
    }

    if ((401 == code) && (attempt == 0) && ParseDigestChallenge(http.header("WWW-Authenticate"))) {
      http.end();
      continue;
    }

    http.end();
    break;
  }

  return code;
}

/**
 * @brief Parse digest authentication challenge
 *
 * @param String - WWW-Authenticate header
 * @return bool - true if the challenge is valid
 */
bool PrusaLink::ParseDigestChallenge(String i_header) {
  if (!i_header.startsWith(F("Digest"))) {
    log->AddEvent(LogLevel_Warning, F("PrusaLink: unsupported authentication: "), i_header);
    return false;
  }

  DigestRealm = GetDigestParam(i_header, F("realm"));
  DigestNonce = GetDigestParam(i_header, F("nonce"));
  DigestOpaque = GetDigestParam(i_header, F("opaque"));
  DigestQop = (GetDigestParam(i_header, F("qop")).indexOf(F("auth")) >= 0);
  DigestNonceCount = 0;

  return (DigestNonce.length() > 0);
}

/**
 * @brief Get parameter from digest authentication header. For example realm="Printer API"
 *
 * @param String - header
 * @param String - parameter name
 * @return String - parameter value
 */
String PrusaLink::GetDigestParam(String i_header, String i_name) {
  int start = i_header.indexOf(i_name + "=");
  if (start < 0) {
    return "";
  }

  start += i_name.length() + 1;
  int end = 0;
  if (i_header.charAt(start) == '"') {
    start++;
    end = i_header.indexOf('"', start);
  } else {
    end = i_header.indexOf(',', start);
  }

  if (end < 0) {
    end = i_header.length();
  }

  return i_header.substring(start, end);
}

/**
 * @brief Make digest authorization header value, RFC 7616 with MD5
 *
 * @param String - HTTP method
 * @param String - request uri
 * @param String - user name
 * @param String - password
 * @return String - Authorization header value
 */
String PrusaLink::GetDigestAuthorization(String i_method, String i_uri, String i_user, String i_password) {
  String ha1 = Md5(i_user + ":" + DigestRealm + ":" + i_password);
  String ha2 = Md5(i_method + ":" + i_uri);
  String auth = "Digest username=\"" + i_user + "\", realm=\"" + DigestRealm + "\", nonce=\"" + DigestNonce + "\", uri=\"" + i_uri + "\"";

  if (DigestQop) {
    char nc[9];
    DigestNonceCount++;
    snprintf(nc, sizeof(nc), "%08x", (unsigned int)DigestNonceCount);
    String cnonce = String(esp_random(), HEX);
    String response = Md5(ha1 + ":" + DigestNonce + ":" + nc + ":" + cnonce + ":auth:" + ha2);
    auth += ", qop=auth, nc=" + String(nc) + ", cnonce=\"" + cnonce + "\", response=\"" + response + "\"";
  } else {
    String response = Md5(ha1 + ":" + DigestNonce + ":" + ha2);
    auth += ", response=\"" + response + "\"";
  }

  if (DigestOpaque.length() > 0) {
    auth += ", opaque=\"" + DigestOpaque + "\"";
  }

  return auth;
}

/**
 * @brief MD5 hash as hex string
 *
 * @param String - data
 * @return String - hash
 */
String PrusaLink::Md5(String i_data) {
  MD5Builder md5;
  md5.begin();
  md5.add(i_data);
  md5.calculate();
  return md5.toString();
}

/**
 * @brief Process response from /api/v1/status
 *
 * @param String - JSON body
 * @return none
 */
void PrusaLink::ProcessStatus(String i_body) {
  JsonDocument filter;
  filter["printer"]["state"] = true;
  filter["job"]["progress"] = true;

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, i_body, DeserializationOption::Filter(filter));
  if (error) {
    Reachable = false;
    String status = "Invalid response from PrusaLink: " + String(error.c_str());
    SetStatus(status);
    log->AddEvent(LogLevel_Warning, status);
    return;
  }

  String state = doc["printer"]["state"] | "N/A";
  JobProgress = doc["job"]["progress"] | 0.0;
  state.toUpperCase();

  /* printing, paused, needs attention or busy (calibration, homing, ...) is an active printer */
  bool printing = ((state == F("PRINTING")) || (state == F("PAUSED")) || (state == F("ATTENTION")) || (state == F("BUSY")));

  if ((Reachable) && (printing != Printing)) {
    StateChanged = true;
  }
  if (state != GetPrinterState()) {
    log->AddEvent(LogLevel_Info, F("PrusaLink: printer state: "), state);
  }

  SetPrinterState(state);
  Printing = printing;
  Reachable = true;
  SetStatus(F("OK"));
}

/**
 * @brief Set enable flag
 *
 * @param bool - enable
 * @return none
 */
void PrusaLink::SetEnable(bool i_data) {
  Enable = i_data;
  config->SavePrusaLinkEnable(Enable);
  ClearState();
  PollCounter = PRUSALINK_POLL_INTERVAL;
  PollInterval = PRUSALINK_POLL_INTERVAL;
}

/**
 * @brief Set printer hostname or IP address
 *
 * @param String - hostname
 * @return none
 */
void PrusaLink::SetHost(String i_data) {
  /* accept "http://192.168.1.10/" from the browser address bar */
  i_data.trim();
  if (i_data.startsWith(F("http://"))) {
    i_data = i_data.substring(7);
  }
  while (i_data.endsWith(F("/"))) {
    i_data.remove(i_data.length() - 1);
  }
  xSemaphoreTake(CfgMutex, portMAX_DELAY);
  Host = i_data;
  xSemaphoreGive(CfgMutex);
  config->SavePrusaLinkHost(i_data);
  ResetDigest = true;
  ClearState();
  PollCounter = PRUSALINK_POLL_INTERVAL;
  PollInterval = PRUSALINK_POLL_INTERVAL;
}

/**
 * @brief Set PrusaLink user name
 *
 * @param String - user name
 * @return none
 */
void PrusaLink::SetUser(String i_data) {
  String user = (i_data.length() > 0) ? i_data : String(PRUSALINK_DEFAULT_USER);
  xSemaphoreTake(CfgMutex, portMAX_DELAY);
  User = user;
  xSemaphoreGive(CfgMutex);
  config->SavePrusaLinkUser(user);
  ResetDigest = true;
  PollCounter = PRUSALINK_POLL_INTERVAL;
  PollInterval = PRUSALINK_POLL_INTERVAL;
}

/**
 * @brief Set PrusaLink password
 *
 * @param String - password
 * @return none
 */
void PrusaLink::SetPassword(String i_data) {
  xSemaphoreTake(CfgMutex, portMAX_DELAY);
  Password = i_data;
  xSemaphoreGive(CfgMutex);
  config->SavePrusaLinkPassword(i_data);
  ResetDigest = true;
  PollCounter = PRUSALINK_POLL_INTERVAL;
  PollInterval = PRUSALINK_POLL_INTERVAL;
}

bool PrusaLink::GetEnable() {
  return Enable;
}

String PrusaLink::GetHost() {
  xSemaphoreTake(CfgMutex, portMAX_DELAY);
  String ret = Host;
  xSemaphoreGive(CfgMutex);
  return ret;
}

String PrusaLink::GetUser() {
  xSemaphoreTake(CfgMutex, portMAX_DELAY);
  String ret = User;
  xSemaphoreGive(CfgMutex);
  return ret;
}

bool PrusaLink::GetReachable() {
  return Reachable;
}

bool PrusaLink::GetPrinting() {
  return Printing;
}

/**
 * @brief Check if the printing state changed. The flag is cleared by this call
 *
 * @return bool - true if the printer started or finished printing
 */
bool PrusaLink::ConsumeStateChange() {
  bool ret = StateChanged;
  StateChanged = false;
  return ret;
}

String PrusaLink::GetPrinterState() {
  xSemaphoreTake(CfgMutex, portMAX_DELAY);
  String ret = PrinterState;
  xSemaphoreGive(CfgMutex);
  return ret;
}

float PrusaLink::GetJobProgress() {
  return JobProgress;
}

String PrusaLink::GetStatus() {
  xSemaphoreTake(CfgMutex, portMAX_DELAY);
  String ret = Status;
  xSemaphoreGive(CfgMutex);
  return ret;
}

bool PrusaLink::GetLoginError() {
  return LoginError;
}

/* EOF */
