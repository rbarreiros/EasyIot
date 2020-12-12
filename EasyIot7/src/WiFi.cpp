#include "WiFi.h"
#include "constants.h"
#include <JustWifi.h>
#include "Config.h"
#include "WebServer.h"
#include "Mqtt.h"
#include <ESP8266mDNS.h>
#include <esp-knx-ip.h>
#include "CloudIO.h"
int retryCount = 0;
unsigned long connectedOn = 0ul;
String getApName()
{
  String version = String(VERSION, 3);
  version.replace(".", "x");
  return "OnOfre-" + String(getAtualConfig().chipId) + "-" + version;
}
bool wifiConnected()
{
  return jw.connected();
}
void reloadWiFiConfig()
{
  jw.disconnect();
  jw.setHostname(getAtualConfig().nodeId);
  jw.cleanNetworks();
  jw.setSoftAP(getApName().c_str(), getAtualConfig().apSecret);

  if (getAtualConfig().staticIp)
  {
    jw.addNetwork(getAtualConfig().wifiSSID, getAtualConfig().wifiSecret, getAtualConfig().wifiIp, getAtualConfig().wifiGw, getAtualConfig().wifiMask, getAtualConfig().wifiGw, true);
  }
  else if (strlen(getAtualConfig().wifiSecret) > 0)
  {
    jw.addNetwork(getAtualConfig().wifiSSID, getAtualConfig().wifiSecret);
  }
  else
  {
    jw.addNetwork(getAtualConfig().wifiSSID);
  }
}
void infoWifi()
{

  if (WiFi.isConnected())
  {
    connectedOn = millis();
    uint8_t *bssid = WiFi.BSSID();
#ifdef DEBUG
    Log.notice("%s MODE STA -------------------------------------" CR, tags::wifi);
    Log.notice("%s SSID  %s  " CR, tags::wifi, WiFi.SSID().c_str());
    Log.notice("%s BSSID %X:%X:%X:%X:%X:%X" CR, tags::wifi, bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    Log.notice("%s CH    %d   " CR, tags::wifi, WiFi.channel());
    Log.notice("%s RSSI  %d " CR, tags::wifi, WiFi.RSSI());
    Log.notice("%s IP    %s  " CR, tags::wifi, WiFi.localIP().toString().c_str());
    Log.notice("%s MAC   %s  " CR, tags::wifi, WiFi.macAddress().c_str());
    Log.notice("%s GW    %s " CR, tags::wifi, WiFi.gatewayIP().toString().c_str());
    Log.notice("%s MASK  %s " CR, tags::wifi, WiFi.subnetMask().toString().c_str());
    Log.notice("%s DNS   %s " CR, tags::wifi, WiFi.dnsIP().toString().c_str());
    Log.notice("%s HOST  %s " CR, tags::wifi, WiFi.hostname().c_str());
    Log.notice("----------------------------------------------" CR);
#endif
  }

  if (WiFi.getMode() & WIFI_AP)
  {
#ifdef DEBUG
    Log.notice("%s MODE AP --------------------------------------" CR, tags::wifi);
    Log.notice("%s SSID %s " CR, tags::wifi, jw.getAPSSID().c_str());
    Log.notice("%s IP  %s  " CR, tags::wifi, WiFi.softAPIP().toString().c_str());
    Log.notice("%s MAC  %s " CR, tags::wifi, WiFi.softAPmacAddress().c_str());
    Log.notice("----------------------------------------------" CR);
#endif
    refreshMDNS(getAtualConfig().nodeId);
  }
}
void enableScan()
{
  jw.enableScan(true);
}
void scanNewWifiNetworks()
{

  unsigned char result = WiFi.scanNetworks();
  if (result == WIFI_SCAN_FAILED)
  {
#ifdef DEBUG
    Log.notice("%s Scan Failed" CR, tags::wifi);
#endif
  }
  else if (result == 0)
  {
#ifdef DEBUG
    Log.notice("%s No networks found " CR, tags::wifi);
#endif
  }
  else
  {
    const size_t CAPACITY = JSON_ARRAY_SIZE(result) + 200;
    DynamicJsonDocument doc(CAPACITY);
    JsonArray object = doc.to<JsonArray>();
    for (int8_t i = 0; i < result; ++i)
    {
      String ssid_scan;
      int32_t rssi_scan;
      uint8_t sec_scan;
      uint8_t *BSSID_scan;
      int32_t chan_scan;
      bool hidden_scan;
      WiFi.getNetworkInfo(i, ssid_scan, sec_scan, rssi_scan, BSSID_scan, chan_scan, hidden_scan);
      object.add(ssid_scan);

#ifdef DEBUG
      Log.notice("%s Network found %s" CR, tags::wifi, ssid_scan.c_str());
#endif
    }
    String networks = "";
    serializeJson(doc, networks);
    sendToServerEvents("wifi-networks", networks.c_str());
  }
  WiFi.scanDelete();
}
void dissableAP()
{
  jw.enableAP(false);
}

void infoCallback(justwifi_messages_t code, char *parameter)
{

  switch (code)
  {
#ifdef DEBUG
  case MESSAGE_TURNING_OFF:
    Log.notice("%s Turning OFF" CR, tags::wifi);
    break;
  case MESSAGE_TURNING_ON:
    Log.notice("%s Turning ON" CR, tags::wifi);
    break;
  case MESSAGE_SCANNING:
    Log.notice("%s Scanning" CR, tags::wifi);
    break;
  case MESSAGE_SCAN_FAILED:
    Log.error("%s Scan failed" CR, tags::wifi);
    break;
  case MESSAGE_NO_NETWORKS:
    Log.warning("%s No networks found" CR, tags::wifi);

    break;
  case MESSAGE_NO_KNOWN_NETWORKS:
    Log.warning("%s No known networks found" CR, tags::wifi);
    break;
  case MESSAGE_FOUND_NETWORK:
    Log.warning("%s Network found %s" CR, tags::wifi, parameter);
    break;
  case MESSAGE_CONNECTING:
    Log.notice("%s Connecting to %s" CR, tags::wifi, parameter);
    break;
  case MESSAGE_CONNECT_WAITING:
    // too much noise
    break;
  case MESSAGE_CONNECT_FAILED:
    Log.error("%s Could not connect to %s" CR, tags::wifi, parameter);
    break;
#endif
  case MESSAGE_CONNECTED:
    if (strlen(getAtualConfig().wifiSSID) == 0)
    {
      strlcpy(getAtualConfig().wifiSSID, WiFi.SSID().c_str(), sizeof(getAtualConfig().wifiSSID));
      strlcpy(getAtualConfig().wifiSecret, WiFi.psk().c_str(), sizeof(getAtualConfig().wifiSecret));
      getAtualConfig().save();
    }

    knx.start(nullptr);
    infoWifi();
    connectoToCloudIO();
    setupWebserverAsync();
    break;
#ifdef DEBUG
  case MESSAGE_DISCONNECTED:
    Log.warning("%s Disconnected" CR, tags::wifi);
    break;
#endif
  case MESSAGE_ACCESSPOINT_CREATED:
    infoWifi();
    setupWebserverAsync();
    break;

  case MESSAGE_ACCESSPOINT_DESTROYED:
#ifdef DEBUG
    Log.notice("%s Disconnecting access point" CR, tags::wifi);
#endif
    break;
  case MESSAGE_ACCESSPOINT_CREATING:
#ifdef DEBUG
    Log.notice("%s Creating access point" CR, tags::wifi);
#endif

    break;
#ifdef DEBUG
  case MESSAGE_ACCESSPOINT_FAILED:
    Log.error("%s Could not create access point" CR, tags::wifi);
    break;
  case MESSAGE_WPS_START:
    Log.notice("%s WPS started" CR, tags::wifi);
    break;
  case MESSAGE_WPS_SUCCESS:
    Log.notice("%s WPS succeded!" CR, tags::wifi);
    break;
  case MESSAGE_WPS_ERROR:
    Log.error("%s WPS failed" CR, tags::wifi);
    break;
  case MESSAGE_SMARTCONFIG_START:
    Log.notice("%s Smart Config started" CR, tags::wifi);
    break;
  case MESSAGE_SMARTCONFIG_SUCCESS:

    Log.notice("%smart Config succeded!" CR, tags::wifi);
    break;
  case MESSAGE_SMARTCONFIG_ERROR:
    Log.error("%s Smart Config failed" CR, tags::wifi);
  case MESSAGE_HOSTNAME_ERROR:
    Log.error("%s Hostname Error" CR, tags::wifi);
    break;
#endif
  }
}

void refreshMDNS(const char *lastName)
{
  MDNS.removeService(lastName, "bhonofre", "tcp");
  MDNS.close();
  if (MDNS.begin(String(getAtualConfig().nodeId), INADDR_ANY, 10))
  {
    MDNS.addService("bhonofre", "tcp", 80);
    MDNS.addServiceTxt("bhonofre", "tcp", "hardwareId", String(ESP.getChipId()));
    MDNS.addServiceTxt("bhonofre", "tcp", "firmware", String(VERSION, 3));
    MDNS.addServiceTxt("bhonofre", "tcp", "wifi", String(getAtualConfig().wifiSSID));
    MDNS.addServiceTxt("bhonofre", "tcp", "firmwareMode", constantsConfig::firmwareMode);
  }
  else
  {
#ifdef DEBUG
    Log.error("%s MDNS Error" CR, tags::wifi);
#endif
  }
}
void mdnsCallback(justwifi_messages_t code, char *parameter)
{

  if (code == MESSAGE_CONNECTED)
  {
    refreshMDNS(getAtualConfig().nodeId);
  }
}
void setupWiFi()
{
  WiFi.setPhyMode(WIFI_PHY_MODE_11N);
  jw.setHostname(getAtualConfig().nodeId);
  jw.subscribe(infoCallback);
  jw.subscribe(mdnsCallback);

#if JUSTWIFI_ENABLE_SMARTCONFIG
  if (strlen(getAtualConfig().wifiSSID) == 0)
    jw.startSmartConfig();
#endif
  jw.setSoftAP(getApName().c_str(), getAtualConfig().apSecret);
  jw.enableAP(false);
  jw.enableAPFallback(true);
  jw.enableSTA(true);
  reloadWiFiConfig();
}

void loopWiFi()
{
  if ((WiFi.getMode() & WIFI_AP) && WiFi.isConnected() && connectedOn + 60000 < millis())
  {
    dissableAP();
  }
  jw.loop();
  MDNS.update();
  webserverServicesLoop();
}