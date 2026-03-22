//update?id=${stopId}&interval=${interval}&ssid=${ssid}&pass=${pass}
#include <Arduino.h>
#include "FS.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Preferences.h>

#define FORMAT_LITTLEFS_IF_FAILED true
String ssid = "";
String pass = "";
int id = 0;
int interval = 0;

const char* apSsid = "JolandaAP";
const char* apPass = "Admin123";

const char* PARAM_INPUT_1 = "id";
const char* PARAM_INPUT_2 = "interval";
const char* PARAM_INPUT_3 = "ssid";
const char* PARAM_INPUT_4 = "pass";

AsyncWebServer server(80);

Preferences preferences;


String header;

void readFile(fs::FS &fs, const char * path){
    Serial.printf("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return;
    }

    Serial.println("- read from file:");
    while(file.available()){
        Serial.write(file.read());
    }
    file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("- failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("- file written");
    } else {
        Serial.println("- write failed");
    }
    file.close();
}


void setup(){
    preferences.begin("meteostanice", false); 
    loadConfig();
    Serial.begin(115200);

    if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
        Serial.println("LittleFS Mount Failed");
    }

    connectWiFi();

    delay(1000);
    server.begin();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(LittleFS, "/config.html", String(), false);
      Serial.println("GET na /");

    });

    server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("GET na /update");
        String inputMessage1;
        String inputMessage2;
        String inputMessage3;
        String inputMessage4;

        if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2)) {
            inputMessage1 = request->getParam(PARAM_INPUT_1)->value(); //id
            inputMessage2 = request->getParam(PARAM_INPUT_2)->value(); //interval
            inputMessage3 = request->getParam(PARAM_INPUT_3)->value(); //ssid
            inputMessage4 = request->getParam(PARAM_INPUT_4)->value(); //pass
            bool changed = false;
            if(id != inputMessage1.toInt()){
                id = inputMessage1.toInt();
                changed = true;
            }
            if(interval != inputMessage2.toInt()){
                interval = inputMessage2.toInt();
                changed = true;
            }
            if(ssid != inputMessage3){
                ssid = inputMessage3;
                changed = true;
            }
            if(pass != inputMessage4){
                pass = inputMessage4;
                changed = true;
            }
            if(changed){
                saveConfig();
                connectWiFi();
            }
        }
        request->send(LittleFS, "/config.html", String(), false);
    });
}

void loop(){

}

void connectWiFi(){
    if(ssid == ""){
        Serial.println("Nemám žádnou WiFi síť k připojení!");
        enableFallbackAP();
        return;
    }
    Serial.print("Pokus o připojení na síť ");
    Serial.print(ssid);
    WiFi.begin(ssid.c_str(), pass.c_str());
    int x = 0;
    while(WiFi.status() != WL_CONNECTED && x < 10){
        delay(500);
        Serial.print(".");
        x++;
    }
    if(WiFi.status() == WL_CONNECTED){
        Serial.println("...připojen!");
        Serial.print("IP adresa zařízení: ");
        Serial.println(WiFi.localIP());
        WiFi.softAPdisconnect(true);
    }
    else{
        Serial.println("...selhal!");
        enableFallbackAP();
    }
}

void enableFallbackAP(){
    Serial.print("Zapínám fallback AP...");
    WiFi.softAP(apSsid, apPass);
    while(WiFi.softAPIP() == IPAddress(0, 0, 0, 0)){
        Serial.print(".");
    }
    if(WiFi.softAPIP() != IPAddress(0, 0, 0, 0)){
        Serial.println("...zapnuto!");
        Serial.print("IP adresa: ");
        Serial.println(WiFi.softAPIP());
    }
}

void saveConfig(){
    preferences.putInt("id", id);
    preferences.putInt("interval", interval);
    preferences.putString("ssid", ssid);
    preferences.putString("pass", pass);
}

void loadConfig(){
    id = preferences.getInt("id", 0);
    interval = preferences.getInt("interval", 10);
    ssid = preferences.getString("ssid", "");
    pass = preferences.getString("pass", "");
}