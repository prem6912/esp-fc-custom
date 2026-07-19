#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include "EspNowRcLink/Transmitter.h"
#include "webpage.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <WiFiUdp.h>
#include <esp_wifi.h>

int8_t last_fc_rssi = -100;

extern EspNowRcLink::Transmitter tx;

void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
  const uint8_t* src_mac = ppkt->payload + 10;
  if (tx.isConnected() && std::equal(src_mac, src_mac + 6, tx.getPeer())) {
    last_fc_rssi = ppkt->rx_ctrl.rssi;
  }
}

// WiFi Configuration
const char* ap_ssid = "ESP-FC-Transmitter";
const char* ap_password = "flightcontroller"; // Must be at least 8 characters

// Web and WebSocket Servers
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// UDP Configuration
WiFiUDP udp;
const unsigned int udpPort = 82;
char udpPacketBuffer[255];

// ESP-NOW Transmitter
EspNowRcLink::Transmitter tx;

// Channel values (1000 - 2000)
// 0: Roll, 1: Pitch, 2: Throttle, 3: Yaw, 4: AUX1 (Arm)
volatile uint16_t rc_channels[8] = {1500, 1500, 1000, 1500, 1000, 1500, 1500, 1500};
unsigned long last_packet_time = 0;
unsigned long packet_count = 0;

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      // Failsafe: Reset channels to neutral/safe values on disconnect
      rc_channels[0] = 1500; // Roll center
      rc_channels[1] = 1500; // Pitch center
      rc_channels[2] = 1000; // Throttle min
      rc_channels[3] = 1500; // Yaw center
      rc_channels[4] = 1000; // AUX1 (Disarm)
      rc_channels[5] = 1000; // AUX2 (ALTHOLD off)
      break;
      
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Connected from %s\n", num, ip.toString().c_str());
      break;
    }
      
    case WStype_TEXT: {
      // Expecting CSV payload: "roll,pitch,throttle,yaw,arm,althold"
      // Example: "1500,1500,1000,1500,1000,1000"
      int roll, pitch, throttle, yaw, arm, althold;
      int parsed = sscanf((char*)payload, "%d,%d,%d,%d,%d,%d", &roll, &pitch, &throttle, &yaw, &arm, &althold);
      if (parsed >= 5) {
        rc_channels[0] = constrain(roll, 1000, 2000);
        rc_channels[1] = constrain(pitch, 1000, 2000);
        rc_channels[2] = constrain(throttle, 1000, 2000);
        rc_channels[3] = constrain(yaw, 1000, 2000);
        rc_channels[4] = constrain(arm, 1000, 2000);
        if (parsed == 6) {
          rc_channels[5] = constrain(althold, 1000, 2000);
        } else {
          rc_channels[5] = 1000;
        }
        
        packet_count++;
        last_packet_time = millis();
      }
      break;
    }
    default:
      break;
  }
}

void setup() {
  // Disable brownout detector to prevent bootloops on weak USB power
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- ESP-FC Webpage Transmitter Starting ---");

  // 1. Set up WiFi Access Point on Channel 1
  WiFi.mode(WIFI_AP);
  // We specify channel 1 to ensure ESP-NOW and WiFi AP operate on the same channel
  WiFi.softAP(ap_ssid, ap_password, 1, 0, 4); 
  
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("Access Point SSID: ");
  Serial.println(ap_ssid);
  Serial.print("Access Point IP: ");
  Serial.println(apIP);

  // Enable promiscuous mode to capture RSSI of incoming ESP-NOW packets
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(promiscuous_rx_cb);

  // 2. Initialize ESP-NOW Transmitter
  // Pass 'false' so it doesn't override our WiFi AP configuration
  bool tx_ok = tx.begin(false);
  Serial.printf("ESP-NOW Transmitter Init: %s\n", tx_ok ? "SUCCESS" : "FAILED");

  // 3. Set up Web Server
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP Web Server started.");

  // 4. Set up WebSocket Server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket Server started.");

  // 5. Set up UDP Port Listener
  udp.begin(udpPort);
  Serial.printf("UDP Server started on port %d.\n", udpPort);
}

void loop() {
  // Handle Web server clients
  server.handleClient();
  
  // Handle WebSocket events
  webSocket.loop();

  // Check for incoming UDP packets
  int packetSize = udp.parsePacket();
  if (packetSize) {
    int len = udp.read(udpPacketBuffer, 255);
    if (len > 0) {
      udpPacketBuffer[len] = 0; // null terminator
      int roll, pitch, throttle, yaw, arm, althold;
      // Expecting CSV format: "roll,pitch,throttle,yaw,arm,althold"
      int parsed = sscanf(udpPacketBuffer, "%d,%d,%d,%d,%d,%d", &roll, &pitch, &throttle, &yaw, &arm, &althold);
      if (parsed >= 5) {
        rc_channels[0] = constrain(roll, 1000, 2000);
        rc_channels[1] = constrain(pitch, 1000, 2000);
        rc_channels[2] = constrain(throttle, 1000, 2000);
        rc_channels[3] = constrain(yaw, 1000, 2000);
        rc_channels[4] = constrain(arm, 1000, 2000);
        if (parsed == 6) {
          rc_channels[5] = constrain(althold, 1000, 2000);
        } else {
          rc_channels[5] = 1000;
        }
        packet_count++;
        last_packet_time = millis();
      }
    }
  }

  // Send RC data via ESP-NOW at 100Hz (every 10ms)
  unsigned long now = millis();
  static unsigned long sendNext = now + 10;

  if (now >= sendNext) {
    // If we haven't received a packet from the webpage in 1 second, trigger failsafe
    if (now - last_packet_time > 1000) {
      rc_channels[0] = 1500;
      rc_channels[1] = 1500;
      rc_channels[2] = 1000;
      rc_channels[3] = 1500;
      rc_channels[4] = 1000; // Disarm
      rc_channels[5] = 1000; // AUX2 (ALTHOLD off)
    }

    // Load channel values into transmitter
    for (size_t c = EspNowRcLink::RC_CHANNEL_MIN; c <= EspNowRcLink::RC_CHANNEL_MAX; c++) {
      tx.setChannel(c, rc_channels[c]);
    }
    
    // Commit the frame for transmission
    tx.commit();
    
    sendNext = now + 10;
  }

  // Handle packet transmission and pairing in the background
  tx.update();

  // Broadcast battery telemetry to WebSocket clients at 5Hz (every 200ms)
  static unsigned long lastTelemetrySend = 0;
  if (now - lastTelemetrySend > 200) {
    float voltage = tx.getSensor(0) / 100.0f;
    float cellVoltage = tx.getSensor(1) / 100.0f;
    int percentage = tx.getSensor(2);
    int cells = tx.getSensor(3);
    
    bool fc_connected = tx.isConnected() && (now - tx.getLastRecvTime() < 2000);
    int fc_status = fc_connected ? 1 : 0;
    int fc_rssi = fc_connected ? last_fc_rssi : -100;
    
    wifi_sta_list_t station_list;
    esp_wifi_ap_get_sta_list(&station_list);
    int phone_status = (station_list.num > 0) ? 1 : 0;
    int phone_rssi = (station_list.num > 0) ? station_list.sta[0].rssi : -100;
    
    char telemetryPayload[128];
    if (cells > 0 && cells <= 6 && voltage > 1.0f) {
      snprintf(telemetryPayload, sizeof(telemetryPayload), "t,%.2f,%.2f,%d,%d,%d,%d,%d,%d",
               voltage, cellVoltage, percentage, cells, fc_status, phone_status, fc_rssi, phone_rssi);
    } else {
      snprintf(telemetryPayload, sizeof(telemetryPayload), "t,0.00,0.00,0,0,%d,%d,%d,%d",
               fc_status, phone_status, fc_rssi, phone_rssi);
    }
    webSocket.broadcastTXT(telemetryPayload);
    lastTelemetrySend = now;
  }

  // Debug logging every 2 seconds
  static unsigned long lastLog = 0;
  if (now - lastLog > 2000) {
    Serial.printf("Status: R:%d P:%d T:%d Y:%d A:%d H:%d | Web Packet Rate: %.1f Hz\n",
                  rc_channels[0], rc_channels[1], rc_channels[2], rc_channels[3], rc_channels[4], rc_channels[5],
                  (float)packet_count / 2.0f);
    packet_count = 0;
    lastLog = now;
  }
}
