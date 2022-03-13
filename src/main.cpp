#include <Arduino.h>

#include "NimBLEDevice.h"
#include "NimBLEBeacon.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoHttpClient.h>

#include <SimpleTimer.h>

#include "private.h"

#define ENDIAN_CHANGE_U16(x)  ((((x)&0xFF00)>>8) + (((x)&0xFF)<<8))
#define BREWFATHER_MIN_DELAY  (15 * 60 * 1000)

BLEScan* pBLEScan;

#define DEBUG 1

struct tilt {
    // static
    unsigned int index;
    BLEUUID uuid;
    String color;
    // configurable
    String label;
    ulong postInterval;
    // dynamic
    uint16_t gravity;     // SG * 1000 or SG * 10000 for Tilt Pro/HD
    uint16_t temperature; // Deg F _or_ Deg F * 10 for Tilt Pro/HD
    time_t last_heard;
    bool posted;
};

tilt tilt_array[] = {
  { 0, BLEUUID::fromString("A495BB10-C5B1-4B44-B512-1370F02D74DE"), "red", "", BREWFATHER_MIN_DELAY },
  { 1, BLEUUID::fromString("A495BB20-C5B1-4B44-B512-1370F02D74DE"), "green", "", BREWFATHER_MIN_DELAY },
  { 2, BLEUUID::fromString("A495BB30-C5B1-4B44-B512-1370F02D74DE"), "black", "", BREWFATHER_MIN_DELAY },
  { 3, BLEUUID::fromString("A495BB40-C5B1-4B44-B512-1370F02D74DE"), "purple", "", BREWFATHER_MIN_DELAY },
  { 4, BLEUUID::fromString("A495BB50-C5B1-4B44-B512-1370F02D74DE"), "orange", "", BREWFATHER_MIN_DELAY },
  { 5, BLEUUID::fromString("A495BB60-C5B1-4B44-B512-1370F02D74DE"), "blue", "", BREWFATHER_MIN_DELAY },
  { 6, BLEUUID::fromString("A495BB70-C5B1-4B44-B512-1370F02D74DE"), "yellow", "", BREWFATHER_MIN_DELAY },
  { 7, BLEUUID::fromString("A495BB80-C5B1-4B44-B512-1370F02D74DE"), "pink", "", BREWFATHER_MIN_DELAY },
  // Tilt Pro / https://tilthydrometer.com/products/tilt-pro-wireless-hydrometer-and-thermometer
  { 8, BLEUUID::fromString("A495BB10-C5B1-4B44-B512-1370F02D74DE"), "red*hd", "", BREWFATHER_MIN_DELAY },
  { 9, BLEUUID::fromString("A495BB20-C5B1-4B44-B512-1370F02D74DE"), "green*hd", "", BREWFATHER_MIN_DELAY },
  {10, BLEUUID::fromString("A495BB30-C5B1-4B44-B512-1370F02D74DE"), "black*hd", "", BREWFATHER_MIN_DELAY },
  {11, BLEUUID::fromString("A495BB40-C5B1-4B44-B512-1370F02D74DE"), "purple*hd", "", BREWFATHER_MIN_DELAY },
  {12, BLEUUID::fromString("A495BB50-C5B1-4B44-B512-1370F02D74DE"), "orange*hd", "", BREWFATHER_MIN_DELAY },
  {13, BLEUUID::fromString("A495BB60-C5B1-4B44-B512-1370F02D74DE"), "blue*hd", "", BREWFATHER_MIN_DELAY },
  {14, BLEUUID::fromString("A495BB70-C5B1-4B44-B512-1370F02D74DE"), "yellow*hd", "", BREWFATHER_MIN_DELAY },
  {15, BLEUUID::fromString("A495BB80-C5B1-4B44-B512-1370F02D74DE"), "pink*hd", "", BREWFATHER_MIN_DELAY },
};

SimpleTimer timer;

class AdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {

  // https://kvurd.com/blog/tilt-hydrometer-ibeacon-data-format/
  bool Tilt(BLEAdvertisedDevice* advertisedDevice) {
    if (!advertisedDevice->haveManufacturerData())
      return false;

    std::string data = advertisedDevice->getManufacturerData();
    if (data.length() != 25)
      return false;

    if ((data [0] != 0x4c) || (data [1] != 0x00))
      return false;

    BLEBeacon beacon = BLEBeacon();
    beacon.setData(data);

#if DEBUG
    // Serial.printf("ID: %04X Major: %d Minor: %d UUID: %s Power: %d\n", beacon.getManufacturerId(),ENDIAN_CHANGE_U16(beacon.getMajor()),ENDIAN_CHANGE_U16(beacon.getMinor()),beacon.getProximityUUID().toString().c_str(),beacon.getSignalPower());
#endif
    BLEUUID uuid = beacon.getProximityUUID();
    
    for(auto tilt: tilt_array) {
      if (!uuid.equals (tilt.uuid))
        continue;
      uint16_t g = ENDIAN_CHANGE_U16(beacon.getMinor());
      bool pro = tilt.color.endsWith("*hd");
      if ((g > 2000) && !pro)
        continue;

      tilt.gravity = g;
      tilt.temperature = ENDIAN_CHANGE_U16(beacon.getMajor());
      tilt.last_heard = time(nullptr);
      tilt.posted = false;
      tilt_array[tilt.index] = tilt;
      timer.enable(tilt.index);
      
      Serial.printf("%s %d %d %s\n", tilt.color.c_str(), tilt.gravity, tilt.temperature, ctime(&tilt.last_heard));
      return true;
    }
    return false;
  }

  virtual void onResult(BLEAdvertisedDevice* advertisedDevice) {
#if DEBUG
//    Serial.print("b"); // show that some activity is happening
#endif
    if (Tilt(advertisedDevice))
      return;
  }
};

AsyncWebServer server(80);

typedef void (*timer_callback_p)(void *);

WiFiClient wifi;
String contentType = "application/json";

void onPost(void *arg) {
  tilt t = tilt_array[(int)arg];
  // disable the timer if we're about to post the same data again
  if (t.posted) {
#if DEBUG
    Serial.printf("Nothing new received from %s since last post, disabling timer.\n", t.color.c_str());
#endif
    timer.disable(t.index);
  } else {
    // Serial.printf("Trying to connect to %s:%d\n", t.host.c_str(), t.port);
    bool pro = (t.color.endsWith ("*hd"));
    String json = "{\n \"name\": \"" + t.color + "\",\n \"temp\": ";
    float temp = t.temperature / (pro ? 10.0f : 1.0f);
    json += String (temp, 1);
    json += ",\n \"temp_unit\": \"F\",\n \"gravity\": ";
    float g = t.gravity / (pro ? 10000.0f : 1000.0f);
    json += String (g, pro ? 4 : 3);
    json += ",\n \"gravity_unit\": \"G\"\n}";

    HttpClient client(wifi, "log.brewfather.net", 80);

    client.post(BREWFATHER_URL, contentType, json);
    int statusCode = client.responseStatusCode();
    String resp = client.responseBody(); // if not called the next `.post` will return `-4`
#if DEBUG
    Serial.printf("POST '%s'\nStatus %d : %s", json.c_str(), statusCode, resp.c_str());
#endif
    client.stop();
    t.posted = (statusCode == 200);
  }
}

String getTiltRest (tilt tilt) {
  bool pro = (tilt.color.endsWith ("*hd"));
  String rest = "{ \"color\": \"" + tilt.color + "\", \"gravity\": \"";
  float g = tilt.gravity / (pro ? 10000.0f : 1000.0f);
  rest += String (g, pro ? 4 : 3);
  rest += "\", \"temperature\": \"";
  if (pro) {
    float t = tilt.temperature / 10.0f;
    rest += String (t, 1);
  } else {
    // asking for 0 decimal gives a " 0" string (dot!)
    rest += tilt.temperature;
  }
  rest += "\", \"time\": \"" + String (tilt.last_heard) + "\" }";
  return rest;
}

void setup() {
#if DEBUG
  Serial.begin(115200);
  delay (2000);
#endif

  if (WiFi.begin(DEFAULT_SSID, DEFAULT_PASSWORD) == WL_CONNECT_FAILED) {
#if DEBUG
     Serial.println("Error starting WiFi");
#endif
  } else {
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
#if DEBUG
      Serial.print(".");
#endif
    }

#if DEBUG
    Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
    Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
#endif

    if (!MDNS.begin("tilt-relay")) {
#if DEBUG
      Serial.println("Error starting mDNS");
#endif
    } else {
      MDNS.addService("http", "tcp", 80);
    }

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {    
      String html = "<html><head><title>Tilt Relay</title>";
      String refresh = request->arg("refresh");
      if (refresh.length() > 0) {
        html += "<meta http-equiv=\"refresh\" content=\"" + refresh + "\">";
      }
      html += "</head><body>";
      html += "<h1>Tilt Relay</h1>";
      html += "<ul>";
      for(auto tilt: tilt_array) {
        if (tilt.last_heard <= 0)
          continue;
        bool pro = (tilt.color.endsWith ("*hd"));
        float g = tilt.gravity / (pro ? 10000.0f : 1000.0f);
        float t = tilt.temperature / (pro ? 10.0f : 1.0f);
        html += "<li>" + tilt.color + " : " + String (g, (pro ? 4 : 3)) + " sg, " + String (t, (pro ? 1 : 0)) + "&deg;F " + ctime (&tilt.last_heard) + "</li>";
      }
      html += "</ul>";
      html += "</body></html>";
      request->send (200, "text/html", html);
    });

    server.on("/getTilts", HTTP_GET, [](AsyncWebServerRequest *request) {
      int time_limit = 0;
      String time = request->arg("time");
      if (time.length () > 0)
        time_limit = time.toInt ();

      String reply = "{\n";
      for(auto tilt: tilt_array) {
        time_t last = tilt.last_heard;
        // filter out data received before the provided timestamp
        if (last <= time_limit)
          continue;
        reply += "  " + getTiltRest (tilt) + ",\n";
      }
      reply += "}\n";
      request->send (200, "text/json", reply);
    });

    server.on("/getTilt", HTTP_GET, [](AsyncWebServerRequest *request) {
      int time_limit = 0;
      String time = request->arg("time");
      if (time.length () > 0)
        time_limit = time.toInt ();

      String color = request->arg("color");

      String reply = "";
      for(auto tilt: tilt_array) {
        time_t last = tilt.last_heard;
        // filter out data received before the provided timestamp
        if (last <= time_limit)
          continue;
        if (color != tilt.color)
          continue;
        reply = getTiltRest (tilt);
        break;
      }
      if (reply.length() > 0)
        request->send(200, "text/json", reply);
      else
        request->send(404, "text/html", "Not found");
    });
  
    server.begin();
  }

  for(auto tilt: tilt_array) {
    int n = timer.setInterval (tilt.postInterval, onPost, (void*) tilt.index);
    timer.disable (n);
  }

  BLEDevice::init("");
  // boost power to maximum - assume we're USB (not battery) powered
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_P9);
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setMaxResults(0);
  pBLEScan->setWindow(99);

  configTzTime("EST5EDT4,M3.2.0/02:00:00,M11.1.0/02:00:00", "pool.ntp.org", "time.nist.gov");
  delay(2000);
}

void loop() {
  if (!pBLEScan->isScanning())
    pBLEScan->start(5);
  timer.run();
}
