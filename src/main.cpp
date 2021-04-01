#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include <SPI.h>
#include <RH_RF95.h>

#define RFM95_CS 15
#define RFM95_RST 16
#define RFM95_INT 5
#define RF95_FREQ 915.0
#define LED 2
RH_RF95 rf95(RFM95_CS, RFM95_INT);
// Singleton instance of the radio driver


const char* ssid = "Emergency Network 2";

int16_t packetnum = 0;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");



void checkMSG() {
    // Now wait for a reply
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    
    if (rf95.recv(buf, &len)) {
        Serial.println((char*)buf);
        ws.textAll((char*)buf);
        // Serial.print("RSSI: ");
        // Serial.println(rf95.lastRssi(), DEC);
    }
}

void sendMSG(String lora_msg) {
    ws.textAll(lora_msg);
    rf95.send(reinterpret_cast<const uint8_t*>(lora_msg.c_str()), lora_msg.length());

    // char radiopacket[20] = "Hello World #      ";
    // itoa(packetnum++, radiopacket+13, 10);
    // // Serial.print("Sending "); Serial.println(radiopacket);
    // radiopacket[19] = 0;

    // // Serial.println("Sending..."); delay(10);
    // rf95.send((uint8_t *)radiopacket, 20);

    // // Serial.println("Waiting for packet to complete..."); delay(10);
    // // rf95.waitPacketSent();
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        String connect_str = "#" + String(client->id()) + " connected from " + client->remoteIP().toString().c_str();
        // ws.textAll(connect_str);
        // sendMSG(connect_str);
        Serial.println(connect_str);
    } else if (type == WS_EVT_DISCONNECT) {
        String disconnect_str = "#" + String(client->id()) + " disconnected";
        // ws.textAll(disconnect_str);
        // sendMSG(disconnect_str);
        Serial.println(disconnect_str);
    } else if (type == WS_EVT_DATA) {
        String message_to_send = "";
        AwsFrameInfo * info = (AwsFrameInfo*)arg;
        if(info->final && info->index == 0 && info->len == len) {
            //the whole message is in a single frame and we got all of it's data
            if(info->opcode == WS_TEXT){
                data[len] = 0;
                message_to_send = (char*)data;
            } else {
                Serial.printf("#%u %s[%llu]: ", client->id(), (info->opcode == WS_TEXT)?"text":"binary", info->len);
                for(size_t i=0; i < info->len; i++){
                    Serial.printf("%02x ", data[i]);
                }
                Serial.printf("\n");
                message_to_send = "IDK it's binary crap";
            }
        } else {
            //message is comprised of multiple frames or the frame is split into multiple packets
            if(info->index == 0){
                if(info->num == 0)
                    Serial.printf("#%u %s\n", client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
                Serial.printf("#%u frame[%u] start[%llu]\n", client->id(), info->num, info->len);
            }

            if(info->message_opcode == WS_TEXT){
                data[len] = 0;
                message_to_send = (char*)data;
            } else {
                Serial.printf("#%u frame[%u] %s[%llu - %llu]: ", client->id(), info->num, (info->message_opcode == WS_TEXT)?"text":"binary", info->index, info->index + len);
                for(size_t i=0; i < len; i++){
                    Serial.printf("%02x ", data[i]);
                }
                Serial.printf("\n");
                message_to_send = "IDK it's binary crap";
            }

            if((info->index + len) == info->len){
                Serial.printf("#%u frame[%u] end[%llu]\n", client->id(), info->num, info->len);
                if(info->final){
                    Serial.printf("#%u %s end\n", client->id(), (info->message_opcode == WS_TEXT)?"text":"binary");
                }
            }
        }
        String send_message = "#" + String(client->id()) + ": " + message_to_send;
        Serial.println(send_message);
        if(info->opcode == WS_TEXT) {
            // client->text(send_message);
            // ws.textAll(send_message);
            sendMSG(send_message);

        } else {
            // client->binary(send_message);
            ws.binaryAll(send_message);
        }
    }
}

void initWebSocket() {
    ws.onEvent(onEvent);
    server.addHandler(&ws);
}

void setup() {
    Serial.begin(115200);

    pinMode(LED, OUTPUT);
    digitalWrite(LED, HIGH);
    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, HIGH);
    delay(100);
    Serial.println("\n");
    Serial.println("Gateway Module starting...\n");
    
    digitalWrite(RFM95_RST, LOW);
    delay(10);
    digitalWrite(RFM95_RST, HIGH);
    delay(10);

    rf95.init();
    rf95.setFrequency(RF95_FREQ);

    Serial.println("Initializing AP...\n");
    WiFi.softAP(ssid);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    LittleFS.begin();

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html");
    });

    initWebSocket();

    server.begin();
}

void loop() {
    ws.cleanupClients();
    checkMSG();
}