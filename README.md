<div align="center">

# 💊 Pill Dispenser Inteligente

**Dispenser de remédios com alertas automáticos e notificação ao cuidador via Telegram**

Projeto acadêmico • Engenharia de Software / Conectividade de Sistemas Ciberfísicos

</div>

---

## 📖 Sobre o projeto

Idosos que moram sozinhos frequentemente esquecem de tomar seus remédios no horário certo, e ninguém confirma se a dose foi tomada. Isso pode causar erros de medicação e até internações.

O **Pill Dispenser Inteligente** resolve isso: na hora certa, o sistema acende uma luz, toca um alarme e mostra no display qual compartimento tomar. Quando o idoso aperta o botão de confirmação, um **bot do Telegram** avisa automaticamente o responsável de que a dose foi tomada.

---

## 🧰 Materiais utilizados

| Componente | Função |
|---|---|
| 🧠 ESP32 | Controlador central + Wi-Fi |
| 📺 Display OLED I2C 0.96" | Mostra hora e nº do compartimento |
| 🔊 Buzzer ativo | Alerta sonoro |
| 💡 LED | Alerta visual |
| 🔘 Botão | Confirmação da dose pelo idoso |
| 💊 Organizador de comprimidos | Estrutura física (5 a 7 compartimentos) |
| 📲 Bot do Telegram | Notifica o cuidador em tempo real |

---

## ⚙️ Como funciona

```
Hora certa → 🔊 + 💡 + 📺 avisam o idoso
     ↓
Idoso toma o remédio e aperta o 🔘
     ↓
Bot do Telegram avisa o cuidador ✅
```

---

<div align="center">

*Desenvolvido por Henrique Gnatkovski, Julia Moura, Letícia Prata, Sabrina Bernardi e Yasmin Luz*

</div>
