#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <WiFi.h>
#include <PubSubClient.h>

Adafruit_MPU6050 mpu;

// WiFi config
const char* ssid = "IME_WIFI_MREZE";
const char* password = "LOZINKA";

// MQTT Broker config
const char* mqttServer = "mqtt.flespi.io";
const int mqttPort = 1883;
const char* mqttUser = "FLESPI_TOKEN";
const char* mqttPassword = "";

// Topic i JSON polja koje mozes mijenjati
const char* mqttTopic = "Trafostanica1/Trafo1";
const char* jsonField1 = "AccelZ";
const char* jsonField2 = "Temperature";

WiFiClient espClient;
PubSubClient client(espClient);

// Payload config.
const float SAMPLE_FREQ = 1; // U kHz
const int BUFFER_SIZE = 20;

// Otpornici za prevojnu tacku T=20.58 [°C]
// R1=680000; // Paralelni otpornik 680k
// R2=4700;   // Serijski otpornik 4.7k

// Koeficijenti pravca za prevojnu tacku T=20.58 [°C]
const float k = 0.072;
const float n = -18.858;
// Formula T=(V-n)/k [K]

void setup() {
  // Serial setup----------
  Serial.begin(115200);
  delay(10);
  
  // WIFI Setup--------------------------------------
  WiFi.begin(ssid, password);
  Serial.println("ESP32 Booted");
  Serial.println("Wifi starting, Trying to connect...");
  
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t < 10 * 500) {
      Serial.print(".");
    } else {
      Serial.println("");
      t = millis();
      Serial.print("Trying to connect");
    }
    delay(500);
  }
  
  Serial.println("\nWiFi connected successfully");
  Serial.println(WiFi.localIP());
  
  // MQTT Setup--------------------------------------
  client.setServer(mqttServer, mqttPort);
  
  // MPU6050 Setup-----------------------------------
  // Try to initialize!
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }
  
  Serial.println("MPU6050 Found!");
  
  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  Serial.print("Accelerometer range set to: ");
  switch (mpu.getAccelerometerRange()) {
    case MPU6050_RANGE_2_G:
      Serial.println("+-2G");
      break;
    case MPU6050_RANGE_4_G:
      Serial.println("+-4G");
      break;
    case MPU6050_RANGE_8_G:
      Serial.println("+-8G");
      break;
    case MPU6050_RANGE_16_G:
      Serial.println("+-16G");
      break;
  }
  
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  Serial.print("Gyro range set to: ");
  switch (mpu.getGyroRange()) {
    case MPU6050_RANGE_250_DEG:
      Serial.println("+- 250 deg/s");
      break;
    case MPU6050_RANGE_500_DEG:
      Serial.println("+- 500 deg/s");
      break;
    case MPU6050_RANGE_1000_DEG:
      Serial.println("+- 1000 deg/s");
      break;
    case MPU6050_RANGE_2000_DEG:
      Serial.println("+- 2000 deg/s");
      break;
  }
  
  mpu.setFilterBandwidth(MPU6050_BAND_260_HZ); // sve iznad ove freq se filtrira
  Serial.print("Filter bandwidth set to: ");
  switch (mpu.getFilterBandwidth()) {
    case MPU6050_BAND_260_HZ:
      Serial.println("260 Hz");
      break;
    case MPU6050_BAND_184_HZ:
      Serial.println("184 Hz");
      break;
    case MPU6050_BAND_94_HZ:
      Serial.println("94 Hz");
      break;
    case MPU6050_BAND_44_HZ:
      Serial.println("44 Hz");
      break;
    case MPU6050_BAND_21_HZ:
      Serial.println("21 Hz");
      break;
    case MPU6050_BAND_10_HZ:
      Serial.println("10 Hz");
      break;
    case MPU6050_BAND_5_HZ:
      Serial.println("5 Hz");
      break;
  }
  
  Serial.println("");
  delay(100);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str(), mqttUser, mqttPassword)) {
      Serial.println("Connected successfully!");
    } else {
      Serial.print("Connection failed, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

void loop() {
  // MQTT Povezivanje-------------------------------
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // Get new sensor events with the readings
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  
  /* Print out the values */
  // Serial.println(a.acceleration.z-10); // minus 10 zbog gravitacije 9.81~10
  
  // Payload Unos-----------------------------------
  String payload = "{";
  payload += "\"";
  payload += jsonField1;
  payload += "\":[";
  
  // Prvi field NIZ---------------------------------
  for (int i = 0; i < BUFFER_SIZE; i++) {
    // Get fresh reading for each sample
    mpu.getEvent(&a, &g, &temp);
    payload += String(a.acceleration.z - 10); // Svoje vrijednosti
    
    if (i < BUFFER_SIZE - 1) {
      payload += ",";
    }
    delay(1000 / SAMPLE_FREQ); // Convert kHz to ms delay
  }
  
  payload += "],\"";
  payload += jsonField2;
  payload += "\":";
  
  // Drugi field -----------------------------------
  int adcValue = analogRead(34);
  float voltage = adcValue * (3.3 / 4095.0);
  float TempC = (voltage - n) / k - 273.15;
  payload += String(TempC); // Svoje vrijednosti
  payload += "}";
  
  client.publish(mqttTopic, payload.c_str());
  Serial.println("Poslato: " + payload);
  
  // delay(2000);
}