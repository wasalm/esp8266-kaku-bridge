#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <FS.h>
#include <LittleFS.h>
#include "ntp.h"
#include "NewRemoteTransmitter.h"

// Constants
#define RF_PIN D5
#define URL "https://vps.andries-salm.com/spiegel/dk/device.php";

// General variables
String username = "";
String password = "";
String klantcode = "";

NewRemoteTransmitter transmitter(0, RF_PIN, 260, 4);
WiFiManager wm;

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
}

void setupWifi()
{
  // reset settings - wipe stored credentials for testing
  // these are stored by the esp library
  wm.resetSettings();

  WiFiManagerParameter usernameField("username", "DK Gebruikersnaam", username.c_str(), 256);
  WiFiManagerParameter passwordField("password", "DK Wachtwoord", password.c_str(), 256, "type=\"password\"");
  WiFiManagerParameter klantcodeField("klantcode", "DK Klantcode", klantcode.c_str(), 256, "placeholder=\"1234\"");

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
    delay(10000);
    ESP.restart();
  }

  Serial.println("Connected to WiFi");
}

// void handleMessage(FB_msg &msg);
// void setupTelegram()
// {
//   bot.setToken(telegramToken);
//   bot.attach(handleMessage);

//   // Setup menu
//   inlineKeyboardLabels = "";
//   inlineKeyboardIds = "";
//   for (long i = 0; i < numberOfChannels; i++)
//   {
//     inlineKeyboardLabels += String(i + 1) + " on";
//     inlineKeyboardLabels += " \t ";
//     inlineKeyboardLabels += String(i + 1) + " off";
//     inlineKeyboardLabels += " \n ";

//     inlineKeyboardIds += "ON_" + String(i);
//     inlineKeyboardIds += ", ";
//     inlineKeyboardIds += "OFF_" + String(i);
//     inlineKeyboardIds += ", ";
//   }
//   inlineKeyboardLabels += "Settings";
//   inlineKeyboardIds += "settings";

//   // check connection
//   uint8_t res = bot.tick();
//   if (res > 1)
//   {
//     Serial.println("Unable to connect to telegram");
//     wm.resetSettings();
//     delay(1000);
//     ESP.restart();
//   }
// }

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

  setupTransmitter();
  setupStorage();
  setupWifi();
  setupNTP();
  //   setupTelegram();
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

// bool isAuthorized(uint32_t userId);
// void authorize(uint32_t userId);
// void deauthorize(uint32_t userId);
// void loopTelegram()
// {
//   uint8_t res = bot.tick();
//   if (res > 1)
//   {
//     Serial.print("TICK: ");

//     Serial.println(res);
//   }
// }

void loop()
{
  //   loopTelegram();
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

    LittleFS.format();

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

// /*
//  * Function related to telegram
//  */

// bool isAuthorized(uint32_t userId)
// {
//   for (int i = 0; i < MAX_USERS; i++)
//   {
//     if (users[i] == userId)
//     {
//       return true;
//     }
//   }
//   return false;
// }

// void authorize(uint32_t userId)
// {
//   for (int i = 0; i < MAX_USERS; i++)
//   {
//     if (users[i] == 0)
//     {
//       users[i] = userId;
//       saveUsers();
//       return;
//     }
//   }
// }

// void deauthorize(uint32_t userId)
// {
//   for (int i = 0; i < MAX_USERS; i++)
//   {
//     if (users[i] == userId)
//     {
//       users[i] = 0;
//     }
//   }
//   saveUsers();
// }

// void handleMessage(FB_msg &msg)
// {
//   if (isAuthorized(msg.userID.toInt()))
//   {
//     if (msg.query)
//     {
//       if (msg.data.equals("settings"))
//       {
//         bot.inlineMenuCallback("Here are the possible settings:", settingsKeyboardLabels, settingsKeyboardIds, msg.chatID);
//       }
//       else if (msg.data.equals("password"))
//       {
//         String reply = "The password is: " + telegramPassword;
//         bot.sendMessage(reply, msg.chatID);
//       }
//       else if (msg.data.equals("logoff"))
//       {
//         deauthorize(msg.userID.toInt());
//         String reply = "You are logged off.";
//         bot.closeMenuText(reply, msg.chatID);
//       }
//       else if (msg.data.equals("reset"))
//       {
//         // Generate random number before allowing to continue
//         randomSeed(ESP.getCycleCount());
//         resetCode = random(100000, 999999);
//         String reply = "Are you sure? Type 'Reset " + String(resetCode, DEC) + "' to reset this device to factory settings.";
//         bot.closeMenuText(reply, msg.chatID);
//       }
//       else
//       {
//         String id;
//         for (long i = 0; i < numberOfChannels; i++)
//         {
//           // String label = String(i + 1) + " on";
//           id = "ON_" + String(i);
//           if (msg.data.equals(id))
//           {
//             transmitter.sendUnit(i,true);
//             bot.sendMessage("Device is turned on.", msg.chatID);
//             return;
//           }

//           id = "OFF_" + String(i);
//           if (msg.data.equals(id))
//           {
//             transmitter.sendUnit(i,false);
//             bot.sendMessage("Device is turned off.", msg.chatID);
//             return;
//           }
//         }
//       }
//     }
//     else
//     {
//       if (msg.text.equalsIgnoreCase("Reset " + String(resetCode, DEC)))
//       {
//         if (resetCode != -1)
//         {
//           String reply = "Device will be reset.";
//           bot.sendMessage(reply, msg.chatID);
//           delay(1000);
//           wm.resetSettings();
//           LittleFS.format();
//           delay(1000);
//           ESP.restart();
//         }
//       }
//       else
//       {
//         bot.inlineMenuCallback("What do you want to do?", inlineKeyboardLabels, inlineKeyboardIds, msg.chatID);
//       }
//     }
//   }
//   else
//   {
//     if (msg.text.equals(telegramPassword))
//     {
//       authorize(msg.userID.toInt());

//       String reply = "Dear " + msg.first_name + ", you are logged on. Type /start to control your devices.";
//       bot.showMenuText(reply, "Start", msg.chatID);
//     }
//     else
//     {
//       String reply = "Dear " + msg.first_name + ", please give the secret code before you continue.";
//       bot.closeMenuText(reply, msg.chatID);
//     }
//   }
// }
