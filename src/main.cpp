#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <GParser.h>
#include <array>
#include <cstring>
#include <string>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <EncButton.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>

#define COLOR_ARRAY_START_ADDRESS 0

#define PIXEL_QUANTITY 150

#define HOSTNAME "led_strip"

DNSServer dns;

Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_QUANTITY, D2, NEO_GRBW + NEO_KHZ800);

AsyncWebServer server(80);

EncButton<EB_CALLBACK, D5> wifi_reset_button;

std::array<uint32_t, PIXEL_QUANTITY> read_color_array_from_eeprom() {
    std::array<uint32_t, PIXEL_QUANTITY> color_array{0};
    return color_array;
}

void save_color_array_to_eeprom(std::array<uint32_t, PIXEL_QUANTITY> &target) {

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

void setup() {
    Serial.begin(115200);

    strip.begin();
    strip.setBrightness(255);
    strip.show();

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

    if (millis() - last_update > 30000) {
        Serial.println("update mdns");
        MDNS.update();
        last_update = millis();
    }

    if (millis() - last_heap_check_time > 500) {
        last_heap_check_time = millis();
        Serial.print("Free Heap: ");
        Serial.println(ESP.getFreeHeap());
    }
}