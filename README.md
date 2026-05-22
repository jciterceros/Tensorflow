# Inspecao de Qualidade com Teachable Machine + MQTT + ESP32

Projeto de visao computacional para classificacao de tampas via webcam.

O fluxo atual funciona assim:
1. O script Python (`teachable.py`) classifica cada frame com um modelo Keras exportado do Teachable Machine.
2. Quando a confianca passa de 80%, ele publica o evento no MQTT em `linha_producao/alertas`.
3. O ESP32 (`mqtt_HofferPLC/mqtt_HofferPLC.ino`) recebe o evento, aciona uma saida digital por 5 segundos e publica `servico completo` em `linha_producao/status`.
4. O Python so libera uma nova publicacao apos receber `servico completo` (handshake anti-repeticao).

## Estrutura do projeto

```text
Tensorflow/
|-- teachable.py
|-- keras_model.h5
|-- labels.txt
|-- requirements.txt
|-- docker-compose.yml
|-- assets/
|   |-- converted_keras/
|   |   |-- keras_model.h5
|   |   `-- labels.txt
|   |-- ok/
|   `-- not ok/
`-- mqtt_HofferPLC/
    |-- mqtt_HofferPLC.ino
    `-- requirements.txt
```

Arquivos realmente usados na execucao do Python:
- `teachable.py`
- `keras_model.h5` (na raiz)
- `labels.txt` (na raiz)

## Requisitos

- Windows com Python 3.12+
- Webcam conectada
- Docker (opcional, para subir Mosquitto local)
- Arduino IDE (opcional, para gravar ESP32)

## Dependencias Python

Instaladas via `requirements.txt`:
- `opencv-python==4.13.0.92`
- `numpy==2.0.2`
- `paho-mqtt==1.6.1`
- `tensorflow==2.18.1`
- `tf-keras==2.18.0`

Observacao:
- O script define `TF_USE_LEGACY_KERAS=1` para compatibilidade com modelo `.h5`.

## Setup rapido (somente visao)

Se quiser validar apenas classificacao na webcam (sem ESP32):

```powershell
cd C:\Projetos\Tensorflow
python -m venv .venv
.\.venv\Scripts\activate
pip install --no-cache-dir -r requirements.txt
python teachable.py
```

Pressione `q` para fechar.

## Setup completo (visao + MQTT + ESP32)

### 1) Ambiente Python

```powershell
cd C:\Projetos\Tensorflow
python -m venv .venv
.\.venv\Scripts\activate
pip install --no-cache-dir -r requirements.txt
```

### 2) Subir broker MQTT local

```powershell
docker compose up -d
docker compose ps
```

Broker exposto em `localhost:1883`.

### 3) Rodar classificador Python

```powershell
.\.venv\Scripts\activate
python teachable.py
```

### 4) Gravar ESP32

No arquivo `mqtt_HofferPLC/mqtt_HofferPLC.ino`, ajuste:
- `WIFI_SSID`
- `WIFI_PASSWORD`
- `MQTT_SERVER` (IP do PC onde o Mosquitto esta rodando)

Importante:
- No ESP32, nao use `localhost`. Use o IP real da maquina (ex.: `192.168.0.27`).

Bibliotecas/placa sugeridas em `mqtt_HofferPLC/requirements.txt`:
- ESP32 by Espressif Systems
- PubSubClient by Nick O'Leary

## Contrato MQTT

Topicos:
- Publicado pelo Python: `linha_producao/alertas`
- Publicado pelo ESP32: `linha_producao/status`

Payloads:
- Python -> ESP32 (exemplo):
  - `Tampa verde detectada | Verde | confianca: 0.92`
  - `Tampa vermelha detectada | Vermelho | confianca: 0.88`
- ESP32 -> Python:
  - `servico completo`

Comportamento de bloqueio no Python:
- Depois de publicar uma classificacao valida, entra em modo "aguardando servico".
- Enquanto estiver aguardando, nao publica novas deteccoes.
- So libera nova publicacao quando recebe `servico completo` no topico de status.

## Modelo e labels

O script carrega:
- `keras_model.h5` na raiz
- `labels.txt` na raiz

Formato esperado de `labels.txt`:

```text
0 Verde
1 Vermelho
```

No estado atual do repositorio:
- `labels.txt` (raiz) usa `Verde` e `Vermelho` e esta alinhado com o dicionario `CLASS_INFO`.
- `assets/converted_keras/labels.txt` ainda contem `Class 1` e `Class 2` (arquivo de referencia/backup).

Se trocar o modelo e os nomes de classe, ajuste tambem o `CLASS_INFO` em `teachable.py`.

## Variaveis de ambiente (Python)

O `teachable.py` le as seguintes variaveis:

| Variavel | Padrao | Uso |
| --- | --- | --- |
| `MQTT_BROKER` | `localhost` | Host do broker MQTT |
| `MQTT_PORT` | `1883` | Porta do broker |
| `MQTT_TOPIC` | `linha_producao/alertas` | Topico de publicacao do Python |
| `MQTT_TOPIC_STATUS` | `linha_producao/status` | Topico de status recebido do ESP32 |

Exemplo de execucao customizada:

```powershell
$env:MQTT_BROKER="192.168.0.27"
$env:MQTT_PORT="1883"
$env:MQTT_TOPIC="linha_producao/alertas"
$env:MQTT_TOPIC_STATUS="linha_producao/status"
python teachable.py
```

## Testes uteis

Escutar alertas publicados pelo Python:

```powershell
docker compose exec mosquitto mosquitto_sub -t "linha_producao/alertas"
```

Escutar status publicados pelo ESP32:

```powershell
docker compose exec mosquitto mosquitto_sub -t "linha_producao/status"
```

## Troubleshooting

| Problema | Causa comum | Solucao |
| --- | --- | --- |
| Erro ao ativar `.venv` (`No Python at ...`) | Ambiente virtual copiado de outra maquina | Apagar `.venv` e recriar com `python -m venv .venv` |
| Webcam nao abre | Camera ocupada por outro app / indice errado | Fechar outros apps e testar outro indice no `VideoCapture` |
| Erro no load do `.h5` | Incompatibilidade Keras | Garantir `tf-keras` instalado e `TF_USE_LEGACY_KERAS=1` |
| ESP32 nao conecta no broker | Uso de `localhost` no microcontrolador | Trocar por IP do PC com Mosquitto |
| Python publica uma vez e para | Handshake aguardando `servico completo` | Verificar se ESP32 publica no topico `linha_producao/status` |

## Estado atual do projeto

- `python teachable.py` executa sem erro no ambiente atual.
- O README foi atualizado para refletir o comportamento real do codigo (inclusive handshake MQTT com ESP32).


