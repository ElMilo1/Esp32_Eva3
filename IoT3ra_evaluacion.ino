#include <WiFi.h>
#include <FirebaseESP32.h>

// ====== CONFIGURACIÓN DE WIFI ======
const char* WIFI_SSID     = "MiloWon";
const char* WIFI_PASSWORD = "19301829Mailo.";

// ====== CONFIGURACIÓN DE FIREBASE ======
#define FIREBASE_HOST "https://iot3-31836-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "hP3PXbOLV6l9q82YletCMfxAk3iAVm1XcFSosdwD"

FirebaseData stream;
FirebaseAuth auth;
FirebaseConfig config;

// ====== LED ======
const int LED_PIN = 2;   // LED conectado al GPIO 2
bool lucesFachadaOn = false;

// ====== FUNCIONES ======
void aplicarEstado() {
  digitalWrite(LED_PIN, lucesFachadaOn ? HIGH : LOW);
  Serial.printf("[ACCION] LED GPIO2 => %s\n", lucesFachadaOn ? "ENCENDIDO" : "APAGADO");
}

// ====== CALLBACK DE STREAM ======
void streamCallback(StreamData data) {
  Serial.printf("Stream path: %s\n", data.dataPath().c_str());
  Serial.printf("Data type: %s\n", data.dataType().c_str());

  if (data.dataType() == "json") {
    FirebaseJson* json = data.jsonObjectPtr();
    FirebaseJsonData jData;
    if (json->get(jData, "luces_fachada_on") && jData.type == "bool") {
      lucesFachadaOn = jData.boolValue;
    }
  }
  else if (data.dataType() == "boolean") {
    if (data.dataPath() == "/luces_fachada_on") {
      lucesFachadaOn = data.boolData();
    }
  }

  aplicarEstado();
}

void streamTimeoutCallback(bool timeout) {
  if (timeout) {
    Serial.println("[RTDB] Stream timeout, reconectando...");
  }
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectado.");

  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (!Firebase.beginStream(stream, "/estado_dispositivos")) {
    Serial.println("Error al iniciar stream: " + stream.errorReason());
  } else {
    Firebase.setStreamCallback(stream, streamCallback, streamTimeoutCallback);
    Serial.println("Stream iniciado en /estado_dispositivos");
  }
}

// ====== LOOP ======
void loop() {
  // El stream se mantiene automáticamente con el callback
}