//++ include library for wifi connectivity
#include <WiFi.h>
#include <WiFiClientSecure.h>

//++ include libarary to handle mqtt oepration
#include <PubSubClient.h>

//++ include libarray to manage function with tast scheduling
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Include Preferences library for internal memory storage
#include <Preferences.h>
#include <ArduinoJson.h>

//++ Library required for LCD display
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

//++ Library for RFID
#include <MFRC522v2.h>
#include <MFRC522DriverI2C.h>
#include <MFRC522Debug.h>

//++ Include library for fingerprint sensor
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>

//++ library for DHT sensor
#include <DHT.h>

//++ Include library for time functions
#include <time.h>

//++ Include library for telegram bot
#include <UniversalTelegramBot.h>

//++ Pin Declaration for DHT22
#define DHT_IN_PIN 4
#define DHT_TYPE DHT22

//++ Initializing the DHT sensor
DHT dht(DHT_IN_PIN, DHT_TYPE);

//++ Variable to store the temperature & Humidity Value
float temperature = 0;
float humidity = 0;

//++ Pin Declaration for Smoke Detection
#define MQ2_IN_PIN 34

//++ Variable to store the Smoke Value
int Smoke_Value = 0;

//++ Variable to enable Door Lock
#define Solenoid_door_lock 5

//++ Pin Declaration for Door Status
#define Door_Status_PIN 15

//++ Variable to store the Door Status
int door_status = 0;

// Checks for new messages on telebot response every 1 second.
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

//++ Initialize RFID reader
const uint8_t customAddress = 0x28;                   //++ I2C address for RFID
TwoWire& customI2C = Wire;                            //++ Use default Wire instance for I2C
MFRC522DriverI2C driver{ customAddress, customI2C };  //++ Create I2C driver for RFID
MFRC522 mfrc522{ driver };                            //++ Create MFRC522 instance

// Define the hardware serial for ESP32 on pins 16 (RX) and 17 (TX)
HardwareSerial mySerial(1);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// FreeRTOS task handle
TaskHandle_t FingerprintTaskHandle;
TaskHandle_t RfidTaskHandle;
TaskHandle_t telebotTaskHandle;

//++ Person Database
String tagUID[] = {
  "f1361f1c",
  "d1df6a1c",
  "40107f1d",
  "53e797a7",
  "e1039d1c"
};

String People_Detail[5][5] = {
  { "Rupesh Thakur", "4420" },
  { "Khan Gulam", "4421" },
  { "Khalid Shaikh", "4422" },
  { "Neenu Maam", "Special_01" },
  { "Sagrika Maam", "Special_02" }
};

// Wi-Fi credentials
#define DEFAULT_WIFI_SSID "IRON_MAN_2.4G"
#define DEFAULT_WIFI_PASSWORD "Hewlett@123"
String WIFI_SSID;
String WIFI_PASSWORD;


// MQTT Broker Credential & Topics Declaration
const char* mqtt_server = "192.168.209.215";
const char* mqttusername = "admin";
const char* mqttpassword = "root";

//++ MQTT Topics to publish
#define MQTT_PUB_DHT_TEMP "dht/tempreature"
#define MQTT_PUB_DHT_HUM "dht/humidity"
#define MQTT_PUB_MQ2 "mq2/smoke"
#define MQTT_PUB_MQ3 "mq3/alcohal"
#define MQTT_PUB_DOOR_STATUS "door/status"
#define MQTT_PUB_AUTHENTICATION "Door/authentication"
#define MQTT_PUB_WIFI_SCAN "wifi/scan"
#define MQTT_PUB_UNAUTHORIZED_USER "User/unauthorized"

//++ MQTT Topics to subscribe
#define MQTT_SUB_DOOR_LOCK "home/door_lock"
#define MQTT_SUB_WIFI_UPDATE "wifi/update"
#define MQTT_SUB_WIFI_SCAN_REQUEST "wifi/scan/request"
#define MQTT_SUB_TELE_BOT "telebot/msg"

// Initialize Telegram BOT
#define BOTtoken "7532866664:AAGTpCO-DsGUvxooQKm_0ADmukOCmhI1AmY"  // Bot Token (Get from Botfather)
#define CHAT_ID "1950589090"

//++ Initialize the secure wifi client Object and MQTT clients
WiFiClient espClient;
WiFiClientSecure Clients;
PubSubClient client(espClient);
UniversalTelegramBot bot(BOTtoken, Clients);

// Declare Preferences object globally
Preferences preferences;

//++ Variable declaration for LCD Display
#define LCD_ADDRESS 0x27
#define LCD_COLS 16
#define LCD_ROWS 2
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);

// Custom character data for Heart emoji
byte heartEmoji[8] = {
  B00000,
  B01010,
  B11111,
  B11111,
  B11111,
  B01110,
  B00100,
  B00000,
};

byte smileEmoji[8] = {
  B00000,
  B10001,
  B00000,
  B00000,
  B10001,
  B01110,
  B00000,
};

byte happyEmoji[8] = {
  0b00000,
  0b00100,
  0b01010,
  0b00000,
  0b10001,
  0b01110,
  0b00000,
  0b00000
};

byte sadEmoji[8] = {
  0b00000,
  0b01010,
  0b01010,
  0b00000,
  0b00000,
  0b01110,
  0b10001,
  0b00000
};

byte smokeEmoji[8] = {
  0b00100,
  0b01110,
  0b11111,
  0b01110,
  0b00100,
  0b00000,
  0b00000,
  0b00000
};

// Define the text to scroll
String scrollText = "Welcome to the IoT Based Security System, Designed and Crafted by Rupesh";

//++ variable for time
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;
struct tm timeinfo;

//++ Define varibale to track the fingerprint is scanner or not
bool Is_fingerprint_scanned = false;

//++ function to sperate lines
void printSeparationLine() {
  Serial.println(" ");
}

//++ Function to connect to WiFi
void setupWiFi() {
  Serial.println("Connecting to Wi-Fi");

  // Open preferences in read-write mode
  preferences.begin("wifiCreds", false);

  // Check if SSID and password are stored
  WIFI_SSID = preferences.getString("ssid", DEFAULT_WIFI_SSID);              // If not found, use default SSID
  WIFI_PASSWORD = preferences.getString("password", DEFAULT_WIFI_PASSWORD);  // If not found, use default password

  // Attempt to connect using stored credentials
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WI-FI CONNECTED");

  Serial.print("IP ADDRESS: ");
  Serial.println(WiFi.localIP());
  printSeparationLine();
}

//++ Function to Read Temperature & Humidity
void get_temperature_and_humidity() {
  humidity = dht.readHumidity();        // Read humidity from DHT22 sensor
  temperature = dht.readTemperature();  // Read temperature from DHT22 sensor

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    Serial.print("TEMPERATURE: ");
    Serial.println(temperature);
    Serial.print("HUMIDITY: ");
    Serial.println(humidity);

    client.loop();

    if (client.publish(MQTT_PUB_DHT_HUM, String(humidity).c_str())) {
      Serial.println("DHT22 HUMIDITY DATA PUBLISHED\n");
    } else {
      Serial.println("FAILED TO PUBLISH HUMIDITY DATA\n");
    }

    if (client.publish(MQTT_PUB_DHT_TEMP, String(temperature).c_str())) {
      Serial.println("DHT22 TEMP DATA PUBLISHED\n");
    } else {
      Serial.println("FAILED TO PUBLISH TEMP DATA\n");
    }
    printSeparationLine();
  }
}

//++ Function to read the Smoke Value
void get_smoke_value() {
  Smoke_Value = analogRead(MQ2_IN_PIN);
  Serial.print("SMOKE SENSOR(MQ2): ");
  Serial.println(Smoke_Value);

  client.loop();

  if (client.publish(MQTT_PUB_MQ2, String(Smoke_Value).c_str())) {
    Serial.println("SMOKE DATA PUBLISHED\n");
  } else {
    Serial.println("FAILED TO PUBLISH TEMP SMOKE VALUE\n");
  }
  printSeparationLine();
}

// ++ Function to read the Door status values from reed switch
void get_door_status() {
  door_status = digitalRead(Door_Status_PIN);
  Serial.print("DOOR STATUS: ");
  Serial.println(door_status);

  client.loop();

  if (client.publish(MQTT_PUB_DOOR_STATUS, String(door_status).c_str())) {
    Serial.println("DOOR STATUS DATA PUBLISHED\n");
  } else {
    Serial.println("FAILED TO PUBLISH DOOR STATUS\n");
  }
}

//++ Function to controll the door lock
void open_the_lock() {
  digitalWrite(Solenoid_door_lock, HIGH);
  vTaskDelay(2000 / portTICK_PERIOD_MS);
  digitalWrite(Solenoid_door_lock, LOW);
  printSeparationLine();
}

//++ Helper routine to dump a byte array as hex values to Serial.
void printHex(byte* buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

//++ Helper routine to dump a byte array as dec values to Serial.
void printDec(byte* buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}

//++ Function to verify the person through RFID Access Control
void authenticate_person_through_rfid(void* parameter) {
  Serial.print("\nSTARTING THE RFID ACCESS CONTROL\n");
  while (true) {
    if (Is_fingerprint_scanned == false) {

      lcd.clear();

      //++ printing the current time on the display
      Morning_time_printing_on_lcd();

      vTaskDelay(pdMS_TO_TICKS(2000));

      lcd.clear();

      //++ printing the sensor values on display
      display_temp_humidity_and_smoke();

      //++ Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
      if (mfrc522.PICC_IsNewCardPresent()) {

        //++ Verify if the NUID has been readed
        if (mfrc522.PICC_ReadCardSerial()) {

          MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);

          String uidString = "";
          for (byte i = 0; i < mfrc522.uid.size; i++) {
            uidString.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : ""));
            uidString.concat(String(mfrc522.uid.uidByte[i], HEX));
          }

          //++ Flag to check if a match is found
          bool isMatchFound = false;

          // Check if the UID matches any pre-saved tag
          for (int i = 0; i < sizeof(tagUID) / sizeof(tagUID[0]); i++) {
            if (uidString == tagUID[i]) {
              Serial.print("WELCOME ");
              Serial.println(People_Detail[i][0]);

              // print the authenticate user name on lcd
              lcd.clear();
              lcd.setCursor(4, 0);
              lcd.print("WELCOME");
              lcd.setCursor(12, 0);

              // Print the Heart emoji
              lcd.write(byte(0));
              lcd.setCursor(2, 1);
              lcd.print(People_Detail[i][0]);

              //++ Publish an MQTT message on topic authentication log
              client.publish(MQTT_PUB_AUTHENTICATION, People_Detail[i][0].c_str(), true);
              Serial.println("AUTHENTICATION LOG DATA PUBLISHED");

              //++ open the door lock
              open_the_lock();
              isMatchFound = true;
              break;
            }
          }

          //++ If no match is found after the loop, print "WRONG CARD SCAN"
          if (!isMatchFound) {
            lcd.clear();
            lcd.setCursor(1, 0);
            lcd.print("WRONG CARD SCAN");
            lcd.setCursor(8, 1);
            lcd.write(byte(3));

            //++ Publish an MQTT message for unauthorized user detected
            client.publish(MQTT_PUB_UNAUTHORIZED_USER, "Detected", true);
            Serial.println("UNAUHTORIZED USER LOG DATA PUBLISHED");
          }

          //++ print the hex and dec data of tags
          Serial.println(F("The NUID tag is:"));
          Serial.print(F("In hex: "));
          printHex(mfrc522.uid.uidByte, mfrc522.uid.size);
          Serial.println();
          Serial.print(F("In dec: "));
          printDec(mfrc522.uid.uidByte, mfrc522.uid.size);
          Serial.println();

          // Halt PICC
          mfrc522.PICC_HaltA();

          // Stop encryption on PCD
          mfrc522.PCD_StopCrypto1();
        }
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
}

//++ function to handle door_lock control function
void handleDoorLock(const String& message) {
  if (message == "lock") {
    Serial.println("DOOR LOCKED");
    digitalWrite(Solenoid_door_lock, HIGH);  // Lock the door
  } else if (message == "unlock") {
    Serial.println("DOOR UNLOCKED");
    digitalWrite(Solenoid_door_lock, LOW);  // Unlock the door
  } else {
    Serial.println("Unknown door lock command received.");
  }
  printSeparationLine();
}

// Function to clear Wi-Fi credentials
void clearWiFiCredentials() {
  preferences.clear();  // Clears all stored keys and values
  Serial.println("Wi-Fi credentials cleared!");
}

// Close preferences before resetting or exiting
void closePreferences() {
  preferences.end();
}

// Function to update and save new Wi-Fi credentials
void updateWiFiCredentials(String newSSID, String newPassword) {

  // clear all store WiFi credentials
  clearWiFiCredentials();

  // Store the new SSID and password in Preferences
  preferences.putString("ssid", newSSID);
  preferences.putString("password", newPassword);

  // Disconnect from current WiFi
  WiFi.disconnect();

  // Wait for a moment to ensure the disconnect is properly handled
  delay(2000);

  // Reconnect to the new WiFi credentials
  setupWiFi();
  Serial.println("WIFI CREDIANTIAL UPDATED!");
  closePreferences();
}

//++ function to handle wifi_update operation
void handleWiFiCredentials(const String& message) {
  DynamicJsonDocument doc(200);                                        // Create JSON document
  DeserializationError error = deserializeJson(doc, message.c_str());  // Parse the message into the JSON document

  if (error) {
    Serial.print(F("Failed to parse WiFi credentials: "));
    Serial.println(error.c_str());
    return;
  }

  // Directly extract SSID and password from the JSON document
  if (doc["ssid"] && doc["password"]) {
    WIFI_SSID = doc["ssid"].as<String>();
    WIFI_PASSWORD = doc["password"].as<String>();

    Serial.print("RECEIVED: NEW WIFI CREDIENTIALS - SSID: ");
    Serial.print(WIFI_SSID);
    Serial.print(", Password: ");
    Serial.println(WIFI_PASSWORD);
    updateWiFiCredentials(WIFI_SSID, WIFI_PASSWORD);

  } else {
    Serial.println("Invalid WiFi credentials format.");
  }
  printSeparationLine();
}

//++ Function to scan the wifi networks
void scanWiFiNetworks() {
  int numNetworks = WiFi.scanNetworks();
  DynamicJsonDocument doc(1024);
  JsonArray ssidArray = doc.createNestedArray("networks");

  for (int i = 0; i < numNetworks; i++) {
    JsonObject ssidObj = ssidArray.createNestedObject();
    ssidObj["ssid"] = WiFi.SSID(i);
    ssidObj["rssi"] = WiFi.RSSI(i);
    ssidObj["encryptionType"] = WiFi.encryptionType(i);
  }

  // Publish the SSID list as JSON
  String jsonOutput;
  serializeJson(doc, jsonOutput);

  //++ Publish the list of SSID's on topic
  client.publish(MQTT_PUB_WIFI_SCAN, jsonOutput.c_str(), true);
  Serial.print("WIFI SCAN RESULT PUBLISHED ");
  Serial.println(jsonOutput);
  printSeparationLine();
}

// Callback function for MQTT messages
void callback(char* topic, byte* payload, unsigned int length) {

  // Convert the incoming message to a string
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("MESSAGE RECEIVED ON TOPIC: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println(message);

  // Handle door lock/unlock
  if (strcmp(topic, MQTT_SUB_DOOR_LOCK) == 0) {
    handleDoorLock(message);
  }

  // Handle WiFi credential update
  if (strcmp(topic, MQTT_SUB_WIFI_UPDATE) == 0) {
    handleWiFiCredentials(message);
  }

  // Handle Wi-Fi scan request
  if (strcmp(topic, MQTT_SUB_WIFI_SCAN_REQUEST) == 0) {
    scanWiFiNetworks();
  }
  printSeparationLine();
}

//++ function to subscribe all topic on mqtt connect
void onMqttConnect() {
  Serial.print("MQTT CONNECTED ");
  printSeparationLine();

  // Subscribe to door_lock topic
  client.subscribe(MQTT_SUB_DOOR_LOCK, 1);
  Serial.println("DOOR_LOCK TOPIC SUBSCRIBED ");

  // Subscribe to the WiFi credentials update topic
  client.subscribe(MQTT_SUB_WIFI_UPDATE, 1);
  Serial.println("WIFI-UPDATE TOPIC SUBSCRIBED ");

  // Subscribe to the WiFi Scan Request topic
  client.subscribe(MQTT_SUB_WIFI_SCAN_REQUEST, 1);
  Serial.println("WIFI_SCAN TOPIC SUBSCRIBED ");

  printSeparationLine();
}

//++ Function to reconnect to MQTT broker
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("b0:b2:1c:a7:35:48",mqttusername,mqttpassword)) {  // connect the mqtt client with an unique client ID
      Serial.println("connected");
      onMqttConnect();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

//++ Function to publish data
void MQTT_Reconnection() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}

// Task to handle MQTT publishing
void mqttTask(void* parameter) {
  while (true) {
    MQTT_Reconnection();

    //++ Reading Temperature & Humdity
    get_temperature_and_humidity();

    //++ Reading Smoke Value
    get_smoke_value();

    //++ Reading the Door Status
    get_door_status();

    vTaskDelay(pdMS_TO_TICKS(5000));  // Publish every 10 seconds
  }
}

// Fingerprint matching function
uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return 0;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return 0;

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.print("Found ID #");
    Serial.print(finger.fingerID);
    Serial.print(" with confidence of ");
    Serial.println(finger.confidence);
    return finger.fingerID;
  }
  return 0;
}

// FreeRTOS task for fingerprint matching
void fingerprintMatchTask(void* pvParameters) {
  while (true) {
    int fingerprintID = getFingerprintID();
    if (fingerprintID > 0) {
      Is_fingerprint_scanned = true;

      // Display the owner's name
      Serial.print("FINGERPRINT MATCHED FOUND WITH: ");
      Serial.println(People_Detail[fingerprintID - 1][0]);

      // Print the Heart emoji and matched user name
      lcd.clear();
      lcd.setCursor(4, 0);
      lcd.print("WELCOME");
      lcd.setCursor(12, 0);
      lcd.write(byte(0));
      lcd.setCursor(2, 1);
      lcd.print(People_Detail[fingerprintID - 1][0]);

      Is_fingerprint_scanned = false;

      vTaskDelay(pdMS_TO_TICKS(2000));

      // If a match is found, activate the relay
      open_the_lock();

      //++ Publish an Authentication Log data
      client.publish(MQTT_PUB_AUTHENTICATION, String(People_Detail[fingerprintID - 1][0]).c_str(), true);
      Serial.println("AUTHENTICATION LOG DATA PUBLISHED");
    }
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

// Handle what happens when you receive new messages
void handleNewMessages(int numNewMessages) {
  Serial.print("Processing New Incoming Messages: ");
  Serial.println(String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++) {
    // Chat ID of the requester
    String chat_id = String(bot.messages[i].chat_id);

    // If the user is unauthorized, send an error message
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "⚠️ *Access Denied*\n\nYou are not authorized to use this system. Please contact the administrator if you believe this is an error.", "");
      continue;
    }

    // Print the received message
    String text = bot.messages[i].text;
    Serial.println("Message received: " + text);

    // Get the sender's name
    String from_name = bot.messages[i].from_name;

    // Handle the /start command
    if (text == "/start") {
      String welcome = "👋 *Welcome to the IoT Security System*, " + from_name + ".\n\n";
      welcome += "You are now connected to the system. Use the following commands to interact with the available services:\n\n";
      welcome += "• */sensor_value*: Retrieve the current sensor data (temperature, humidity, gas levels).\n";
      welcome += "• */door_status*: Check the current status of the door lock.\n\n";
      welcome += "For further assistance or to report any issues, contact the system administrator.";
      bot.sendMessage(chat_id, welcome, "Markdown");

      // Handle sensor values when user enters "sensor_value"
    } else if (text == "/sensor_value") {

      // Format the message to display sensor values
      String sensorData = "📊 *Current Sensor Readings*\n\n";
      sensorData += "📍 *Temperature*: " + String(temperature) + " C\n";
      sensorData += "📍 *Humidity*: " + String(humidity) + " %\n";
      sensorData += "📍 *Gas Concentration (MQ2)*: " + String(Smoke_Value) + " PPM\n";
      // sensorData += "📍 *Alcohol Level (MQ3)*: " + String(mq3_value) + " PPM\n\n";
      sensorData += "Please review the data above and take necessary action if required.";
      bot.sendMessage(chat_id, sensorData, "Markdown");

      // Handle /door command to show door lock status
    } else if (text == "/door_status") {
      // Assuming doorLockStatus is a variable representing the door lock state (1 for locked, 0 for unlocked)
      int doorLockStatus = digitalRead(Door_Status_PIN);

      String doorStatusMessage = "🚪 *Door Status*\n\n";
      if (doorLockStatus == LOW) {
        doorStatusMessage += "The door is currently *locked*. Everything is secure.";
      } else {
        doorStatusMessage += "The door is *unlocked*. Please ensure the door is secured if necessary.";
      }
      bot.sendMessage(chat_id, doorStatusMessage, "Markdown");

      // Handle invalid commands
    } else {
      String invalidMsg = "❗ *Unrecognized Command*\n\n";
      invalidMsg += "The command you entered is not valid. Please use /start to see the list of valid commands or contact the administrator for support.";
      bot.sendMessage(chat_id, invalidMsg, "Markdown");
    }
  }
}

//++ TASK to automate response of telebot for admin
void telebot_response_Task(void* pvParameters) {
  while (true) {
    // handle the incomming message on telegram bot and take response based on prompt
    if (millis() > lastTimeBotRan + botRequestDelay) {
      int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

      while (numNewMessages) {
        Serial.println("GOT NEW MSG FROM TELEBOT");
        handleNewMessages(numNewMessages);
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      }
      lastTimeBotRan = millis();
    }
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }
}

void setup() {
  //++ Initialize serial communication at 115200 baud rate
  Serial.begin(115200);
  mySerial.begin(57600, SERIAL_8N1, 16, 17);

  lcd.begin();  // initialize the lcd
  lcd.backlight();

  //++ Load the custom character into CGRAM
  lcd.createChar(0, heartEmoji);
  lcd.createChar(1, smileEmoji);
  lcd.createChar(2, happyEmoji);  // Create happy emoji
  lcd.createChar(3, sadEmoji);    // Create sad emoji
  lcd.createChar(4, smokeEmoji);

  //++ Clear the LCD
  lcd.clear();
  lcd.setCursor(8, 1);
  lcd.write(byte(1));
  Serial.println("\n\INTRO TEXT DISPLAYING\n");

  //++ Print the scrolling text
  for (int i = 0; i < scrollText.length(); i++) {
    if (i == scrollText.length() - 14) {
      break;
    }
    lcd.setCursor(0, 0);
    lcd.print(scrollText.substring(i, i + 16));
    delay(400);  // Adjust the scrolling speed here
  }

  // Clear the LCD
  lcd.clear();

  //++ Initializing the WiFi
  setupWiFi();

  //++ printing the connected wifi ssid on the lcd screen
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("WiFi Network");
  lcd.setCursor(2, 1);
  lcd.print(WIFI_SSID);
  delay(2000);

  //++ Init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  //++ Starting the DHT22 Sensor
  dht.begin();

  //++ Initalizing the Smoke Sensor
  pinMode(MQ2_IN_PIN, INPUT);

  //++ Initializing the Door Lock
  pinMode(Solenoid_door_lock, OUTPUT);

  //++ Initializing the Door Status Reader
  pinMode(Door_Status_PIN, INPUT);

  //++ Initializing the RFID Reader
  mfrc522.PCD_Init();

  // Initialize the fingerprint sensor
  finger.begin(57600);
  delay(5);

  //++ Check whether the fingerprint sensor is connected or not
  while (!finger.verifyPassword()) {
    Serial.println("Did not find fingerprint sensor :(");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("DID NOT FIND");
    lcd.setCursor(0, 1);
    lcd.print("FINGERPRINT SENSOR");
    delay(1000);  // Wait for 1 second before retrying
  }
  Serial.println("FOUND FINGERPRINT SENSOR!");

  // Check if there are any stored fingerprints
  finger.getTemplateCount();
  if (finger.templateCount == 0) {
    Serial.println("No fingerprints enrolled.");
  } else {
    Serial.print("SENSORS CONTAINS: ");
    Serial.print(finger.templateCount);
    Serial.println(" DATA");
  }

  // Add root certificate for api.telegram.org
  Clients.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  //++ Initializing the MQTT Broker
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  //++ MQTT Reconnection Handling & Data Publish
  xTaskCreate(mqttTask, "MQTT Task", 2048, NULL, 1, NULL);

  //++ RFID BASED ACCESS CONTROLLING
  xTaskCreate(authenticate_person_through_rfid, "RFID Task", 2048, NULL, 2, NULL);

  //++ Fingerprint BASED ACESS CONTROLLING
  xTaskCreate(fingerprintMatchTask, "authenticationTask", 2048, NULL, 3, &FingerprintTaskHandle);

  //++ TELE_BOT RESPONSE HANDLING TASK
  xTaskCreate(telebot_response_Task, "telebotTask", 8192, NULL, 4, &telebotTaskHandle);
}

void loop() {
  // nothing here all task handle using task scheduling
}

//++ Function to print the short form of the day name
String getDayShortName(int day) {
  switch (day) {
    case 0: return "Sun";
    case 1: return "Mon";
    case 2: return "Tue";
    case 3: return "Wed";
    case 4: return "Thu";
    case 5: return "Fri";
    case 6: return "Sat";
    default: return "";
  }
}

//++ Function to print the morning time on the lcd s
void Morning_time_printing_on_lcd() {
  getLocalTime(&timeinfo);

  // Determine if it's morning, afternoon, or evening
  float hour = timeinfo.tm_hour;
  float min = timeinfo.tm_min;
  float current_time = hour + (min / 60) + 5.5;
  const char* timeOfDay;

  // check the present status of the day
  if (current_time >= 5 && current_time < 12) {
    timeOfDay = "Good Morning ";
  } else if (current_time >= 12 && current_time < 18) {
    timeOfDay = "Good Afternoon ";
  } else {
    timeOfDay = "Good Evening ";
  }

  // Print "Good Morning" with a smiley on the first line
  lcd.setCursor(0, 0);
  lcd.print(timeOfDay);
  lcd.write(byte(1));

  // Print date and time on the second line
  lcd.setCursor(0, 1);
  lcd.print(getDayShortName(timeinfo.tm_wday));
  lcd.print(" ");
  lcd.print(String(timeinfo.tm_mday));  // Print date
  lcd.print(" ");

  if (timeinfo.tm_min + 30 > 60) {
    lcd.print(String(timeinfo.tm_hour + 6) + ":" + String(timeinfo.tm_min - 30) + ":" + String(timeinfo.tm_sec));  // Print time
  } else {
    lcd.print(String(timeinfo.tm_hour + 5) + ":" + String(timeinfo.tm_min + 30) + ":" + String(timeinfo.tm_sec));  // Print time
  }
}

//++ Funtion to display the multi sensor values on the lcd screen
void display_temp_humidity_and_smoke() {
  lcd.setCursor(2, 0);
  lcd.print("RH:");
  lcd.setCursor(6, 0);
  lcd.print(humidity);
  lcd.setCursor(11, 0);
  lcd.print("%");
  lcd.setCursor(13, 0);
  lcd.print("MQ2");

  lcd.setCursor(0, 1);
  lcd.print("TEMP:");
  lcd.setCursor(6, 1);
  lcd.print(temperature);
  lcd.setCursor(11, 1);
  lcd.print("C");

  lcd.setCursor(13, 1);
  lcd.print(Smoke_Value);  //http://192.168.135.125
}
