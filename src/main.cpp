#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

const char* ssid = "UW MPSK";
const char* password = "xKYV;j?44#"; // Replace with your network password
#define DATABASE_URL "https://esp32-firebase-demo-53641-default-rtdb.firebaseio.com/" // Replace with your database URL
#define API_KEY "AIzaSyA7Iunk4YsBKpnsnVu62YsjVQHhT6Crwxc" // Replace with your API key
#define MAX_WIFI_RETRIES 5 // Maximum number of WiFi connection retries

int uploadInterval = 5000; // 1 seconds 2 upload

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
int count = 0;
bool signupOK = false;

// HC-SR04 Pins
const int trigPin = 2;
const int echoPin = 3;

// Define sound speed in cm/usec
const float soundSpeed = 0.034;

// Function prototypes
float measureDistance();
void connectToWiFi();
void initFirebase();
void sendDataToFirebase(float distance);

void setup() {
  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  connectToWiFi();
  initFirebase();

  unsigned long startTime = millis();
  unsigned long lastTimeObjectDetected = millis();

  while (true) {
    float distance = measureDistance();

    // Check if distance is greater than 50cm
    if (distance > 50) {
      // Check if 30 seconds have passed since the object was last detected within 50cm
      if (millis() - lastTimeObjectDetected > 30000) {
        // Send a final message before going to sleep
        Serial.println("No object detected within 50cm for 30 seconds. Going to sleep.");
        sendDataToFirebase(distance);  // Send the last distance reading before sleep

        // Deep sleep for 30 seconds
        esp_sleep_enable_timer_wakeup(30000 * 1000); // Time in microseconds
        esp_deep_sleep_start();
      }
    } else { // if less than 50cm, send to firebase every 2 seconds
      // Reset the timer as the object is detected within 50cm
      lastTimeObjectDetected = millis();
      if (millis() - startTime > 2000) {
        sendDataToFirebase(distance);
        startTime = millis();
      }
    }

    delay(100); // Delay to prevent too frequent sensor readings
  }
}

void loop(){
  // This is not used
}

float measureDistance()
{
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH);
  float distance = duration * soundSpeed / 2;

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");
  return distance;
}

void connectToWiFi()
{
  // Print the device's MAC address.
  Serial.println(WiFi.macAddress());
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  int wifiCnt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    wifiCnt++;
    if (wifiCnt > MAX_WIFI_RETRIES){
      Serial.println("WiFi connection failed");
      ESP.restart();
    }
  }
  Serial.println("Connected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void initFirebase()
{
  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
}

void sendDataToFirebase(float distance){
    if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > uploadInterval || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();
    // Write an Float number on the database path test/float
    if (Firebase.RTDB.pushFloat(&fbdo, "test/distance", distance)){
      Serial.println("PASSED");
      Serial.print("PATH: ");
      Serial.println(fbdo.dataPath());
      Serial.print("TYPE: " );
      Serial.println(fbdo.dataType());
    } else {
      Serial.println("FAILED");
      Serial.print("REASON: ");
      Serial.println(fbdo.errorReason());
    }
    count++;
  }
}