/**
 * @file sensor_transmitter.ino
 * @brief Lectura de sensores ambientales y transmisión mediante ESP-NOW.
 *
 * Este programa recoge datos de temperatura, humedad, luminosidad, CO2 y humedad del suelo,
 * los empaqueta en una estructura y los envía a un receptor definido mediante ESP-NOW.
 */

#include <esp_now.h>
#include <WiFi.h>
#include <DHT.h>

/// Pin digital al que está conectado el sensor DHT.
#define DHTPIN 4

/// Tipo de sensor DHT (DHT11 en este caso).
#define DHTTYPE DHT11

/// Pin analógico conectado al LDR.
#define LDR_PIN 34

/// Objeto para lectura del sensor DHT.
DHT dht(DHTPIN, DHTTYPE);

/// Dirección MAC del receptor ESP-NOW.
uint8_t broadcastAddress[] = {0xC8, 0xF0, 0x9E, 0x7B, 0x78, 0x88};

// Variables para almacenamiento de lecturas de sensores
float temperature;         ///< Temperatura en grados Celsius
float humidity;            ///< Humedad relativa en porcentaje
uint16_t luminosity;       ///< Luminosidad en valor ADC
float CO2;                 ///< Concentración estimada de CO2 en ppm
float valHumsuelo;         ///< Humedad del suelo en porcentaje

// Variables y constantes para sensor de CO2 (MQ-135)
const int sensorPin = 35;     ///< Pin del sensor MQ
const int humsuelo = 33;      ///< Pin del sensor de humedad del suelo
const float RL = 10000.0;     ///< Resistencia de carga en ohmios
const float R0 = 10000.0;     ///< Resistencia base (calibrada)
int adcValue = 0;
float voltage = 0;
float Rs = 0;
float ratio = 0;

/// Resultado del envío (éxito o fallo).
String success;

/**
 * @struct struct_message
 * @brief Estructura de datos enviada vía ESP-NOW.
 */
typedef struct struct_message {
  float temp;        ///< Temperatura
  float hum;         ///< Humedad
  uint16_t lum;      ///< Luminosidad
  float vCO2;        ///< CO2 estimado
  float humSuelo;    ///< Humedad del suelo
} struct_message;

/// Instancia de la estructura de datos a enviar.
struct_message readingsToSend;

/// Información del peer ESP-NOW.
esp_now_peer_info_t peerInfo;

/**
 * @brief Callback al enviar datos por ESP-NOW.
 * 
 * @param mac_addr Dirección MAC del receptor.
 * @param status Estado del envío (éxito o fallo).
 */
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nEstado del envío:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Éxito" : "Fallo");
  success = (status == ESP_NOW_SEND_SUCCESS) ? "Éxito :)" : "Fallo :(";
}

/**
 * @brief Función de configuración. Inicializa sensores, ESP-NOW y el peer receptor.
 */
void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(LDR_PIN, INPUT);
  pinMode(humsuelo, INPUT);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error inicializando ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Fallo al agregar peer");
    return;
  }
}

/**
 * @brief Función principal de bucle. Lee sensores, calcula valores, envía por ESP-NOW y muestra resultados por consola.
 */
void loop() {
  // Leer sensores
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  luminosity = analogRead(LDR_PIN);
  valHumsuelo = map(analogRead(humsuelo), 4092, 0, 0, 100);
  adcValue = analogRead(sensorPin);
  voltage = adcValue * (3.3 / 4095.0);
  Rs = RL * (3.3 - voltage) / voltage;
  ratio = Rs / R0;
  CO2 = pow(10, (-2.769 * log10(ratio) + 2.691));

  // Validación de lecturas
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Error al leer el sensor DHT");
    return;
  }

  // Asignación de datos
  readingsToSend.temp = temperature;
  readingsToSend.hum = humidity;
  readingsToSend.lum = luminosity;
  readingsToSend.vCO2 = CO2;
  readingsToSend.humSuelo = valHumsuelo;

  // Enviar datos
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &readingsToSend, sizeof(readingsToSend));
  if (result == ESP_OK) {
    Serial.println("Datos enviados exitosamente");
  } else {
    Serial.println("Error al enviar los datos");
  }

  // Mostrar por consola
  Serial.println("LECTURAS ENVIADAS:");
  Serial.print("Temperatura: ");
  Serial.print(temperature);
  Serial.println(" ºC");

  Serial.print("Humedad: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Luminosidad: ");
  Serial.print(luminosity);
  Serial.println(" (valor ADC)");

  Serial.print("PPM CO2 estimado: ");
  Serial.print(CO2);
  Serial.println("ppm");

  Serial.print("Humedad del suelo: ");
  Serial.print(valHumsuelo);
  Serial.println(" %");

  delay(1000); // Espera 1 segundo
}
