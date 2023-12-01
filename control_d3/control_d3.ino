#include <WiFi.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_PIR.h>
#include <AccelStepper.h>
#include <Arduino.h>
#include <WebSocketsServer.h>
#include "esp_camera.h"
#include "FirebaseESP32.h"

#define motorPin1 14
#define motorPin2 12
#define motorPin3 13
#define motorPin4 15

AccelStepper stepper(AccelStepper::FULL4WIRE, motorPin1, motorPin2, motorPin3, motorPin4);

Servo motor;
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
Adafruit_PIR pir = Adafruit_PIR(2);  // PIR Motion Sensor connected to pin 2

const int motorPin = 2;
const int angleForward = 145;
const int angleBackward = -145;
const int delayTime = 15000;
const int weightThreshold = 4000;
const int currentPosition = 0;
const int pirPin = 2;  // PIR Motion Sensor pin

const char* ssid = "YourWiFiSSID";
const char* password = "YourWiFiPassword";
const char* firebaseHost = "esp32fb-761ec";
const char* firebaseAuth = "BTE9beuyeLR5QT8KPfDEroNUqdf1";
const char* firebaseStoragePath = "gs://esp32fb-761ec.appspot.com";

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

const char* firebaseStorageBucket = "your-firebase-storage-bucket";
FirebaseStorage storage;
FirebaseData fbdo;

WiFiServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
bool isStreaming = false;
bool isMotorOn = false;

unsigned long previousMillis = 0;
unsigned long captureInterval = 30000;  // Capture image every 30 seconds
unsigned long motorStartTime = 0;
const long motorTimeout = 600000;  // 10 minutes timeout

void setup() {
  Serial.begin(115200);
  pinMode(pirPin, INPUT);  // Set PIR Motion Sensor pin as input
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  Firebase.begin(firebaseHost, firebaseAuth);
  storage.begin(firebaseStorageBucket, firebaseAuth);

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  motor.attach(motorPin);
  mlx.begin();
  connectToWiFi();
  cameraSetup();
  server.begin();
}

void cameraSetup() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
}

void rotateMotor(int angle) {
  motor.write(angle);
  delay(1000);
}

bool checkConditions() {
  bool noLivingObjectsDetected = mlx.readObjectTempC() < 37;
  bool weightNotExceedThreshold = true;
  bool motionDetected = digitalRead(pirPin) == HIGH;  // Check PIR Motion Sensor
  return noLivingObjectsDetected && weightNotExceedThreshold && !motionDetected;
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
}

void toggleMotor() {
  rotateMotor(angleForward);
  delay(delayTime);
  rotateMotor(0);
  delay(delayTime);
  rotateMotor(angleBackward);
  delay(delayTime);
  rotateMotor(currentPosition);
}

void uploadImageToFirebase(fs::FS &fs, const char * path) {
  File file = fs.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.println("Uploading image to Firebase Storage...");
  String fileName = "/" + String(millis()) + ".jpg";
  if (Firebase.Storage.upload(storage, firebaseStoragePath + fileName, file, "image/jpeg")) {
    Serial.println("Upload success");
  } else {
    Serial.println("Upload failed");
  }

  file.close();
}

void captureAndUploadImage() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  fs::FS &fs = SD;
  uploadImageToFirebase(fs, "/capture.jpg");
  esp_camera_fb_return(fb);
}

void handleRequest() {
  WiFiClient client = server.available();
  if (client) {
    Serial.println("New client connected");
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (currentLine.startsWith("GET /toggleMotor")) {
            toggleMotor();
          }
          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println();
          break;
        } else {
          currentLine += c;
        }
      }
    }
    client.stop();
    Serial.println("Client disconnected");
  }
}

void loop() {
  unsigned long currentMillis = millis();  // ดึงค่าเวลาปัจจุบัน

  if (webSocket.available()) {
    String message = webSocket.readString(); // ตรวจสอบว่าได้รับค่าเวลาใหม่หรือไม่
    if (message.startsWith("setInterval:")) {
      captureInterval = message.substring(13).toInt(); // ถ้าได้รับค่าเวลาใหม่ ให้อัปเดตค่า captureInterval
    }
  }

  if (currentMillis - previousMillis >= captureInterval) {
    previousMillis = currentMillis;  // บันทึกค่าเวลาปัจจุบัน
    if (checkConditions()) {
      toggleMotor();
    } else {
      captureAndUploadImage();
    }
  }

  if (isMotorOn) {
    if (currentMillis - motorStartTime >= motorTimeout) {
      isMotorOn = false;
      motorStartTime = 0;
    }
  }

  handleRequest();
}
