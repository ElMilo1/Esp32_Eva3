#include <WiFi.h>
#include <FirebaseESP32.h>
#include <ESP32Servo.h>

// ====== CONFIGURACIÓN DE WIFI ======
const char* WIFI_SSID     = "MiloWon";
const char* WIFI_PASSWORD = "19301829Mailo.";

// ====== CONFIGURACIÓN DE FIREBASE ======
#define FIREBASE_HOST "https://iot3-31836-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "hP3PXbOLV6l9q82YletCMfxAk3iAVm1XcFSosdwD"

FirebaseData stream;
FirebaseAuth auth;
FirebaseConfig config;

// ====== LEDS FACHADA (todos juntos) ======
const int LED_FACHADA[5] = {2, 4, 16, 17, 18}; // 5 LEDs en fachada
bool fachadaOn = false;

// ====== SERVO (rotación continua) ======
Servo portonServo;
const int SERVO_PIN = 15;
bool portonAbierto = false;

// ====== SENSORES IR ======
const int SENSOR_LUZ_PIN   = 32;
const int SENSOR_PORTON_PIN = 33;

// ====== FLAGS REMOTOS ======
bool lucesSensorOn = true;
bool portonSensorOn = true;

// ====== LEDS DE ESTADO ======
const int LED_AMARILLO = 26;
const int LED_VERDE    = 27;
const int LED_ROJO     = 14;
const int LED_AZUL     = 25;

// ====== PARÁMETRO DE DISTANCIA ======
float distanciaProgramada = 2.0; // valor por defecto
const uint16_t TIEMPO_BASE_CICLO_MS = 2000;

// ====== ACCIONES ======
void aplicarFachada() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(LED_FACHADA[i], fachadaOn ? HIGH : LOW);
  }
  Serial.printf("Fachada => %s\n", fachadaOn ? "ENCENDIDA" : "APAGADA");
}

void moverPorton(bool abrir) {
  uint32_t tiempoMovimiento = (uint32_t)(TIEMPO_BASE_CICLO_MS * distanciaProgramada);

  if (abrir) {
    portonServo.write(180);
    delay(tiempoMovimiento);
    portonServo.write(90);
    Serial.printf("Portón ABIERTO (%.2f ciclos)\n", distanciaProgramada);
  } else {
    portonServo.write(0);
    delay(tiempoMovimiento);
    portonServo.write(90);
    Serial.printf("Portón CERRADO (%.2f ciclos)\n", distanciaProgramada);
  }
}

// ====== CALLBACK DE STREAM ======
void streamCallback(StreamData data) {
  digitalWrite(LED_AZUL, HIGH);
  delay(200);
  digitalWrite(LED_AZUL, LOW);

  if (data.dataType() == "json") {
    FirebaseJson *json = data.jsonObjectPtr();
    FirebaseJsonData jData;

    // Fachada
    if (json->get(jData, "fachada_on") && jData.type == "bool") {
      fachadaOn = jData.boolValue;
      aplicarFachada();
    }

    // Portón
    if (json->get(jData, "porton_abierto") && jData.type == "bool") {
      portonAbierto = jData.boolValue;
      moverPorton(portonAbierto);
    }

    // Sensores habilitados
    if (json->get(jData, "luces_sensor_on") && jData.type == "bool") {
      lucesSensorOn = jData.boolValue;
    }
    if (json->get(jData, "porton_sensor_on") && jData.type == "bool") {
      portonSensorOn = jData.boolValue;
    }

    // Distancia programada
    if (json->get(jData, "distancia_programada") && jData.type == "double") {
      distanciaProgramada = jData.doubleValue;
      Serial.printf("Distancia programada actualizada: %.2f ciclos\n", distanciaProgramada);
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

  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (!Firebase.beginStream(stream, "/estado_dispositivos/Fachada")) {
    Serial.println("Error al iniciar stream: " + stream.errorReason());
  } else {
    Firebase.setStreamCallback(stream, streamCallback, streamTimeoutCallback);
    Serial.println("Stream iniciado en /estado_dispositivos/Fachada");
  }
}

// ====== LOOP ======
void loop() {
  if (lucesSensorOn && digitalRead(SENSOR_LUZ_PIN) == HIGH) {
    fachadaOn = true;
    aplicarFachada();
    Firebase.setBool(stream, "/estado_dispositivos/Fachada/fachada_on", true);
  }

  if (portonSensorOn && digitalRead(SENSOR_PORTON_PIN) == HIGH) {
    portonAbierto = true;
    moverPorton(true);
    Firebase.setBool(stream, "/estado_dispositivos/porton_abierto", true);
  }
}