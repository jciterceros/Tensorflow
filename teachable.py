import cv2
import numpy as np
import paho.mqtt.client as mqtt
import os

os.environ["TF_USE_LEGACY_KERAS"] = "1"
import tensorflow as tf

# 1. Configuração do MQTT (Mosquitto)
MQTT_BROKER = os.getenv("MQTT_BROKER", "localhost")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_TOPIC = os.getenv("MQTT_TOPIC", "linha_producao/alertas")
MQTT_TOPIC_STATUS = os.getenv("MQTT_TOPIC_STATUS", "linha_producao/status")
MSG_SERVICO_COMPLETO = "servico completo"
CONFIDENCE_THRESHOLD = 0.80

# Verde = tampa verde | Vermelho = tampa vermelha (cores em BGR para OpenCV)
CLASS_INFO = {
    "Verde": {"label": "Tampa verde", "color": (0, 255, 0)},
    "Vermelho": {"label": "Tampa vermelha", "color": (0, 0, 255)},
}

aguardando_servico = False
servico_completo_recebido = False


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        client.subscribe(MQTT_TOPIC_STATUS)
        print(f"Subscrito em {MQTT_TOPIC_STATUS} (aguarda ESP32)")


def on_message(client, userdata, msg):
    global servico_completo_recebido
    if msg.topic != MQTT_TOPIC_STATUS:
        return
    try:
        payload = msg.payload.decode("utf-8").strip()
    except UnicodeDecodeError:
        return
    if payload == MSG_SERVICO_COMPLETO:
        servico_completo_recebido = True


client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_start()


def load_labels(path):
    """Lê labels.txt do Teachable Machine (formato: '0 Verde')."""
    labels = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split(maxsplit=1)
            labels.append(parts[1] if len(parts) > 1 else parts[0])
    return labels


# 2. Carregar o modelo do Teachable Machine
model = tf.keras.models.load_model("keras_model.h5", compile=False)
class_names = load_labels("labels.txt")

# 3. Inicializar a Webcam
camera = cv2.VideoCapture(0)

print("Sistema iniciado. Pressione 'q' para sair.")
last_mqtt_class = None

while True:
    if servico_completo_recebido:
        aguardando_servico = False
        last_mqtt_class = None
        servico_completo_recebido = False
        print("ESP32: servico completo — nova detecção permitida")

    ret, frame = camera.read()
    if not ret:
        break

    # Redimensionar a imagem para o formato que o modelo espera (224x224)
    image = cv2.resize(frame, (224, 224), interpolation=cv2.INTER_AREA)
    image = np.asarray(image, dtype=np.float32).reshape(1, 224, 224, 3)
    
    # Normalizar a imagem
    image = (image / 127.5) - 1

    # Executar a previsão da IA
    prediction = model.predict(image, verbose=0)
    index = np.argmax(prediction)
    class_name = class_names[index]
    confidence_score = prediction[0][index]

    if confidence_score > CONFIDENCE_THRESHOLD and class_name in CLASS_INFO:
        info = CLASS_INFO[class_name]
        texto = f"{info['label']} ({class_name}) {confidence_score:.0%}"
        cv2.putText(
            frame, texto, (10, 50),
            cv2.FONT_HERSHEY_SIMPLEX, 1, info["color"], 2,
        )
        if aguardando_servico:
            status = "Aguardando ESP32 (servico completo)..."
            cor_status = (0, 255, 255)
        elif class_name != last_mqtt_class:
            mensagem = f"{info['label']} detectada | {class_name} | confiança: {confidence_score:.2f}"
            client.publish(MQTT_TOPIC, mensagem)
            last_mqtt_class = class_name
            aguardando_servico = True
            print(f"MQTT enviado: {mensagem}")
            status = None
            cor_status = None
        else:
            status = None
            cor_status = None

        if status:
            cv2.putText(
                frame, status, (10, 90),
                cv2.FONT_HERSHEY_SIMPLEX, 0.7, cor_status, 2,
            )
    else:
        cv2.putText(
            frame, "Aguardando detecção...", (10, 50),
            cv2.FONT_HERSHEY_SIMPLEX, 0.8, (200, 200, 200), 2,
        )

    # Mostrar o feed da webcam na tela
    cv2.imshow("Inspeção de Qualidade", frame)

    # Fecha o programa ao apertar a tecla 'q'
    if cv2.waitKey(1) & 0xFF == ord("q"):
        break

camera.release()
cv2.destroyAllWindows()
client.disconnect()
