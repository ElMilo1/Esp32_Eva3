/*
  ESP32 + Firebase Realtime Database + OLED SSD1306
  Proyecto: iot3-31836
  Recupera SIEMPRE el objeto completo "estado_dispositivos"
  y actualiza LEDs + pantalla OLED.
*/

#include <WiFi.h>
#include <FirebaseESP32.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ====== CONFIGURACIÓN DE WIFI ======
const char* WIFI_SSID     = "MiloWon";
const char* WIFI_PASSWORD = "19301829Mailo.";

// ====== CONFIGURACIÓN DE FIREBASE ======
#define FIREBASE_HOST "https://iot3-31836-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "hP3PXbOLV6l9q82YletCMfxAk3iAVm1XcFSosdwD"

FirebaseData stream;
FirebaseAuth auth;
FirebaseConfig config;

// ====== MAPEADO DE LEDS ======
const int LUZ_FACHADA_PIN  = 2;
const int PORTON_LED_PIN   = 4;
const int EXTRA_LED_PIN    = 5;

// ====== ESTADOS ======
bool lucesFachadaOn = false;
bool portonAbierto  = false;
bool ledOn          = false;

// ====== OLED SSD1306 ======
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ====== FUNCIONES ======
void aplicarEstados() {
  digitalWrite(LUZ_FACHADA_PIN, lucesFachadaOn ? HIGH : LOW);
  digitalWrite(PORTON_LED_PIN, portonAbierto ? HIGH : LOW);
  digitalWrite(EXTRA_LED_PIN, ledOn ? HIGH : LOW);

  Serial.printf("[ACCION] LEDs => fachada=%s, porton=%s, extra=%s\n",
                lucesFachadaOn ? "ON" : "OFF",
                portonAbierto ? "ON" : "OFF",
                ledOn ? "ON" : "OFF");

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Estado Dispositivos:");
  display.setCursor(0, 16);
  display.printf("Fachada: %s", lucesFachadaOn ? "ON" : "OFF");
  display.setCursor(0, 28);
  display.printf("Porton:  %s", portonAbierto ? "ABIERTO" : "CERRADO");
  display.setCursor(0, 40);
  display.printf("LED extra: %s", ledOn ? "ON" : "OFF");
  display.display();
}

// ====== CALLBACK DE STREAM ======
void streamCallback(StreamData data) {
  Serial.printf("Stream path: %s\n", data.dataPath().c_str());
  Serial.printf("Data type: %s\n", data.dataType().c_str());

  // Siempre esperamos un JSON completo en /estado_dispositivos
  if (data.dataType() == "json") {
    FirebaseJson* json = data.jsonObjectPtr();
    FirebaseJsonData jData;

    if (json->get(jData, "luces_fachada_on") && jData.type == "bool") {
      lucesFachadaOn = jData.boolValue;
    }
    if (json->get(jData, "porton_abierto") && jData.type == "bool") {
      portonAbierto = jData.boolValue;
    }
    if (json->get(jData, "led_on") && jData.type == "bool") {
      ledOn = jData.boolValue;
    }

    aplicarEstados();
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

  pinMode(LUZ_FACHADA_PIN, OUTPUT);
  pinMode(PORTON_LED_PIN, OUTPUT);
  pinMode(EXTRA_LED_PIN, OUTPUT);

  digitalWrite(LUZ_FACHADA_PIN, LOW);
  digitalWrite(PORTON_LED_PIN, LOW);
  digitalWrite(EXTRA_LED_PIN, LOW);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Error al iniciar SSD1306");
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Conectando Wi-Fi...");
  display.display();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando a Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectado.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Wi-Fi conectado!");
  display.setCursor(0, 16);
  display.print("IP: ");
  display.println(WiFi.localIP());
  display.display();

  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Suscribirse al nodo completo /estado_dispositivos
  if (!Firebase.beginStream(stream, "/estado_dispositivos")) {
    Serial.println("Error al iniciar stream: " + stream.errorReason());
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Error stream Firebase");
    display.display();
  } else {
    Firebase.setStreamCallback(stream, streamCallback, streamTimeoutCallback);
    Serial.println("Stream iniciado en /estado_dispositivos");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Firebase conectado!");
    display.display();
  }
}

// ====== LOOP ======
void loop() {
  // El stream se mantiene automáticamente con el callback
}