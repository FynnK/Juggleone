#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "ESPAsyncWebServer.h"
#include "AsyncElegantOTA.h"
#include <TinyMPU6050.h>
#include <SPIFFS.h>
#include "driver/adc.h"
#include <esp_wifi.h>
#include <esp_bt.h>
#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <AsyncWebSocket.h>
#include "rotatingbuffer.hpp"
#include "json.hpp"

constexpr auto LED_EN = 32;
constexpr auto LED_CNT = 8;
constexpr auto LED_PIN = 16;
constexpr auto BUTTON_PIN = 0;
constexpr auto MPU_INT_PIN = 19;
constexpr auto MPU6050_INT_PIN_CFG = 0x37;
constexpr auto MPU6050_INT_ENABLE = 0x38;
constexpr auto MPU_ARRAY_SIZE = 50;
constexpr auto MAX_LIGHTING_MODE = 3;

constexpr auto ssid = "EspBall";
constexpr auto stat_ssid = "retaeper";
constexpr auto password = "welchespasswort";

constexpr auto accFileString = "/acc.txt";
constexpr auto logFileString = "/log.txt";

uint32_t startTime;
uint32_t endTime;
int mpuCnt = 0;

bool isUpdating = false;
bool logging = false;
bool log10k = false;

int lightingMode = 3;

int buttonState = HIGH;
bool buttonHandled = true;
bool buttonIsDown = false;
bool mpuLogging = true;

bool updaterRunning = true;
uint8_t brightness = 15;
uint16_t hue = 0;

uint32_t lastDownTime;
uint32_t lastUpTime;
int lastDT = 0;
bool logAccHandle = true;
uint32_t ledArr[8] = {};

RotatingBuffer<MPU6050Data, MPU_ARRAY_SIZE> mpuData{};
float accTot;

TaskHandle_t lightingTaskHandle;
TaskHandle_t updaterTaskHandle;
TaskHandle_t mpuTaskHandle;

Adafruit_NeoPixel leds(LED_CNT, LED_PIN, NEO_GRBW + NEO_KHZ800);

MPU6050 mpu(Wire);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");


struct pixel {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t w;
};

pixel pixels[8] = {};


void IRAM_ATTR buttonChanged()
{
  buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == HIGH)
  {
    lastUpTime = xTaskGetTickCountFromISR();
    buttonIsDown = false;
    buttonHandled = false;
  }
  else if (buttonState == LOW)
  {
    lastDownTime = xTaskGetTickCountFromISR();
    buttonIsDown = true;
  }
}

void log(String text)
{
  if (logging)
  {
    File logFile = SPIFFS.open(logFileString, FILE_APPEND);
    logFile.print(text);
    logFile.close();
  }
}

void logAcc()
{
  File accFile = SPIFFS.open(accFileString, FILE_WRITE);
  for (int i = 0; i < MPU_ARRAY_SIZE; i++)
  {
    accFile.print(mpuData[i].accel.x());
    accFile.print(',');
    accFile.print(mpuData[i].accel.y());
    accFile.print(',');
    accFile.print(mpuData[i].accel.z());
    accFile.print(',');
    accFile.print(mpuData[i].orient.x());
    accFile.print(',');
    accFile.print(mpuData[i].orient.y());
    accFile.print(',');
    accFile.print(mpuData[i].orient.z());
  }
  accFile.close();
}

void logln(String text)
{
  if (logging)
  {
    File logFile = SPIFFS.open(logFileString, FILE_APPEND);
    logFile.println(text);
    logFile.close();
  }
}

void lightingTask(void *pvParameters)
{
  log("lightingTask started on core: ");
  logln(String(xPortGetCoreID()));

  while (true)
  {
    vTaskDelay(10);

    if (lightingMode == 0)
    {
      for (int i = 0; i < 8; i++)
      {
        leds.setPixelColor(i, leds.ColorHSV((uint16_t)(accTot * 8192), 255, 255));
      }
    }
    else if (lightingMode == 1)
    {
      for (int i = 0; i < 8; i++)
      {
        leds.setPixelColor(i, leds.ColorHSV((xTaskGetTickCount() * 10 + i * 8192) % 65535, 255, 255));
      }
    }
    else if (lightingMode == 2)
    {
      for (int i = 0; i < 8; i++)
      {
        leds.setPixelColor(i, leds.ColorHSV((xTaskGetTickCount() * 10) % 65535, 255, 255));
      }
    }else if (lightingMode == 3)
    {
      for (int i = 0; i < 8; i++)
      {
        leds.setPixelColor(i, leds.Color(pixels[i].r, pixels[i].g, pixels[i].b, pixels[i].w));
      }
    }

    if (leds.getBrightness() != brightness)
    {
      leds.setBrightness(brightness % 256);
    }
    if(leds.canShow())
    leds.show();
  }
}

void mpuTask(void *pvParameters)
{
  log("mpuTask started on core: ");
  logln(String(xPortGetCoreID()));

  mpu.Initialize();
  mpu.RegisterWrite(MPU6050_ACCEL_CONFIG, 0b00010000);
  mpu.RegisterWrite(MPU6050_INT_PIN_CFG, 0b00110000);
  mpu.RegisterWrite(MPU6050_INT_ENABLE, 0b00000001);

  startTime = xTaskGetTickCount();

  while (true)
  {
    if (digitalRead(MPU_INT_PIN) == HIGH && mpuLogging)
    {
      mpu.Execute();
      mpuData.push(mpu);

      auto sq = [](float x) { return x * x; };
      (void)sq; // supress warning
      accTot = sqrt(sq(mpuData[0].accel.x()) + sq(mpuData[0].accel.y()) + sq(mpuData[0].accel.z()));
      mpuCnt++;

      if (mpuCnt == 9999)
      {
        endTime = xTaskGetTickCount();
        log10k = true;
      }
    }
    vTaskDelay(8);
  }
}

void setLightingMode(uint8_t inMode){
  lightingMode = inMode%(MAX_LIGHTING_MODE+1);
}

void setBrightness(uint8_t inBrightness){
  brightness = inBrightness%256;
}
void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
  if(type == WS_EVT_CONNECT){

    logln("Websocket client connection received");

  } else if(type == WS_EVT_DISCONNECT){
    logln("Client disconnected");

  } else if(type == WS_EVT_DATA){

    StaticJsonDocument<1200> doc;
      DeserializationError error = deserializeJson(doc, data);
      setBrightness(doc["brightness"]);
      setLightingMode(doc["mode"]);
      for(int i = 0; i < 8; i++){
        pixels[i].r = doc["pixels"][i][0];
        pixels[i].g = doc["pixels"][i][1];
        pixels[i].b = doc["pixels"][i][2];
        pixels[i].w = doc["pixels"][i][3];
      }
    client->text(error.c_str());
  }
}



void updaterTask(void *pvParameters)
{

  leds.setPixelColor(0, 255, 10, 30, 10);
  leds.show();

  esp_wifi_start();
  WiFi.setTxPower(WIFI_POWER_5dBm);
  WiFi.softAP(ssid, password);

  log("SoftAP started with ip: ");
  logln(WiFi.softAPIP().toString());

  log("updater running on core: ");
  logln(String(xPortGetCoreID()));

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/update");
  });


  server.on("/format", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "formatting SPIFFS");

    SPIFFS.format();
  });


  server.on("/files", HTTP_GET, [](AsyncWebServerRequest *request) {
    String out;  
    File root = SPIFFS.open("/");
    File file = root.openNextFile();

    while (file)
    {
      out.concat(file.name());
      out.concat("\n");
      file = root.openNextFile();
    }
    request->send(200, "text/plain", out);
  });

  server.on("/logAcc", HTTP_GET, [](AsyncWebServerRequest *request) {
    vTaskSuspend(mpuTaskHandle);
    logAcc();
    vTaskResume(mpuTaskHandle);
    request->send(200, "text/plain", "loggingAccs");
  });

  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "restarting");
    ESP.restart();
  });

  server.on("/clearAccLogs", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (SPIFFS.exists(accFileString))
      ;
    {
      SPIFFS.remove(accFileString);
    }
    request->send(200, "text/plain", "accLogs Cleared");
  });

  server.on("/availableSpiffs", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (SPIFFS.exists(logFileString))
    {
      int freeBytes = SPIFFS.totalBytes() - SPIFFS.usedBytes();
      String msg = "there is " + (String)freeBytes + " Bytes available in SPIFFS";
      request->send(200, "text/plain", msg);
    }
  });

  server.on("/clearLogs", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (SPIFFS.exists(logFileString))
      ;
    {
      SPIFFS.remove(logFileString);
    }
    request->send(200, "text/plain", "logs Cleared");
  });

  server.on("/accLogs", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (SPIFFS.exists(accFileString))
    {
      request->send(SPIFFS, accFileString, "text/plain");
    }
  });

  server.on("/getAccsCsv", HTTP_GET, [](AsyncWebServerRequest *request) {
    String buffer;
    for (int i = 0; i < MPU_ARRAY_SIZE; i++) {
      buffer.concat(mpuData[i].timestamp);
      buffer.concat(',');
      buffer.concat(mpuData[i].accel.x());
      buffer.concat(',');
      buffer.concat(mpuData[i].accel.y());
      buffer.concat(',');
      buffer.concat(mpuData[i].accel.z());
      buffer.concat(',');
      buffer.concat(mpuData[i].orient.x());
      buffer.concat(',');
      buffer.concat(mpuData[i].orient.y());
      buffer.concat(',');
      buffer.concat(mpuData[i].orient.z());
      buffer.concat("\r\n");
    }

    request->send(200, "text/plain", buffer);
  });


  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    mpuLogging = false;

    for(int  i = 0; i < 32; ++i) {
      char* data = reinterpret_cast<char*>(malloc(1<<i));
      if(!data) {
        log("failed to allocate elements");
        auto lambda = [=]{
          switch(i) {
            case 0: return "0";
            case 1: return "1";
            case 2: return "2";
            case 3: return "3";
            case 4: return "4";
            case 5: return "5";
            case 6: return "6";
            case 7: return "7";
            case 8: return "8";
            case 9: return "9";
            case 10: return "10";
            case 11: return "11";
            case 12: return "12";
            case 13: return "13";
            case 14: return "14";
            case 15: return "15";
            case 16: return "16";
            case 17: return "17";
            case 18: return "18";
            case 19: return "19";
          }
          return "wtf";
        };
        logln(lambda());
        break;
      } else {
        free(data);
      }
    }

    json::JSONBuilder builder;
    builder.add(mpuCnt).to(builder.baseObject(), "newestVal");
    builder.add(1e-2).to(builder.baseObject(), "timeDelta");

    auto accel = builder.add(json::Object{}).to(builder.baseObject(), "accel").get<json::Object>();
    auto accelX = builder.add(json::Array{}).to(*accel, "X").get<json::Array>();
    auto accelY = builder.add(json::Array{}).to(*accel, "Y").get<json::Array>();
    auto accelZ = builder.add(json::Array{}).to(*accel, "Z").get<json::Array>();
    auto orient = builder.add(json::Object{}).to(builder.baseObject(), "orient").get<json::Object>();
    auto orientX = builder.add(json::Array{}).to(*orient, "X").get<json::Array>();
    auto orientY = builder.add(json::Array{}).to(*orient, "Y").get<json::Array>();
    auto orientZ = builder.add(json::Array{}).to(*orient, "Z").get<json::Array>();
    for(int i = 0; i < mpuData.SIZE; ++i) {
      builder.add(mpuData[i].accel.x()).to(*accelX);
      builder.add(mpuData[i].accel.y()).to(*accelY);
      builder.add(mpuData[i].accel.z()).to(*accelZ);
      builder.add(mpuData[i].orient.x()).to(*orientX);
      builder.add(mpuData[i].orient.y()).to(*orientY);
      builder.add(mpuData[i].orient.z()).to(*orientZ);
    }
    request->send(200, "application/json", builder.serialize().str.c_str());

    /*
    constexpr auto SIZE = JSON_ARRAY_SIZE(6*MPU_ARRAY_SIZE) + JSON_OBJECT_SIZE(10) + JSON_STRING_SIZE(30);
    AsyncJsonResponse response{false, SIZE};
    auto&buffer = response.getRoot();

    buffer["newestVal"] = 0;
    buffer["TimeDelta"] = 0;

    auto accel = buffer.createNestedObject("accel");
    {
    auto arr = accel.createNestedArray("accX");
    //for(auto & el : accX) arr.add(el);
    for(int i = 0; i < mpuData.SIZE; ++i) arr.add(mpuData[i].accel.x());
    }
    {
    auto arr = accel.createNestedArray("accY");
    for(int i = 0; i < mpuData.SIZE; ++i) arr.add(mpuData[i].accel.y());
    }
    {
    auto arr = accel.createNestedArray("accZ");
    for(int i = 0; i < mpuData.SIZE; ++i) arr.add(mpuData[i].accel.z());
    }
    auto orient = buffer.createNestedObject("orient");
    {
    auto arr = orient.createNestedArray("angX");
    for(int i = 0; i < mpuData.SIZE; ++i) arr.add(mpuData[i].orient.x());
    }
    {
    auto arr = orient.createNestedArray("angY");
     for(int i = 0; i < mpuData.SIZE; ++i) arr.add(mpuData[i].orient.y());
   }
    {
    auto arr = orient.createNestedArray("angZ");
    for(int i = 0; i < mpuData.SIZE; ++i) arr.add(mpuData[i].orient.z());
    }

    response.setLength();
    request->send(&response);
    */
    mpuLogging = true;
  });


server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request){
  String json = "[";
  int n = WiFi.scanComplete();
  if(n == -2){
    WiFi.scanNetworks(true);
  } else if(n){
    for (int i = 0; i < n; ++i){
      if(i) json += ",";
      json += "{";
      json += "\"rssi\":"+String(WiFi.RSSI(i));
      json += ",\"ssid\":\""+WiFi.SSID(i)+"\"";
      json += ",\"bssid\":\""+WiFi.BSSIDstr(i)+"\"";
      json += ",\"channel\":"+String(WiFi.channel(i));
      json += ",\"secure\":"+String(WiFi.encryptionType(i));
      json += "}";
    }
    WiFi.scanDelete();
    if(WiFi.scanComplete() == -2){
      WiFi.scanNetworks(true);
    }
  }
  json += "]";
  request->send(200, "application/json", json);
  json = String();
});

  server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (SPIFFS.exists(logFileString))
    {
      request->send(SPIFFS, logFileString, "text/plain");
    }else
    {
      request->send(200, "text/plain", "no logs available");
    }
  });

  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    int paramsNr = request->params();

    for (int i = 0; i < paramsNr; i++)
    {
      request->send(200, "text/plain", "message received");
      AsyncWebParameter *p = request->getParam(i);
      if (p->name() == "brightness")
      {
        int val = p->value().toInt();
        if (val >= 0 && val <= 255)
        {
          brightness = val;
          leds.setBrightness(val);
          leds.show();
        }
      }

      if (p->name() == "hue")
      {
        int val = p->value().toInt();
        if (val >= 0 && val <= 65536)
        {
          leds.fill(leds.gamma32(leds.ColorHSV(val, 255, 255)));
          leds.show();
        }
      }
    }
  });
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  AsyncElegantOTA.begin(&server); // Start ElegantOTA
  server.begin();

  logln("HTTP server started");

  while (updaterRunning)
  {
    isUpdating = true;
    AsyncElegantOTA.loop();
    isUpdating = false;
    vTaskDelay(10);
  }

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  btStop();

  adc_power_off();
  esp_wifi_stop();
  esp_bt_controller_disable();
  vTaskDelete(NULL);
}

void setup()
{

  if (!SPIFFS.begin(true))
  {
    Serial.begin(115200);
    Serial.println("An Error has occurred while mounting SPIFFS");
    Serial.end();
    return;
  }
  else
  {
    logging = true;
    logln("----------------------System initialized and SPIFFS mounted-------------------------------");
  }

  log("TickTime is : ");
  logln((String)(1000.0f / xPortGetTickRateHz()));

  leds.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  pinMode(LED_EN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(MPU_INT_PIN, INPUT);

  digitalWrite(LED_EN, HIGH);

  leds.fill(leds.ColorHSV(0, 255, 255));
  leds.setBrightness(15);
  leds.show();

  attachInterrupt(BUTTON_PIN, buttonChanged, CHANGE);

  xTaskCreatePinnedToCore(lightingTask, "lightingTask", 10000, NULL, 2, &lightingTaskHandle, 1);
  xTaskCreatePinnedToCore(updaterTask, "updaterTask", 10000, NULL, 2, &updaterTaskHandle, 0);
  xTaskCreatePinnedToCore(mpuTask, "mpuTask", 10000, NULL, 1, &mpuTaskHandle, 1);
}

void handleLongPress()
{
  if (updaterRunning)
  {
    updaterRunning = false;
    logln("updater turned off");
  }
  else if (updaterRunning == false)
  {
    updaterRunning = true;
    xTaskCreatePinnedToCore(updaterTask, "updaterTask", 10000, NULL, 2, &updaterTaskHandle, 0);
    logln("updater turned on");
  }
}

void handleShortPress()
{
  setLightingMode(lightingMode+1);
}

void loop()
{
  vTaskDelay(10);

  int dt = lastUpTime - lastDownTime;
  if (dt != lastDT && !buttonHandled)
  {
    if (dt >= 30 && dt <= 1000)
    {
      handleShortPress();
    }
    else if (dt >= 1000 && dt <= 5000)
    {
      handleLongPress();
    }
    lastDT = dt;
    buttonHandled = true;

    if (log10k)
    {
      log("10k took: ");
      log((String)(endTime - startTime));
      logln("ms");
      log10k = false;
    }
  };
}