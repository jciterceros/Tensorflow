/*
 * ESP32 — controla direção do Motor A (L298N) conforme classificação MQTT do teachable.py
 *
 * Mensagens no tópico (exemplo):
 *   "Tampa verde detectada | Verde | confiança: 0.92"
 *   "Tampa vermelha detectada | Vermelho | confiança: 0.88"
 *
 * Requisitos (Arduino IDE / PlatformIO):
 *   - Placa: ESP32 Dev Module
 *   - Biblioteca: PubSubClient (Nick O'Leary)
 *
 * Ligações sugeridas (Motor A no L298N):
 *   GPIO 4  -> IN1 (sentido direita)
 *   GPIO 16 -> IN2 (sentido esquerda)
 *
 * Após 5 s com a saída ativa, publica em linha_producao/status: "servico completo"
 */

#include <WiFi.h>
#include <PubSubClient.h>

// --- Wi-Fi: hotspot Windows (Configurações → Hotspot móvel) ---
const char* WIFI_SSID     = "IOT2026";
const char* WIFI_PASSWORD = "!Eps32-2026!";

// Gateway padrão do hotspot Windows (192.168.137.0/24). Não use "localhost" no ESP32.
const char* MQTT_SERVER = "192.168.137.1";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_TOPIC        = "linha_producao/alertas";   // subscreve (teachable.py)
const char* MQTT_TOPIC_STATUS = "linha_producao/status";    // publica ao terminar
const char* MQTT_CLIENT_ID    = "esp32_linha_producao";
const char* MSG_SERVICO_COMPLETO = "servico completo";

// --- Controle do Motor A (L298N) ---
const uint8_t IN1 = 4;
const uint8_t IN2 = 16;
const unsigned long SAIDA_MS = 5000;  // tempo com saída em HIGH

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

unsigned long direitaAteMs  = 0;
unsigned long esquerdaAteMs = 0;

void conectarWiFi() {
  Serial.print("Wi-Fi: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());
}

void conectarMQTT() {
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setBufferSize(256);

  while (!mqtt.connected()) {
    Serial.print("MQTT: ");
    if (mqtt.connect(MQTT_CLIENT_ID)) {
      Serial.println("conectado");
      mqtt.subscribe(MQTT_TOPIC);
      Serial.print("Subscrito em ");
      Serial.println(MQTT_TOPIC);
    } else {
      Serial.print("falhou, rc=");
      Serial.print(mqtt.state());
      Serial.println(" — nova tentativa em 3s");
      delay(3000);
    }
  }
}

bool contem(const char* texto, const char* trecho) {
  return strstr(texto, trecho) != nullptr;
}

void sentidoDireita() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  direitaAteMs = millis() + SAIDA_MS;
  esquerdaAteMs = 0;
  Serial.println("Motor A: sentido DIREITA por 5 s (IN1=HIGH, IN2=LOW)");
}

void sentidoEsquerda() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  esquerdaAteMs = millis() + SAIDA_MS;
  direitaAteMs = 0;
  Serial.println("Motor A: sentido ESQUERDA por 5 s (IN1=LOW, IN2=HIGH)");
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  char msg[256];
  if (length >= sizeof(msg)) {
    length = sizeof(msg) - 1;
  }
  memcpy(msg, payload, length);
  msg[length] = '\0';

  Serial.print("MQTT [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(msg);

  // Ordem: classificação explícita no payload (| Verde | ou | Vermelho |)
  if (contem(msg, "| Verde |") || contem(msg, "Tampa verde")) {
    sentidoDireita();
    return;
  }
  if (contem(msg, "| Vermelho |") || contem(msg, "Tampa vermelha")) {
    sentidoEsquerda();
    return;
  }

  if (strcmp(msg, MSG_SERVICO_COMPLETO) == 0) {
    return;  // ignora a propria confirmacao (se publicar no mesmo topico)
  }

  Serial.println("Mensagem ignorada (classe nao reconhecida)");
}

void enviarServicoCompleto() {
  if (mqtt.publish(MQTT_TOPIC_STATUS, MSG_SERVICO_COMPLETO)) {
    Serial.println("MQTT publicado: servico completo");
  } else {
    Serial.println("Falha ao publicar servico completo");
  }
}

void pararMotor() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
}

void atualizarSaidas() {
  unsigned long agora = millis();

  if (direitaAteMs != 0 && agora >= direitaAteMs) {
    pararMotor();
    direitaAteMs = 0;
    Serial.println("Motor A: parada apos sentido DIREITA");
    enviarServicoCompleto();
  }
  if (esquerdaAteMs != 0 && agora >= esquerdaAteMs) {
    pararMotor();
    esquerdaAteMs = 0;
    Serial.println("Motor A: parada apos sentido ESQUERDA");
    enviarServicoCompleto();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pararMotor();

  conectarWiFi();
  conectarMQTT();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }
  if (!mqtt.connected()) {
    conectarMQTT();
  }

  mqtt.loop();
  atualizarSaidas();
}
