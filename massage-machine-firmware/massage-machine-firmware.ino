#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Update.h>
#include <HTTPClient.h>

/* ================= CONFIG ================= */
const char* WIFI_SSID = "ANDGATETECH";
const char* WIFI_PASS = "simanto654";

const char* MQTT_BROKER = "608224a1459a4d05b34cf126b792a1ff.s1.eu.hivemq.cloud";
const int   MQTT_PORT   = 8883;
const char* MQTT_USER   = "andgatetech";
const char* MQTT_PASS   = "Simanto@848577";

#define DEVICE_ID "ESP32-TEST-001"
#define BRAND     "projectx"

#define CMD_TOPIC    BRAND "/cmd"
#define EVENT_TOPIC  BRAND "/event"
#define HEALTH_TOPIC BRAND "/health"

#define HEALTH_INTERVAL_MS (5UL * 60UL * 1000UL)

/* ================= GLOBALS ================= */
WiFiClientSecure net;
PubSubClient mqtt(net);
Preferences prefs;

/* ================= STATE ================= */
enum MachineState { OFF, IDLE, RUNNING, PAUSED };
MachineState currentState = OFF;

struct Session {
  String tenant_id = "";
  String session_id = "";
  String active_part = "all";
  uint8_t intensity = 1;
  unsigned long session_start = 0;
  unsigned long session_end = 0;
  unsigned long accumulated_sec = 0;
} current;

unsigned long lastHealth = 0;

/* ================= HARDWARE STUBS ================= */
void hwPowerOn()  { Serial.println("[HW] POWER ON"); }
void hwPowerOff() { Serial.println("[HW] POWER OFF"); }
void hwStart(const String& p){ Serial.println("[HW] START " + p); }
void hwStop(){ Serial.println("[HW] STOP"); }
void hwPause(){ Serial.println("[HW] PAUSE"); }
void hwIntensity(uint8_t i){ Serial.printf("[HW] INT=%d\n", i); }

/* ================= HELPERS ================= */
const char* stateStr() {
  switch(currentState){
    case OFF: return "off";
    case IDLE: return "idle";
    case RUNNING: return "running";
    default: return "paused";
  }
}

unsigned long elapsedSec() {
  if(currentState == RUNNING)
    return current.accumulated_sec + (millis() - current.session_start) / 1000UL;
  return current.accumulated_sec;
}

void publishJSON(const char* topic, JsonDocument& d){
  char buf[512];
  serializeJson(d, buf);
  mqtt.publish(topic, buf);
}

void publishEvent(const char* evt, const char* reason=""){
  StaticJsonDocument<384> d;
  d["tenant_id"] = current.tenant_id;
  d["device_id"] = DEVICE_ID;
  d["session_id"] = current.session_id;
  d["event"] = evt;
  d["state"] = stateStr();
  d["elapsed_sec"] = elapsedSec();
  if(strlen(reason)) d["reason"] = reason;
  publishJSON(EVENT_TOPIC, d);
}

/* ================= FLASH ================= */
String sessionKey(const String &tenant, const String &device){
  return tenant + "/" + device;
}

void persistSession() {
  String baseKey = sessionKey(current.tenant_id, DEVICE_ID);
  prefs.putString((baseKey + "/sid").c_str(), current.session_id);
  prefs.putULong((baseKey + "/start").c_str(), current.session_start);
  prefs.putULong((baseKey + "/end").c_str(), current.session_end);
  prefs.putString((baseKey + "/part").c_str(), current.active_part);
  prefs.putUChar((baseKey + "/int").c_str(), current.intensity);
  prefs.putULong((baseKey + "/acc").c_str(), current.accumulated_sec);
}

void loadSession(const String &tenant, const String &device){
  String baseKey = sessionKey(tenant, device);
  current.tenant_id = tenant;
  current.session_id = prefs.getString((baseKey + "/sid").c_str(), "");
  current.session_start = prefs.getULong((baseKey + "/start").c_str(), 0);
  current.session_end = prefs.getULong((baseKey + "/end").c_str(), 0);
  current.active_part = prefs.getString((baseKey + "/part").c_str(), "all");
  current.intensity = prefs.getUChar((baseKey + "/int").c_str(), 1);
  current.accumulated_sec = prefs.getULong((baseKey + "/acc").c_str(), 0);

  if(current.session_id.length() && currentState != OFF)
    currentState = IDLE;
  else
    currentState = OFF;
}

void clearSessionFlash(){
  String baseKey = sessionKey(current.tenant_id, DEVICE_ID);
  prefs.remove((baseKey + "/sid").c_str());
  prefs.remove((baseKey + "/start").c_str());
  prefs.remove((baseKey + "/end").c_str());
  prefs.remove((baseKey + "/part").c_str());
  prefs.remove((baseKey + "/int").c_str());
  prefs.remove((baseKey + "/acc").c_str());
}

/* ================= SESSION ================= */
void endSession(const char* reason){
  if(current.session_id == "") return;
  if(currentState == RUNNING)
    current.accumulated_sec += (millis() - current.session_start)/1000UL;

  hwStop();
  currentState = IDLE;
  publishEvent("session_ended", reason);

  current.session_id = "";
  current.accumulated_sec = 0;
  current.session_start = 0;
  current.session_end = 0;
  current.active_part = "all";
  current.intensity = 1;

  clearSessionFlash();
}

/* ================= COMMAND PROCESS ================= */
void processCommand(JsonDocument &c){
  const char* task = c["task"];
  if(!task) return;

  String cmdTenant = c["tenant_id"] | "";
  String cmdDevice = c["device_id"] | "";
  
  if(cmdTenant.length() == 0 || cmdDevice.length() == 0){
    Serial.println("[SEC] tenant_id or device_id missing");
    return;
  }

  // Security: commands must match this device
  if(cmdDevice != DEVICE_ID){
    Serial.println("[SEC] device_id mismatch");
    return;
  }

  if(strcmp(task,"set_tenant")==0){
    current.tenant_id = cmdTenant;
    prefs.putString("tenant", current.tenant_id);
    publishEvent("tenant_set");
    return;
  }

  // All other commands require tenant to match
  if(current.tenant_id != cmdTenant){
    Serial.println("[SEC] tenant mismatch");
    return;
  }

  if(strcmp(task,"power_on")==0){
    if(currentState==OFF){
      hwPowerOn();
      currentState = IDLE;
      publishEvent("power_on");
    }
  }
  else if(strcmp(task,"power_off")==0){
    if(currentState!=OFF){
      endSession("power_off");
      hwPowerOff();
      currentState = OFF;
      publishEvent("power_off");
    }
  }
  else if(strcmp(task,"activate")==0 || strcmp(task,"force_activate")==0){
    if(strcmp(task,"force_activate")==0) endSession("force_activate");

    if(currentState==RUNNING){
      publishEvent("session_already_running");
      return;
    }

    current.active_part = c["part"] | "all";
    current.intensity = c["intensity"] | 1;
    unsigned long dur = (c["duration"] | 60) * 1000UL;

    // Resume existing session if available
    if(current.session_id.length()){
      currentState = IDLE;
      persistSession();
      publishEvent("resumed_existing_session");
      return;
    }

    current.session_id = String(millis(), HEX);
    current.accumulated_sec = 0;
    current.session_start = millis();
    current.session_end = millis() + dur;

    hwStart(current.active_part);
    hwIntensity(current.intensity);
    currentState = RUNNING;
    persistSession();
    publishEvent("activated");
  }
  else if(strcmp(task,"pause")==0){
    if(currentState==RUNNING){
      current.accumulated_sec += (millis() - current.session_start)/1000UL;
      hwPause();
      currentState = PAUSED;
      persistSession();
      publishEvent("paused");
    }
  }
  else if(strcmp(task,"resume")==0){
    if(currentState==PAUSED || (currentState==IDLE && current.session_id.length())){
      current.session_start = millis();
      hwStart(current.active_part);
      currentState = RUNNING;
      persistSession();
      publishEvent("resumed");
    }
  }
  else if(strcmp(task,"stop")==0){
    endSession("manual");
  }
  else if(strcmp(task,"intensity")==0){
    current.intensity = constrain((int)c["level"],1,3);
    hwIntensity(current.intensity);
    persistSession();
    publishEvent("intensity_changed");
  }
  else if(strcmp(task,"check")==0){
    StaticJsonDocument<256> d;
    d["tenant_id"] = current.tenant_id;
    d["device_id"] = DEVICE_ID;
    d["state"] = stateStr();
    d["session_id"] = current.session_id;
    d["elapsed_sec"] = elapsedSec();
    publishJSON(HEALTH_TOPIC,d);
  }
  else if(strcmp(task,"ota")==0){
    const char* url = c["url"];
    if(url && strlen(url)>0){
      publishEvent("ota_start");
      WiFiClient client;
      HTTPClient http;
      http.begin(client,url);
      int code = http.GET();
      if(code==200){
        int len = http.getSize();
        WiFiClient* stream = http.getStreamPtr();
        if(Update.begin(len)){
          size_t written = Update.writeStream(*stream);
          if(written==len){
            if(Update.end(true)){
              publishEvent("ota_success");
              ESP.restart();
            }else{
              publishEvent("ota_failed","update_end_fail");
            }
          }else publishEvent("ota_failed","write_stream_fail");
        }else publishEvent("ota_failed","update_begin_fail");
      }else publishEvent("ota_failed","http_code_error");
      http.end();
    }
  }
}

/* ================= MQTT ================= */
void mqttCallback(char*, byte* payload, unsigned int len){
  StaticJsonDocument<512> c;
  if(!deserializeJson(c,payload,len)) processCommand(c);
}

/* ================= SETUP / LOOP ================= */
void setup(){
  Serial.begin(115200);
  net.setInsecure();

  prefs.begin("mm",false);
  current.tenant_id = prefs.getString("tenant","");
  loadSession(current.tenant_id,DEVICE_ID);

  WiFi.begin(WIFI_SSID,WIFI_PASS);
  while(WiFi.status()!=WL_CONNECTED) delay(300);

  mqtt.setServer(MQTT_BROKER,MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  while(!mqtt.connect(DEVICE_ID,MQTT_USER,MQTT_PASS)) delay(1000);
  mqtt.subscribe(CMD_TOPIC);
}

void loop(){
  mqtt.loop();
  unsigned long now = millis();

  if(currentState==RUNNING && now>current.session_end){
    endSession("completed");
  }

  if(now - lastHealth > HEALTH_INTERVAL_MS){
    StaticJsonDocument<128> d;
    d["tenant_id"] = current.tenant_id;
    d["device_id"] = DEVICE_ID;
    d["state"] = stateStr();
    d["session_id"] = current.session_id;
    d["elapsed_sec"] = elapsedSec();
    publishJSON(HEALTH_TOPIC,d);
    lastHealth = now;
  }
}
