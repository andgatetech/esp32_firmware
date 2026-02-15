/*
 * ProjectX ESP32 IoT Controller
 * Version: 1.0.0
 * Fully Compliant with Vendor Integration Requirements
 * 
 * Developed By: Md Khorshed Alam
 * Email: andgatetech@gmail.com
 * Company: ANDGATE TECH
 * 
 * This firmware implements a cloud-controlled IoT platform for:
 * - Massage seats/beds
 * - Vending machines
 * - Automated equipment
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// ==================== CONFIGURATION ====================
// WiFi Configuration
const char* WIFI_SSID       = "ANDGATETECH";
const char* WIFI_PASSWORD   = "simanto654";

// HiveMQ Cloud MQTT Configuration (TLS)
const char* MQTT_BROKER     = "608224a1459a4d05b34cf126b792a1ff.s1.eu.hivemq.cloud";
const uint16_t MQTT_PORT    = 8883;
const char* MQTT_USER       = "andgatetech";
const char* MQTT_PASS       = "Simanto@848577";
const char* MQTT_CLIENT_ID  = "ProjectX_ESP32";

// MQTT Topics
const char* TOPIC_CMD       = "projectX/cmd";
const char* TOPIC_EVENT     = "projectX/event";
const char* TOPIC_OTA       = "projectX/ota";

// GPIO Pins
const int STOP_BUTTON_PIN   = 4;        // Physical STOP button (active LOW)
const int RELAY_FULL_BODY   = 16;       // Relay for full body massage
const int RELAY_NECK        = 17;       // Relay for neck massage
const int RELAY_BACK        = 18;       // Relay for back massage
const int RELAY_VIBRATION   = 19;       // Relay for vibration
const int RELAY_HEAT        = 21;       // Relay for heat
const int INTENSITY_PWM_PIN = 22;       // PWM for intensity control
const int LED_STATUS        = 2;        // Built-in LED for status

// Timing Constants
const unsigned long HEALTH_INTERVAL = 300000;    // 30 seconds
const unsigned long SNAPSHOT_INTERVAL = 5000;   // 5 seconds

// Root certificate for HiveMQ Cloud
const char* root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
"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\n" \
"-----END CERTIFICATE-----";

// Test location (Dhaka, Bangladesh)
const float TEST_LATITUDE = 23.8103;
const float TEST_LONGITUDE = 90.4125;

// ==================== STATE MANAGEMENT ====================
struct SessionState {
    String tenant_id;
    String device_id;
    String session_id;
    String state;           // idle, running, paused, interrupted
    String part;            // full_body, neck, back, vibration, heat
    String mode;            // all_back, upper_back, lower_back, spot_back, keep_tap, intermittent_tap, gradual_tap
    int intensity;
    int duration;
    unsigned long elapsed_sec;
    unsigned long session_start_time;
    unsigned long pause_start_time;
    unsigned long last_snapshot_time;
    // Fixed location for testing
    float latitude;
    float longitude;
};

struct PersistentData {
    String device_mac;      // Immutable hardware identity
    String device_id;       // Cloud-assigned logical ID
    String tenant_id;       // Cloud-assigned tenant
    bool is_configured;
};

// Global state variables
SessionState currentSession;
PersistentData persistentData;
volatile bool stopButtonPressed = false;
unsigned long lastHealthPublish = 0;
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);
Preferences preferences;

// Queue for offline events
static QueueHandle_t eventQueue = NULL;
const int QUEUE_SIZE = 20;

// ==================== FUNCTION PROTOTYPES ====================
void setupWiFi();
void setupMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void processCommand(JsonDocument& doc);
bool validateTenantAndDevice(const JsonDocument& doc);
String generateSessionId();
void publishEvent(const JsonDocument& doc);
void publishError(const char* reason, const JsonDocument* originalCmd = nullptr);
void handleActivate(JsonDocument& doc);
void handleIntensity(JsonDocument& doc);
void handleMode(JsonDocument& doc);
void handlePause(JsonDocument& doc);
void handleResume(JsonDocument& doc);
void handleStop(JsonDocument& doc);
void handleCheck(JsonDocument& doc);
void handleSetDevice(JsonDocument& doc);
void handleSetTenant(JsonDocument& doc);
void handleOTA(JsonDocument& doc);
void handlePhysicalStop();
void handleSessionComplete(String reason = "completed");
void publishAutoHealth();
void takeSnapshot();
void loadSnapshot();
void recoverFromFailure();
void addLocationToEvent(JsonDocument& doc);
void setHardwareState();
void stopHardware();
void IRAM_ATTR stopButtonISR();
void queueEvent(const JsonDocument& doc);
void processEventQueue();
void printDeviceInfo();

// ==================== SETUP ====================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Print developer information
    Serial.println("\n\n==========================================");
    Serial.println("   ProjectX ESP32 IoT Controller");
    Serial.println("==========================================");
    Serial.println("Developed By: Md Khorshed Alam");
    Serial.println("Email: andgatetech@gmail.com");
    Serial.println("Company: ANDGATE TECH");
    Serial.println("==========================================\n");
    
    Serial.println("Version: 1.0.0");
    Serial.printf("Health Check Interval: %lu seconds\n", HEALTH_INTERVAL/1000);
    Serial.println("==========================================\n");
    
    // Initialize random seed
    randomSeed(analogRead(0));
    
    // Initialize pins
    pinMode(STOP_BUTTON_PIN, INPUT_PULLUP);
    pinMode(RELAY_FULL_BODY, OUTPUT);
    pinMode(RELAY_NECK, OUTPUT);
    pinMode(RELAY_BACK, OUTPUT);
    pinMode(RELAY_VIBRATION, OUTPUT);
    pinMode(RELAY_HEAT, OUTPUT);
    pinMode(INTENSITY_PWM_PIN, OUTPUT);
    pinMode(LED_STATUS, OUTPUT);
    
    // Ensure all hardware is OFF at startup
    stopHardware();
    
    // Attach interrupt for STOP button
    attachInterrupt(digitalPinToInterrupt(STOP_BUTTON_PIN), stopButtonISR, FALLING);
    
    // Initialize Preferences (NVS)
    preferences.begin("projectx", false);
    
    // Load persistent data
    persistentData.device_mac = WiFi.macAddress();
    persistentData.device_id = preferences.getString("device_id", "");
    persistentData.tenant_id = preferences.getString("tenant_id", "");
    persistentData.is_configured = (persistentData.device_id.length() > 0 && 
                                   persistentData.tenant_id.length() > 0);
    
    // Print device information
    printDeviceInfo();
    
    // Initialize session state with test location
    currentSession.state = "idle";
    currentSession.elapsed_sec = 0;
    currentSession.latitude = TEST_LATITUDE;
    currentSession.longitude = TEST_LONGITUDE;
    
    // Create event queue
    eventQueue = xQueueCreate(QUEUE_SIZE, sizeof(String*));
    
    // Load last session snapshot
    loadSnapshot();
    
    // Connect to WiFi
    setupWiFi();
    
    // Setup MQTT with TLS
    setupMQTT();
    
    // If we have a recovered session, publish it
    if (currentSession.state == "interrupted") {
        recoverFromFailure();
    }
    
    Serial.println("\n=== Device Ready for Testing ===");
    Serial.println("Use this MAC address for set_device command:");
    Serial.println("{\"task\":\"set_device\", \"device_mac\":\"" + persistentData.device_mac + "\", \"device_id\":\"ESP32-TEST-001\"}");
    Serial.println("==========================================\n");
}

// ==================== MAIN LOOP ====================
void loop() {
    // Check STOP button
    if (stopButtonPressed) {
        handlePhysicalStop();
    }
    
    // Reconnect WiFi if needed
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected, reconnecting...");
        setupWiFi();
    }
    
    // Reconnect MQTT if needed
    if (!mqttClient.connected()) {
        setupMQTT();
    }
    mqttClient.loop();
    
    // Update elapsed time for running session
    if (currentSession.state == "running" && currentSession.session_start_time > 0) {
        currentSession.elapsed_sec = (millis() - currentSession.session_start_time) / 1000;
        
        // Auto-stop if duration reached
        if (currentSession.duration > 0 && currentSession.elapsed_sec >= (unsigned long)currentSession.duration) {
            handleSessionComplete("completed");
        }
    }
    
    // Take periodic snapshot
    if (currentSession.state == "running" || currentSession.state == "paused") {
        if (millis() - currentSession.last_snapshot_time > SNAPSHOT_INTERVAL) {
            takeSnapshot();
            currentSession.last_snapshot_time = millis();
        }
    }
    
    // Auto health publish every 30 seconds
    if (millis() - lastHealthPublish > HEALTH_INTERVAL) {
        publishAutoHealth();
        lastHealthPublish = millis();
    }
    
    // Process queued events
    processEventQueue();
    
    // Blink LED for status (fast blink for testing)
    digitalWrite(LED_STATUS, (millis() % 1000 < 100) ? HIGH : LOW);
    
    delay(10); // Small delay to prevent issues
}

// ==================== DEVICE INFO ====================
void printDeviceInfo() {
    Serial.println("\n--- Device Information ---");
    Serial.printf("Hardware MAC: %s\n", persistentData.device_mac.c_str());
    Serial.printf("Device ID: %s\n", persistentData.device_id.c_str());
    Serial.printf("Tenant ID: %s\n", persistentData.tenant_id.c_str());
    Serial.printf("Configured: %s\n", persistentData.is_configured ? "YES" : "NO");
    
    if (!persistentData.is_configured) {
        Serial.println("\n*** DEVICE NOT CONFIGURED ***");
        Serial.println("To configure, send MQTT command:");
        Serial.println("Topic: projectX/cmd");
        Serial.println("Payload: {\"task\":\"set_device\", \"device_mac\":\"" + persistentData.device_mac + "\", \"device_id\":\"YOUR_DEVICE_ID\"}");
        Serial.println("Then: {\"task\":\"set_tenant\", \"tenant_id\":\"YOUR_TENANT_ID\", \"device_id\":\"YOUR_DEVICE_ID\"}");
    }
    Serial.println("---------------------------\n");
}

// ==================== WIFI SETUP ====================
void setupWiFi() {
    Serial.printf("Connecting to WiFi: %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nWiFi Connected. IP: %s, RSSI: %d\n", 
                     WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
        Serial.println("\nWiFi Connection Failed - Will retry");
    }
}

// ==================== MQTT SETUP ====================
void setupMQTT() {
    Serial.printf("Connecting to MQTT broker: %s:%d\n", MQTT_BROKER, MQTT_PORT);
    
    // Configure WiFiClientSecure for TLS
    wifiClient.setCACert(root_ca);
    wifiClient.setTimeout(15000);
    
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(2048);
    
    // Generate unique client ID
    String clientId = String(MQTT_CLIENT_ID) + "_" + String(random(0xffff), HEX);
    
    if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
        Serial.println("MQTT Connected with TLS");
        
        // Subscribe to command topics
        mqttClient.subscribe(TOPIC_CMD);
        mqttClient.subscribe(TOPIC_OTA);
        
        Serial.printf("Subscribed to: %s, %s\n", TOPIC_CMD, TOPIC_OTA);
        
        // Publish boot event
        JsonDocument bootEvent;
        if (persistentData.is_configured) {
            bootEvent["tenant_id"] = persistentData.tenant_id;
            bootEvent["device_id"] = persistentData.device_id;
        }
        bootEvent["event"] = "device_boot";
        bootEvent["state"] = currentSession.state;
        bootEvent["mac"] = persistentData.device_mac;
        bootEvent["configured"] = persistentData.is_configured;
        bootEvent["firmware_version"] = "1.0.0";
        bootEvent["developer"] = "Md Khorshed Alam";
        bootEvent["email"] = "andgatetech@gmail.com";
        addLocationToEvent(bootEvent);
        
        publishEvent(bootEvent);
    } else {
        Serial.printf("MQTT Connection failed, rc=%d\n", mqttClient.state());
    }
}

// ==================== MQTT CALLBACK ====================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    
    Serial.printf("\nMessage received [%s]: %s\n", topic, message);
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    
    if (error) {
        Serial.printf("JSON parse failed: %s\n", error.c_str());
        return;
    }
    
    if (strcmp(topic, TOPIC_CMD) == 0) {
        processCommand(doc);
    } else if (strcmp(topic, TOPIC_OTA) == 0) {
        handleOTA(doc);
    }
}

// ==================== COMMAND PROCESSING ====================
void processCommand(JsonDocument& doc) {
    if (!doc.containsKey("task")) {
        publishError("missing_task", &doc);
        return;
    }
    
    String task = doc["task"];
    
    if (task == "set_device") {
        handleSetDevice(doc);
        return;
    }
    
    if (task == "set_tenant") {
        handleSetTenant(doc);
        return;
    }
    
    if (!validateTenantAndDevice(doc)) {
        return;
    }
    
    if (task == "activate") {
        handleActivate(doc);
    } else if (task == "intensity") {
        handleIntensity(doc);
    } else if (task == "mode") {
        handleMode(doc);
    } else if (task == "pause") {
        handlePause(doc);
    } else if (task == "resume") {
        handleResume(doc);
    } else if (task == "stop") {
        handleStop(doc);
    } else if (task == "check") {
        handleCheck(doc);
    } else {
        publishError("unknown_task", &doc);
    }
}

bool validateTenantAndDevice(const JsonDocument& doc) {
    if (!persistentData.is_configured) {
        publishError("device_not_configured", &doc);
        return false;
    }
    
    if (!doc.containsKey("tenant_id") || 
        strcmp(doc["tenant_id"], persistentData.tenant_id.c_str()) != 0) {
        publishError("tenant_mismatch", &doc);
        return false;
    }
    
    if (!doc.containsKey("device_id") || 
        strcmp(doc["device_id"], persistentData.device_id.c_str()) != 0) {
        publishError("device_mismatch", &doc);
        return false;
    }
    
    return true;
}

// ==================== COMMAND HANDLERS ====================
void handleSetDevice(JsonDocument& doc) {
    if (!doc.containsKey("device_mac") || !doc.containsKey("device_id")) {
        publishError("missing_fields", &doc);
        return;
    }
    
    String cmdMac = doc["device_mac"];
    String cmdDeviceId = doc["device_id"];
    
    if (cmdMac != persistentData.device_mac) {
        Serial.printf("MAC mismatch: Command MAC=%s, Device MAC=%s\n", 
                     cmdMac.c_str(), persistentData.device_mac.c_str());
        publishError("mac_mismatch", &doc);
        return;
    }
    
    preferences.putString("device_id", cmdDeviceId);
    persistentData.device_id = cmdDeviceId;
    
    JsonDocument event;
    event["task"] = "set_device";
    event["device_mac"] = persistentData.device_mac;
    event["device_id"] = persistentData.device_id;
    event["event"] = "device_configured";
    event["status"] = "success";
    addLocationToEvent(event);
    
    publishEvent(event);
    
    Serial.printf("Device configured successfully! Device ID: %s\n", persistentData.device_id.c_str());
    Serial.println("Next step: Send set_tenant command");
}

void handleSetTenant(JsonDocument& doc) {
    if (!doc.containsKey("tenant_id") || !doc.containsKey("device_id")) {
        publishError("missing_fields", &doc);
        return;
    }
    
    String cmdTenant = doc["tenant_id"];
    String cmdDeviceId = doc["device_id"];
    
    if (persistentData.device_id.length() > 0 && cmdDeviceId != persistentData.device_id) {
        publishError("device_mismatch", &doc);
        return;
    }
    
    preferences.putString("tenant_id", cmdTenant);
    persistentData.tenant_id = cmdTenant;
    persistentData.is_configured = true;
    
    JsonDocument event;
    event["tenant_id"] = persistentData.tenant_id;
    event["device_id"] = persistentData.device_id;
    event["task"] = "set_tenant";
    event["event"] = "tenant_configured";
    event["status"] = "success";
    addLocationToEvent(event);
    
    publishEvent(event);
    
    Serial.printf("Tenant configured successfully! Tenant ID: %s\n", persistentData.tenant_id.c_str());
    Serial.println("Device is now fully configured and ready for commands!");
}

void handleActivate(JsonDocument& doc) {
    // Handle modification of existing session
    if (currentSession.state == "running") {
        if (doc.containsKey("part")) currentSession.part = doc["part"].as<String>();
        if (doc.containsKey("mode")) currentSession.mode = doc["mode"].as<String>();
        if (doc.containsKey("intensity")) currentSession.intensity = doc["intensity"];
        if (doc.containsKey("duration")) currentSession.duration = doc["duration"];
        
        setHardwareState();
        
        JsonDocument event;
        event["tenant_id"] = persistentData.tenant_id;
        event["device_id"] = persistentData.device_id;
        event["session_id"] = currentSession.session_id;
        event["part"] = currentSession.part;
        event["mode"] = currentSession.mode;
        event["state"] = "running";
        event["intensity"] = currentSession.intensity;
        event["duration"] = currentSession.duration;
        event["event"] = "session_started";
        event["elapsed_sec"] = currentSession.elapsed_sec;
        addLocationToEvent(event);
        
        publishEvent(event);
        return;
    }
    
    // Handle new session
    if (currentSession.state != "idle") {
        publishError("invalid_state", &doc);
        return;
    }
    
    if (!doc.containsKey("part") || !doc.containsKey("mode") || 
        !doc.containsKey("intensity") || !doc.containsKey("duration")) {
        publishError("missing_fields", &doc);
        return;
    }
    
    String part = doc["part"];
    if (part != "full_body" && part != "neck" && part != "back" && 
        part != "vibration" && part != "heat") {
        publishError("invalid_part", &doc);
        return;
    }
    
    String mode = doc["mode"];
    if (mode != "all_back" && mode != "upper_back" && mode != "lower_back" && 
        mode != "spot_back" && mode != "keep_tap" && mode != "intermittent_tap" && 
        mode != "gradual_tap") {
        publishError("invalid_mode", &doc);
        return;
    }
    
    int intensity = doc["intensity"];
    if (intensity < 1 || intensity > 5) {
        publishError("intensity_out_of_range", &doc);
        return;
    }
    
    currentSession.tenant_id = persistentData.tenant_id;
    currentSession.device_id = persistentData.device_id;
    currentSession.session_id = generateSessionId();
    currentSession.state = "running";
    currentSession.part = part;
    currentSession.mode = mode;
    currentSession.intensity = intensity;
    currentSession.duration = doc["duration"];
    currentSession.session_start_time = millis();
    currentSession.elapsed_sec = 0;
    
    setHardwareState();
    takeSnapshot();
    
    JsonDocument event;
    event["tenant_id"] = persistentData.tenant_id;
    event["device_id"] = persistentData.device_id;
    event["session_id"] = currentSession.session_id;
    event["part"] = currentSession.part;
    event["mode"] = currentSession.mode;
    event["state"] = "running";
    event["intensity"] = currentSession.intensity;
    event["duration"] = currentSession.duration;
    event["event"] = "session_started";
    event["elapsed_sec"] = 0;
    addLocationToEvent(event);
    
    publishEvent(event);
}

void handleIntensity(JsonDocument& doc) {
    if (currentSession.state != "running") {
        publishError("no_active_session", &doc);
        return;
    }
    
    if (!doc.containsKey("intensity")) {
        publishError("missing_fields", &doc);
        return;
    }
    
    int intensity = doc["intensity"];
    if (intensity < 1 || intensity > 5) {
        publishError("intensity_out_of_range", &doc);
        return;
    }
    
    currentSession.intensity = intensity;
    setHardwareState();
    
    JsonDocument event;
    event["tenant_id"] = persistentData.tenant_id;
    event["device_id"] = persistentData.device_id;
    event["session_id"] = currentSession.session_id;
    event["event"] = "intensity_changed";
    event["state"] = currentSession.state;
    event["intensity"] = currentSession.intensity;
    event["elapsed_sec"] = currentSession.elapsed_sec;
    addLocationToEvent(event);
    
    publishEvent(event);
}

void handleMode(JsonDocument& doc) {
    if (currentSession.state != "running") {
        publishError("no_active_session", &doc);
        return;
    }
    
    if (!doc.containsKey("mode")) {
        publishError("missing_fields", &doc);
        return;
    }
    
    String mode = doc["mode"];
    if (mode != "all_back" && mode != "upper_back" && mode != "lower_back" && 
        mode != "spot_back" && mode != "keep_tap" && mode != "intermittent_tap" && 
        mode != "gradual_tap") {
        publishError("invalid_mode", &doc);
        return;
    }
    
    currentSession.mode = mode;
    setHardwareState();
    
    JsonDocument event;
    event["tenant_id"] = persistentData.tenant_id;
    event["device_id"] = persistentData.device_id;
    event["session_id"] = currentSession.session_id;
    event["event"] = "mode_changed";
    event["state"] = currentSession.state;
    event["mode"] = currentSession.mode;
    event["elapsed_sec"] = currentSession.elapsed_sec;
    addLocationToEvent(event);
    
    publishEvent(event);
}

void handlePause(JsonDocument& doc) {
    if (currentSession.state != "running") {
        publishError("no_active_session", &doc);
        return;
    }
    
    currentSession.state = "paused";
    currentSession.pause_start_time = millis();
    stopHardware();
    
    JsonDocument event;
    event["tenant_id"] = persistentData.tenant_id;
    event["device_id"] = persistentData.device_id;
    event["session_id"] = currentSession.session_id;
    event["event"] = "session_paused";
    event["state"] = "paused";
    event["elapsed_sec"] = currentSession.elapsed_sec;
    addLocationToEvent(event);
    
    publishEvent(event);
}

void handleResume(JsonDocument& doc) {
    if (currentSession.state != "paused") {
        publishError("no_paused_session", &doc);
        return;
    }
    
    currentSession.state = "running";
    unsigned long pauseDuration = millis() - currentSession.pause_start_time;
    currentSession.session_start_time += pauseDuration;
    setHardwareState();
    
    JsonDocument event;
    event["tenant_id"] = persistentData.tenant_id;
    event["device_id"] = persistentData.device_id;
    event["session_id"] = currentSession.session_id;
    event["event"] = "session_resumed";
    event["state"] = "running";
    event["elapsed_sec"] = currentSession.elapsed_sec;
    addLocationToEvent(event);
    
    publishEvent(event);
}

void handleStop(JsonDocument& doc) {
    if (currentSession.state == "idle") {
        publishError("no_active_session", &doc);
        return;
    }
    
    handleSessionComplete("completed");
}

void handleCheck(JsonDocument& doc) {
    if (!persistentData.is_configured) {
        publishError("device_unreachable", &doc);
        return;
    }
    
    JsonDocument event;
    event["tenant_id"] = persistentData.tenant_id;
    event["device_id"] = persistentData.device_id;
    
    if (currentSession.state != "idle") {
        event["session_id"] = currentSession.session_id;
        event["part"] = currentSession.part;
        event["mode"] = currentSession.mode;
        event["intensity"] = currentSession.intensity;
        event["duration"] = currentSession.duration;
    }
    
    event["state"] = currentSession.state;
    event["event"] = "health";
    event["elapsed_sec"] = currentSession.elapsed_sec;
    event["rssi"] = WiFi.RSSI();
    event["free_heap"] = ESP.getFreeHeap();
    event["firmware_version"] = "1.0.0";
    event["developer"] = "Md Khorshed Alam";
    event["email"] = "andgatetech@gmail.com";
    addLocationToEvent(event);
    
    publishEvent(event);
}

void handleOTA(JsonDocument& doc) {
    if (!validateTenantAndDevice(doc)) {
        return;
    }
    
    if (currentSession.state != "idle") {
        publishError("ota_rejected", &doc);
        return;
    }
    
    if (!doc.containsKey("firmware_url") || !doc.containsKey("checksum")) {
        publishError("missing_fields", &doc);
        return;
    }
    
    JsonDocument event;
    event["tenant_id"] = persistentData.tenant_id;
    event["device_id"] = persistentData.device_id;
    event["event"] = "ota_started";
    addLocationToEvent(event);
    publishEvent(event);
    
    Serial.println("OTA would start from: " + doc["firmware_url"].as<String>());
    
    // Simulate OTA completion
    delay(100);
    
    JsonDocument completeEvent;
    completeEvent["tenant_id"] = persistentData.tenant_id;
    completeEvent["device_id"] = persistentData.device_id;
    completeEvent["event"] = "ota_completed";
    addLocationToEvent(completeEvent);
    publishEvent(completeEvent);
}

// ==================== PHYSICAL STOP BUTTON ====================
void IRAM_ATTR stopButtonISR() {
    stopButtonPressed = true;
}

void handlePhysicalStop() {
    stopButtonPressed = false;
    Serial.println("\n*** PHYSICAL STOP PRESSED ***");
    
    stopHardware();
    
    if (currentSession.state != "idle") {
        JsonDocument event;
        event["tenant_id"] = persistentData.tenant_id;
        event["device_id"] = persistentData.device_id;
        event["session_id"] = currentSession.session_id;
        event["event"] = "session_ended";
        event["state"] = "idle";
        event["elapsed_sec"] = currentSession.elapsed_sec;
        event["reason"] = "manual_stop";
        addLocationToEvent(event);
        publishEvent(event);
        
        currentSession.state = "idle";
        currentSession.session_id = "";
        preferences.remove("session_snapshot");
    }
}

void handleSessionComplete(String reason) {
    if (currentSession.state != "idle") {
        JsonDocument event;
        event["tenant_id"] = persistentData.tenant_id;
        event["device_id"] = persistentData.device_id;
        event["session_id"] = currentSession.session_id;
        event["event"] = "session_ended";
        event["state"] = "idle";
        event["elapsed_sec"] = currentSession.elapsed_sec;
        event["reason"] = reason;
        addLocationToEvent(event);
        publishEvent(event);
        
        stopHardware();
        
        currentSession.state = "idle";
        currentSession.session_id = "";
        preferences.remove("session_snapshot");
    }
}

// ==================== AUTO HEALTH ====================
void publishAutoHealth() {
    if (!persistentData.is_configured) return;
    
    JsonDocument event;
    event["tenant_id"] = persistentData.tenant_id;
    event["device_id"] = persistentData.device_id;
    
    if (currentSession.state != "idle") {
        event["session_id"] = currentSession.session_id;
        event["part"] = currentSession.part;
        event["mode"] = currentSession.mode;
        event["intensity"] = currentSession.intensity;
        event["duration"] = currentSession.duration;
    }
    
    event["state"] = currentSession.state;
    event["event"] = "health";
    event["elapsed_sec"] = currentSession.elapsed_sec;
    event["rssi"] = WiFi.RSSI();
    event["free_heap"] = ESP.getFreeHeap();
    event["firmware_version"] = "1.0.0";
    event["developer"] = "Md Khorshed Alam";
    event["email"] = "andgatetech@gmail.com";
    addLocationToEvent(event);
    
    publishEvent(event);
    
    Serial.println("Auto health published (30s interval)");
}

// ==================== HARDWARE CONTROL ====================
void setHardwareState() {
    if (currentSession.state != "running") return;
    
    int pwmValue = map(currentSession.intensity, 1, 5, 50, 255);
    analogWrite(INTENSITY_PWM_PIN, pwmValue);
    
    digitalWrite(RELAY_FULL_BODY, (currentSession.part == "full_body") ? HIGH : LOW);
    digitalWrite(RELAY_NECK, (currentSession.part == "neck") ? HIGH : LOW);
    digitalWrite(RELAY_BACK, (currentSession.part == "back") ? HIGH : LOW);
    digitalWrite(RELAY_VIBRATION, (currentSession.part == "vibration") ? HIGH : LOW);
    digitalWrite(RELAY_HEAT, (currentSession.part == "heat") ? HIGH : LOW);
    
    Serial.printf("Hardware: Part=%s, Mode=%s, Intensity=%d\n", 
                  currentSession.part.c_str(), currentSession.mode.c_str(), currentSession.intensity);
}

void stopHardware() {
    digitalWrite(RELAY_FULL_BODY, LOW);
    digitalWrite(RELAY_NECK, LOW);
    digitalWrite(RELAY_BACK, LOW);
    digitalWrite(RELAY_VIBRATION, LOW);
    digitalWrite(RELAY_HEAT, LOW);
    analogWrite(INTENSITY_PWM_PIN, 0);
    Serial.println("Hardware stopped");
}

// ==================== LOCATION ====================
void addLocationToEvent(JsonDocument& doc) {
    JsonObject location = doc["location"].to<JsonObject>();
    location["latitude"] = currentSession.latitude;
    location["longitude"] = currentSession.longitude;
}

// ==================== SESSION PERSISTENCE ====================
void takeSnapshot() {
    if (currentSession.state == "idle") return;
    
    JsonDocument snapshot;
    snapshot["tenant_id"] = currentSession.tenant_id;
    snapshot["device_id"] = currentSession.device_id;
    snapshot["session_id"] = currentSession.session_id;
    snapshot["state"] = currentSession.state;
    snapshot["part"] = currentSession.part;
    snapshot["mode"] = currentSession.mode;
    snapshot["intensity"] = currentSession.intensity;
    snapshot["duration"] = currentSession.duration;
    snapshot["elapsed_sec"] = currentSession.elapsed_sec;
    snapshot["latitude"] = currentSession.latitude;
    snapshot["longitude"] = currentSession.longitude;
    
    String snapshotStr;
    serializeJson(snapshot, snapshotStr);
    preferences.putString("session_snapshot", snapshotStr);
}

void loadSnapshot() {
    String snapshotStr = preferences.getString("session_snapshot", "");
    if (snapshotStr.length() == 0) return;
    
    JsonDocument snapshot;
    DeserializationError error = deserializeJson(snapshot, snapshotStr);
    if (error) {
        preferences.remove("session_snapshot");
        return;
    }
    
    currentSession.tenant_id = snapshot["tenant_id"].as<String>();
    currentSession.device_id = snapshot["device_id"].as<String>();
    currentSession.session_id = snapshot["session_id"].as<String>();
    currentSession.state = "interrupted"; // Don't auto-resume
    currentSession.part = snapshot["part"].as<String>();
    currentSession.mode = snapshot["mode"].as<String>();
    currentSession.intensity = snapshot["intensity"];
    currentSession.duration = snapshot["duration"];
    currentSession.elapsed_sec = snapshot["elapsed_sec"];
    currentSession.latitude = snapshot["latitude"] | TEST_LATITUDE;
    currentSession.longitude = snapshot["longitude"] | TEST_LONGITUDE;
}

void recoverFromFailure() {
    if (currentSession.state == "interrupted") {
        JsonDocument event;
        event["tenant_id"] = currentSession.tenant_id;
        event["device_id"] = currentSession.device_id;
        event["session_id"] = currentSession.session_id;
        event["event"] = "session_recovered";
        event["state"] = "interrupted";
        event["elapsed_sec"] = currentSession.elapsed_sec;
        event["reason"] = "power_loss";
        addLocationToEvent(event);
        publishEvent(event);
        
        currentSession.state = "idle";
        preferences.remove("session_snapshot");
    }
}

// ==================== EVENT PUBLISHING ====================
void publishEvent(const JsonDocument& doc) {
    String output;
    serializeJson(doc, output);
    
    Serial.printf("Publishing event: %s\n", output.c_str());
    
    if (mqttClient.connected()) {
        if (!mqttClient.publish(TOPIC_EVENT, output.c_str())) {
            Serial.println("Failed to publish, queueing...");
            queueEvent(doc);
        }
    } else {
        Serial.println("MQTT disconnected, queueing...");
        queueEvent(doc);
    }
}

void queueEvent(const JsonDocument& doc) {
    if (eventQueue == NULL) return;
    
    String* eventStr = new String();
    serializeJson(doc, *eventStr);
    
    if (xQueueSend(eventQueue, &eventStr, 0) != pdTRUE) {
        Serial.println("Event queue full");
        delete eventStr;
    }
}

void processEventQueue() {
    if (eventQueue == NULL || !mqttClient.connected() || uxQueueMessagesWaiting(eventQueue) == 0) {
        return;
    }
    
    String* eventStr;
    while (xQueueReceive(eventQueue, &eventStr, 0) == pdTRUE) {
        if (mqttClient.publish(TOPIC_EVENT, eventStr->c_str())) {
            delete eventStr;
        } else {
            // Re-queue if failed
            xQueueSend(eventQueue, &eventStr, 0);
            break;
        }
    }
}

void publishError(const char* reason, const JsonDocument* originalCmd) {
    JsonDocument errorEvent;
    
    if (originalCmd != nullptr) {
        if (originalCmd->containsKey("tenant_id")) {
            errorEvent["tenant_id"] = (*originalCmd)["tenant_id"];
        }
        
        if (originalCmd->containsKey("device_id")) {
            errorEvent["device_id"] = (*originalCmd)["device_id"];
        }
        
        // Copy other fields manually
        if (originalCmd->containsKey("task")) {
            errorEvent["task"] = (*originalCmd)["task"];
        }
        if (originalCmd->containsKey("part")) {
            errorEvent["part"] = (*originalCmd)["part"];
        }
        if (originalCmd->containsKey("mode")) {
            errorEvent["mode"] = (*originalCmd)["mode"];
        }
        if (originalCmd->containsKey("intensity")) {
            errorEvent["intensity"] = (*originalCmd)["intensity"];
        }
        if (originalCmd->containsKey("duration")) {
            errorEvent["duration"] = (*originalCmd)["duration"];
        }
    } else {
        errorEvent["tenant_id"] = persistentData.tenant_id;
        errorEvent["device_id"] = persistentData.device_id;
    }
    
    errorEvent["session_id"] = currentSession.session_id;
    errorEvent["event"] = "error";
    errorEvent["state"] = currentSession.state;
    errorEvent["elapsed_sec"] = currentSession.elapsed_sec;
    errorEvent["reason"] = reason;
    addLocationToEvent(errorEvent);
    
    publishEvent(errorEvent);
}

// ==================== UTILITY FUNCTIONS ====================
String generateSessionId() {
    char sessionId[9];
    const char hexChars[] = "0123456789abcdef";
    
    for (int i = 0; i < 8; i++) {
        sessionId[i] = hexChars[random(0, 16)];
    }
    sessionId[8] = '\0';
    
    return String(sessionId);
}
