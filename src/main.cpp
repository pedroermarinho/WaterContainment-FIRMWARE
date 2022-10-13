// Bibliotecas
#include <HTTPClient.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Wire.h>

#define SENSOR 27
#define LED_ALARME 2
#define LED_MQTT 4

WiFiManager wifiManager; // Objeto de manipulação do wi-fi
WiFiClient wifiClient;

PubSubClient mqttClient(wifiClient);

long currentMillis = 0;
long previousMillis = 0;
int interval = 1000;
float calibrationFactor = 4.5;
volatile byte pulseCount;
byte pulse1Sec = 0;
float flowRate;

void IRAM_ATTR pulseCounter()
{
    pulseCount++;
}

void configModeCallback(WiFiManager *myWiFiManager);
void saveConfigCallback();
void mqttCallback(char *topic, byte *payload, unsigned int length);
void reconnect();
String getJson(float value);
void onAlarm();

void setup()
{

    Serial.begin(115200);
    Serial.println("...");

    pinMode(LED_ALARME, OUTPUT);
    pinMode(LED_MQTT, OUTPUT);

    pinMode(SENSOR, INPUT_PULLUP);

    pulseCount = 0;
    flowRate = 0.0;
    previousMillis = 0;

    attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);

    if (WiFi.status() != WL_CONNECTED)
    {
        if (!wifiManager.autoConnect("WATER-CONTAINMENT-CONFIG", "12345678")) // Função para se autoconectar na rede
        {
            Serial.println("Falha ao conectar"); // Se caso não conectar na rede mostra mensagem de falha
        }
        else
        {
            Serial.println("Conectado na Rede!!!");
        }
    }
    else
    {
        Serial.println("Conectado na Rede!!!");
    }

    // callback para quando entra em modo de configuração AP
    wifiManager.setAPCallback(configModeCallback);
    // callback para quando se conecta em uma rede, ou seja, quando passa a trabalhar em modo estação
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    mqttClient.setServer("192.168.0.55", 1883);
    mqttClient.setCallback(mqttCallback);

    Wire.begin();
}

int cont = 0;

void loop()
{
    currentMillis = millis();
    if (currentMillis - previousMillis > interval)
    {
        if (!mqttClient.connected())
        {
            reconnect();
        }

        pulse1Sec = pulseCount;
        pulseCount = 0;

        flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) / calibrationFactor;
        previousMillis = millis();

        // Print the flow rate for this second in litres / minute
        Serial.print("Flow rate: ");
        Serial.print(int(flowRate)); // Print the integer part of the variable
        Serial.print("L/min");
        Serial.print("\n"); // Print tab space

        mqttClient.loop();

        if (int(flowRate) > 0)
        {
            mqttClient.publish("SENSORES", getJson(flowRate).c_str());
        }

        Serial.println(mqttClient.state());
        Serial.println(mqttClient.connected());
    }
    // delay(3000);
}

// callback que indica que o ESP entrou no modo AP
void configModeCallback(WiFiManager *myWiFiManager)
{
    Serial.println("Entrou no modo de configuração");

    Serial.println(WiFi.softAPIP());                      // imprime o IP do AP
    Serial.println(myWiFiManager->getConfigPortalSSID()); // imprime o SSID criado da rede
}

// Callback que indica que salvamos uma nova rede para se conectar (modo estação)
void saveConfigCallback()
{
    Serial.println("Configuração salva");
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived on topic: ");
    Serial.print(topic);
    Serial.print(". Message: ");
    String messageTemp;

    for (int i = 0; i < length; i++)
    {
        messageTemp += (char)payload[i];
    }

    if (String(topic) == "ALARMES")
    {
        Serial.print("Changing output to ");
        if (messageTemp == "on")
        {
            Serial.println("on");
            onAlarm();
        }
    }

    Serial.println(messageTemp);
}

void reconnect()
{
    // Loop until we're reconnected
    while (!mqttClient.connected())
    {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (mqttClient.connect("teste", "watercontainment", "watercontainment123"))
        {
            Serial.println("connected");
            digitalWrite(LED_MQTT, LOW);
            // Subscribe
            mqttClient.subscribe("ALARMES");
        }
        else
        {
            digitalWrite(LED_MQTT, HIGH);
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

String getJson(float value)
{
    String json = "{";
    json += "\"litrosMin\":\"" + String(value) + "\"";
    json += "}";
    return json;
}

void onAlarm()
{
    digitalWrite(LED_ALARME, HIGH);
    delay(5000);
    digitalWrite(LED_ALARME, LOW);
}
