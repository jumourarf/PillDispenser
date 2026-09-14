# 💊 Pill Dispenser Inteligente

Sistema embarcado de lembrete e confirmação de medicação para idosos, construído com **ESP32**, alertas locais (display, som e luz) e notificação automática ao cuidador via **Telegram**.

> Projeto acadêmico — Engenharia de Software / Conectividade de Sistemas Ciberfísicos

---

## 🩺 O Problema

Idosos que vivem sozinhos frequentemente esquecem de tomar seus remédios nos horários corretos, especialmente quando o tratamento envolve várias doses ao longo do dia. Sem um cuidador presente, ninguém confirma se a medicação foi tomada — o que pode gerar erros de medicação e, em casos graves, internações evitáveis.

## 💡 A Solução

Um dispenser inteligente construído sobre uma caixa organizadora de comprimidos com **compartimentos numerados** (5 a 7 doses/dia). O sistema:

1. 🌐 Sabe a hora certa via **Wi-Fi + NTP**
2. 🔔 Avisa o idoso na hora certa com **display + som + luz**
3. ✅ Deixa o idoso **confirmar** que tomou o remédio com um botão físico
4. 📲 Avisa o cuidador **em tempo real** via **bot do Telegram**

---

## ⚙️ Como Funciona

```
1. ESP32 liga → conecta ao Wi-Fi → sincroniza hora via NTP
2. Sistema compara a hora atual com a tabela de horários programados
3. No horário certo:
     → LED acende
     → Buzzer toca
     → Display OLED mostra hora + número do compartimento
4. Idoso retira o remédio do compartimento indicado
5. Idoso aperta o botão de confirmação
6. ESP32 envia mensagem ao bot do Telegram:
     "✅ [Nome] tomou a medicação das 12:00 — compartimento 2."
```

---

## 🏗️ Arquitetura

```
                    ┌──────────────────┐
                    │   Wi-Fi / NTP     │
                    └────────┬──────────┘
                             │
                    ┌────────▼──────────┐
                    │       ESP32        │  ← controlador central
                    └───┬───┬───┬────┬───┘
                        │   │   │    │
            ┌───────────┘   │   │    └───────────┐
            ▼               ▼   ▼                ▼
     ┌────────────┐  ┌─────────┐  ┌─────────┐  ┌──────────────┐
     │ Display OLED│  │ Buzzer  │  │  LED    │  │ Botão (idoso)│
     └────────────┘  └─────────┘  └─────────┘  └──────┬───────┘
                                                        │
                                                        ▼
                                             ┌───────────────────┐
                                             │  Bot do Telegram    │
                                             │ (avisa o cuidador)  │
                                             └───────────────────┘

---

## 🧰 Materiais Utilizados (BOM)

| Componente | Função | Situação |
|---|---|---|
| ESP32 (DevKit) | Controlador central, Wi-Fi | ✅ Já possuía |
| Buzzer ativo 5V | Alerta sonoro | ✅ Já possuía |
| LED | Alerta visual | ✅ Já possuía |
| Display OLED I2C 0.96" (SSD1306, 128x64) | Mostra hora + nº do compartimento | 🛒 Comprado — ~R$ 25 |
| Botão (push-button) | Confirmação de dose pelo idoso | 🛒 Comprado — ~R$ 2 |
| Resistor 220Ω | Proteção do LED / pull-up do botão | 🛒 Comprado — ~R$ 1 |
| Organizador de comprimidos (5-7 compartimentos) | Estrutura física | 🛒 Comprado — ~R$ 20 |
| Protoboard + jumpers | Montagem sem solda | 🛒/✅ *(confirmar)* |
| Cabo USB (USB-C ou Micro-USB, conforme o ESP32) | Programação | 🛒/✅ *(confirmar)* |

**Bibliotecas de software:**

| Biblioteca | Uso |
|---|---|
| `WiFi.h` | Conexão Wi-Fi do ESP32 |
| `time.h` / `configTime()` | Sincronização de hora via NTP |
| `Adafruit_SSD1306` + `Adafruit_GFX` | Controle do display OLED |
| `UniversalTelegramBot` | Envio de mensagens ao Telegram |
| `ArduinoJson` | Dependência da biblioteca do Telegram |

---


## 📄 Licença

*A definir pela equipe (ex: MIT License).*
