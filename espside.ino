#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>

// ---------- User Configuration ----------
const char* WIFI_SSID   = "xxxx";
const char* WIFI_PASS   = "xxxxx";

const char* MQTT_BROKER = "xxxxxx";   
const int   MQTT_PORT   = 1234;
const char* MQTT_TOPIC  = "stm32/status";

// Use UART2 on ESP32 (Serial2)
#define STM32_RX_PIN 16    
#define STM32_TX_PIN 17    

// NTP config
const char* NTP_SERVER_1 = "pool.ntp.org";
const char* NTP_SERVER_2 = "time.nist.gov";
const long  GMT_OFFSET_SEC = 19800;  
const int   DAYLIGHT_OFFSET_SEC = 0;

// Globals
WiFiClient espClient;
PubSubClient mqttClient(espClient);
HardwareSerial STM32Serial(2); 
String uartLine = "";

// ---------- WiFi ----------
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());
}

// ---------- Time (NTP) ----------
void setupTime() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);

  Serial.print("Waiting for time sync");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.println("Time synchronized");
}

String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "1970-01-01T00:00:00"; // fallback
  }

  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  return String(buf);
}

// ---------- MQTT ----------
void connectMQTT() {
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);

  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    if (mqttClient.connect("esp32_stm32_bridge")) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 1s");
      delay(1000);
    }
  }
}

void publishMessage(const String& msgFromSTM32) {
  String ts = getTimestamp();

  String payload = String("{\"msg\":\"") + msgFromSTM32 + "\",\"ts\":\"" + ts + "\"}";

  bool ok = mqttClient.publish(MQTT_TOPIC, payload.c_str());
  Serial.print("MQTT publish: ");
  Serial.println(payload);

  if (!ok) {
    Serial.println("MQTT publish failed");
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);  

  STM32Serial.begin(115200, SERIAL_8N1, STM32_RX_PIN, STM32_TX_PIN);
  Serial.println("UART2 initialized for STM32");

  connectWiFi();
  setupTime();
  connectMQTT();
}

// ---------- Main Loop ----------
void loop() {
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();

  while (STM32Serial.available() > 0) {
    char c = (char)STM32Serial.read();

    if (c == '\n') {
     
      uartLine.trim();  

      if (uartLine.length() > 0) {
        publishMessage(uartLine);
      }

  
      uartLine = "";
    } else {
      uartLine += c;
    }
  }

  
}
