<div align="center">

# 💊 Pill Dispenser Inteligente

**Dispenser de remédios com alertas automáticos e notificação ao cuidador via Telegram**

Projeto acadêmico • Engenharia de Software / Conectividade de Sistemas Ciberfísicos

</div>

---

## 📖 Sobre o projeto

Idosos que moram sozinhos frequentemente esquecem de tomar seus remédios no horário certo, especialmente quando o tratamento envolve várias doses ao longo do dia — café da manhã, almoço, tarde, jantar e noite. Sem um cuidador presente para confirmar a adesão ao tratamento, não há garantia de que a medicação foi tomada na hora certa, nem no compartimento correto. Esse tipo de falha é uma das causas mais comuns de erros de medicação em idosos, podendo levar ao agravamento de doenças crônicas e, em casos mais sérios, a internações evitáveis.

O **Pill Dispenser Inteligente** foi criado para fechar essa lacuna sem depender da presença constante de um cuidador. O projeto parte de uma caixa organizadora de comprimidos comum, com compartimentos numerados — um para cada horário de dose do dia — e adiciona um pequeno sistema eletrônico embarcado em cima dela.

Um microcontrolador **ESP32** conecta-se à internet via Wi-Fi assim que é ligado e sincroniza automaticamente o horário através de **NTP**, sem precisar de configuração manual de data/hora. A partir daí, o sistema mantém uma tabela com os horários programados de medicação. Quando o horário de uma dose chega, três coisas acontecem ao mesmo tempo: um **LED** acende, um **buzzer** toca um alarme sonoro, e o **display OLED** mostra o horário atual junto com o número do compartimento que deve ser retirado — reduzindo a chance de o idoso se confundir e tomar o remédio errado.

O idoso então retira a medicação do compartimento indicado e pressiona um **botão físico** de confirmação. Esse clique é interpretado pelo ESP32 como a prova de que a dose foi tomada, e o sistema aciona automaticamente um **bot do Telegram**, enviando uma mensagem em tempo real para o celular do responsável ou cuidador informando qual dose foi confirmada e em que horário. Assim, mesmo à distância, a família consegue acompanhar se o tratamento está sendo seguido corretamente, sem precisar ligar ou visitar o idoso só para checar isso.

Nesta primeira versão, o sistema depende de conexão Wi-Fi ativa para manter a hora certa — a adição de um módulo RTC físico como reforço offline está prevista como uma melhoria futura, assim como o envio de um segundo alerta caso o botão não seja pressionado dentro de um tempo limite após o alarme.

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
