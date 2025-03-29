#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <FS.h>
#include <LittleFS.h>

WiFiManager wm;

// General settings
long numberOfChannels = 1;
String telegramToken;
String telegramPassword = "Digitaal Kantoor"; // Default password

#define MAX_USERS 50
uint32_t users[MAX_USERS]; // Array of users


String readFile(const char *path);
void writeFile(const char *path, String data);
void getUsers();
void saveUsers();
void saveParamCallback();

void setupStorage()
{
  if (!LittleFS.begin())
  {
    Serial.println("LittleFS Mount Failed");
    return;
  }

  if (LittleFS.exists("numberOfChannels"))
  {
    String result = readFile("numberOfChannels");
    Serial.println("Number of channels: " + result);
    numberOfChannels = result.toInt();
  }

  if (LittleFS.exists("telegramToken"))
  {
    telegramToken = readFile("telegramToken");
    Serial.println("Token: " + telegramToken);
  }

  if (LittleFS.exists("telegramPassword"))
  {
    telegramPassword = readFile("telegramPassword");
    Serial.println("Password: " + telegramPassword);
  }

  if (LittleFS.exists("users"))
  {
    getUsers();
  }
}

void setupWifi()
{
  // reset settings - wipe stored credentials for testing
  // these are stored by the esp library
  wm.resetSettings();

  String numberOfChannelsString = String(numberOfChannels);
  WiFiManagerParameter numberOfChannelsField("numberOfChannels", "Number of receivers", numberOfChannelsString.c_str(), 10, "type=\"number\" min=\"1\" max=\"16\"");
  WiFiManagerParameter telegramTokenField("telegramToken", "Telegram Token", telegramToken.c_str(), 256, "placeholder=\"000000000:XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\"");
  WiFiManagerParameter telegramPasswordField("telegramPassword", "Telegram Password", telegramPassword.c_str(), 256);

  wm.addParameter(&numberOfChannelsField);
  wm.addParameter(&telegramTokenField);
  wm.addParameter(&telegramPasswordField);
  wm.setSaveParamsCallback(saveParamCallback);

  std::vector<const char *> menu = {"param", "wifi", "sep", "info", "restart", "exit"};
  wm.setMenu(menu);

  wm.setClass("invert"); // dark mode

  bool res;
  res = wm.autoConnect("KAKU Bridge"); // anonymous ap

  if (!res)
  {
    Serial.println("Failed to connect");
    delay(10000);
    ESP.restart();
  }
  
  Serial.println("Connected to WiFi");
}

void setup()
{
  Serial.begin(115200);
  delay(5000);
  Serial.println("\n\nSTARING\n\n"); // temp

  setupStorage();
  setupWifi();
}

void loop()
{
  // put your main code here, to run repeatedly:
  Serial.println("loop");
  delay(1000);
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
  Serial.println("PARAM numberOfChannels = " + getParam("numberOfChannels"));
  Serial.println("PARAM telegramToken = " + getParam("telegramToken"));
  Serial.println("PARAM telegramPassword = " + getParam("telegramPassword"));

  numberOfChannels = getParam("numberOfChannels").toInt();
  telegramToken = getParam("telegramToken");
  telegramPassword = getParam("telegramPassword");

  LittleFS.format();

  writeFile("numberOfChannels", getParam("numberOfChannels")); 
  writeFile("telegramToken", telegramToken); 
  writeFile("telegramPassword", telegramPassword); 
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

    if (!file || file.isDirectory()) {
      Serial.println("- failed to open file for reading");
      return result;
    }

    while (file.available()) {
      result += (char) file.read();
    }
    file.close();
  return result;
}

void writeFile(const char *path, String data)
{
  File file = LittleFS.open(path, "w");
  if(!file){
      Serial.println("Failed to open file for writing");
      return;
  }

  if(!file.print(data)){
      Serial.print("Write failed for ");
      Serial.println(path);
  }
  file.close();
}

void getUsers() {
  Serial.println("Retreive users from storage.");

    File file = LittleFS.open("users", "r");

     int i = 0;
     while(file.available()) {
      String line = file.readStringUntil('\n');
      users[i] = line.toInt();
      i++;
    }
    
    file.close();
}

void saveUsers() {
  File file = LittleFS.open("users", "w");
  for (int i = 0; i < MAX_USERS; i++) {
    if(users[i] != 0) 
    {
      file.println(users[i], DEC);
    }
  }
  file.close();
}

