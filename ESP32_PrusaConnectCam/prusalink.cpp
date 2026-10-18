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
  DigestQop = false;
  DigestNonceCount = 0;
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
  PrinterState = F("N/A");
  JobProgress = 0.0;
  Reachable = false;
  Printing = false;
  StateChanged = false;
  Status = Enable ? F("Waiting for the first request") : F("Disabled");
}

/**
 * @brief Read printer state every PRUSALINK_POLL_INTERVAL. Call it once per second
 *
 * @param none
 * @return none
 */
void PrusaLink::Loop() {
  if ((false == Enable) || (Host.length() == 0)) {
    return;
  }

  PollCounter++;
  if (PollCounter < PRUSALINK_POLL_INTERVAL) {
    return;
  }
  PollCounter = 0;

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  String body = "";
  int code = RequestStatus(&body);

  if (200 == code) {
    ProcessStatus(body);
  } else {
    if (Reachable) {
      log->AddEvent(LogLevel_Warning, F("PrusaLink: printer is not reachable. Code: "), String(code));
    }
    Reachable = false;
    Printing = false;
    PrinterState = F("N/A");
    if (401 == code) {
      Status = F("Wrong PrusaLink user name or password");
    } else if (code <= 0) {
      Status = F("Printer is not reachable");
    } else {
      Status = "PrusaLink error: " + String(code);
    }
  }
}

/**
 * @brief Send GET /api/v1/status, with digest authentication
 *
 * @param String * - output, response body
 * @return int - HTTP code. Value <= 0 is a connection error
 */
int PrusaLink::RequestStatus(String *o_body) {
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

    if (!http.begin(client, Host, 80, uri)) {
      return -1;
    }
    http.addHeader(F("Accept"), F("application/json"));
    if (DigestNonce.length() > 0) {
      http.addHeader(F("Authorization"), GetDigestAuthorization(F("GET"), uri));
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
 * @return String - Authorization header value
 */
String PrusaLink::GetDigestAuthorization(String i_method, String i_uri) {
  String ha1 = Md5(User + ":" + DigestRealm + ":" + Password);
  String ha2 = Md5(i_method + ":" + i_uri);
  String auth = "Digest username=\"" + User + "\", realm=\"" + DigestRealm + "\", nonce=\"" + DigestNonce + "\", uri=\"" + i_uri + "\"";

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
    Status = "Invalid response from PrusaLink: " + String(error.c_str());
    log->AddEvent(LogLevel_Warning, Status);
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
  if (state != PrinterState) {
    log->AddEvent(LogLevel_Info, F("PrusaLink: printer state: "), state);
  }

  PrinterState = state;
  Printing = printing;
  Reachable = true;
  Status = F("OK");
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
  Host = i_data;
  config->SavePrusaLinkHost(Host);
  DigestNonce = "";
  ClearState();
  PollCounter = PRUSALINK_POLL_INTERVAL;
}

/**
 * @brief Set PrusaLink user name
 *
 * @param String - user name
 * @return none
 */
void PrusaLink::SetUser(String i_data) {
  User = (i_data.length() > 0) ? i_data : String(PRUSALINK_DEFAULT_USER);
  config->SavePrusaLinkUser(User);
  DigestNonce = "";
  PollCounter = PRUSALINK_POLL_INTERVAL;
}

/**
 * @brief Set PrusaLink password
 *
 * @param String - password
 * @return none
 */
void PrusaLink::SetPassword(String i_data) {
  Password = i_data;
  config->SavePrusaLinkPassword(Password);
  DigestNonce = "";
  PollCounter = PRUSALINK_POLL_INTERVAL;
}

bool PrusaLink::GetEnable() {
  return Enable;
}

String PrusaLink::GetHost() {
  return Host;
}

String PrusaLink::GetUser() {
  return User;
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
  return PrinterState;
}

float PrusaLink::GetJobProgress() {
  return JobProgress;
}

String PrusaLink::GetStatus() {
  return Status;
}

/* EOF */
