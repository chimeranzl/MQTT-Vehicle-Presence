// Vehicle Presence Detection
// Based on RSSI signal strength from Vehicle to Wireless AP
//
// Last update: 14/08/2019
// Author: chimera
// Version 1.1
//

#include <PubSubClient.h>
#include <ESP8266WiFi.h>

// Modify these to suit your Wifi and Vehicle
IPAddress MQTT_SERVER(172, 16, 223, 254);
const char* cs_SSID = "yourssid";
const char* cs_wifi_Password = "yourpassword";
const String cs_MQTTPrefix = "openhab/vehicle/";
const String cs_VehicleID = "ford";


// MQTT - Publish these
const String OPENHABSTATUS = cs_MQTTPrefix + cs_VehicleID + "/status";
const String OPENHABMYIPADDRESS = cs_MQTTPrefix + cs_VehicleID + "/ipaddress";
const String OPENHABVEHICLERSSI = cs_MQTTPrefix + cs_VehicleID + "/rssi";
const String OPENHABVEHICLESTATE = cs_MQTTPrefix + cs_VehicleID + "/homeoraway";


// Milliseconds to wait between signal strength tests
unsigned long cl_SampleDelayMillis = 500;
// Number of samples to take every 'SampleDelayMillis' (before making a decision)
const int ci_SamplesToTake = 4;
// Buffer added to baseline to qualify for making better "leaving" or "arriving" decisions
const int ci_RSSIBuffer = 8;
// Send alive every (LWT)
const int ci_SendAliveEvery = 10;

// Variables
bool b_GetBaseline;
long l_RSSIBaseline = 0;
unsigned long ul_Alive = millis();
//long l_Alive = 0;

// Define message buffer and publish string
char mqtt_topic[40];
char message_buff[15];

// Wifi Client
WiFiClient wifiClient;
String wemosIPAddress;

// Callback to Arduino from MQTT (inbound message arrives for a subscription)
void callback(char* topic, byte* payload, unsigned int length) 
{
  // Not needed in this instance
}

// Define Publish / Subscribe client (must be defined after callback function above if in use)
PubSubClient mqttClient(MQTT_SERVER, 1883, callback, wifiClient);

//
// Setup the ESP for operation
//
void setup()
{

  // Set builtin LED as connection indicator, turn OFF
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Debug USB Serial
  Serial.begin(115200);
  Serial.println(" ");

}

//
// Check Wifi, MQTT and RSSI signal strength
//
void loop()
{

  // Check WiFi, connect if needed
  if (WiFi.status() != WL_CONNECTED) 
  {
    ConnectWIFI();
  }
  
  // Check MQTT, connect if needed
  if (!mqttClient.connected())
  {
    // Connect to MQTT broker
    ConnectMQTT();

    // Get baseline on startup
    Serial.println(String(ci_SamplesToTake) + " samples taken every " + String(cl_SampleDelayMillis) + "msec (over period of " + String(cl_SampleDelayMillis * ci_SamplesToTake) + " msecs)");
    b_GetBaseline = true;

  }

  // Double check we're connected, get RSSI (no point getting signal strength unless we can tell OpenHAB)
  if (mqttClient.connected())
  {
    GetAverageRSSI();
    SendMQTTAlive();  
  }
  
  // MQTT loop
  if (mqttClient.connected());
  {
    mqttClient.loop();
  }

}


//
// Starts WIFI connection
//
void ConnectWIFI()
{

  // If we are not connected
  if (WiFi.status() != WL_CONNECTED)
  {
    int iTries = 0;

    Serial.println("Starting WIFI connection");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.begin(cs_SSID, cs_wifi_Password);

    // If not WiFi connected, retry every 100ms for 2 minutes
    while (WiFi.status() != WL_CONNECTED)
    {
      iTries++;
      Serial.print(".");
      delay(200);

      // If can't get to Wifi for 2 minutes, reboot ESP
      if (iTries > 600)
      {
        Serial.println("TOO MANY WIFI ATTEMPTS, REBOOTING!");
        ESP.reset();
      }
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println(WiFi.localIP());

    // Turn LED ON (connected)
    digitalWrite(LED_BUILTIN, LOW);

    // Let network have a chance to start up
    delay(200);

  }

}


//
// Connect to MQTT Broker
//
void ConnectMQTT()
{
  Serial.println("Connecting to MQTT Broker");
  int iRetries = 0;

  // WiFi must be connected, try connecting to MQTT
  while (mqttClient.connect(cs_VehicleID.c_str(), OPENHABSTATUS.c_str(), 0, 0, "0") != 1)
  {

    // Flash LED (effectively adds a small delay between retries as well)
    Serial.println("Error connecting to MQTT (State:" + String(mqttClient.state()) + ")");
    for (int iPos = 1; iPos <= 5; iPos++)
    {
      // Flash pin
      digitalWrite(LED_BUILTIN, LOW);
      delay(50);
      digitalWrite(LED_BUILTIN, HIGH);
      Serial.print(".");
    }

    // Double check WiFi state or # retries while in this loop just incase we timed it badly!
    if (WiFi.status() != WL_CONNECTED || iRetries > 5)
    {
      Serial.println("No MQTT connection...");
      break;
    }

    // Make sure we're not stuck here forever, loop and reconnect WiFi if needed
    iRetries++;

  }

  // Tell MQTT we're alive and our IP
  SendMQTTAlive();
  
}


//
// Publish MQTT data to MQTT broker
//
void PublishMQTTMessage(String sMQTTSub, String sMQTTData)
{

  // Quick check we're still connected (otherwise ESP crash!)
  if (mqttClient.connected())
  {
    // Define and send message about zone state
    sMQTTData.trim();
    sMQTTSub.trim();

    // Convert to char arrays
    sMQTTSub.toCharArray(mqtt_topic, sMQTTSub.length() + 1);
    sMQTTData.toCharArray(message_buff, sMQTTData.length() + 1);

    // Publish MQTT message
    mqttClient.publish(mqtt_topic, message_buff);

    // Flash LED twice to indicate sending MQTT data
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
  }

}


//
// Get samples of RSSI signal strength send to MQTT
//
void GetAverageRSSI()
{

  // Variables
  long l_CurrRSSI = 0;
  long l_SampleRSSI = 0;
  long l_SampleCounter = 0;
  
  // Iterate for X samples
  for (int i_SampleCounter; i_SampleCounter <= ci_SamplesToTake; i_SampleCounter++)
  {
    // Get current RSSI Signal Strength
    if (WiFi.status() == WL_CONNECTED) 
    {
      l_CurrRSSI = WiFi.RSSI();
    }
    else
    {
      l_CurrRSSI = 0;
      l_SampleRSSI = 0;
      break;
    }

    // Add new RSSI to sample data
    l_SampleRSSI = l_SampleRSSI + l_CurrRSSI;

    // Delay between samples
    delay(cl_SampleDelayMillis);

  }  
  
  // Average the RSSI samples over samples to take
  long l_AverageRSSI = l_SampleRSSI / ci_SamplesToTake;

  // Check we got a valid average (RSSI is in negative dBm)
  if (l_AverageRSSI < 0)
  {
    // If getting the baseline
    if (b_GetBaseline == true)  // If creating the baseline (once on startup usually)
    {
      // Signal allowance - add some buffer (so RSSI seen as slightly weaker)
      Serial.println("Setting baseline to first data sample =" + String(l_AverageRSSI - ci_RSSIBuffer) + " (with buffer of -" + String(ci_RSSIBuffer) + ")");
      l_RSSIBaseline = l_AverageRSSI - ci_RSSIBuffer;
      b_GetBaseline = false;
    }
    else
    {
      // Analyze new sample against original baseline
      if (l_AverageRSSI >= l_RSSIBaseline)
      {
        Serial.println("Home (Average RSSI=" + String(l_AverageRSSI) + " > Baseline=" + String(l_RSSIBaseline) + ")");
        PublishMQTTMessage(OPENHABVEHICLESTATE, "1");
      }
      else if (l_AverageRSSI < l_RSSIBaseline)
      {
        Serial.println("Away (Average RSSI=" + String(l_AverageRSSI) + " < Baseline=" + String(l_RSSIBaseline) + ")");
        PublishMQTTMessage(OPENHABVEHICLESTATE, "0");
      }
    }
  }
  else
  {
    // Get baseline again
    b_GetBaseline  = true;
  }
  
}

//
// Send keep alive MQTT LWT message
//
void SendMQTTAlive()
{

  // Let OH know vehicle is still on every X seconds
  if ((millis() - ul_Alive) > (ci_SendAliveEvery * 1000))
  {
    Serial.println("Telling OpenHAB we're still alive");
    PublishMQTTMessage(OPENHABSTATUS, "1");
    if (WiFi.localIP().toString() != wemosIPAddress)
    {
      wemosIPAddress=WiFi.localIP().toString();
      PublishMQTTMessage(OPENHABMYIPADDRESS, wemosIPAddress);
    }
    ul_Alive = millis();
  }

}
