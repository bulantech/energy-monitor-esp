#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>  // V 5.13.4         //https://github.com/bblanchon/ArduinoJson
#include <BlynkSimpleEsp8266.h>
#include <MicroGear.h>

#define SW_PIN 0
#define LED_BUILTIN 2

#define APPID   "Energy3PhaseMeter"
#define KEY     "lJLnvWmtNosRgOK"
#define SECRET  "4eoDEiI9AT216VSVpImrfknXQ"
#define ALIAS   "esp8266"

//define your default values here, if there are different values in config.json, they are overwritten.
char mqtt_server[40];
char mqtt_port[6] = "8080";
char blynk_token[34] = "ad0cd4da292e49c4bd7c969cb4e74cfd";

char ct1_curent[6] = "10";
char ct2_curent[6] = "50";
char ct3_curent[6] = "100";

char netpie_appid[30] = APPID;
char netpie_key[30] = KEY;
char netpie_secret[30] = SECRET;

//flag for saving data
bool shouldSaveConfig = false;

WiFiClient client;
BlynkTimer timer1;
BlynkTimer timer2;
int microgearTimer = 0;
MicroGear microgear(client);

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

          strcpy(netpie_appid, json["netpie_appid"]);
          strcpy(netpie_key, json["netpie_key"]);
          strcpy(netpie_secret, json["netpie_secret"]);

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

  WiFiManagerParameter custom_netpie_appid("netpie_appid", "netpie appid", netpie_appid, 30);
  WiFiManagerParameter custom_netpie_key("netpie_key", "netpie key", netpie_key, 30);
  WiFiManagerParameter custom_netpie_secret("netpie_secret", "netpie secret", netpie_secret, 30);
    
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
//  wifiManager.addParameter(&custom_mqtt_server);
//  wifiManager.addParameter(&custom_mqtt_port);
  WiFiManagerParameter blynk_text("<br><span>BLYNK</span>");
  wifiManager.addParameter(&blynk_text);
  wifiManager.addParameter(&custom_blynk_token);

  WiFiManagerParameter ct_text("<br><br><span>CT (AMP)</span>");
  wifiManager.addParameter(&ct_text);
  wifiManager.addParameter(&custom_ct1_curent);
  wifiManager.addParameter(&custom_ct2_curent);
  wifiManager.addParameter(&custom_ct3_curent);

  WiFiManagerParameter netpie_text("<br><br><span>Netpie</span>");
  wifiManager.addParameter(&netpie_text);
  wifiManager.addParameter(&custom_netpie_appid);
  wifiManager.addParameter(&custom_netpie_key);
  wifiManager.addParameter(&custom_netpie_secret);

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
//  Serial.println("custom_blynk_token.getValue() -> " + String(custom_blynk_token.getValue()));
  strcpy(blynk_token, custom_blynk_token.getValue());
  strcpy(ct1_curent, custom_ct1_curent.getValue());
  strcpy(ct2_curent, custom_ct2_curent.getValue());
  strcpy(ct3_curent, custom_ct3_curent.getValue());

  strcpy(ct3_curent, custom_netpie_appid.getValue());
  strcpy(ct3_curent, custom_netpie_key.getValue());
  strcpy(ct3_curent, custom_netpie_secret.getValue());
  
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
//    json["mqtt_server"] = mqtt_server;
//    json["mqtt_port"] = mqtt_port;
    Serial.println("blynk_token -> "+String(blynk_token));
    json["blynk_token"] = blynk_token;
    json["ct1_curent"] = ct1_curent;
    json["ct2_curent"] = ct2_curent;
    json["ct3_curent"] = ct3_curent;

    json["netpie_appid"] = netpie_appid;
    json["netpie_key"] = netpie_key;
    json["netpie_secret"] = netpie_secret;

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

  /* Add Event listeners */
  /* Call onMsghandler() when new message arraives */
  microgear.on(MESSAGE,onMsghandler);

  /* Call onFoundgear() when new gear appear */
  microgear.on(PRESENT,onFoundgear);

  /* Call onLostgear() when some gear goes offline */
  microgear.on(ABSENT,onLostgear);

  /* Call onConnected() when NETPIE connection is established */
  microgear.on(CONNECTED,onConnected);

  /* Initial with KEY, SECRET and also set the ALIAS here */
//  microgear.init(KEY,SECRET,ALIAS);
  microgear.init(netpie_key,netpie_secret,ALIAS);

  /* connect to NETPIE to a specific APPID */
//  microgear.connect(APPID);
  microgear.connect(netpie_appid);
    
  timer1.setInterval(3000L, myTimerEvent);
  timer2.setInterval(1000L, microgearrEvent);
  digitalWrite(LED_BUILTIN, 1); //off
  
}

void loop() {
  // put your main code here, to run repeatedly:
  Blynk.run();
  timer1.run(); // Initiates BlynkTimer
  timer2.run();
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void myTimerEvent(){
  digitalWrite(LED_BUILTIN, 0);
  delay(20);
  digitalWrite(LED_BUILTIN, 1);
}

void microgearrEvent(){
  /* To check if the microgear is still connected */
  if (microgear.connected()) {
//      Serial.println("connected");

      /* Call this method regularly otherwise the connection may be lost */
      microgear.loop();

      if (microgearTimer >= 5000) {
          Serial.println("Publish...");

          /* Chat with the microgear named ALIAS which is myself */
          microgear.chat(ALIAS,"20,30,40");
          microgearTimer = 0;
      } 
      else microgearTimer += 1000;
  }
  else {
      Serial.println("connection lost, reconnect...");
      if (microgearTimer >= 5000) {
          microgear.connect(APPID);
          microgearTimer = 0;
      }
      else microgearTimer += 1000;
  }
}

/* If a new message arrives, do this */
void onMsghandler(char *topic, uint8_t* msg, unsigned int msglen) {
    Serial.print("Incoming message --> ");
    msg[msglen] = '\0';
    Serial.println((char *)msg);
}

void onFoundgear(char *attribute, uint8_t* msg, unsigned int msglen) {
    Serial.print("Found new member --> ");
    for (int i=0; i<msglen; i++)
        Serial.print((char)msg[i]);
    Serial.println();  
}

void onLostgear(char *attribute, uint8_t* msg, unsigned int msglen) {
    Serial.print("Lost member --> ");
    for (int i=0; i<msglen; i++)
        Serial.print((char)msg[i]);
    Serial.println();
}

/* When a microgear is connected, do this */
void onConnected(char *attribute, uint8_t* msg, unsigned int msglen) {
    Serial.println("Connected to NETPIE...");
    /* Set the alias of this microgear ALIAS */
    microgear.setAlias(ALIAS);
}
