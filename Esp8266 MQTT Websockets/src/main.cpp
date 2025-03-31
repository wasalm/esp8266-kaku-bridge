#include <Arduino.h>
#include <WiFiManager.h>
#include <time.h>
#include <FS.h>
#include <LittleFS.h>
#include "NewRemoteTransmitter.h"

#include <CertStoreBearSSL.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "StringStream.h"

// Constants
#define RF_PIN D5
#define NUM_OF_UNITS 16
#define URL "https://vps.andries-salm.com/spiegel/dk/device.php";

// General variables
bool hasSetup = false;
String username = "";
String password = "";
String klantcode = "";

NewRemoteTransmitter transmitter(0, RF_PIN, 260, 4);
WiFiManager wm;
BearSSL::CertStore certStore;
BearSSL::WiFiClientSecure mqttWiFiClient;
PubSubClient mqttClient;

String mqttHost = "";
String mqttPort = "";
String mqttUser = "";
String mqttPass = "";
String mqttClientId = "";
String mqttBaseTopic = "";

String readFile(const char *path);
void writeFile(const char *path, String data);
void saveParamCallback();

void setupStorage()
{
  if (!LittleFS.begin())
  {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  hasSetup = LittleFS.exists("hasSetup");

  if (LittleFS.exists("username"))
  {
    username = readFile("username");
    Serial.println("Username: " + username);
  }

  if (LittleFS.exists("password"))
  {
    password = readFile("password");
    Serial.println("Password: " + password);
  }

  if (LittleFS.exists("klantcode"))
  {
    klantcode = readFile("klantcode");
    Serial.println("Klantcode: " + klantcode);
  }

  int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
  Serial.printf("Number of CA certs read: %d\n", numCerts);
  if (numCerts == 0)
  {
    Serial.printf("No certs found. Did you run certs-from-mozilla.py and upload the LittleFS directory before running?\n");
  }
}

void setupNTP()
{
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

void setupWifi()
{
  // reset settings - wipe stored credentials for testing
  // these are stored by the esp library
  if (!hasSetup)
  {
    wm.resetSettings();
  }

  WiFiManagerParameter usernameField("username", "DK Gebruikersnaam", username.c_str(), 256);
  WiFiManagerParameter passwordField("password", "DK Wachtwoord", password.c_str(), 256, "type=\"password\"");
  WiFiManagerParameter klantcodeField("klantcode", "DK Klantcode", klantcode.c_str(), 256);

  wm.addParameter(&usernameField);
  wm.addParameter(&passwordField);
  wm.addParameter(&klantcodeField);
  wm.setSaveParamsCallback(saveParamCallback);

  std::vector<const char *> menu = {"param", "wifi", "sep", "info", "restart", "exit"};
  wm.setMenu(menu);

  wm.setClass("invert"); // dark mode

  bool res;
  res = wm.autoConnect("KaKu Bridge"); // anonymous ap

  if (!res)
  {
    Serial.println("Failed to connect");
    LittleFS.remove("hasSetup");
    delay(1000);
    ESP.restart();
  }

  Serial.println("Connected to WiFi");
}

String urlencode(String str);
String getUniqueID();

void setupMQTTConfig()
{
  // First we will retreive the login data from DK
  BearSSL::WiFiClientSecure wifiClient;
  HTTPClient httpClient;
  wifiClient.setCertStore(&certStore);

  String url = URL;
  url += "?user=" + urlencode(username);
  url += "&pass=" + urlencode(password);
  url += "&code=" + urlencode(klantcode);
  url += "&device=" + getUniqueID();
  url += "&type=rf433v1";

  if (!httpClient.begin(wifiClient, url)) // Initiate connection
  {
    Serial.printf("[HTTP} Unable to connect\n");
    LittleFS.remove("hasSetup");
    delay(1000);
    ESP.restart();
  }

  int httpCode = httpClient.GET(); // Make request

  if (httpCode <= 0 || httpCode >= 400)
  {
    Serial.print("[HTTP] GET... failed, error: ");
    Serial.println(httpCode);

    httpClient.end();

    LittleFS.remove("hasSetup");
    delay(1000);
    ESP.restart();
  }

  String payload = httpClient.getString(); // Get response
  StringStream stream(&payload);
  httpClient.end();

  mqttHost = stream.readStringUntil('\n');
  mqttPort = stream.readStringUntil('\n');
  mqttUser = stream.readStringUntil('\n');
  mqttPass = stream.readStringUntil('\n');
  mqttClientId = stream.readStringUntil('\n');
  mqttBaseTopic = stream.readStringUntil('\n');

  Serial.println("Host: " + mqttHost);
  Serial.println("Port: " + mqttPort);
  Serial.println("Username: " + mqttUser);
  Serial.println("Password: " + mqttPass);
  Serial.println("ClientId: " + mqttClientId);
  Serial.println("Topic: " + mqttBaseTopic);
  
}

void handleMessage(char* topic, uint8_t * payload, size_t length);
void setupMQTT()
{
  mqttWiFiClient.setCertStore(&certStore);
  mqttClient.setClient(mqttWiFiClient);
  mqttClient.setServer(mqttHost.c_str(), mqttPort.toInt());
  mqttClient.setCallback(handleMessage);

  if (mqttClient.connect(mqttClientId.c_str(), mqttUser.c_str(), mqttPass.c_str()))
  {
    Serial.println("Connected to MQTT");

    for (int i = 0; i < NUM_OF_UNITS; i++)
    {
      String topic = mqttBaseTopic + "/channel" + i + "/set";
      mqttClient.subscribe(topic.c_str());
    }
  }
}

void setupTransmitter()
{
  uint8_t mac[6];
  wifi_get_macaddr(STATION_IF, mac);

  // Make each transmitter unique
  transmitter._address = (uint32_t)(mac[3] << 16 | mac[4] << 8 | mac[5]);
}

void setup()
{
  delay(5000);
  Serial.begin(115200);
  Serial.println("Hello world");

  setupTransmitter();
  setupStorage();
  setupWifi();
  setupNTP();
  setupMQTTConfig();
  setupMQTT();
}

void loopRestartTimer()
{
  if (millis() >= 1000 * 60 * 60 * 6)
  {
    // Device Running more than 6 hours
    time_t now = time(nullptr);
    struct tm timeinfo;

    gmtime_r(&now, &timeinfo);
    if (timeinfo.tm_hour == 3)
    {
      // Is it 3 o clock GMT at night? (4 or 5 amsterdam time)
      // If yes to both, please reset.
      ESP.restart();
    }
  }
}

void loopMQTT()
{
  mqttClient.loop();
}

void loop()
{
  loopMQTT();
  loopRestartTimer();
}

/*
 * Function related to the Wifi setup
 */

String getParam(String name)
{
  // read parameter from server, for customhmtl input
  String value;
  if (wm.server->hasArg(name))
  {
    value = wm.server->arg(name);
  }
  return value;
}

void saveParamCallback()
{
  Serial.println("[CALLBACK] saveParamCallback fired");
  Serial.println("PARAM username = " + getParam("username"));
  Serial.println("PARAM password = " + getParam("password"));
  Serial.println("PARAM klantcode = " + getParam("klantcode"));

  username = getParam("username");
  password = getParam("password");
  klantcode = getParam("klantcode");

  writeFile("hasSetup", "hasSetup");
  writeFile("username", username);
  writeFile("password", password);
  writeFile("klantcode", klantcode);
}

/*
 * Functions related to Flash Storage
 */

String readFile(const char *path)
{
  String result;
  Serial.printf("Reading file: %s\r\n", path);

  if (!LittleFS.exists(path))
  {
    // File does not exist
    return result;
  }

  File file = LittleFS.open(path, "r");

  if (!file || file.isDirectory())
  {
    Serial.println("- failed to open file for reading");
    return result;
  }

  while (file.available())
  {
    result += (char)file.read();
  }
  file.close();
  return result;
}

void writeFile(const char *path, String data)
{
  File file = LittleFS.open(path, "w");
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }

  if (!file.print(data))
  {
    Serial.print("Write failed for ");
    Serial.println(path);
  }
  file.close();
}

/*
 * Functions needed for MQTT
 */
String urlencode(String str)
{
  String encodedString = "";
  char c;
  char code0;
  char code1;
  // char code2;
  for (unsigned int i = 0; i < str.length(); i++)
  {
    c = str.charAt(i);
    if (c == ' ')
    {
      encodedString += '+';
    }
    else if (isalnum(c))
    {
      encodedString += c;
    }
    else
    {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9)
      {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9)
      {
        code0 = c - 10 + 'A';
      }
      // code2 = '\0';
      encodedString += '%';
      encodedString += code0;
      encodedString += code1;
      // encodedString+=code2;
    }
    yield();
  }
  return encodedString;
}

String getUniqueID()
{
  String result = WiFi.macAddress();
  result.remove(0, 9);
  result.toLowerCase();
  result.replace(":", "");
  return result;
}

void handleMessage(char* topic, uint8_t * payload, size_t length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (size_t i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  //TODO

}