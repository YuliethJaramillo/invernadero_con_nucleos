/**
 * @file invernadero_control.ino
 * @brief Sistema de monitoreo y control ambiental para invernadero usando ESP-NOW, RTC y Telegram.
 * 
 * Este código permite:
 * - Recibir datos de sensores remotos mediante ESP-NOW.
 * - Evaluar condiciones para activar actuadores (ventilador, bomba, etc).
 * - Enviar alertas por Telegram si se superan ciertos umbrales.
 * - Guardar registros con fecha y hora utilizando un módulo RTC.
 * 
 * Compatible con ESP32 y ESP8266.
 */

 // Librerías para comunicación ESP-NOW, I2C y WiFi
#include "StateMachineLib.h"
#include "AsyncTaskLib.h"
#include <esp_now.h>
#include "esp_wifi.h"
#include <WiFi.h>
#include <Wire.h>
#include <RTClib.h>

RTC_DS3231 rtc;  // Asegúrate de haber inicializado tu RTC en el setup()


/**
 * @brief Uso de core 1 (o 0 si es un núcleo).
 */
// Inclusión condicional para ESP8266 o ESP32
#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

// Use only core 1 for demo purposes
#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

// Librerías para conexión segura HTTPS y bot de Telegram
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// --- Configuración de WiFi ---
const char* ssid = "M55yeisy";
const char* password = "alomia26";

// --- Credenciales del bot de Telegram ---
#define BOTtoken "7041403052:AAGQKjcVL78QBhM8YHTvOE2NN8V8pXs9DN8"
#define CHAT_ID "8010625386"  // ID del chat donde se enviarán mensajes

#ifdef ESP8266
  // Certificado raíz para validar conexión segura en ESP8266
  X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif

// Cliente seguro y bot de Telegram
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

//----------FUNCIONES LOGICAS PARA WIFI Y ESP-NOW--------------
//variables a actualizar 
float temp = 0;
float hum = 0;
int lum = 0;
float CO2 = 0;
float valHumsuelo = 0;

//----------ESP-NOW--------------------------------------------
uint8_t macSensores[6] = {0xE0, 0x5A, 0x1B, 0x95, 0x25, 0xD4};
uint8_t macActuadores[6]  = {0x88, 0x13, 0xBF, 0x07, 0xF7, 0xC0}; //88:13:bf:07:f7:c0
//uint8_t macLum[6]  = {0xC8, 0xF0, 0x9E, 0x7B, 0x78, 0x90};

//estructura de datos para enviar
// Estado del envío
String success;

/**
 * @brief Estructura de datos recibidos desde sensores.
 */
typedef struct struct_message{
  float temperatura;
  float humedad;
  uint16_t luminosidad;  // entero
  float vCO2;
  float humedadSuelo;
} struct_message;
struct_message incomingReadings;

/**
 * @brief Estructura de datos enviados a los actuadores.
 */
typedef struct struct_message2 {
  bool eVentilador;
  bool eBomba;
  bool eLed;
  bool eAlarma;
  bool eCalor;
} struct_message2;
struct_message2 readingsToSend;

esp_now_peer_info_t peerInfo;    // Info del peer para emparejamiento

char macStr[18];  // Para mostrar la MAC como texto

// --- Agrega un peer (dispositivo remoto) a la red ESP-NOW ---
/**
 * @brief Agrega un dispositivo remoto a la red ESP-NOW.
 * @param mac Dirección MAC del peer.
 */
void addPeer(uint8_t *mac) {
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Fallo al agregar peer");
    } else {
      Serial.println("Peer agregado con éxito");
      snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      Serial.println(macStr);
    }
}

/**
 * @brief Callback al recibir datos vía ESP-NOW.
 * @param info Información del remitente.
 * @param incomingData Datos recibidos.
 * @param len Longitud de los datos.
 */
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));

  // Imprime la MAC del remitente
  //Serial.print("Datos recibidos de MAC: ");
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           info->src_addr[0], info->src_addr[1], info->src_addr[2],
           info->src_addr[3], info->src_addr[4], info->src_addr[5]);
  //Serial.println(macStr);
  // Identifica el sensor según la MAC y actualiza variable correspondiente
  if (memcmp(info->src_addr, macSensores, 6) == 0) {
    Serial.print("Temperatura: ");
    Serial.println(incomingReadings.temperatura);
    temp = incomingReadings.temperatura;
    Serial.print("Humedad: ");
    Serial.println(incomingReadings.humedad);
    hum = incomingReadings.humedad;
    Serial.print("Luz: ");
    Serial.println(incomingReadings.luminosidad);
    lum = incomingReadings.luminosidad;
    Serial.print("CO2: ");
    Serial.println(incomingReadings.vCO2);
    CO2 = incomingReadings.vCO2;
    Serial.print("Humedad Suelo: ");
    Serial.println(incomingReadings.humedadSuelo);
    valHumsuelo = incomingReadings.humedadSuelo;}
   //else if (memcmp(info->src_addr, macHum, 6) == 0) {
   //else if (memcmp(info->src_addr, macLum, 6) == 0) {
   else {
    Serial.println("MAC desconocida");
  }}

/**
 * @brief Obtiene la fecha y hora actual del RTC.
 * @return Fecha y hora en formato "dd/mm/yyyy hh:mm:ss".
 */
String obtenerFechaHora() {
  DateTime now = rtc.now();
  char fechaHora[25];
  snprintf(fechaHora, sizeof(fechaHora), "%02d/%02d/%04d %02d:%02d:%02d",
           now.day(), now.month(), now.year(),
           now.hour(), now.minute(), now.second());
  return String(fechaHora);
}

/**
 * @brief Formatea la lectura de sensores en formato JSON.
 * @param temperaturaf Temperatura.
 * @param humedadf Humedad relativa.
 * @param luminosidadf Luminosidad.
 * @param CO2f Concentración de CO2.
 * @param humedadSuelof Humedad del suelo.
 * @return Lectura formateada en JSON.
 */
String formatearLecturaSensores(float temperaturaf, float humedadf, uint16_t luminosidadf, float CO2f, float humedadSuelof) {
  String fecha_hora = obtenerFechaHora();

  char buffer[256];
  snprintf(buffer, sizeof(buffer),
           "{ \"fecha_hora\": \"%s\", \"temperatura\": %.2f, \"humedad\": %.2f, \"luminosidad\": %u, \"CO2\": %.2f, \"humedad_suelo\": %.2f }",
           fecha_hora.c_str(), temperaturaf, humedadf, luminosidadf, CO2f, humedadSuelof);

  return String(buffer);
}

/**
 * @brief Guarda la lectura actual formateada en memoria (SD o futura implementación).
 */
void guardarEnMemoria(){
  String lectura = formatearLecturaSensores(temp, hum, lum, CO2, valHumsuelo); 
}

/**
 * @brief Evalúa condiciones para activar actuadores y actualiza la estructura de envío.
 */
void variablesEnvio(){
  Serial.println("condicones para enviar");
  if (temp > 28 || hum > 60 || CO2 > 1800) {
    readingsToSend.eVentilador = true;
  } else {readingsToSend.eVentilador = false;}
  if (temp < 18){
    readingsToSend.eCalor = true;
  }else {readingsToSend.eCalor = false;}
  if (lum > 3500){
    readingsToSend.eAlarma = true;
  } else {readingsToSend.eAlarma = false;}
  if (lum < 2500){
    readingsToSend.eLed = true;
  } else {readingsToSend.eLed = false;}
  if (valHumsuelo < 60){
    readingsToSend.eBomba = true;
  } else {readingsToSend.eBomba = false;}
}
// Callback de envío

/**
 * @brief Callback al enviar datos a través de ESP-NOW.
 * @param macActuadores Dirección MAC del destinatario.
 * @param status Estado del envío.
 */
void OnDataSent(const uint8_t *macActuadores, esp_now_send_status_t status) {
  Serial.println("enviooooooooooo");
  Serial.print("\r\nEstado del envío:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Éxito" : "Fallo");
  success = (status == ESP_NOW_SEND_SUCCESS) ? "Éxito :)" : "Fallo :(";
}
//------------FUNCIONES DE TELEGRAM---------------------------
// --- Generación de alarma cuando se superan límites ---
/**
 * @brief Envía alerta a Telegram si se superan límites de variables.
 */
void generacionAlarma() {
  Serial.println("inicio");
   if (temp > 28||temp < 18 || hum > 60 || lum > 3500||lum < 2500 || CO2 >= 1800 || valHumsuelo < 60) {
    Serial.println("inicio condicional");
    //DateTime now = rtc.now();  // Obtiene fecha y hora actual
    //Serial.println("despues de rtc");

    //char fechaHora[25];
    //snprintf(fechaHora, sizeof(fechaHora), "%02d/%02d/%04d %02d:%02d:%02d",
      //       now.day(), now.month(), now.year(),
        //     now.hour(), now.minute(), now.second());

    char msg[520];  // Asegúrate de que el tamaño sea suficiente
    snprintf(msg, sizeof(msg),
             "‼️ ¡¡LÍMITE DE VARIABLES SUPERADO!!\n"
             "#INVERNADERO\n"
             //"🕒 Fecha y hora: %s\n"
             "🌡 Temp: %.2f °C\n"
             "💧 Humedad: %.2f %%\n"
             "☀️ Luz: %u\n"
             "🌫 CO₂: %.2f PPM\n"
             "#FIN",
         temp, hum, lum, CO2);

    Serial.println("despues del formateo de datos");
    Serial.println(msg);
    bot.sendMessage(CHAT_ID, msg, "");
    Serial.println("despues de enviar el mensaje a telegram");
  }
}

// Funciones para guardar datos en la memoria SD
// RTC
RTC_DS3231 rtc;

// Estructura de datos del sensor a guardar
/**
 * @struct SensorData
 * @brief Estructura de datos que se podrían guardar en la tarjeta SD.
 */
struct SensorData {
  float Stemperatura;
  float Shumedad;
  uint16_t Sluminosidad;
  float SvCO2;
  float ShumedadSuelo;
};

// Pines SPI (ajusta si usas otros)
#define SD_CS 5

/**
 * @brief Inicializa la tarjeta SD.
 * @return true si tuvo éxito, false en caso contrario.
 */
bool initSD() {
    if (!SD.begin(SD_CS)) {
        Serial.println("❌ Falló la inicialización de la tarjeta SD");
        return false;
    }
    Serial.println("✅ Tarjeta SD inicializada correctamente");
    return true;
}

/**
 * @brief Inicializa el reloj RTC.
 * @return true si tuvo éxito, false en caso contrario.
 */
bool initRTC() {
    if (!rtc.begin()) {
        Serial.println("❌ No se pudo iniciar el RTC");
        return false;
    }
    if (rtc.lostPower()) {
        Serial.println("⚠️ RTC sin hora válida, se establece con hora de compilación");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    return true;
}

String getTimestampFromRTC() {
    DateTime now = rtc.now();
    char buf[20];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());
    return String(buf);
}

String getFolderPath(const String& timestamp) {
    String date = timestamp.substring(0, 10);    // YYYY-MM-DD
    String hour = timestamp.substring(11, 13);   // HH
    return "/" + date + "/" + hour;
}

String getFilePath(const String& timestamp) {
    return getFolderPath(timestamp) + "/data.csv";
}

void ensureDirectoriesExist(const String& folderPath) {
    String current = "";
    for (int i = 1; i < folderPath.length(); ++i) {
        if (folderPath[i] == '/') {
            current = folderPath.substring(0, i);
            if (!SD.exists(current)) {
                SD.mkdir(current);
            }
        }
    }
    if (!SD.exists(folderPath)) {
        SD.mkdir(folderPath);
    }
}

bool logSensorData(const String& timestamp, const SensorData& data) {
    String folderPath = getFolderPath(timestamp);
    String filePath = getFilePath(timestamp);

    ensureDirectoriesExist(folderPath);

    File file = SD.open(filePath, FILE_APPEND);
    if (!file) {
        Serial.println("❌ No se pudo abrir el archivo para escritura.");
        return false;
    }

    // Escribir encabezado si el archivo está vacío
    if (file.size() == 0) {
        file.println("timestamp,nodeId,rssi,temp,hum,light,co2ppm,soilMoisture");
    }

    // Construcción de la línea CSV
    String csvLine = timestamp + "," + nodeId + "," + String(rssi) + "," +
                     String(data.Stemperatura, 2) + "," +
                     String(data.Shumedad, 2) + "," +
                     String(data.Sluminosidad) + "," +
                     String(data.SvCO2, 2) + "," +
                     String(data.ShumedadSuelo, 2);

    if (file.println(csvLine) == 0) {
        Serial.println("❌ Error al escribir en el archivo.");
        file.close();
        return false;
    }

    file.close();
    return true;
}

//----------------Tareas para conmutar entre esp now y wifi
// Bandera para controlar el modo actual de la conmutacion
volatile bool useWiFi = false;
volatile bool adicion_peers = false;

// variables de tiempo
static const int espera_conexion_wifi = 200;
static const int limpiar_hardware = 200;
static const int tiempo_taskESPNow = 1000;
static const int tiempo_envio_datos = 200;
static const int tiempo_taskWiFi = 1000;

//Tareas
TaskHandle_t ESPNowTask = NULL;
TaskHandle_t WiFiTask = NULL;


void switchToWiFi() {
  if(temp > 28||temp < 18 || hum > 60 || lum > 3500||lum < 2500 || CO2 >= 1800 || valHumsuelo < 60){
  esp_now_deinit();
  WiFi.disconnect(true);
  adicion_peers = false;
  vTaskDelay(limpiar_hardware/portTICK_PERIOD_MS);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
   Serial.println("conectando...");
   vTaskDelay(espera_conexion_wifi/portTICK_PERIOD_MS);
  }}
  else{useWiFi = false;}
}

void switchToESPNow() {
  WiFi.disconnect(true);
  vTaskDelay(limpiar_hardware/portTICK_PERIOD_MS);
  WiFi.mode(WIFI_STA); // Modo obligatorio para ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error inicializando ESP-NOW");
  }
  else{Serial.println("inicio correctamente");}
}
 
// Tarea para manejar ESP-NOW
void taskESPNow(void *parameter) {
  while (1) {
    if (!useWiFi) {
      // Ejecutar tareas ESP-NOW
      //esp_now_register_recv_cb(OnDataRecv);
      vTaskDelay(200 / portTICK_PERIOD_MS);  // Espera datos
      
      if (adicion_peers == false){
      addPeer(macSensores);
      addPeer(macActuadores);
      //addPeer(macLum);
      esp_now_register_recv_cb(OnDataRecv);

      //guaradar variables medidas en memorias cada 1 seg en subcarpetas por hora, subcarpetas generadas por dia
      SensorData data = {temp, hum, lum, CO2, valHumsuelo};
      String timestamp = getTimestampFromRTC();
      logSensorData(timestamp, "NODE1", -60, data, 1);
      variablesEnvio();
      Serial.println("despues de funcion envio");
      esp_now_register_send_cb(OnDataSent);
      //Enviar datos
      esp_err_t result = esp_now_send(macActuadores, (uint8_t *) &readingsToSend, sizeof(readingsToSend));
      if (result == ESP_OK) {
        Serial.println("Datos enviados exitosamente");
      } else {
        Serial.println("Error al enviar los datos");
      }
      adicion_peers = true;
      }
    
      vTaskDelay(500 / portTICK_PERIOD_MS);
      // Cambiar a WiFi
      useWiFi = true;
      switchToWiFi();
    }
    vTaskDelay(tiempo_taskESPNow / portTICK_PERIOD_MS);
  }
}

// Tarea para manejar WiFi
void taskWiFi(void *parameter) {
  while (1) {
     if(useWiFi == true){
       if (WiFi.status() != WL_CONNECTED) {
       Serial.println("conectando...");
       switchToWiFi();
       }
       else
       {Serial.println("conectado");}

       while (WiFi.status() != WL_CONNECTED){
        Serial.println("conectando...");
       }
      generacionAlarma();

      // Cambiar a ESP-NOW
      useWiFi = false;
      switchToESPNow();
    }
    vTaskDelay(tiempo_taskWiFi/ portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
   Serial.begin(115200);
   Wire.begin();

    initRTC();
    initSD();
  WiFi.mode(WIFI_STA);
  esp_now_init();
  // Inicializa ESP-NOW
    if (esp_now_init() != ESP_OK) {
    Serial.println("Error inicializando ESP-NOW");
    return;}
 
  // Configuración del cliente seguro según ESP8266 o ESP32
  #ifdef ESP8266
    configTime(0, 0, "pool.ntp.org");
    client.setTrustAnchors(&cert);
  #endif
  #ifdef ESP32
    client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  #endif

  xTaskCreatePinnedToCore(taskESPNow, "ESPNowTask", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(taskWiFi, "WiFiTask", 8192, NULL, 1, NULL, 0);
}

void loop() {
}   

