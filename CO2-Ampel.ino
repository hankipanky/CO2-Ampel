#include "Secrets.h"
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHTesp.h>
#include <Adafruit_NeoPixel.h>

const char* mqttServer = "mqtt.thingspeak.com";
char mqttUserName[] = "Esp8266AirQuality";   // Use any name.

static const char alphanum[] = "0123456789"
                               "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                               "abcdefghijklmnopqrstuvwxyz";  // For random generation of client ID.
unsigned long lastConnectionTime = 0;
const unsigned long postingInterval = 60L * 1000L; // Post data interval

SoftwareSerial co2Serial(D1, D2); // RX, TX Pinns festlegen
DHTesp dht;

#define PIXEL_PIN   D5    // Digital IO pin connected to the NeoPixels.
#define PIXEL_COUNT 3
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);


WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

char temperatureCString[6];

void setup() {
  Serial.begin(115200);
  delay(10);

  pixels.begin();
  pixels.clear();
  Serial.println("Pixelstrip initialized");

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);

  // WiFi.begin(ssid, password);
  WiFi.begin(WLAN_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());

  co2Serial.begin(9600);

  dht.setup(D3, DHTesp::DHT11);

  Serial.println();
  mqtt.setServer(mqttServer, 1883);

  Serial.println("Completed setup");
}

void reconnect()
{
  char clientID[9];

  // Loop until reconnected.
  while (!mqtt.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Generate ClientID
    for (int i = 0; i < 8; i++) {
      clientID[i] = alphanum[random(51)];
    }
    clientID[8] = '\0';

    // Connect to the MQTT broker.
    if (mqtt.connect(clientID, mqttUserName, MQTT_PASS))
    {
      Serial.println("connected");
    } else
    {
      Serial.print("failed, rc=");
      // Print reason the connection failed.
      // See https://pubsubclient.knolleary.net/api.html#state for the failure code explanation.
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

int getCO2() {
  byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
  char antwort[9];
  co2Serial.write(cmd, 9);
  co2Serial.readBytes(antwort, 9);
  if (antwort[0] != 0xFF) return -1;
  if (antwort[1] != 0x86) return -1;
  int antwortHigh = (int) antwort[2]; // CO2 High Byte
  int antwortLow = (int) antwort[3];  // CO2 Low Byte
  int ppm = (256 * antwortHigh) + antwortLow;
  return ppm;                         // Antwort des MH-Z19 CO2 Sensors in ppm
}

uint32_t getPixelColour(int co2) {
  if (co2 < 800) return pixels.Color(0, 150, 0);
  if (co2 < 900) return pixels.Color(200, 200, 0);
  if (co2 < 1000) return pixels.Color(250, 200, 0);
  if (co2 < 1100) return pixels.Color(250, 100, 0);
  if (co2 < 1200) return pixels.Color(250, 50, 0);
  return pixels.Color(250, 0, 0);
}


void loop() {
  if (!mqtt.connected()) {
    reconnect();
  }
  mqtt.loop();

  long now = millis();
  if (now - lastConnectionTime > postingInterval) {
    lastConnectionTime = now;
    String data = "";

    int co2 = getCO2();
    Serial.print("CO2: ");
    Serial.print(co2);
    Serial.println("ppm");
    data += "field1=" + String(co2, DEC);

    float temp = dht.getTemperature();
    Serial.print("Temp: ");
    Serial.print(temp);
    Serial.println("°C");
    if (!isnan(temp)) {
      data += "&field2=" + String(temp, 1);
    }
    
    float humidity = dht.getHumidity();
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println("%");
    if (!isnan(humidity)) {
      data += "&field3=" + String(humidity, 1);
    }
    
    int length = data.length();
    const char *msgBuffer;
    msgBuffer = data.c_str();
    Serial.println(msgBuffer);

    // Create a topic string and publish data to ThingSpeak channel feed.
    String topicString = "channels/" + String(MQTT_CHANNEL_ID) + "/publish/" + String(MQTT_API_KEY);
    length = topicString.length();
    const char *topicBuffer;
    topicBuffer = topicString.c_str();
    if (mqtt.publish( topicBuffer, msgBuffer )) {
      Serial.println("Message published!");
    } else {
      Serial.println("Error publishing message!");
    }

    pixels.clear();
    pixels.setPixelColor(0, getPixelColour(co2));
    pixels.setPixelColor(1, getPixelColour(co2));
    pixels.setPixelColor(2, getPixelColour(co2));
    pixels.show();
    Serial.println("Pixelstrip activated");

  }
  delay(100);

}
