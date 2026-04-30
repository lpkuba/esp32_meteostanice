//update?id=${stopId}&interval=${interval}&ssid=${ssid}&pass=${pass}
#include <Arduino.h>
#include "FS.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_ST7789.h>  // Hardware-specific library for ST7789
#include <SPI.h>
#include "Fonts/FreeMonoBold30pt7b.h"  //40 mezi řádky optimál
#include "Fonts/FreeSans18pt7b.h"      //40 mezi řádky optimál
#include "Fonts/FreeSans12pt7b.h"
#include "Fonts/FreeSansBold12pt7b.h"
#include "FreeSans9pt7b.h"
#include "FreeSansBold9pt7b.h"
#include "gfxlatin2.h"
#include "FreeSans12pt8b.h"
#include "FreeSans9pt8b.h"
#include "FreeSans8pt8b.h"
#include "time.h"


const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
const char* ntpServer = "pool.ntp.org";
char tempText[100];


#define TFT_CS 5    // Chip select control pin
#define TFT_DC 22   // Data Command control pin
#define TFT_RST 21  // Reset pin (could connect to Arduino RESET pin)

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

const int16_t
  bits = 20,         // Fractional resolution
  pixelWidth = 320,  // TFT dimensions
  pixelHeight = 240,
  iterations = 128;  // Fractal iteration limit or 'dwell'

#define FORMAT_LITTLEFS_IF_FAILED true
String ssid = "";
String pass = "";
String id = "1";
String api = "";
int onlInterval = 15;
int offInterval = 5;
int onlTimer = 0;
int offTimer = 0;

long currentMillis = 0;
long startMillis = 0;
bool wifiConnecting = false;
bool apiAvailable = false;
bool clockBlink = false;
const char *apSsid = "JolandaAP";
const char *apPass = "Admin123";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
JsonDocument confJson;
JsonDocument infoJson;
JsonDocument depJson;
HTTPClient http;
Preferences preferences;
void setup() {

  tft.init(240, 320);
  tft.invertDisplay(0);
  tft.setRotation(1);
  delay(100);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 135);
  tft.setFont(&FreeSans9pt7b);
  strcpy(tempText, "JOLANDAmeteo inicializace...");
  utf8tocp(tempText);
  tft.print(tempText);
  preferences.begin("meteostanice", false);
  loadConfig();
  Serial.begin(115200);

  if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)) {
    Serial.println("LittleFS Mount Failed");
  }

  connectWiFi();
  initWebSocket();

  delay(1000);
  server.begin();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/config.html", String(), false);
    Serial.println("GET na /");
  });
  startMillis = millis();
}
uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint16_t getColor(String type) {
  // PID barvy
  if (type == "metroA") return rgb565(0, 165, 98);
  if (type == "metroB") return rgb565(248, 179, 34);
  if (type == "metroC") return rgb565(207, 0, 61);
  if (type == "bus") return rgb565(0, 120, 160);
  if (type == "trolleybus") return rgb565(128, 22, 111);
  if (type == "tram") return rgb565(120, 2, 0);
  if (type == "train") return rgb565(15, 30, 65);
  if (type == "funicular") return rgb565(201, 208, 34);
  if (type == "ferry") return rgb565(0, 164, 167);
  if (type == "replacement") return rgb565(255, 170, 30);

  // Vlastní barvy
  if (type == "green") return rgb565(0, 139, 35);
  if (type == "orange") return rgb565(255, 123, 46);
  if (type == "yellow") return rgb565(255, 232, 79);
  if (type == "blue") return rgb565(18, 121, 255);
  if (type == "red") return rgb565(221, 17, 17);
  if (type == "darkgray") return rgb565(55, 55, 55);

  // Default bílá
  return rgb565(255, 255, 255);
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char *)data;
    deserializeJson(confJson, data);
    String dataType = confJson["dataType"];
    Serial.println(dataType);
    if (dataType == "wifi") {
      ssid = confJson["data"]["ssid"].as<String>();
      pass = confJson["data"]["pass"].as<String>();

      Serial.println(ssid);
      Serial.println(pass);
      connectWiFi();
    } else if (dataType == "sources") {
      if (confJson["data"]["api"] != "") {
        api = String(confJson["data"]["api"]);
      }

      Serial.print("Starý onlInterval:");
      Serial.println(onlInterval);
      onlInterval = int(confJson["data"]["onlInterval"]);
      offInterval = int(confJson["data"]["offInterval"]);
      id = confJson["data"]["stopId"].as<String>();
      Serial.print("Nový onlInterval");
      Serial.println(onlInterval);
    }
    saveConfig();
    webSocketSendToAll(2);
  }
}


void renderBoundaries() {
  tft.drawRect(150, 70, 15, 40, ST77XX_RED);      //vteřinová dvojtečka
  tft.drawRect(0, 125, 320, 20, ST77XX_BLUE);     //název zastávky
  tft.drawRect(0, 145, 58, 85, ST77XX_CYAN);      //čísla linek
  tft.drawRect(60, 145, 190, 85, ST77XX_ORANGE);  //konečné
  tft.drawRect(250, 145, 80, 85, ST77XX_GREEN);   //časy
}

void clearArea(int id) {  // 0 = full displej; 1 = dvojtečka hodin; 2 = název zastávky; 3 = celá obrazovka odjezdů mimo název; 4 = čísla linek; 5 = konečné; 6 = časy;
  if (id == 0) {
    tft.fillScreen(ST77XX_BLACK);
  }
  if (id == 1) {
    tft.fillRect(150, 70, 15, 40, ST77XX_BLACK);
  }
  if (id == 2) {
    tft.fillRect(0, 125, 320, 20, ST77XX_BLACK);
  }
  if (id == 4 || id == 3) {
    tft.fillRect(0, 145, 58, 85, ST77XX_BLACK);  //čísla linek
  }
  if (id == 5 || id == 3) {
    tft.fillRect(60, 145, 190, 85, ST77XX_BLACK);  //konečné
  }
  if (id == 6 || id == 3) {
    tft.fillRect(250, 145, 80, 85, ST77XX_BLACK);  //časy
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void loop() {
  currentMillis = millis();
  if (currentMillis - startMillis > 1000) {
    renderClock();
    update();
    onlTimer++;
    offTimer++;
    if (onlTimer >= onlInterval) {
      Serial.println("Online fetch");
      fetchData();
      onlTimer = 0;
    }
    if (offTimer >= offInterval) {
      Serial.println("Offline fetch");
      readSensors();
      offTimer = 0;
    }
    startMillis = currentMillis;
  }
}

void update() {
  Serial.println("Periodická aktualizace void update() - 1s");
  //update info > wifi
  if (wifiConnecting == false) {
    String wifiStatus = "error";
    switch (WiFi.status()) {
      case WL_CONNECTED: wifiStatus = "connected"; break;
      case WL_CONNECT_FAILED: wifiStatus = "failed"; break;
      case WL_CONNECTION_LOST: wifiStatus = "lost"; break;
      case WL_DISCONNECTED: wifiStatus = "disconnected"; break;
    }
    //Serial.println(wifiStatus); -> debug wifi
    String ipAddress = WiFi.localIP().toString();
    String uptime = String(millis());
    infoJson["data"]["wifi"]["ssid"] = ssid;
    infoJson["data"]["wifi"]["state"] = wifiStatus;
    infoJson["data"]["wifi"]["address"] = ipAddress;
    infoJson["data"]["system"]["uptime"] = uptime;
    webSocketSendToAll(1);
  }
}

void connectWiFi() {
  wifiConnecting = true;
  if (ssid == "") {
    Serial.println("Nemám žádnou WiFi síť k připojení!");
    infoJson["data"]["wifi"]["ssid"] = "<no ssid>";
    infoJson["data"]["wifi"]["state"] = "disconnected";
    infoJson["data"]["wifi"]["address"] = "0.0.0.0";
    wifiConnecting = false;
    enableFallbackAP();
    return;
  }
  Serial.print("Pokus o připojení na síť ");
  Serial.print(ssid);
  infoJson["data"]["wifi"]["ssid"] = ssid;
  infoJson["data"]["wifi"]["state"] = "connecting";
  infoJson["data"]["wifi"]["address"] = "0.0.0.0";
  webSocketSendToAll(1);
  WiFi.disconnect();
  WiFi.begin(ssid.c_str(), pass.c_str());
  int x = 0;
  while (WiFi.status() != WL_CONNECTED && x < 10) {
    delay(500);
    Serial.print(".");
    x++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("...připojen!");
    Serial.print("IP adresa zařízení: ");
    Serial.println(WiFi.localIP());
    infoJson["data"]["wifi"]["state"] = "connected";
    infoJson["data"]["wifi"]["address"] = WiFi.localIP();
    infoJson["data"]["wifi"]["strength"] = WiFi.RSSI();
    WiFi.softAPdisconnect(true);

    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    renderClock();
  } else {
    infoJson["data"]["wifi"]["state"] = "failed";
    infoJson["data"]["wifi"]["address"] = "0.0.0.0";
    Serial.println("...selhal!");
    enableFallbackAP();
  }
  webSocketSendToAll(1);
  wifiConnecting = false;
}

void enableFallbackAP() {
  Serial.print("Zapínám fallback AP...");
  WiFi.softAP(apSsid, apPass);
  while (WiFi.softAPIP() == IPAddress(0, 0, 0, 0)) {
    Serial.print(".");
  }
  if (WiFi.softAPIP() != IPAddress(0, 0, 0, 0)) {
    Serial.println("...zapnuto!");
    Serial.print("IP adresa: ");
    Serial.println(WiFi.softAPIP());
  }
}

void saveConfig() {
  preferences.putInt("onlInterval", onlInterval);
  preferences.putInt("offInterval", offInterval);
  preferences.putString("ssid", ssid);
  preferences.putString("pass", pass);
  preferences.putString("id", id);
  preferences.putString("api", api);
}

void loadConfig() {
  id = preferences.getString("id", "1");
  onlInterval = preferences.getInt("onlInterval", 10);
  offInterval = preferences.getInt("offInterval", 10);
  ssid = preferences.getString("ssid", "");
  pass = preferences.getString("pass", "");
  api = preferences.getString("api", "");
  confJson["data"]["id"] = id;
}

void webSocketSendToAll(int num) {
  String data;
  if (num == 0) {  //0 = config
    confJson["dataType"] = "config";
    serializeJson(confJson, data);
  } else if (num == 1) {  //1 = status
    infoJson["dataType"] = "info";
    serializeJson(infoJson, data);
  } else if (num == 2) {
    data = "ok";
  }
  ws.textAll(data);
}

void webSocketSendToAll(String data) {
  ws.textAll(data);
}

void fetchData() {
  //http.begin("https://lpkuba.github.io/response.json");
  //http.begin("https://lpkuba.github.io/response_lihovar_most.json");
  String link = "https://api.golemio.cz/v2/pid/departureboards?aswIds=";
  link += id;
  link += "&preferredTimezone=Europe_Prague&mode=departures&filter=routeHeadingOnceFill&skip=canceled&limit=4&total=4&offset=0&appendHeadsignsLimit=3";
  http.begin(link);


  
  http.addHeader("X-Access-Token", api);
  int httpResponseCode = http.GET();
  Serial.print("HTTP response kód: ");
  Serial.println(httpResponseCode);
  if (httpResponseCode == 200) {
    String payload = http.getString();
    deserializeJson(depJson, payload);
    //Serial.println("HTTP response string:");
    //Serial.println(payload);
    apiAvailable = true;
    Serial.println("...úspěch");
  } else {
    apiAvailable = false;
    Serial.println("...neúspěch");
  }
  renderDepartures();
}

void renderDepartures() {

  Serial.println("renderDepartures() zavoláno");
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 135);
  tft.setFont(&FreeSans8pt8b);
  clearArea(2);
  if(apiAvailable == false){
    strcpy(tempText, "API nedostupné!");
    utf8tocp(tempText);
    tft.print(tempText);

    return;
  }
  strcpy(tempText, depJson["stops"][0]["stop_name"].as<const char*>());
  utf8tocp(tempText);
  tft.print(tempText);
  int firstX = 2;
  int firstY = 165;
  int lineSpacing = 30;  //line vyska spis nez spacing sorry
  int lineAmount = int(depJson["departures"].size());
  Serial.println("post variable init zavoláno");
  Serial.println(lineAmount);
    clearArea(3);
  
  for (int i = 0; i < lineAmount; i++) {
    Serial.print("ahoj z loopu ");
    Serial.print(i);
    Serial.println("tá iterace");
    tft.setCursor(firstX, firstY+2 + lineSpacing * i);
    tft.setFont(&FreeSansBold12pt7b);
    tft.setTextColor(getTransportColor(i));
    strcpy(tempText, depJson["departures"][i]["route"]["short_name"].as<const char *>());
    utf8tocp(tempText);
    tft.print(tempText);
    tft.setCursor(firstX + 50, firstY + lineSpacing * i);
    tft.setFont(&FreeSans9pt8b);
    tft.setTextColor(ST77XX_WHITE);
    strcpy(tempText, depJson["departures"][i]["trip"]["headsign"].as<const char *>());
    utf8tocp(tempText);
    tft.print(tempText);
    tft.setCursor(firstX + 250, firstY + lineSpacing * i);
    char departure[10];
    String departureStr = depJson["departures"][i]["departure_timestamp"]["minutes"].as<String>();
    int delayMin = depJson["departures"][i]["delay"]["minutes"].as<int>();
    if (delayMin < 3) {
      tft.setTextColor(getColor("green"));
    } 
    else if (delayMin < 7) {
      tft.setTextColor(getColor("orange"));
    } 
    else {
      tft.setTextColor(getColor("red"));
    }
    if (departureStr == "<1") {
      strcpy(departure, "<<<<");
      tft.setTextColor(ST77XX_WHITE);
    } 
    else {
      strcpy(departure, departureStr.c_str());
      strcat(departure, " min");
    }
    tft.print(departure);
  }

  /*tft.setCursor(firstX, firstY - 2);
    tft.setFont(&FreeSansBold12pt7b);
    tft.setTextColor(getColor("metroA"));
    tft.print("A");
    tft.setCursor(firstX + 50 , firstY);
    tft.setFont(&FreeSans12pt8b);
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Depo Hostivar");
    tft.setCursor(firstX + 250, firstY);
    tft.setTextColor(getColor("orange"));
    tft.print("2 min");
    linePosition++;
    tft.setCursor(firstX , firstY + lineSpacing*linePosition);
    tft.setFont(&FreeSansBold12pt7b);
    tft.setTextColor(getColor("bus"));
    tft.print("199");
    tft.setCursor(firstX + 50, firstY + lineSpacing*linePosition);
    tft.setFont(&FreeSans12pt8b);
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Sidliste Malesice");
    tft.setCursor(firstX + 250, firstY + lineSpacing*linePosition);
    tft.setTextColor(getColor("green"));
    tft.print("3 min");
    linePosition++;
    tft.setCursor(firstX , firstY + lineSpacing*linePosition);
    tft.setFont(&FreeSansBold12pt7b);
    tft.setTextColor(getColor("train"));
    tft.print("S61");
    tft.setCursor(firstX + 50, firstY + lineSpacing*linePosition);
    tft.setFont(&FreeSans12pt8b);
    tft.setTextColor(ST77XX_WHITE);
    tft.print("Praha hl.n.");
    tft.setCursor(firstX + 250, firstY + lineSpacing*linePosition);
    tft.setTextColor(getColor("green"));
    tft.print("3 min");*/
}

uint16_t getTransportColor(int id) {
  int type = depJson["departures"][id]["route"]["type"];
  bool replacement = depJson["departures"][id]["route"]["is_substitute_transport"].as<bool>();
  if (replacement) {
    return getColor("replacement");
  }
  if (type == 0) {
    return getColor("tram");
  }
  if (type == 1) {
    String line = depJson["departures"][id]["route"]["short_name"].as<String>();
    if (line == "A") {
      return getColor("metroA");
    }
    if (line == "B") {
      return getColor("metroB");
    }
    if (line == "C") {
      return getColor("metroC");
    }
  }
  if (type == 2) {
    return getColor("train");
  }
  if (type == 3) {
    return getColor("bus");
  }
  if (type == 4) {
    return getColor("ferry");
  }
  if (type == 7) {
    return getColor("funicular");
  }
  if (type == 11) {
    return getColor("trolleybus");
  }
}

void readSensors() {
  Serial.println("CHYBÍ TI TY ZASRANÝ SENZORY KURVA");
  renderMeteo(10, 25);
}

void renderMeteo(int temperature, int humidity) {
  tft.setTextColor(ST77XX_WHITE);
  tft.fillRect(0, 0, 320, 45, getColor("darkgray"));
  tft.setCursor(20, 30);
  tft.setFont(&FreeSans18pt7b);
  tft.print(temperature);
  tft.write(0xF8);
  tft.print("c");
  tft.setCursor(220, 30);
  tft.print(humidity);
  tft.print("%");
}


void renderClock() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  char timeString[6];
  strftime(timeString, sizeof(timeString), "%H:%M", &timeinfo); 
  tft.setCursor(70, 100);
  tft.setFont(&FreeMonoBold30pt7b);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.print(timeString);
  clockBlink = !clockBlink;
  if (clockBlink) {
    tft.fillRect(150, 70, 15, 40, ST77XX_BLACK);
  }
}