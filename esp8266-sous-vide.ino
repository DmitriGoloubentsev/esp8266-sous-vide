#include <DallasTemperature.h>

#include <ESP8266WiFi.h>
#include "./DNSServer.h"                  // Patched lib for captive portal
#include <OneWire.h>

#define TASKER_MAX_TASKS 32
#include "Tasker.h"

#include "FS.h"

OneWire  ds(5); // line with DS1820 thermometer

int relayPin(4); // line with relay to power applience

// Your sous-vide is an AP with captive portal.
const char* my_ssid = "Sous-Vide-Cooker";
const char* my_password = "12345678";

// SSID connect to 
String ssid = "WIFI-SSID-CONNECT_TO";
String password = "WIFIPASSWORD";

float target = 54.0;
int relay_state = 0;
int time_to_change = 0;
int on_state_period = 30;
int min_off_state_period = 120;
int led_blink = 0;
float tempC = -33.0;
float maxTemp = -33.0;
int time_before_target = 0;
int time_at_target = 0;
int reached_target = 0;

DallasTemperature sensors(&ds);

// arrays to hold device address
DeviceAddress insideThermometer;

IPAddress localIP;
 
int ledPin = LED_BUILTIN; // GPIO13

Tasker tasker;

const byte        DNS_PORT = 53;          // Capture DNS requests on port 53
IPAddress         apIP(10, 10, 10, 1);    // Private network for server
DNSServer         dnsServer;              // Create the DNS object

WiFiServer server(80);
 
void setup() {
  Serial.begin(115200);
  delay(10);

   pinMode(relayPin, OUTPUT);
 digitalWrite(relayPin, HIGH);

  // always use this to "mount" the filesystem
  bool result = SPIFFS.begin();
  Serial.println("SPIFFS opened: " + result);

  FSInfo fs_info;
  SPIFFS.info(fs_info);

  Serial.println("Free bytes: "); Serial.println(fs_info.totalBytes); Serial.println("\n");

  loadSettings();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(my_ssid, my_password);
//  WiFi.begin(ssidc, passwordc);
  WiFi.begin(ssid.c_str(), password.c_str());
  for (int i = 0; i < 30 && (WiFi.status() != WL_CONNECTED); ++i) {
    Serial.print("."); delay(1000);
  }
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(my_ssid, my_password);    
  }

  dnsServer.start(DNS_PORT, "*", apIP);
  
  tasker.setInterval(updateSensors, 1000);
  tasker.setInterval(update_relay, 1000);

  server.begin();
}

void saveSettings() {
      // open the file in write mode
    File f = SPIFFS.open("/settings.txt", "w");
    if (!f) {
      Serial.println("file creation failed");
    }
    // now write two lines in key/value style with  end-of-line characters
    f.println(ssid);
    f.println(password);
    f.println(target);
    f.println(on_state_period);
    f.println(min_off_state_period);

}
void loadSettings() {
  File f = SPIFFS.open("/settings.txt", "r");
  
  if (!f) {
    Serial.println("File doesn't exist yet. Creating it");
    saveSettings();
  } else {
    // we could open the file
    ssid = f.readStringUntil('\n'); ssid.trim();
    password = f.readStringUntil('\n'); password.trim();
    target = f.readStringUntil('\n').toFloat();
    on_state_period = f.readStringUntil('\n').toInt();
    min_off_state_period = f.readStringUntil('\n').toInt();
    Serial.print("Read config ssid = ");Serial.println(ssid.c_str());
    Serial.print("Read config password = ");Serial.println(password.c_str());
  }
  f.close();
}

float printTemperature(DeviceAddress deviceAddress)
{
  // method 1 - slower
  //Serial.print("Temp C: ");
  //Serial.print(sensors.getTempC(deviceAddress));
  //Serial.print(" Temp F: ");
  //Serial.print(sensors.getTempF(deviceAddress)); // Makes a second call to getTempC and then converts to Fahrenheit

  // method 2 - faster
  float tempC = sensors.getTempC(deviceAddress);
  Serial.print("Temp C: ");
  Serial.print(tempC);
  Serial.print(" Temp F: ");
  Serial.println(DallasTemperature::toFahrenheit(tempC)); // Converts tempC to Fahrenheit
  return tempC;
}

void updateSensors(int) {
  sensors.begin();
  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0"); 
  delay(100);
  sensors.requestTemperatures(); // Send the command to get temperatures
  tempC = printTemperature(insideThermometer);
  if (tempC > maxTemp) { maxTemp = tempC; }
}

void update_relay(int i) {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, led_blink);
  led_blink = (led_blink + 1) & 1;

  if (tempC > target) { reached_target = 1; }
  if (reached_target == 0) { ++time_before_target; }
  if (reached_target && tempC > (target - 1)) { ++time_at_target; }
  
  if (time_to_change > 0) --time_to_change;
  if (time_to_change <= 0) {
    if (relay_state) {
      relay_state = 0;
        pinMode(relayPin, OUTPUT);
        digitalWrite(relayPin, LOW);
        Serial.println("TURNOFF \n");
      time_to_change = min_off_state_period;
    } else {
      if (tempC < target) {
        relay_state = 1;
        pinMode(relayPin, OUTPUT);
        digitalWrite(relayPin, HIGH);
        Serial.println("TURNON \n");
        time_to_change = on_state_period;      
      }
    }
  }
}

void loop(void) {
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;

  tasker.loop();
  dnsServer.processNextRequest();
  loop_web_client();
}

String getArg(const char* param, String& req) {
    int pos = req.indexOf(param);
  if (pos != -1) {
    int ss = std::min(req.indexOf("&",pos), req.indexOf(" ",pos));

    String res = req.substring(pos+strlen(param), ss == -1 ? req.length() : ss);
    res.replace("+", " ");
    
    return res;
  }
  return "";
}

void loop_web_client() 
{
  // Check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // Read the first line of the request
  String req = client.readStringUntil('\r');
  Serial.println(req);
  client.flush();

  bool changed = false;
  String param = getArg("target=", req);
  if (param != "") {
    changed = changed || (target != param.toFloat());
    target = param.toFloat();
  }   
  param = getArg("on_time=", req);
  if (param != "") {
    changed = changed || (on_state_period != param.toInt());
    on_state_period = param.toInt();
  }   
  param = getArg("off_time=", req);
  if (param != "") {
    changed = changed || (min_off_state_period != param.toInt());
    min_off_state_period = param.toInt();
  }   
  bool wifi_changed = false;
  param = getArg("ssid=", req);
  if (param != "") {
    wifi_changed = wifi_changed || (ssid != param);
    ssid = param;
  }
  param = getArg("password=", req);
  if (param != "") {
    wifi_changed = wifi_changed || (password != param);
    password = param;
  }
  if (wifi_changed || min_off_state_period) {
    saveSettings();
  }
  if (wifi_changed) {
    // WiFi.begin(ssid.c_str(), password.c_str());
  }

  // Match the request
  int val = -1; // We'll use 'val' to keep track of both the
                // request type (read/set) and value if set.
  if (req.indexOf("/led/0") != -1)
    val = 0; // Will write LED low
  else if (req.indexOf("/led/1") != -1)
    val = 1; // Will write LED high
  else if (req.indexOf("/read") != -1)
    val = -2; // Will print pin reads
  // Otherwise request will be invalid. We'll say as much in HTML



  client.flush();

  // Prepare the response. Start with the common header:
  String s = "HTTP/1.1 200 OK\r\n";
  s += "Content-Type: text/html\r\n\r\n";
    s += "<!DOCTYPE HTML><html>";
  s += "<head><meta http-equiv=refresh content=15>";
  s += "</head>";
  s += "<h2>SmartPowerBar</h2>";
  s += "Current Temperature : "  + String(tempC) + "<br>";
  s += "Current power is : ";
  s += (relay_state ? "ON" : "OFF");
  s += " <br>";
  s += "Sleep for : " + String(time_to_change) + " seconds<br>";
  s += "<form>";
  s += "Set target temperature: <input type=number name=target min=0 max=85 step=0.5 value=" + String(target) + " size=4><br>";
  s += "Set time for ON state: <input type=number name=on_time  min=5 max=120 step=1 value=" + String(on_state_period) + " size=4><br>";
  s += "Set minimum time for OFF state: <input type=number name=off_time min=5 max=1200 step=1 value=" + String(min_off_state_period) + " size=4><br>";
  s += "local Wifi to CONNECT to: <input name=ssid value=\"" + String(ssid) + "\"><br>";
  s += "local Wifi password: <input name=password value=\"" + String(password) + "\"><br>";
  s += "<input name=end value=end type=hidden><br>";
  s += "<input type=submit><br>";

  s += "Maximum temperature : "  + String(maxTemp) + "<br>";
  s += "Time before target : "  + String(time_before_target / 60) + " minutes <br>";
  s += "Time at target : "  + String(time_at_target / 60) + " minutes <br>";
  s += "Wifi to <strong>";
  s += ssid;
  s += "</strong>";
  s += WiFi.status() == WL_CONNECTED ? " CONNECTED " : " NOT CONNECTED ";
  s += "<br>Local IP : http://";
  s += WiFi.localIP().toString();
  s += "<br>";

  s += "</form></html>\n";

  // Send the response to the client
  client.print(s);
  delay(1);
  Serial.println("Client disonnected");

  // The client will actually be disconnected 
  // when the function returns and 'client' object is detroyed
}
