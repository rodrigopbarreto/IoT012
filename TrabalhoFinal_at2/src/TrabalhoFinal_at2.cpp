/*
 * Trabalho Final - IT012 - Atividade 2 - Rodrigo Barreto
 *
 * Estacao de monitoramento meteorologic0 - Temperatura e pressao
 * Criacao de pagina de configuracao HTML
 * Visualização das medidas em display OLED e dashboard do Thingspeak
 *
 * 
 */

/*******************************************************************************
    Inclusões
*******************************************************************************/



#include <Arduino.h>
#include "DHT.h"
#include "esp_wifi.h"
#include <WiFi.h>


#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}
#include <ArduinoJson.h>
#include <AsyncMqttClient.h>



#include <Fonts/FreeSerif9pt7b.h>

#include "ThingSpeak.h"

#include "LittleFS.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>


/*******************************************************************************
    Definições de constantes e variáveis globais
*******************************************************************************/
const char *WIFI_SSID = "it012-AP";     // Rede wifi AP para conexao inicial
const char *WIFI_PASSWORD = "password";  // Senha rede AP
WiFiClient client;

// Cria objeto do Webserver na porta 80 (padrão HTTP)
AsyncWebServer server(80);

// Variáveis String para armazenar valores da página HTML de configuracao
String g_ssid;
String g_password;
String g_thingspeak_chan;
String g_ts_api_write;
String g_device_name;

// Caminhos dos arquivos para salvar os valores das credenciais
const char *g_ssidPath = "/ssid.txt";
const char *g_passwordPath = "/password.txt";
const char *g_thingspeak_chanPath = "/ts_channel.txt";
const char *g_ts_api_writePath = "/ts_api_write.txt";
const char *g_device_namePath = "/device_name.txt";

// Temporização - intervalo de espera por conexão Wifi
unsigned long g_previousMillis = 0;
const long g_interval = 30000;

// Sensor DHT11
#define DHT_READ (15) // pino de leitura do sensor
#define DHT_TYPE DHT11 // tipo de sensor utilizado pela lib DHT
DHT dht(DHT_READ, DHT_TYPE); // Objeto de controle do DHT11
float g_temperature;
float g_humidity;
float g_pressure;

// Definicoes do display OLED
#define OLED_WIDTH (128) // largura do display OLED (pixels)
#define OLED_HEIGHT (64) // altura do display OLED (pixels)
#define OLED_ADDRESS (0x3C) // endereço I²C do display
static Adafruit_SSD1306 display // objeto de controle do SSD1306
    (OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

/*******************************************************************************
    Implementação: Funções auxiliares
*******************************************************************************/
void littlefsInit()
{
  if (!LittleFS.begin(true))
  {
    Serial.println("Erro ao montar o sistema de arquivos LittleFS");
    return;
  }
  Serial.println("Sistema de arquivos LittleFS montado com sucesso.");
}

// Lê arquivos com o LittleFS
String readFile(const char *path)
{
  Serial.printf("Lendo arquivo: %s\r\n", path);

  File file = LittleFS.open(path);
  if (!file || file.isDirectory())
  {
    Serial.printf("\r\nfalha ao abrir o arquivo... %s", path);
    return String();
  }

  String fileContent;
  while (file.available())
  {
    fileContent = file.readStringUntil('\n');
    break;
  }
  return fileContent;
}

// Escreve arquivos com o LittleFS
void writeFile(const char *path, const char *message)
{
  Serial.printf("Escrevendo arquivo: %s\r\n", path);

  File file = LittleFS.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.printf("\r\nfalha ao abrir o arquivo... %s", path);
    return;
  }
  if (file.print(message))
  {
    Serial.printf("\r\narquivo %s editado.", path);
  }
  else
  {
    Serial.printf("\r\nescrita no arquivo %s falhou... ", path);
  }
}

// Callbacks para requisições de recursos do servidor
void serverOnGetRoot(AsyncWebServerRequest *request)
{
  request->send(LittleFS, "/index.html", "text/html");
}

void serverOnGetStyle(AsyncWebServerRequest *request)
{
  request->send(LittleFS, "/style.css", "text/css");
}

void serverOnGetFavicon(AsyncWebServerRequest *request)
{
  request->send(LittleFS, "/favicon.png", "image/png");
}

void serverOnPost(AsyncWebServerRequest *request)
{
  int params = request->params();

  for (int i = 0; i < params; i++)
  {
    AsyncWebParameter *p = request->getParam(i);
    if (p->isPost())
    {
      if (p->name() == "ssid")
      {
        g_ssid = p->value().c_str();
        Serial.print("SSID definido como ");
        Serial.println(g_ssid);

        // Escreve WIFI_SSID no arquivo
        writeFile(g_ssidPath, g_ssid.c_str());
      }
      if (p->name() == "password")
      {
        g_password = p->value().c_str();
        Serial.print("Senha definida como ");
        Serial.println(g_password);

        // Escreve WIFI_PASSWORD no arquivo
        writeFile(g_passwordPath, g_password.c_str());
      }
      if (p->name() == "ts channel") {
                g_thingspeak_chan = p->value().c_str();
                Serial.print("Canal do ThingSpeak definido como: ");
                Serial.println(g_thingspeak_chan);

                // Escreve o canal do Thingspeak  no arquivo
                writeFile(g_thingspeak_chanPath, g_thingspeak_chan.c_str());
            }
      if (p->name() == "ts api write key") {
                g_ts_api_write = p->value().c_str();
                Serial.print("API write key do Thingspeak definida como: ");
                Serial.println(g_ts_api_write);

                // Escreve a write key do Thongspeak no arquivo
                writeFile(g_ts_api_writePath, g_ts_api_write.c_str());
            }
      if (p->name() == "device name") {
                g_device_name = p->value().c_str();
                Serial.print("Nome do dispositivo definodo como: ");
                Serial.println(g_device_name);

                // Escreve nome do device no arquivo
                writeFile(g_device_namePath, g_device_name.c_str());
            }       
    }
  }

  // Após escrever no arquivo, envia mensagem de texto simples ao browser
  request->send(200, "text/plain", "Finalizado - o ESP32 vai reiniciar e se conectar ao seu AP definido.");

  // Reinicia o ESP32
  delay(2000);
  ESP.restart();
}

// Inicializa a conexão Wifi
bool initWiFi()
{
  // Se o valor de g_ssid for não-nulo, uma rede Wifi foi provida pela página do
  // servidor. Se for, o ESP32 iniciará em modo AP.
  if (g_ssid == "")
  {
    Serial.println("SSID indefinido (ainda não foi escrito no arquivo, ou a leitura falhou).");
    return false;
  }

  // Se há um SSID e PASSWORD salvos, conecta-se à esta rede.
  WiFi.mode(WIFI_STA);
  WiFi.begin(g_ssid.c_str(), g_password.c_str());
  Serial.println("Conectando à Wifi...");

  unsigned long currentMillis = millis();
  g_previousMillis = currentMillis;

  while (WiFi.status() != WL_CONNECTED)
  {
    currentMillis = millis();
    if (currentMillis - g_previousMillis >= g_interval)
    {
      Serial.println("Falha em conectar.");
      return false;
    }
  }

  // Inicia o Thingspeak
  ThingSpeak.begin(client);
    Serial.println("ThingSpeak client iniciado com sucesso.");
  return true;

  // Exibe o endereço IP local obtido
  Serial.println(WiFi.localIP());


  }
// Rotina de leitura do sensor DHT11
esp_err_t sensorRead()
{
    g_temperature = dht.readTemperature();
    g_humidity = dht.readHumidity();
   
    // Verifica se alguma leitura falhou
    if (isnan(g_humidity) || isnan(g_temperature)) {
        Serial.printf("\r\n[sensorRead] Erro - leitura inválida...");
        return ESP_FAIL;
    } else {
        return ESP_OK;
    }
}
// Publica os dados no Thingspeak
void sensorPublish()
{
    int errorCode;
    ThingSpeak.setField(1, g_temperature);
    ThingSpeak.setField(2, g_humidity);
 
    errorCode = ThingSpeak.writeFields((long)g_thingspeak_chan.c_str(), g_ts_api_write.c_str());
    if (errorCode != 200) {
        Serial.println("Erro ao atualizar os canais - código HTTP: " + String(errorCode));
    } else {
        Serial.printf("\r\n[sensorPublish] Dados publicado no ThingSpeak. Canal: %lu ", g_thingspeak_chan.c_str());
    }
}

/*******************************************************************************
    Implementação: setup & loop
*******************************************************************************/
void setup()
{
  // Log inicial da placa
  Serial.begin(115200);
  Serial.print("\r\n --- Trabajo Final IT012 - At2 --- \n");

  // Inicia o sistema de arquivos
  littlefsInit();

  // Configura LED_BUILTIN (GPIO2) como pino de saída
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Carrega os valores lidos com o LittleFS
  g_ssid = readFile(g_ssidPath);
  g_password = readFile(g_passwordPath);
  g_thingspeak_chan = readFile(g_thingspeak_chanPath);
  g_ts_api_write = readFile(g_ts_api_writePath);
  g_device_name = readFile(g_device_namePath);
  Serial.println(g_ssid);
  Serial.println(g_password);
  Serial.println(g_thingspeak_chan);
  Serial.println(g_ts_api_write);
  Serial.println(g_device_name);

  // Inicializa o display
    display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
    display.setTextColor(WHITE);
    display.clearDisplay();
    display.display();

    // Inicializa o sensor DHT11
    dht.begin();

  if (!initWiFi())
  {
    // Seta o ESP32 para o modo AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Access Point criado com endereço IP ");
    Serial.println(WiFi.softAPIP());

    // Callbacks da página principal do servidor de provisioning
    server.on("/", HTTP_GET, serverOnGetRoot);
    server.on("/style.css", HTTP_GET, serverOnGetStyle);
    server.on("/favicon.png", HTTP_GET, serverOnGetFavicon);

    // Ao clicar no botão "Enviar" para enviar as credenciais, o servidor receberá uma
    // requisição do tipo POST, tratada a seguir
    server.on("/", HTTP_POST, serverOnPost);

    // Como ainda não há credenciais para acessar a rede wifi,
    // Inicia o Webserver em modo AP
    server.begin();

    // Limpa o display para mostrar os dados lidos
    display.clearDisplay();
  }
}

void loop()
{
    unsigned long currentMillis = millis();

    if (WiFi.status() == WL_CONNECTED && WiFi.getMode() == WIFI_MODE_STA) {
        // A cada "interval" ms, publica dados em tópicos adequados
        if (currentMillis - g_previousMillis >= g_interval) {

            g_previousMillis = currentMillis;
            // Lê dados do sensor e publica se a leitura não falhou
            if (sensorRead() == ESP_OK) {
                sensorPublish();
            }

            // Limpa a tela do display e mostra o nome do exemplo
            display.clearDisplay();

            // Mostra nome do dispositivo
            display.setCursor(0, 0);
            display.printf("%s", g_device_name.c_str());

            // Mostra Temperatura no display
            display.drawRoundRect(0, 8, 126, 16, 6, WHITE);
            display.setCursor(4, 12);
            display.printf("Temperatura: %0.1fC", dht.readTemperature());

            // Mostra Humidade no display
            display.drawRoundRect(0, 26, 126, 16, 6, WHITE);
            display.setCursor(4, 30);
            display.printf("Umidade: %0.1f%", dht.readHumidity());

            // Atualiza tela do display OLED
            display.display();
        }
    } else {
        if (currentMillis - g_previousMillis >= g_interval) {

            g_previousMillis = currentMillis;
        }
        digitalWrite(LED_BUILTIN, HIGH);
        delay(1000);
        digitalWrite(LED_BUILTIN, LOW);
        delay(1000);
    }

}