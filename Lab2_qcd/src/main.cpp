#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <UrlEncode.h>

#define VR 33 //VR pin
#define led 18 //led pin
#define ldr 32 //idr pin
#define DHT_PIN 19 //DHT pin
#define DHT_TYPE DHT22 //DHT22

const char* ssid = "kotoha";
const char* password = "Jimmy1ac";

// MQTT broker
const char* MQTT_username = "4522lab2";
const char* MQTT_password = "12344321";

// MQTT broker ip
const char* mqtt_server = "broker.emqx.io";

// Initializes the espClient
WiFiClient espClient;
PubSubClient client(espClient);

// Initialize DHT sensor
DHT dht(DHT_PIN, DHT_TYPE);

// Timers auxiliar variables
long now = millis();
long lastMeasure = 0;

// connects ESP to wifi
void setup_wifi() {
  delay(10);
// We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected - ESP IP address: ");
  Serial.println(WiFi.localIP());
}
//led state
int less = 30;
int exceeds = 65;
bool ledState = false;
//mqtt clb
void callback(String topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

// Feel free to add more if statements to control more GPIOs with MQTT

// If a message is received. on or off.
  if (String(topic)=="LED"){
    Serial.print("Changing pump to ");
    if(messageTemp == "on"){
      digitalWrite(led, HIGH);
      Serial.print("on");
      ledState = true;
    }
    else if(messageTemp == "off"){
      digitalWrite(led, LOW);
      Serial.print("off");
      ledState = false;
    }
  }
  Serial.println();
}

// This functions reconnects your ESP8266 to your MQTT broker
// Change the function below if you want to subscribe to more topics with your ESP8266
void reconnect() {
// Loop until we're reconnected
while (!client.connected()) {
Serial.print("Attempting MQTT connection...");
// Attempt to connect
  if (client.connect("Client", MQTT_username, MQTT_password)) {
    Serial.println("connected");
    // Subscribe or resubscribe to a topic
    // You can subscribe to more topics (to control more LEDs in this example)
    client.subscribe("LED");
    }
    else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 5 seconds");
    // Wait 5 seconds before retrying
    delay(5000);
    }
  }
}

String phoneNumber = "+85251673200";
String apiKey = "8121606";

void sendMessage(String message){

// Data to send with HTTP POST
  String url = "https://api.callmebot.com/whatsapp.php?phone=" + phoneNumber + "&apikey=" + apiKey + "&text=" + urlEncode(message);
  HTTPClient http;
  http.begin(url);

// Specify content-type header
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

// Send HTTP POST request
int httpResponseCode = http.POST(url);
  if (httpResponseCode == 200){
    Serial.print("Message sent successfully");
  }
  else{
    Serial.println("Error sending the message");
    Serial.print("HTTP response code: ");
    Serial.println(httpResponseCode);
  }

// Free resources
  http.end();
}
//sets ESP to Outputs, starts the serial 115200
// Sets mqtt broker and sets the callback function
// The callback function is what receives messages and actually controls the LEDs
void setup() {
  pinMode(led, OUTPUT);

  dht.begin();

  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

}
//api set time
unsigned long Fill_water = 0;

void loop() {
  //map VR & IDR
  int VRvalue = analogRead(VR);
  int SoilMoisture = map(VRvalue, 0, 4095, 0, 100);
  int ldrvalue = analogRead(ldr);
  int brightness = map(ldrvalue, 0, 4095, 0, 100);
  if (!client.connected()) {
    reconnect();
  }
  if(!client.loop())
    client.connect("Client", MQTT_username, MQTT_password);

  now = millis();
  // Publishes new temperature and humidity every 1 seconds
  if (now - lastMeasure > 1000) {
    lastMeasure = now;
    // Sensor reading(slow)
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

  // Publishes Temperature and Humidity values
  client.publish("dht/temperature", String(temperature).c_str());
  client.publish("dht/humidity", String(humidity).c_str());
  //Uncomment to publish temperature in F degrees
  client.publish("soil moisture", String(SoilMoisture).c_str());
  client.publish("ldr/brightness", String(brightness).c_str());
  }


  //set led if less than 30
  if (SoilMoisture < less && !ledState) {
    digitalWrite(led, HIGH);  // op LED
    ledState = true;
    client.publish("LED", "on");
  } else if(SoilMoisture > exceeds && ledState){
    digitalWrite(led, LOW);   // cl LED
    ledState = false;
    client.publish("LED", "off");
  }
  //if led on & less than30 over 30s
  if (ledState && SoilMoisture < 30) {
    // timer st
    if (Fill_water == 0) {
      Fill_water = millis();
    }
    // 30s send msg
    if (millis() - Fill_water >= 30000) {
        sendMessage("The water tank is running out! Please fill it up.");
        // reset timer(30s)
        Fill_water = 0;
    }
  } 
  else {
    // reset timer
    Fill_water = 0;
  }
}