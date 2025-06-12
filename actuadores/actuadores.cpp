/**
 * @file sistema_control.ino
 * @brief Sistema de control basado en ESP32 usando ESP-NOW para activar dispositivos (ventilador, bomba, etc.).
 * @details Este código permite la recepción de comandos por ESP-NOW desde un nodo central y activa dispositivos en función de las señales recibidas.
 */

#include "StateMachineLib.h"
#include "AsyncTaskLib.h"
#include <esp_now.h>
#include "esp_wifi.h"
#include <WiFi.h>
#include <Wire.h>

#define RELAY_BOMBA 21
#define RELAY_VENTILADOR 22
#define LED_PIN 18
#define AIRE 17

int a = 13, b = 12, c = 14, d = 27, e = 26, f = 25, g = 33;
int pins[] = {a, b, c, d, e, f, g};

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

// Variables de estado
bool Ventilador = false;
bool Bomba = false;
bool Led = false;
bool Alarma = false;
bool Calor = false;

// Dirección MAC del emisor
uint8_t macNucleoC[6] = {0xC8, 0xF0, 0x9E, 0x7B, 0x78, 0x88};

/**
 * @brief Estructura para recibir estados de los dispositivos.
 */
typedef struct struct_message {
  bool eVentilador;
  bool eBomba;
  bool eLed;
  bool eAlarma;
  bool eCalor;
} struct_message;

struct_message incomingReadings;
esp_now_peer_info_t peerInfo;
char macStr[18];

/**
 * @brief Agrega un peer al sistema ESP-NOW.
 * @param mac Dirección MAC del dispositivo a emparejar.
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
 * @brief Callback al recibir datos por ESP-NOW.
 */
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));

  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           info->src_addr[0], info->src_addr[1], info->src_addr[2],
           info->src_addr[3], info->src_addr[4], info->src_addr[5]);

  if (memcmp(info->src_addr, macNucleoC, 6) == 0) {  
    Serial.print("Ventilador encendido: ");
    Serial.println(incomingReadings.eVentilador);
    Ventilador = incomingReadings.eVentilador;

    Serial.print("Bomba encendida: ");
    Serial.println(incomingReadings.eBomba);
    Bomba = incomingReadings.eBomba;

    Serial.print("Led encendido: ");
    Serial.println(incomingReadings.eLed);
    Led = incomingReadings.eLed;

    Serial.print("Alarma de fuego encendida: ");
    Serial.println(incomingReadings.eAlarma);
    Alarma = incomingReadings.eAlarma;

    Serial.print("Aire acondicionado encendido: ");
    Serial.println(incomingReadings.eCalor);
    Calor = incomingReadings.eCalor;
  } else {
    Serial.println("MAC desconocida");
  }
}

/**
 * @brief Enciende o apaga el ventilador.
 */
void encenderVentilador(){
  digitalWrite(RELAY_VENTILADOR, Ventilador ? LOW : HIGH); 
}

/**
 * @brief Enciende o apaga la bomba.
 */
void encenderBomba(){
  digitalWrite(RELAY_BOMBA, Bomba ? LOW : HIGH);
}

/**
 * @brief Enciende o apaga el LED.
 */
void encenderLed(){
  digitalWrite(LED_PIN, Led ? HIGH : LOW);
}

/**
 * @brief Enciende o apaga la alarma (representada con 7 segmentos).
 */
void encenderAlarma(){
  if (Alarma) {
    digitalWrite(a, HIGH);
    digitalWrite(b, LOW);
    digitalWrite(c, LOW);
    digitalWrite(d, LOW);
    digitalWrite(e, HIGH);
    digitalWrite(f, HIGH);
    digitalWrite(g, HIGH);
  } else {
    for (int i = 0; i < 7; i++) {
      digitalWrite(pins[i], LOW);
    }
  }
}

/**
 * @brief Enciende o apaga el aire acondicionado.
 */
void encenderAire(){
  digitalWrite(AIRE, Calor ? HIGH : LOW);
}

/**
 * @brief Tarea periódica para manejar la bomba.
 */
void tareaBomba(void *parameter) {
  while (true) {
    encenderBomba();
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Tarea periódica para manejar el ventilador.
 */
void tareaVentilador(void *parameter) {
  while (true) {
    encenderVentilador();
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Tarea periódica para manejar el LED.
 */
void tareaLed(void *parameter) {
  while (true) {
    encenderLed();
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Tarea periódica para manejar la alarma.
 */
void tareaAlarma(void *parameter) {
  while (true) {
    encenderAlarma();
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Tarea periódica para manejar el aire acondicionado.
 */
void tareaAire(void *parameter) {
  while (true) {
    encenderAire();
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

/**
 * @brief Configuración inicial del sistema.
 */
void setup(){
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error inicializando ESP-NOW");
    return;
  }

  addPeer(macNucleoC);
  esp_now_register_recv_cb(OnDataRecv);

  pinMode(RELAY_BOMBA, OUTPUT);
  pinMode(RELAY_VENTILADOR, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(AIRE, OUTPUT);
  
  for (int i = 0; i < 7; i++) {
    pinMode(pins[i], OUTPUT);
  }

  digitalWrite(RELAY_BOMBA, HIGH);
  digitalWrite(RELAY_VENTILADOR, HIGH);

  xTaskCreatePinnedToCore(tareaBomba, "Bomba", 1000, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(tareaVentilador, "Ventilador", 1000, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(tareaLed, "Led", 1000, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(tareaAlarma, "Alarma", 1000, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(tareaAire, "Aire", 1000, NULL, 1, NULL, 1);
}

/**
 * @brief Bucle principal (vacío, ya que todo se ejecuta con FreeRTOS).
 */
void loop(){}