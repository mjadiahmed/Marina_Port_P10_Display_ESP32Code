#include "Arduino.h"
#include "WiFi.h"
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include "config.h"
#define MQTT_MAX_PACKET_SIZE 4096

#include <PubSubClient.h>
// #include <SoftwareSerial.h>

// SoftwareSerial mySerial(16, 17); // RX, TX

char *config[26];

char buf[32][64];
int line[16] = {16, 17, 18, 19, 20, 21, 22, 23, 0, 1, 2, 3, 4, 5, 6, 7};
int layers = 0;
bool data = false;

/* Put IP Address details */
IPAddress local_ip;
IPAddress gateway;
IPAddress subnet;
IPAddress dns01;
IPAddress dns02;

AsyncWebServer server(80);

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
char msg[50];
int value = 0;
String MacAdd;
String processor(const String &var);
bool load_config();
bool change_config();
void Print_Screen(char SCREEN[32][64]);
void setup_wifi();
void callback(char *topic, byte *message, unsigned int length);
void reconnect();

void setup()
{
  // init memory of config array
  for (int i = 0; i < 23; i++)
  {
    config[i] = (char *)malloc(sizeof(char) * 20);
  }
  config[23] = (char *)malloc(sizeof(char) * 40);
  config[24] = (char *)malloc(sizeof(char) * 250);

  // Serial port for debugging purposes
  Serial.begin(115200);
  Serial2.begin(9600);

  // Initialize SPIFFS
  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // load config from comfig file
  load_config();

  // Debug get new config
  for (int i = 0; i < 26; i++)
  {
    Serial.println(config[i]);
  }
  Serial.println("[d] My MAC: " + String(WiFi.macAddress()));

  // connect to wifi using config or start AP mode
  if (IS_AP)
  {
    char tmp[15];
    snprintf(tmp, 15, "NEXT_%08X", (uint32_t)ESP.getEfuseMac());
    WiFi.softAP(tmp, _PASSWD_AP);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    Serial.println(WiFi.softAPIP());
  }
  else
  {
    if (!IS_DHCP)
      WiFi.config(local_ip, gateway, subnet);

    WiFi.begin(_SSID, _PASSWD);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(1000);
      Serial.println(".");
    }
    Serial.println(WiFi.localIP());
  }

  // Route to load bootstrap.css file
  server.on("/css/bootstrap.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/css/bootstrap.css", "text/css"); });

  // Route to load simple-sidebar.css file
  server.on("/css/simple-sidebar.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/css/simple-sidebar.css", "text/css"); });

  // Route for root page by default general infos
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/index.html", String(), false, processor); });

  // Route of login page
  server.on("/login", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/login.html", "text/html"); });

  // Route to save new config
  server.on("/config/save", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    String __name = "Next-IO44";
    String __lat = "0";
    String __lon = "0";
    String __desc = "";
    String __passwd = "";

    if (request->hasParam("c_name", true))
      __name = request->getParam("c_name", true)->value();

    if (request->hasParam("desc", true))
      __desc = request->getParam("desc", true)->value();

    if (request->hasParam("lat", true))
      __lat = request->getParam("lat", true)->value();

    if (request->hasParam("lon", true))
      __lon = request->getParam("lon", true)->value();

    if (request->hasParam("passwd", true))
      __passwd = request->getParam("passwd", true)->value();

    strcpy(_PASSWD_AP, __passwd.c_str());
    strcpy(LON, __lon.c_str());
    strcpy(LAT, __lat.c_str());
    strcpy(DESCRIPTION, __desc.c_str());
    strcpy(C_NAME, __name.c_str());

    Serial.println(_PASSWD_AP);
    Serial.println(LON);
    Serial.println(LAT);
    Serial.println(DESCRIPTION);
    Serial.println(C_NAME);

    change_config();
    request->redirect("/"); });

  // Route config page
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *req)
            { req->send(SPIFFS, "/config.html", String(), false, processor); });

  // Route to save new config of wifi
  server.on("/wireless/save", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    String __ssid = "";
    String __passwd = "";

    if (request->hasParam("ssid", true))
      __ssid = request->getParam("ssid", true)->value();

    if (request->hasParam("passwd", true))
      __passwd = request->getParam("passwd", true)->value();

    strcpy(_SSID, __ssid.c_str());
    strcpy(_PASSWD, __passwd.c_str());
    change_config();
    request->redirect("/"); });

  // Route of wireless page
  server.on("/wireless", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/wifi.html", "text/html"); });

  // Route to save new config of wifi
  server.on("/wifi/state", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    char res[20] = "";
    if (!IS_AP)
      sprintf(res, "%s;%s", String(WiFi.BSSIDstr()).c_str(), String(2 * (WiFi.RSSI() + 100)).c_str());
    request->send(200, "text/text", res); });

  // Route to scan all wifi
  server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String req = "";
    int n = WiFi.scanNetworks();
    if (n != 0)
    {
      for (int i = 0; i < n; ++i)
      {
        req += String(WiFi.SSID(i)) + ";" + String(2 * (WiFi.RSSI(i) + 100)) + ";" + String(WiFi.BSSIDstr(i)) + ";" + String(WiFi.channel(i)) + ";" + String(WiFi.encryptionType(i)) + "|";
        delay(10);
      }
    }
    request->send(200, "text/text", req); });

  // Route to save new config of wifi
  server.on("/network/save", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    String __mode = "0";
    String __dhcp = "1";
    String __ip = "192.168.70.17";
    String __gate = "192.168.70.1";
    String __sub = "255.255.255.0";
    String __dns1 = "";
    String __dns2 = "";

    if (request->hasParam("lm", true))
      __mode = request->getParam("lm", true)->value();

    if (request->hasParam("ip", true))
      __ip = request->getParam("ip", true)->value();

    if (request->hasParam("gate", true))
      __gate = request->getParam("gate", true)->value();

    if (request->hasParam("sub", true))
      __sub = request->getParam("sub", true)->value();

    if (request->hasParam("dns01", true))
      __dns1 = request->getParam("dns01", true)->value();

    if (request->hasParam("dns02", true))
      __dns2 = request->getParam("dns02", true)->value();

    if (request->hasParam("ip_m", true))
      __dhcp = request->getParam("ip_m", true)->value();

    strcpy(IP, __ip.c_str());
    strcpy(GATEWAY, __gate.c_str());
    strcpy(SUBNET, __sub.c_str());
    strcpy(DNS01, __dns1.c_str());
    strcpy(DNS02, __dns2.c_str());
    strcpy(AP, __mode.c_str());
    strcpy(DHCP, __dhcp.c_str());

    Serial.println(IP);
    Serial.println(GATEWAY);
    Serial.println(SUBNET);
    Serial.println(DNS01);
    Serial.println(DNS02);
    Serial.println(DHCP);
    Serial.println(AP);

    change_config();
    request->redirect("/"); });

  // Route of network page
  server.on("/network", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/network.html", "text/html"); });

  // Route to save new config of wifi
  server.on("/cloud/save", HTTP_POST, [](AsyncWebServerRequest *request)
            {
    String __trance = "0";
    String __server = "";
    String __port = "8000";
    String __user = "";
    String __passwd = "";
    String __topic = "";
    String __auth = "";

    if (request->hasParam("proto", true))
      __trance = request->getParam("proto", true)->value();

    if (request->hasParam("server", true))
      __server = request->getParam("server", true)->value();

    if (request->hasParam("port", true))
      __port = request->getParam("port", true)->value();
    else
    {
      __port = strcmp(__trance.c_str(), "1") == 0 ? "80" : "1883";
    }

    if (request->hasParam("user", true))
      __user = request->getParam("user", true)->value();

    if (request->hasParam("passwd", true))
      __passwd = request->getParam("passwd", true)->value();

    if (request->hasParam("topic", true))
      __topic = request->getParam("topic", true)->value();

    if (request->hasParam("auth", true))
      __auth = request->getParam("auth", true)->value();

    strcpy(STANS_TYPE, __trance.c_str());
    strcpy(SERVER, __server.c_str());
    strcpy(PORT, __port.c_str());
    strcpy(USER_S, __user.c_str());
    strcpy(PASSWD_S, __passwd.c_str());
    strcpy(TOPIC, __topic.c_str());
    strcpy(AUTH, __auth.c_str());

    change_config();
    request->redirect("/"); });

  // Route of network page
  server.on("/cloud", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(SPIFFS, "/cloud.html", "text/html"); });

  // Start server
  server.begin();
}
bool flag = false;
bool btn_state = false;
u_long last;

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  if (data)
  {
    data = false;
    Print_Screen(buf);
  }
  client.loop();
}

bool load_config()
{
  int index = 0;
  File file_r = SPIFFS.open("/config.txt", "r+");
  if (!file_r)
    return false;

  while (file_r.available())
  {
    String tmp = file_r.readStringUntil('\n');
    tmp.toCharArray(config[index], tmp.length());
    index++;
  }

  if (IS_AP || !IS_DHCP)
  {
    local_ip.fromString(IP);
    gateway.fromString(GATEWAY);
    subnet.fromString(SUBNET);
    Serial.println("static ip address ");
  }
  file_r.close();
  return true;
}

bool change_config()
{
  File file_r = SPIFFS.open("/config.txt", "w");
  if (!file_r)
  {
    return false;
  }
  for (uint8_t i = 0; i < 26; i++)
  {
    file_r.println(config[i]);
  }
  file_r.close();
  return true;
}

String processor(const String &var)
{
  if (var == "l_m")
  {
    return IS_AP ? "AP Mode" : "Client Mode";
  }
  else if (var == "ip")
  {
    return IS_AP ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  }
  else if (var == "mac")
  {
    return String(WiFi.macAddress());
  }
  else if (var == "sub")
  {
    return String(IS_AP ? WiFi.softAPSubnetCIDR() : WiFi.subnetCIDR());
  }
  else if (var == "gat")
  {
    return WiFi.gatewayIP().toString();
  }
  else if (var == "dhcp")
  {
    return IS_DHCP ? "checked" : "";
  }
  else if (var == "static")
  {
    return !IS_DHCP ? "checked" : "";
  }
  else if (var == "rssi")
  {
    return IS_AP ? " -- " : String(2 * (WiFi.RSSI() + 100));
  }
  else if (var == "bssid")
  {
    return IS_AP ? " -- " : WiFi.BSSIDstr();
  }
  else
  {
    return config[atoi(var.c_str())];
  }

  return String();
}

void Print_Screen(char SCREEN[32][64])
{
  for (int y = 0; y < 8; y++)
  {
    Serial2.write('#');
    Serial2.write(y);
    Serial.write('#');
    Serial.write(y);
    for (int x = 0; x < 64; x++)
    {
      // Serial.write((SCREEN[line[y]][x] & 0b00001111) | ((SCREEN[line[y] + 8][x] & 0b00001111) << 4));
      // Serial.printf("0x%.2X ", (SCREEN[line[y]][x] & 0b00001111) | ((SCREEN[line[y] + 8][x] & 0b00001111) << 4));
      Serial2.write((SCREEN[line[y]][x] & 0b00001111) | ((SCREEN[line[y] + 8][x] & 0b00001111) << 4));
    }

    Serial.println("ButtomDisplay");

    for (int x = 0; x < 64; x++)
    {
      // Serial.write((SCREEN[line[y]][x] & 0b00001111) | ((SCREEN[line[y] + 8][x] & 0b00001111) << 4));
      // Serial.printf("0x%.2X ", (SCREEN[line[y + 8]][x] & 0b00001111) | ((SCREEN[line[y + 8] + 8][x] & 0b00001111) << 4));
      Serial2.write((SCREEN[line[y + 8]][x] & 0b00001111) | ((SCREEN[line[y + 8] + 8][x] & 0b00001111) << 4));
    }
    Serial.println();
  }
}
void setup_wifi()
{
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(_SSID);

  WiFi.begin(_SSID, _PASSWD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
void callback(char *topic, byte *message, unsigned int length)
{
  Serial.println("[MQTT] msg received");
  if ((char)message[0] == 0x23)
  {
    Serial.println("msg received with #");
    u8_t index = 0;
    for (u8_t i = 0; i < 16; i++)
    {
      Serial.println("#");

      for (u8_t j = 2; j < 66; j++)
      {
        buf[i][j - 2] = message[(index * 66) + j];
        Serial.printf("0x%.2X ", buf[i][j - 2]);
      }
      index += 1;
    }
    Serial.println();

    for (u8_t i = 0; i < 16; i++)
    {
      for (u8_t j = 2; j < 66; j++)
      {
        buf[i][64 + j - 2] = message[(index * 66) + j];
        Serial.printf("0x%.2X ", buf[i][64 + j - 2]);
      }
      Serial.println();

      index += 1;
    }
    data = true;
  }
}

void reconnect()
{
  // Loop until we're reconnected

  String Macesp = WiFi.macAddress();
  Macesp.replace(":", "");

  while (!client.connected())
  {
    client.setServer(SERVER, 1883);

    if (client.connect(Macesp.c_str()))
    {
      client.setCallback(callback);

      Serial.println("connected to MQTT");

      if (client.subscribe(TOPIC))
      {
        Serial.println("Subscribed successfully !");
      }
    }
    else
    {
      Serial.print("MQTT conenction failed, rc=");
      Serial.print(client.state());
      Serial.print(" try again in 3 seconds (");
      Serial.println("SERVER:" + String(SERVER) + ")");

      delay(3000);
    }
  }

  client.loop();
}
