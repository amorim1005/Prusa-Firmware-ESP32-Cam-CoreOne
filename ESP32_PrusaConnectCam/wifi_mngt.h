/**
   @file wifi_mngt.h

   @brief 

   @author Miroslav Pivovarsky
   Contact: miroslav.pivovarsky@gmail.com

   @bug: no know bug
*/
#pragma once

#include <WiFi.h>
#include <Arduino.h>
#include <esp_task_wdt.h>
#include <ESPmDNS.h>
#include <esp_wifi.h>
#include <esp_eap_client.h>
#include <esp32-hal-cpu.h>

#include "mcu_cfg.h"
#include "var.h"
#include "log.h"
#include "system.h"
#include "camera.h"
#include "sys_led.h"

/* https://github.com/espressif/arduino-esp32/blob/51cb927712e512664a0a0f7b1219fdc18e11b857/libraries/WiFi/src/WiFiGeneric.h#L52 */
/* https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/network/esp_wifi.html#_CPPv429wifi_event_sta_disconnected_t */
void WiFiMngt_WiFiEventScanDone(WiFiEvent_t, WiFiEventInfo_t);
void WiFiMngt_WiFiEventStationStart(WiFiEvent_t, WiFiEventInfo_t);
void WiFiMngt_WiFiEventStationStop(WiFiEvent_t, WiFiEventInfo_t);
void WiFiMngt_WiFiEventStationConnected(WiFiEvent_t, WiFiEventInfo_t);
void WiFiMngt_WiFiEventStationDisconnected(WiFiEvent_t, WiFiEventInfo_t);
void WiFiMngt_WiFiEventGotIP(WiFiEvent_t, WiFiEventInfo_t);
void WiFiMngt_WiFiEventLostIP(WiFiEvent_t, WiFiEventInfo_t);
void WiFiMngt_WiFiEventApStart(WiFiEvent_t , WiFiEventInfo_t);
void WiFiMngt_WiFiEventApStop(WiFiEvent_t , WiFiEventInfo_t);
void WiFiMngt_WiFiEventApStaConnected(WiFiEvent_t , WiFiEventInfo_t);
void WiFiMngt_WiFiEventApStaDisconnected(WiFiEvent_t , WiFiEventInfo_t);
void WiFiMngt_WiFiEventApStaIpAssigned(WiFiEvent_t , WiFiEventInfo_t);
void WiFiMngt_WiFiEventApStaProbeReqRecved(WiFiEvent_t , WiFiEventInfo_t);

/**
 * @brief NetworkIpMethod_enum
 * method for obtaining IP address
 */
enum NetworkIpMethod_enum {
  NetworkIpMethodDhcp = 0,            ///< DHCP IP
  NetworkIpMethodStatic = 1,          ///< STATIC IP
};

class WiFiMngt {
private:
  String WifiSsid;                    ///< WI-FI SSID
  String WifiPassword;                ///< WI-FI password
  bool WifiEapEnable;                 ///< Wi-Fi Enterprise (PEAP) is enabled, connect with WifiEapUsername and WifiEapPassword
  String WifiEapIdentity;             ///< Wi-Fi Enterprise anonymous identity, empty = user name
  String WifiEapUsername;             ///< Wi-Fi Enterprise user name
  String WifiEapPassword;             ///< Wi-Fi Enterprise password
  volatile uint8_t StaConnectFailCount; ///< consecutive failed STA connections (network not found, rejected by the AP)
  bool ServiceMode;                   ///< flag for enable service AP mode after MCU start, until SERVICE_AP_OFF_DELAY after the connection to the Wi-Fi network
  String WiFiMode;                    ///< Wi-Fi mode (AP, STA, STA_AP)
  String SericeApSsid;                ///< Service AP SSID
  bool FirstConnected;                ///< flag about first connecting to WiFi network status
  bool NtpFirstSync;                  ///< flag about first NTP sync status
  bool EnableServiceAp;               ///< flag about enable service AP mode

  uint8_t NetIpMethod;                ///< flag about IP address obtained. 0 - DHCP, 1 - Static
  IPAddress NetStaticIp;              ///< Static IP address of the STA
  IPAddress NetStaticMask;            ///< Static Mask of the STA
  IPAddress NetStaticGateway;         ///< Static Gateway of the STA
  IPAddress NetStaticDns;             ///< Static DNS of the STA

  uint8_t WiFiStaNetworkBssid[6];     ///< BSSID of the network

  bool WiFiStaMultipleNetwork;        ///< flag about multiple STA networks
  bool StartStaWdg;                   ///< flag about start STA watchdog

  IPAddress Service_LocalIp;          ///< Service IP when si module in the AP mode
  IPAddress Service_Gateway;          ///< Service gateway when si module in the AP mode
  IPAddress Service_Subnet;           ///< Service mask when si module in the AP mode
  IPAddress Service_Dns;              ///< Service DNS when is module in the AP mode

  unsigned long StaConnectedSince;      ///< time of the connection to the Wi-Fi network [ms], 0 = not connected
  unsigned long TaskWdg_previousMillis; ///< previous time for task STA watchdog

  String mDNS_record;                 ///< mDNS record, also the camera name. A new name is used by mDNS and DHCP after the next start
  String StartMdns;                   ///< mDNS record used since the start
  String DhcpHostname;                ///< DHCP host name used since the start, made from the mDNS record
  String WifiScanJson;                ///< global variable with wifi networks
  SemaphoreHandle_t CfgMutex;         ///< mutex for the strings shared between tasks: SSID, password, Wi-Fi mode, mDNS record

  Configuration *config;              ///< pointer to configuration class
  Logs *log;                          ///< pointer to log class
  Camera *cam;                        ///< pointer to camera class

public:
  WiFiMngt(Configuration*, Logs*, Camera*);
  ~WiFiMngt() {};

  void LoadCfgFromEeprom();
  void Init();
  void WifiManagement();
  void WiFiReconnect();
  void SetWifiEvents();
  void WiFiStaConnect();
  void StaConnectFailed(uint8_t);
  void StaConnectDone();
  void SyncNtpTime();

  void ScanWiFiNetwork();
  uint8_t ScanWifiNetwork(String);
  bool CheckAvailableWifiNetwork(String);
  int Rssi2Percent(int);
  String TranslateTxPower(wifi_power_t);
  String TranslateWiFiStatus(wl_status_t);
  String TranslateWiFiEncrypion(wifi_auth_mode_t );
  void CreateApSsid();
  void WiFiWatchdog();

  String GetServiceApSsid();
  String GetStaSsid();
  String GetStaBssid();
  String GetStaStatus();
  String GetStaIp();
  String GetAvailableWifiNetworks();
  String GetWiFiMode();
  String GetWifiMac();
  String GetMdns();
  String GetStartMdns();
  String GetDhcpHostname();
  static String MakeDhcpHostname(String);
  bool GetEnableServiceAp();
  bool GetNtpFirstTimeSync();
  bool GetFirstConnection();
  uint8_t GetNetIpMethod();
  String GetNetStaticIp();
  String GetNetStaticMask();
  String GetNetStaticGateway();
  String GetNetStaticDns();
  bool GetStaEapEnable();
  String GetStaEapIdentity();
  String GetStaEapUsername();
  bool GetStaEapPasswordSaved();

  void SetStaCredentials(String, String);
  void SetStaSsid(String);
  void SetStaPassword(String);
  void SetEnableServiceAp(bool);
  void ConnectToSta();
  bool SetMdns(String);
  void SetFirstConnection(bool);
  void SetNetworkConfig(String, String, String, String);
  void SetNetIpMethod(uint8_t);
  void SetStaEapNetwork(String, String, String, String);
  bool SetStaEapEnable(bool);
};

extern WiFiMngt SystemWifiMngt;   ///< global variable for wifi management

/* EOF */