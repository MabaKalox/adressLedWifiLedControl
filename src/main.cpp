#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <GParser.h>
#include <cstring>
#include <string>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <EncButton.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>

#define COLOR_ARRAY_FLAG_ADDRESS 0
#define COLOR_ARRAY_START_ADDRESS 1

#define PIXEL_QUANTITY 150

#define EEPROM_SIZE PIXEL_QUANTITY * sizeof(uint32_t) + 1

#define HOSTNAME "led_strip"

DNSServer dns;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_QUANTITY, D2, NEO_GRBW + NEO_KHZ800);

AsyncWebServer server(80);

EncButton<EB_CALLBACK, D5> wifi_reset_button;

template<class ContentType, int const ContentLength>
void read_array_eeprom(int address, ContentType *to_read_into) {
    for (int i = 0; i < ContentLength; i++) {
        EEPROM.get(address, to_read_into[i]);
        address += sizeof(ContentType);
    }
}

template<class ContentType, int const ContentLength>
void save_array_eeprom(int address, const ContentType *to_save) {
    for (int i = 0; i < ContentLength; i++) {
        EEPROM.put(address, to_save[i]);
        address += sizeof(ContentType);
    }
    EEPROM.commit();
}

void applyColorArray(uint32_t *target) {
    for (uint16_t i = 0; i < PIXEL_QUANTITY; i++) {
        strip.setPixelColor(i, target[i]);
    }
    strip.show();
}

bool set_up_wifi() {
    WiFi.persistent(true);

    AsyncWiFiManager wm(&server, &dns);

    wm.setConfigPortalTimeout(180);

    bool is_connected;

    is_connected = wm.autoConnect("LED_STRIP_WIFI_SETUP"); // Anonymous AP

    if (!is_connected) {
        Serial.println("Failed to connect.");
    } else {
        Serial.print("Connected. Local IP: ");
        Serial.println(WiFi.localIP());
    }

    return is_connected;
}

void set_up_server() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Hello, world");
    });

    server.on("/remove_saved_led_strip_state", HTTP_GET, [](AsyncWebServerRequest *request) {
        EEPROM.put(COLOR_ARRAY_FLAG_ADDRESS, false);
        EEPROM.commit();
        request->send(200, "text/plain", "OK");
    });

    server.on("/set_led_strip", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("color_array", true)) {
            String received_data = request->getParam("color_array", true)->value();
            GParser received_data_parsed(received_data.begin(), ',');
            int received_data_parsed_len = received_data_parsed.split();
            Serial.print("data len: ");
            Serial.println(received_data_parsed_len);
            if (received_data_parsed_len == PIXEL_QUANTITY) {
                uint32_t color_array[PIXEL_QUANTITY]{0};
                for (uint16_t i = 0; i < received_data_parsed_len; i++) {
                    color_array[i] = std::stoul(received_data_parsed[i], nullptr, 16);
                }
                applyColorArray(color_array);
                if (request->hasParam("save", true) && *(request->getParam("save", true)->value().c_str()) == '1') {
                    save_array_eeprom<uint32_t, PIXEL_QUANTITY>(COLOR_ARRAY_START_ADDRESS, color_array);
                    EEPROM.put(COLOR_ARRAY_FLAG_ADDRESS, true);
                    EEPROM.commit();
                    Serial.println("Saving");
                }
                request->send(200, "text/plain", "APPLIED");
            } else {
                request->send(
                        400, "text/plain",
                        "ARRAY LENGTH DOES NOT MATCHES LED QUANTITY"
                );
            }
        } else {
            request->send(
                    400, "text/plain",
                    "ARGUMENT NOT FOUND"
            );
        }
    });

    server.onNotFound([](AsyncWebServerRequest *request) {
        request->send(404, "text/plain", "Not found");
    });

    server.begin();
}

bool set_up_mdns() {
    bool is_ok = MDNS.begin(HOSTNAME);
    MDNS.addService("http", "tcp", 80);
    return is_ok;
}

void reset_wifi() {
    AsyncWiFiManager wm(&server, &dns);
    wm.resetSettings();
    delay(1000);
    ESP.restart();
    Serial.println("Resetting WM data.");
}

void apply_last_setting_if_exists() {
    bool is_set = false;
    EEPROM.get(COLOR_ARRAY_FLAG_ADDRESS, is_set);
    if (is_set) {
        uint32_t color_array[PIXEL_QUANTITY]{0};
        read_array_eeprom<uint32_t, PIXEL_QUANTITY>(COLOR_ARRAY_START_ADDRESS, color_array);
        applyColorArray(color_array);
    }
}

void setup() {
    Serial.begin(115200);
    EEPROM.begin(EEPROM_SIZE);

    strip.begin();
    strip.setBrightness(255);
    strip.show();

    apply_last_setting_if_exists();

    wifi_reset_button.attach(CLICKS_HANDLER, reset_wifi);
    wifi_reset_button.pullUp();

    bool is_wifi_ok = set_up_wifi();
    if (is_wifi_ok) {
        set_up_mdns();
        set_up_server();
    }
}

uint32_t last_update = 0;
uint32_t last_heap_check_time = 0;

void loop() {
    wifi_reset_button.tick();

    if (millis() - last_update > 2000) {
//        Serial.println("update mdns");
        MDNS.update();
        last_update = millis();
    }

//    if (millis() - last_heap_check_time > 500) {
//        last_heap_check_time = millis();
//        Serial.print("Free Heap: ");
//        Serial.println(ESP.getFreeHeap());
//        Serial.print("Wifi status: ");
//        Serial.println(WiFi.isConnected());
//    }

    if (Serial.available()) {
//        EEPROM.put(COLOR_ARRAY_FLAG_ADDRESS, 0);
//        EEPROM.commit();
        bool is_set = false;
        EEPROM.get(COLOR_ARRAY_FLAG_ADDRESS, is_set);
        if (is_set) {
            uint32_t buff[PIXEL_QUANTITY]{0};
            read_array_eeprom<uint32_t, PIXEL_QUANTITY>(COLOR_ARRAY_START_ADDRESS, buff);
            for (unsigned int v : buff) {
                Serial.print(v);
                Serial.print(",");
            }
            Serial.println();
        } else {
            Serial.println("Array not set");
        }
        while (Serial.available()) {
            Serial.read();
        }
    }
}