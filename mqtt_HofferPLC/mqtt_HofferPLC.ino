/*
 * ESP32 — aciona saídas digitais conforme classificação MQTT do teachable.py
 *
 * Mensagens no tópico (exemplo):
 *   "Tampa verde detectada | Verde | confiança: 0.92"
 *   "Tampa vermelha detectada | Vermelho | confiança: 0.88"
 *
 * Requisitos (Arduino IDE / PlatformIO):
 *   - Placa: ESP32 Dev Module
 *   - Biblioteca: PubSubClient (Nick O'Leary)
 *
 * Ligações sugeridas (ajuste os pinos se necessário):
 *   GPIO 25 -> relé / LED / atuador da classificação VERDE
 *   GPIO 26 -> relé / LED / atuador da classificação VERMELHO
 *
 * Após 5 s com a saída ativa, publica em linha_producao/status: "servico completo"
 */

#include <WiFi.h>
#include <PubSubClient.h>

// --- Wi-Fi (mesma rede do PC com Mosquitto) ---
const char* WIFI_SSID     = "Nome do seu WIFI";
const char* WIFI_PASSWORD = "Senha";

// IP do PC onde corre "docker compose up" (não use "localhost" no ESP32)
const char* MQTT_SERVER = "192.168.0.27";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_TOPIC        = "linha_producao/alertas";   // subscreve (teachable.py)
const char* MQTT_TOPIC_STATUS = "linha_producao/status";    // publica ao terminar
const char* MQTT_CLIENT_ID    = "esp32_linha_producao";
const char* MSG_SERVICO_COMPLETO = "servico completo";

// --- Saídas digitais ---
const uint8_t PIN_VERDE    = 4;
const uint8_t PIN_VERMELHO = 16;
const unsigned long SAIDA_MS = 5000;  // tempo com saída em HIGH

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

unsigned long verdeAteMs    = 0;
unsigned long vermelhoAteMs = 0;

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

void acionarVerde() {
  digitalWrite(PIN_VERDE, HIGH);
  digitalWrite(PIN_VERMELHO, LOW);
  verdeAteMs = millis() + SAIDA_MS;
  vermelhoAteMs = 0;
  Serial.println("Saida VERDE (GPIO 25) ON por 5 s");
}

void acionarVermelho() {
  digitalWrite(PIN_VERMELHO, HIGH);
  digitalWrite(PIN_VERDE, LOW);
  vermelhoAteMs = millis() + SAIDA_MS;
  verdeAteMs = 0;
  Serial.println("Saida VERMELHO (GPIO 26) ON por 5 s");
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
    acionarVerde();
    return;
  }
  if (contem(msg, "| Vermelho |") || contem(msg, "Tampa vermelha")) {
    acionarVermelho();
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

void atualizarSaidas() {
  unsigned long agora = millis();

  if (verdeAteMs != 0 && agora >= verdeAteMs) {
    digitalWrite(PIN_VERDE, LOW);
    verdeAteMs = 0;
    Serial.println("Saida VERDE OFF");
    enviarServicoCompleto();
  }
  if (vermelhoAteMs != 0 && agora >= vermelhoAteMs) {
    digitalWrite(PIN_VERMELHO, LOW);
    vermelhoAteMs = 0;
    Serial.println("Saida VERMELHO OFF");
    enviarServicoCompleto();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_VERDE, OUTPUT);
  pinMode(PIN_VERMELHO, OUTPUT);
  digitalWrite(PIN_VERDE, LOW);
  digitalWrite(PIN_VERMELHO, LOW);

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
