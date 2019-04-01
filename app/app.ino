#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>  // V 5.13.4         //https://github.com/bblanchon/ArduinoJson

#include <BlynkSimpleEsp8266.h>

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40];
char mqtt_port[6] = "8080";
char blynk_token[34] = "ad0cd4da292e49c4bd7c969cb4e74cfd";
char ct1_curent[6] = "10";
char ct2_curent[6] = "50";
char ct3_curent[6] = "100";

#define SW_PIN 0
#define LED_BUILTIN 2

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();

  pinMode(SW_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, 0); //on

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  Serial.println("Reset wifi config?:");
  for(int i=5; i>0; i--){
    Serial.print(String(i)+" "); 
    digitalWrite(LED_BUILTIN, 0); //on
    delay(50);
    digitalWrite(LED_BUILTIN, 1); //off
    delay(1000);
  }
  digitalWrite(LED_BUILTIN, 0); //on
  
  //reset saved settings
  if(digitalRead(SW_PIN) == LOW) // Press button
  {
    Serial.println();
    Serial.println("Reset wifi config");
    wifiManager.resetSettings(); 
    SPIFFS.format();
  }    
  
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

//          strcpy(mqtt_server, json["mqtt_server"]);
//          strcpy(mqtt_port, json["mqtt_port"]);
          strcpy(blynk_token, json["blynk_token"]);
          strcpy(ct1_curent, json["ct1_curent"]);
          strcpy(ct2_curent, json["ct2_curent"]);
          strcpy(ct3_curent, json["ct3_curent"]);
          

        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read



  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
//  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
//  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  WiFiManagerParameter custom_blynk_token("blynk", "blynk token", blynk_token, 32);
  WiFiManagerParameter custom_ct1_curent("ct1_curent", "ct1 curent", ct1_curent, 4);
  WiFiManagerParameter custom_ct2_curent("ct2_curent", "ct2 curent", ct2_curent, 4);
  WiFiManagerParameter custom_ct3_curent("ct3_curent", "ct3 curent", ct3_curent, 4);
    
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
//  wifiManager.addParameter(&custom_mqtt_server);
//  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_blynk_token);
  wifiManager.addParameter(&custom_ct1_curent);
  wifiManager.addParameter(&custom_ct2_curent);
  wifiManager.addParameter(&custom_ct3_curent);

  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("EnergyMonitor")) {
    Serial.println("failed to connect and hit timeout");
//    delay(3000);
    for(int i=0; i<10; i++) {
      digitalWrite(LED_BUILTIN, 0); //on
      delay(50);
      digitalWrite(LED_BUILTIN, 1); //off
      delay(500); 
    }
    
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
//  strcpy(mqtt_server, custom_mqtt_server.getValue());
//  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(blynk_token, custom_blynk_token.getValue());
  strcpy(ct1_curent, custom_ct1_curent.getValue());
  strcpy(ct2_curent, custom_ct2_curent.getValue());
  strcpy(ct3_curent, custom_ct3_curent.getValue());
  
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
//    json["mqtt_server"] = mqtt_server;
//    json["mqtt_port"] = mqtt_port;
    json["blynk_token"] = blynk_token;
    json["ct1_curent"] = ct1_curent;
    json["ct2_curent"] = ct2_curent;
    json["ct3_curent"] = ct3_curent;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());

  //Blynk.begin(auth);
  Serial.print("blynk_token: ");
  Serial.println(blynk_token);
  Blynk.config(blynk_token);
  
  digitalWrite(LED_BUILTIN, 1); //off

}

void loop() {
  // put your main code here, to run repeatedly:
  Blynk.run();

}
