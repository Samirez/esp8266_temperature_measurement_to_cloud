#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DHT.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>

const char* ssid = "test1234";
const char* password = "test";

const char *MQTT_HOST = "broker.emqx.io";
const int MQTT_PORT = 1883;
const char *MQTT_CLIENT_ID = "ESP8266 NodeMCU";
const char *MQTT_USER = "askar";
const char *MQTT_PASSWORD = "kushan";
const char *TOPIC = "DHT22/data";

const char* brightnessLevels[] = {"Dark", "Dim", "Light", "Bright", "Very bright"};

#define DHTPIN 5
#define DHTTYPE DHT22
#define LightPin A0

DHT dht(DHTPIN, DHTTYPE);
WiFiClient client;
PubSubClient mqttClient(client);

unsigned long previousMillis = 0;
const long interval = 10000;
float t;
float h;


enum LightLevel { DARK, DIM, LIGHT, BRIGHT, VERY_BRIGHT };
LightLevel light;

AsyncWebServer server(80);

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <style>
    html {
      font-family: Arial;
      display: inline-block;
      margin: 0px auto;
      text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels {
      font-size: 1.5rem;
      vertical-align: middle;
      padding-bottom: 15px;
    }
    .brightness {
      font-size: 1.5rem;
      color: orange;
    }
  </style>
</head>
<body>
  <h2>ESP8266 DHT Server</h2>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="dht-labels">Temperature</span> 
    <span id="temperature">%TEMPERATURE%</span>
    <sup class="units">°C</sup>
  </p>
  <p>
    <i class="fas fa-tint" style="color:#00add6;"></i> 
    <span class="dht-labels">Humidity</span>
    <span id="humidity">%HUMIDITY%</span>
    <sup class="units">%</sup>
  </p>
  <br>
  <p>
    <i class="fas fa-lightbulb" style="color:#f1c40f;"></i> 
    <span class="dht-labels">Light</span>
    <span id="light">%LIGHT%</span>
  </p>
</body>
<script>
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("temperature").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperature", true);
  xhttp.send();
}, 10000 ) ;

setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("humidity").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/humidity", true);
  xhttp.send();
}, 10000 ) ;
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("light").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/light", true);
  xhttp.send();
}, 10000 ) ;
</script>
</html>)rawliteral";


// Replaces placeholder with DHT values
String processor(const String& var){
  if(var == "TEMPERATURE") return String(t);
  if(var == "HUMIDITY") return String(h);
  if(var == "LIGHT") {
    return String(brightnessLevels[light]);
  }
  return String();
}

void readLightSensor() {
  const int levels[] = {40, 800, 2000, 3200};
  const LightLevel lightLevels[] = {DARK, DIM, LIGHT, BRIGHT, VERY_BRIGHT};

  int analogValue = analogRead(LightPin);
  for (int i = 0; i < 4; i++) {
    if (analogValue < levels[i]) {
      light = lightLevels[i];
      return;
    }
  }
  light = VERY_BRIGHT;
}

void callback(char* topic, byte* payload, unsigned int length)
{
    payload[length] = '\0';
    int value = String((char*) payload).toInt();

    Serial.println(topic);
    Serial.println(value);
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
  dht.begin();
  
  // Connect to Wi-Fi
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println(".");
  }

  // Print ESP8266 Local IP Address
  Serial.println(WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(t));
  });
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(h));
  });
  server.on("/light", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", String(light));
  });
  // Start server
  server.begin();

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(callback);

  while (!client.connected()) {
      if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD )) {
          Serial.println("Connected to MQTT broker");
      } else {
          delay(500);
          Serial.print(".");
      }
    }
}
 
void printBrightness() {
  Serial.print("Brightness: ");
  Serial.println(brightnessLevels[light]);
}

void readDHT() {
  h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  t = dht.readTemperature();
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  } else {
    Serial.print("Temperature: "+String(t)+" Humidity: "+String(h)+" ");
  }
}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    readDHT();
    readLightSensor();
    printBrightness();
    String payload = "Temperature: "+String(t)+" Humidity: "+String(h)+" Brightness: "+brightnessLevels[light];
    mqttClient.publish(TOPIC, (const uint8_t*)payload.c_str(), payload.length(), true);
  }
  mqttClient.loop();
}

