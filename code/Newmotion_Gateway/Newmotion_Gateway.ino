/**
 * IotWebConf06MqttApp.ino -- IotWebConf is an ESP8266/ESP32
 *   non blocking WiFi/AP web configuration library for Arduino.
 *   https://github.com/prampec/IotWebConf 
 *
 * Copyright (C) 2018 Balazs Kelemen <prampec+arduino@gmail.com>
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <MQTT.h>
#include <IotWebConf.h>

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "Newmotion Gateway";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "newmotion";

#define STRING_LEN 128
#define NUMBER_LEN 32

// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "mqt3"

// -- When CONFIG_PIN is pulled to ground on startup, the Thing will use the initial
//      password to buld an AP. (E.g. in case of lost password)
#define CONFIG_PIN 2

// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
#define STATUS_PIN LED_BUILTIN

// -- Modbus buffer length
#define MODBUS_LEN 64



// -- Callback method declarations.
void wifiConnected();
void configSaved();
boolean formValidator();
void mqttMessageReceived(String &topic, String &payload);

DNSServer dnsServer;
WebServer server(80);
HTTPUpdateServer httpUpdater;
WiFiClient net;
MQTTClient mqttClient;

// MQTT settings
char mqttServerValue[STRING_LEN];
char mqttUserNameValue[STRING_LEN];
char mqttUserPasswordValue[STRING_LEN];
char mqttCommandTopic[STRING_LEN];
char mqttRawModbusTopic[STRING_LEN];

// Modbus settings
byte modbusData[MODBUS_LEN];
byte modbusDataIndex = 0;
int firstRegister = 0;
int numRegisters = 0;
bool commandSent = false;

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);
IotWebConfSeparator separator1 = IotWebConfSeparator("MQTT Settings");
IotWebConfParameter mqttServerParam = IotWebConfParameter("Server", "mqttServer", mqttServerValue, STRING_LEN);
IotWebConfParameter mqttUserNameParam = IotWebConfParameter("User", "mqttUser", mqttUserNameValue, STRING_LEN);
IotWebConfParameter mqttUserPasswordParam = IotWebConfParameter("Password", "mqttPass", mqttUserPasswordValue, STRING_LEN, "password");
IotWebConfParameter mqttCmdTopicParam = IotWebConfParameter("Command topic", "cmdTopic", mqttCommandTopic, STRING_LEN);
IotWebConfParameter mqttRawModbusTopicParam = IotWebConfParameter("Modbus data topic", "rawTopic", mqttRawModbusTopic, STRING_LEN);

boolean needMqttConnect = false;
boolean needReset = false;
int pinState = HIGH;
unsigned long lastModbusEvent = 0;
unsigned long lastMqttConnectionAttempt = 0;



void setup() 
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting up...");
  Serial1.begin(9600, SERIAL_8E1);

  iotWebConf.setStatusPin(STATUS_PIN);
  iotWebConf.addParameter(&separator1);
  iotWebConf.addParameter(&mqttServerParam);
  iotWebConf.addParameter(&mqttUserNameParam);
  iotWebConf.addParameter(&mqttUserPasswordParam);
  iotWebConf.addParameter(&mqttCmdTopicParam);
  iotWebConf.addParameter(&mqttRawModbusTopicParam);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  iotWebConf.setupUpdateServer(&httpUpdater);

  // -- Initializing the configuration.
  boolean validConfig = iotWebConf.init();
  if (!validConfig)
  {
    mqttServerValue[0] = '\0';
    mqttUserNameValue[0] = '\0';
    mqttUserPasswordValue[0] = '\0';
  }

  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });

  mqttClient.begin(mqttServerValue, net);
  mqttClient.onMessage(mqttMessageReceived);
  
  Serial.println("Ready.");


  // Disable sending on RS485 port
  pinMode(21, OUTPUT);
  digitalWrite(21, LOW);
}

void loop() 
{
  // -- doLoop should be called as frequently as possible.
  iotWebConf.doLoop();
  mqttClient.loop();

  // -- MQTT connection work
  if (needMqttConnect)
  {
    if (connectMqtt())
    {
      needMqttConnect = false;
    }
  }
  else if ((iotWebConf.getState() == IOTWEBCONF_STATE_ONLINE) && (!mqttClient.connected()))
  {
    Serial.println("MQTT reconnect");
    connectMqtt();
  }

  if (needReset)
  {
    Serial.println("Rebooting after 1 second.");
    iotWebConf.delay(1000);
    ESP.restart();
  }

  // -- Gather serial port data and send over MQTT

  unsigned long now = millis();
  
  while (Serial1.available()) 
  {
    byte inByte = Serial1.read();
    modbusDataIndex++;
    if (modbusDataIndex < MODBUS_LEN) modbusData[modbusDataIndex] = inByte;
    else modbusDataIndex = MODBUS_LEN -1;
    lastModbusEvent = now;
  }

  if ((3 < now - lastModbusEvent) && modbusDataIndex != 0)
  {
    String topic = "";
    
    float payload;
    union u_tag {
      byte payloadBytes[4];
      float fval;
    } u;
    
    if ((modbusData[1] == 1) && (modbusData[2] == 3))
    {
      if (modbusDataIndex == 8)
      {
        firstRegister = modbusData[3] * 256 + modbusData[4];
        numRegisters = modbusData[5] * 256 + modbusData[6];
        commandSent = true;
      } 
      else if (commandSent == true)
      {
        commandSent = false;
        for (int i = 0 ; i < numRegisters ; i = i + 2)
        {

          u.payloadBytes[0] = modbusData[7+(i*2)];
          u.payloadBytes[1] = modbusData[6+(i*2)];
          u.payloadBytes[2] = modbusData[5+(i*2)];
          u.payloadBytes[3] = modbusData[4+(i*2)];
          payload = u.fval;

          topic = mqttRawModbusTopic;
          topic += "/";
          topic += String(firstRegister+i, HEX);
          mqttClient.publish(topic, String(payload));          
        }
      }
    }
    modbusDataIndex = 0;
  }
}

/**
 * Handle web requests to "/" path.
 */
void handleRoot()
{
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<link rel=\"stylesheet\" href=\"https://unpkg.com/purecss@2.0.3/build/pure-min.css\" integrity=\"sha384-cg6SkqEOCV1NbJoCu11+bm0NvBRc8IYLRGXkmNrqUBfTjmMYwNKPWBTIKyw9mHNJ\" crossorigin=\"anonymous\">";
  s += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  s += "<title>Newmotion Gateway</title></head><body><h1>Newmotion Gateway</h1>";
  s += "<ul>";
  s += "<li>MQTT server: ";
  s += mqttServerValue;
  s += "</li>";
  s += "</ul>";
  s += "Go to <a href='config'>configure page</a> to change values.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}

void wifiConnected()
{
  needMqttConnect = true;
}

void configSaved()
{
  Serial.println("Configuration was updated.");
  needReset = true;
}

boolean formValidator()
{
  Serial.println("Validating form.");
  boolean valid = true;
  int l = server.arg(mqttServerParam.getId()).length();
  if (l < 3)
  {
    mqttServerParam.errorMessage = "Please provide at least 3 characters!";
    valid = false;
  }

  return valid;
}

boolean connectMqtt() {
  unsigned long now = millis();
  if (1000 > now - lastMqttConnectionAttempt)
  {
    // Do not repeat within 1 sec.
    return false;
  }
  Serial.println("Connecting to MQTT server...");
  if (!connectMqttOptions()) {
    lastMqttConnectionAttempt = now;
    return false;
  }
  Serial.println("Connected!");

  mqttClient.subscribe(mqttCommandTopic);
  return true;
}

boolean connectMqttOptions()
{
  boolean result;
  if (mqttUserPasswordValue[0] != '\0')
  {
    result = mqttClient.connect(iotWebConf.getThingName(), mqttUserNameValue, mqttUserPasswordValue);
  }
  else if (mqttUserNameValue[0] != '\0')
  {
    result = mqttClient.connect(iotWebConf.getThingName(), mqttUserNameValue);
  }
  else
  {
    result = mqttClient.connect(iotWebConf.getThingName());
  }
  return result;
}

void mqttMessageReceived(String &topic, String &payload)
{
  Serial.println("Incoming command: " + topic + " - " + payload);
}
