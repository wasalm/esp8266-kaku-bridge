#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <FS.h>
#include <LittleFS.h>
#include <FastBot.h>
#include "ntp.h"
#include "NewRemoteTransmitter.h"

// Constants
#define RF_PIN D5
#define MAX_USERS 50
#define BOT_MTBS 1000 // mean time between scan messages

// General variables
long numberOfChannels = 1;
String telegramToken;
String telegramPassword = "Digitaal Kantoor"; // Default password

NewRemoteTransmitter transmitter(0, RF_PIN, 260, 4);
WiFiManager wm;
FastBot bot;
int resetCode = -1;

String inlineKeyboardLabels = "";
String inlineKeyboardIds = "";
String settingsKeyboardLabels = "Show password \n Sign out \n Reset receiver";
String settingsKeyboardIds = "password, logoff, reset";

uint32_t users[MAX_USERS]; // Array of users

String readFile(const char *path);
void writeFile(const char *path, String data);
void getUsers();
void saveUsers();
void saveParamCallback();
void handleNewMessages(int numNewMessages);

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

    if (numberOfChannels < 1)
    {
      numberOfChannels = 1;
    }

    if (numberOfChannels >= 16)
    {
      numberOfChannels = 16;
    }
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
  // wm.resetSettings();

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
  res = wm.autoConnect("KaKu Bridge"); // anonymous ap

  if (!res)
  {
    Serial.println("Failed to connect");
    delay(10000);
    ESP.restart();
  }

  Serial.println("Connected to WiFi");
}

void handleMessage(FB_msg &msg);
void setupTelegram()
{
  bot.setToken(telegramToken);
  bot.attach(handleMessage);

  // Setup menu
  inlineKeyboardLabels = "";
  inlineKeyboardIds = "";
  for (long i = 0; i < numberOfChannels; i++)
  {
    inlineKeyboardLabels += String(i + 1) + " on";
    inlineKeyboardLabels += " \t ";
    inlineKeyboardLabels += String(i + 1) + " off";
    inlineKeyboardLabels += " \n ";

    inlineKeyboardIds += "ON_" + String(i);
    inlineKeyboardIds += ", ";
    inlineKeyboardIds += "OFF_" + String(i);
    inlineKeyboardIds += ", ";
  }
  inlineKeyboardLabels += "Settings";
  inlineKeyboardIds += "settings";

  // check connection
  uint8_t res = bot.tick();
  if (res > 1)
  {
    Serial.println("Unable to connect to telegram");
    wm.resetSettings();
    delay(1000);
    ESP.restart();
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
  Serial.begin(115200);
  delay(5000);
  Serial.println("\n\nSTARING\n\n"); // temp

  setupTransmitter();
  setupStorage();
  setupWifi();
  setupNTP();
  setupTelegram();
}

void loopRestartTimer()
{
  if (millis() >= 1000 * 60 * 60 * 6)
  {
    // Device Running more than 6 hours
    if (hour() == 2)
    {
      // Is it 2 o clock GMT at night? (3 or 4 amsterdam time)
      // If yes to both, please reset.
      ESP.restart();
    }
  }
}

bool isAuthorized(uint32_t userId);
void authorize(uint32_t userId);
void deauthorize(uint32_t userId);
void loopTelegram()
{
  uint8_t res = bot.tick();
  if (res > 1)
  {
    Serial.print("TICK: ");

    Serial.println(res);
  }
}

void loop()
{
  loopTelegram();
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

void getUsers()
{
  Serial.println("Retreive users from storage.");

  File file = LittleFS.open("users", "r");

  int i = 0;
  while (file.available())
  {
    String line = file.readStringUntil('\n');
    users[i] = line.toInt();
    i++;
  }

  file.close();
}

void saveUsers()
{
  File file = LittleFS.open("users", "w");
  for (int i = 0; i < MAX_USERS; i++)
  {
    if (users[i] != 0)
    {
      file.println(users[i], DEC);
    }
  }
  file.close();
}

/*
 * Function related to telegram
 */

bool isAuthorized(uint32_t userId)
{
  for (int i = 0; i < MAX_USERS; i++)
  {
    if (users[i] == userId)
    {
      return true;
    }
  }
  return false;
}

void authorize(uint32_t userId)
{
  for (int i = 0; i < MAX_USERS; i++)
  {
    if (users[i] == 0)
    {
      users[i] = userId;
      saveUsers();
      return;
    }
  }
}

void deauthorize(uint32_t userId)
{
  for (int i = 0; i < MAX_USERS; i++)
  {
    if (users[i] == userId)
    {
      users[i] = 0;
    }
  }
  saveUsers();
}

void handleMessage(FB_msg &msg)
{
  Serial.println(msg.toString());

  if (isAuthorized(msg.userID.toInt()))
  {
    if (msg.query)
    {
      Serial.println(msg.data);
      if (msg.data.equals("settings"))
      {
        Serial.println("HI");
        bot.inlineMenuCallback("Here are the possible settings:", settingsKeyboardLabels, settingsKeyboardIds, msg.chatID);
      }
      else if (msg.data.equals("password"))
      {
        String reply = "The password is: " + telegramPassword;
        bot.sendMessage(reply, msg.chatID);
      }
      else if (msg.data.equals("logoff"))
      {
        deauthorize(msg.userID.toInt());
        String reply = "You are logged off.";
        bot.closeMenuText(reply, msg.chatID);
      }
      else if (msg.data.equals("reset"))
      {
        // Generate random number before allowing to continue
        randomSeed(ESP.getCycleCount());
        resetCode = random(100000, 999999);
        String reply = "Are you sure? type 'Reset " + String(resetCode, DEC) + "' to reset this device to factory settings.";
        bot.sendMessage(reply, msg.chatID);
      }
      else
      {
        String id;
        for (long i = 0; i < numberOfChannels; i++)
        {
          // String label = String(i + 1) + " on";
          id = "ON_" + String(i);
          if (msg.data.equals(id))
          {
            transmitter.sendUnit(i,true);
            bot.sendMessage("Device is turned on.", msg.chatID);
            return;
          }

          id = "OFF_" + String(i);
          if (msg.data.equals(id))
          {
            transmitter.sendUnit(i,false);
            bot.sendMessage("Device is turned off.", msg.chatID);
            return;
          }
        }
      }
    }
    else
    {
      if (msg.text.equalsIgnoreCase("Reset " + String(resetCode, DEC)))
      {
        if (resetCode != -1)
        {
          String reply = "Device will be reset.";
          bot.sendMessage(reply, msg.chatID);
          delay(1000);
          wm.resetSettings();
          LittleFS.format();
          delay(1000);
          ESP.restart();
        }
      }
      else
      {
        bot.inlineMenuCallback("What do you want to do?", inlineKeyboardLabels, inlineKeyboardIds, msg.chatID);
      }
    }
  }
  else
  {
    if (msg.text.equals(telegramPassword))
    {
      authorize(msg.userID.toInt());

      String reply = "Dear " + msg.first_name + ", you are logged on. Type /start to control your devices.";
      bot.showMenuText(reply, "Start", msg.chatID);
    }
    else
    {
      String reply = "Dear " + msg.first_name + ", please give the secret code before you continue.";
      bot.closeMenuText(reply, msg.chatID);
    }
  }
}
