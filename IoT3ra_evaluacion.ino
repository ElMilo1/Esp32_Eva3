#include <WiFi.h>
#include <FirebaseESP32.h>
#include <ESP32Servo.h>

// ====== CONFIGURACIÓN DE WIFI ======
const char* WIFI_SSID     = "iPhone de Milowan";
const char* WIFI_PASSWORD = "12345678";

// ====== CONFIGURACIÓN DE FIREBASE ======
#define FIREBASE_HOST "https://iot3-31836-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "hP3PXbOLV6l9q82YletCMfxAk3iAVm1XcFSosdwD"

FirebaseData fbdo;             // para lecturas/escrituras puntuales
FirebaseData streamEstado;     // stream estado_dispositivos
FirebaseData streamConfig;     // stream configuraciones
FirebaseAuth auth;
FirebaseConfig config;

// ====== LEDS FACHADA ======
const int LED_FACHADA[5] = {2, 4, 16, 17, 18};
bool lucesFachadaOn = false;

// ====== SERVO PORTÓN ======
Servo portonServo;
const int SERVO_PIN = 15;
bool portonAbierto = false;
float distanciaProgramada = 1.0; // se sobrescribe desde Firebase
const uint16_t TIEMPO_BASE_CICLO_MS = 2000;

// ====== SENSORES IR ======
const int SENSOR_LUZ_PIN   = 32;
const int SENSOR_PORTON_PIN = 33;
bool lucesSensorOn = false;
bool portonSensorOn = false;

// (edge detection para evitar spam de escrituras)
int lastLuzRead = LOW;
int lastPortonRead = LOW;

// ====== LEDS DE ESTADO ======
const int LED_AMARILLO = 26;
const int LED_VERDE    = 27;
const int LED_ROJO     = 14;
const int LED_AZUL     = 25;

// ====== UTILIDAD VISUAL ======
void pulseAzul(uint16_t ms = 150) {
  digitalWrite(LED_AZUL, HIGH);
  delay(ms);
  digitalWrite(LED_AZUL, LOW);
}

// ====== FUNCIONES ======
void aplicarFachada() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(LED_FACHADA[i], lucesFachadaOn ? HIGH : LOW);
  }
  Serial.printf("Fachada => %s\n", lucesFachadaOn ? "ENCENDIDA" : "APAGADA");
}

void moverPorton(bool abrir) {
  uint32_t tiempoMovimiento = (uint32_t)(TIEMPO_BASE_CICLO_MS * distanciaProgramada);
  portonServo.write(abrir ? 180 : 0);
  delay(tiempoMovimiento);
  portonServo.write(90);
  Serial.printf("Portón %s (%.2f ciclos)\n", abrir ? "ABIERTO" : "CERRADO", distanciaProgramada);
}

// ====== CALLBACKS DE STREAM ======
void callbackEstado(StreamData data) {
  pulseAzul();

  // El stream de /estado_dispositivos entrega cambios con rutas relativas
  // Ejemplos de dataPath(): "/luces_fachada_on", "/porton_abierto"
  String path = data.dataPath();
  String type = data.dataType();

  if (type == "boolean") {
    bool val = data.boolData();
    if (path == "/luces_fachada_on") {
      lucesFachadaOn = val;
      aplicarFachada();
    } else if (path == "/porton_abierto") {
      portonAbierto = val;
      moverPorton(portonAbierto);
    }
  } else if (type == "json") {
    // En caso de que venga un objeto completo (e.g., primera carga)
    FirebaseJson *json = data.jsonObjectPtr();
    FirebaseJsonData jData;

    if (json->get(jData, "luces_fachada_on") && jData.type == "bool") {
      lucesFachadaOn = jData.boolValue;
      aplicarFachada();
    }
    if (json->get(jData, "porton_abierto") && jData.type == "bool") {
      portonAbierto = jData.boolValue;
      moverPorton(portonAbierto);
    }
  }
}

void callbackConfig(StreamData data) {
  pulseAzul();

  // El stream de /configuraciones puede entregar rutas como:
  // "/distancia_programada", "/sensores/luces_sensor_on", "/sensores/porton_sensor_on"
  String path = data.dataPath();
  String type = data.dataType();

  if (type == "double" || type == "int" || type == "float") {
    if (path == "/distancia_programada") {
      distanciaProgramada = data.floatData(); // floatData soporta double/int
      Serial.printf("Distancia programada: %.2f ciclos\n", distanciaProgramada);
    }
  } else if (type == "boolean") {
    bool val = data.boolData();
    if (path == "/sensores/luces_sensor_on") {
      lucesSensorOn = val;
      Serial.printf("Sensores: luces_sensor_on = %s\n", lucesSensorOn ? "true" : "false");
    } else if (path == "/sensores/porton_sensor_on") {
      portonSensorOn = val;
      Serial.printf("Sensores: porton_sensor_on = %s\n", portonSensorOn ? "true" : "false");
    }
  } else if (type == "json") {
    FirebaseJson *json = data.jsonObjectPtr();
    FirebaseJsonData jData;

    if (json->get(jData, "distancia_programada") && jData.type == "double") {
      distanciaProgramada = jData.doubleValue;
      Serial.printf("Distancia programada: %.2f ciclos\n", distanciaProgramada);
    }
    if (json->get(jData, "sensores/luces_sensor_on") && jData.type == "bool") {
      lucesSensorOn = jData.boolValue;
    }
    if (json->get(jData, "sensores/porton_sensor_on") && jData.type == "bool") {
      portonSensorOn = jData.boolValue;
    }
  }
}

void streamTimeoutCallback(bool timeout) {
  if (timeout) {
    Serial.println("[RTDB] Stream timeout, reconectando...");
  }
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 5; i++) {
    pinMode(LED_FACHADA[i], OUTPUT);
    digitalWrite(LED_FACHADA[i], LOW);
  }

  portonServo.attach(SERVO_PIN);
  portonServo.write(90);

  pinMode(SENSOR_LUZ_PIN, INPUT);
  pinMode(SENSOR_PORTON_PIN, INPUT);

  pinMode(LED_AMARILLO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(LED_AZUL, OUTPUT);

  digitalWrite(LED_AMARILLO, LOW);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_ROJO, LOW);
  digitalWrite(LED_AZUL, LOW);

  // Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a Wi-Fi");

  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    digitalWrite(LED_AMARILLO, HIGH);
    delay(300);
    digitalWrite(LED_AMARILLO, LOW);
    delay(300);
    Serial.print(".");
    intentos++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi conectado.");
    digitalWrite(LED_VERDE, HIGH);
  } else {
    Serial.println("\nNo se pudo conectar a Wi-Fi.");
    for (int i = 0; i < 10; i++) {
      digitalWrite(LED_ROJO, HIGH);
      delay(300);
      digitalWrite(LED_ROJO, LOW);
      delay(300);
    }
  }

  // Firebase
  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Streams
  if (!Firebase.beginStream(streamEstado, "/estado_dispositivos")) {
    Serial.println("Error al iniciar stream estado_dispositivos: " + streamEstado.errorReason());
  } else {
    Firebase.setStreamCallback(streamEstado, callbackEstado, streamTimeoutCallback);
    Serial.println("Stream iniciado en /estado_dispositivos");
  }

  if (!Firebase.beginStream(streamConfig, "/configuraciones")) {
    Serial.println("Error al iniciar stream configuraciones: " + streamConfig.errorReason());
  } else {
    Firebase.setStreamCallback(streamConfig, callbackConfig, streamTimeoutCallback);
    Serial.println("Stream iniciado en /configuraciones");
  }

  // ====== LECTURA INICIAL DESDE FIREBASE ======
  // Estado inicial de dispositivos
  if (Firebase.getBool(fbdo, "/estado_dispositivos/luces_fachada_on")) {
    lucesFachadaOn = fbdo.boolData();
    aplicarFachada();
  }
  if (Firebase.getBool(fbdo, "/estado_dispositivos/porton_abierto")) {
    portonAbierto = fbdo.boolData();
    moverPorton(portonAbierto);
  }

  // Configuraciones iniciales
  if (Firebase.getFloat(fbdo, "/configuraciones/distancia_programada")) {
    distanciaProgramada = fbdo.floatData();
    Serial.printf("Distancia inicial: %.2f\n", distanciaProgramada);
  }
  if (Firebase.getBool(fbdo, "/configuraciones/sensores/luces_sensor_on")) {
    lucesSensorOn = fbdo.boolData();
  }
  if (Firebase.getBool(fbdo, "/configuraciones/sensores/porton_sensor_on")) {
    portonSensorOn = fbdo.boolData();
  }

  // Inicialización de “edge” en sensores
  lastLuzRead = digitalRead(SENSOR_LUZ_PIN);
  lastPortonRead = digitalRead(SENSOR_PORTON_PIN);
}

// ====== LOOP ======
void loop() {
  // Sensor de luces controla SOLO fachada si está habilitado (con detección de cambio)
  if (lucesSensorOn) {
    int curr = digitalRead(SENSOR_LUZ_PIN);
    if (curr != lastLuzRead) {
      lastLuzRead = curr;
      bool nuevoEstadoLuces = (curr == HIGH);
      if (Firebase.setBool(fbdo, "/estado_dispositivos/luces_fachada_on", nuevoEstadoLuces)) {
        Serial.printf("Sensor luces -> RTDB: %s\n", nuevoEstadoLuces ? "true" : "false");
      } else {
        Serial.printf("Error set luces_fachada_on: %s\n", fbdo.errorReason().c_str());
      }
    }
  }

  // Sensor de portón controla SOLO servo si está habilitado (con detección de cambio)
  if (portonSensorOn) {
    int curr = digitalRead(SENSOR_PORTON_PIN);
    if (curr != lastPortonRead) {
      lastPortonRead = curr;
      bool nuevoEstadoPorton = (curr == HIGH);
      if (Firebase.setBool(fbdo, "/estado_dispositivos/porton_abierto", nuevoEstadoPorton)) {
        Serial.printf("Sensor portón -> RTDB: %s\n", nuevoEstadoPorton ? "true" : "false");
      } else {
        Serial.printf("Error set porton_abierto: %s\n", fbdo.errorReason().c_str());
      }
    }
  }

  // Pequeño respiro al CPU
  delay(10);
}