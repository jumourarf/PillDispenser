#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <UniversalTelegramBot.h>
#include <Preferences.h>
#include <time.h>

// ================= EMOJIS COMPATÍVEIS COM O COMPILADOR (UTF-8 HEX) =================
#define EMOJI_PILULA   "\xF0\x9F\x92\x8A"
#define EMOJI_SIRENE   "\xF0\x9F\x9A\xA8"
#define EMOJI_RELOGIO  "\xE2\x8F\xB0"
#define EMOJI_CHECK    "\xE2\x9C\x85"
#define EMOJI_CORACAO  "\xF0\x9F\x92\x96"
#define EMOJI_GRAFICO  "\xF0\x9F\x93\x8A"
#define EMOJI_SINO     "\xF0\x9F\x94\x94"
#define EMOJI_HORA     "\xF0\x9F\x95\x92"
#define EMOJI_FOGUETE  "\xF0\x9F\x9A\x80"
#define EMOJI_DEDO     "\xF0\x9F\x91\x89"
#define EMOJI_LAMPADA  "\xF0\x9F\x92\xA1"

// ================= CONFIGURAÇÕES DO TELEGRAM =================
#define BOT_TOKEN "SEU_BOT_TOKEN_AQUI"
#define CHAT_ID   "SEU_CHAT_ID_AQUI"

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

// ================= REDE WI-FI =================
const char* WIFI_SSID = "NOME_DO_SEU_WIFI"; 
const char* WIFI_PASS = "SENHA_DO_SEU_WIFI";

// ================= CONFIGURAÇÕES DO DISPLAY OLED =================
#define LARGURA_TELA 128
#define ALTURA_TELA 64
#define ENDERECO_OLED 0x3C

Adafruit_SSD1306 display(LARGURA_TELA, ALTURA_TELA, &Wire, -1);

// ================= PINAGEM DOS COMPONENTES (BANCADA) =================
const int PINO_LED = 4;     
const int PINO_BUZZER = 18;   
const int PINO_BOTAO = 13;   

WebServer server(80);
Preferences prefs;

// ================= ESTRUTURA DE ALARMES COM POTINHOS (1 a 20) =================
#define MAX_ALARMES 10

struct AlarmeItem {
  int hora;
  int minuto;
  int pote; // Número da caixinha (1 a 20)
  bool ativo;
};

AlarmeItem alarmes[MAX_ALARMES];
int totalAlarmes = 0;
int poteAlarmeAtual = 1; // Pote do alarme que está tocando agora

// ================= ESTADOS DO SISTEMA =================
bool ledLigado = false;
volatile bool alarmeAtivo = false;
int dosesConfirmadas = 0;
bool telaInvertida = false;

// Prevenção de repetição de disparo no mesmo minuto
int ultimoMinutoDisparado = -1;
int ultimaHoraDisparada = -1;

// Alarme contínuo e insistente (bip-bip-bip contínuo sem pausas longas)
unsigned long tempoAlarmeAnterior = 0;
int passoAlarme = 0;
const int duracoesPassos[] = { 140, 80 };
const int totalPassos = 2;

// Controle do botão físico com detecção instantânea
bool botaoPressionadoAnterior = false;
unsigned long ultimoDebounceTime = 0;

unsigned long tempoTelaConfirmacao = 0;
unsigned long ultimoTempoTelegramCheck = 0;
int ultimoMinutoExibido = -1;

// Declarações antecipadas de funções
void desenharTelaEspera();
void desenharTelaAlarme();
void desenharTelaConfirmacao();
void iniciarAlarme(int pote);
void confirmarDose();
void setLed(bool ligado);
void setBuzzer(bool ligado);
void emitirBipe();
void enviarTelegram(String mensagem);
void ordenarAlarmes();
void salvarAlarmesNaFlash();
void carregarAlarmesDaFlash();
String getProximoAlarmeStr();

// ================= CONTROLE DE HARDWARE =================
void setLed(bool ligado) {
  ledLigado = ligado;
  digitalWrite(PINO_LED, ligado ? HIGH : LOW);
}

void setBuzzer(bool ligado) {
  digitalWrite(PINO_BUZZER, ligado ? HIGH : LOW);
}

void emitirBipe() {
  setBuzzer(true);
  delay(80);
  setBuzzer(false);
}

void setBrilhoDisplay(uint8_t brilho) { // 0 a 255
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(brilho);
}

// ================= PERSISTÊNCIA NA MEMÓRIA FLASH (PREFERENCES) =================
void salvarAlarmesNaFlash() {
  prefs.begin("pilldisp", false);
  prefs.putInt("total", totalAlarmes);
  for (int i = 0; i < totalAlarmes; i++) {
    String keyH = "h" + String(i);
    String keyM = "m" + String(i);
    String keyP = "p" + String(i);
    prefs.putInt(keyH.c_str(), alarmes[i].hora);
    prefs.putInt(keyM.c_str(), alarmes[i].minuto);
    prefs.putInt(keyP.c_str(), alarmes[i].pote);
  }
  prefs.end();
}

void carregarAlarmesDaFlash() {
  prefs.begin("pilldisp", true);
  totalAlarmes = prefs.getInt("total", 0);
  if (totalAlarmes > MAX_ALARMES) totalAlarmes = MAX_ALARMES;
  for (int i = 0; i < totalAlarmes; i++) {
    String keyH = "h" + String(i);
    String keyM = "m" + String(i);
    String keyP = "p" + String(i);
    alarmes[i].hora = prefs.getInt(keyH.c_str(), 8);
    alarmes[i].minuto = prefs.getInt(keyM.c_str(), 0);
    alarmes[i].pote = prefs.getInt(keyP.c_str(), i + 1);
    alarmes[i].ativo = true;
  }
  prefs.end();
  ordenarAlarmes();
}

void ordenarAlarmes() {
  for (int i = 0; i < totalAlarmes - 1; i++) {
    for (int j = i + 1; j < totalAlarmes; j++) {
      int minI = alarmes[i].hora * 60 + alarmes[i].minuto;
      int minJ = alarmes[j].hora * 60 + alarmes[j].minuto;
      if (minJ < minI) {
        AlarmeItem temp = alarmes[i];
        alarmes[i] = alarmes[j];
        alarmes[j] = temp;
      }
    }
  }
}

// ================= ENVIO TELEGRAM =================
void enviarTelegram(String mensagem) {
  if (WiFi.status() == WL_CONNECTED) {
    bot.sendMessage(CHAT_ID, mensagem, "Markdown");
  }
}

void enviarAlertaTelegramComBotao(String mensagem) {
  if (WiFi.status() == WL_CONNECTED) {
    String teclado = "[ [{\"text\":\"" + String(EMOJI_CHECK) + " Dose Confirmada\", \"callback_data\":\"/confirmar\"} ] ]";
    bot.sendMessageWithInlineKeyboard(CHAT_ID, mensagem, "Markdown", teclado);
  }
}

// ================= FORMATAÇÃO DE HORAS =================
String getHoraAtualStr() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "--:--";
  }
  char buffer[10];
  strftime(buffer, sizeof(buffer), "%H:%M", &timeinfo);
  return String(buffer);
}

String getHoraFormatada(int h, int m) {
  char buf[10];
  sprintf(buf, "%02d:%02d", h, m);
  return String(buf);
}

String getProximoAlarmeStr() {
  if (totalAlarmes == 0) return "--:--";

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    char buf[24];
    sprintf(buf, "%02d:%02d (Pote %d)", alarmes[0].hora, alarmes[0].minuto, alarmes[0].pote);
    return String(buf);
  }

  int agoraMinutos = timeinfo.tm_hour * 60 + timeinfo.tm_min;

  for (int i = 0; i < totalAlarmes; i++) {
    int alarmeMinutos = alarmes[i].hora * 60 + alarmes[i].minuto;
    if (alarmeMinutos > agoraMinutos) {
      char buf[24];
      sprintf(buf, "%02d:%02d (Pote %d)", alarmes[i].hora, alarmes[i].minuto, alarmes[i].pote);
      return String(buf);
    }
  }

  char buf[24];
  sprintf(buf, "%02d:%02d (Pote %d)", alarmes[0].hora, alarmes[0].minuto, alarmes[0].pote);
  return String(buf);
}

// ================= ANIMAÇÃO 1: REMEDINHO ELEGANTE (4 SEGUNDOS) =================
void animacaoRemedinhoElegante() {
  unsigned long inicio = millis();
  float t = 0.0;

  while (millis() - inicio < 4000) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(22, 6);
    display.print(F("PILL DISPENSER"));
    display.drawLine(20, 17, 107, 17, SSD1306_WHITE);

    float flutuacao = sin(t) * 3.0;
    int cy = 26 + (int)flutuacao;
    int cx = 48;
    int w = 32;
    int h = 15;

    int aura = 2 + (int)(abs(sin(t * 1.5)) * 3.0);
    display.drawRoundRect(cx - aura, cy - aura, w + (aura * 2), h + (aura * 2), 7 + aura, SSD1306_WHITE);

    display.drawRoundRect(cx, cy, w, h, 7, SSD1306_WHITE);
    display.fillRoundRect(cx, cy, w / 2, h, 7, SSD1306_WHITE);
    display.drawFastVLine(cx + (w / 2), cy, h, SSD1306_BLACK);
    display.drawFastHLine(cx + 4, cy + 3, (w / 2) - 8, SSD1306_BLACK);

    int ecgX = ((int)(t * 22.0)) % 138 - 10;
    display.drawFastHLine(0, 54, 128, SSD1306_WHITE);
    if (ecgX > 0 && ecgX < 118) {
      display.drawFastHLine(ecgX, 54, 10, SSD1306_BLACK);
      display.drawLine(ecgX, 54, ecgX + 3, 49, SSD1306_WHITE);
      display.drawLine(ecgX + 3, 49, ecgX + 6, 58, SSD1306_WHITE);
      display.drawLine(ecgX + 6, 58, ecgX + 9, 54, SSD1306_WHITE);
    }

    display.display();
    t += 0.08;
    delay(30);
  }
}

// ================= ANIMAÇÃO 2: INICIANDO PILL DISPENSER (4 SEGUNDOS) =================
void animacaoIniciandoSistema() {
  for (int progresso = 0; progresso <= 100; progresso += 2) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.drawRoundRect(0, 0, 128, 64, 4, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(14, 12);
    display.print(F("Iniciando"));
    display.setCursor(14, 24);
    display.print(F("Pill Dispenser..."));

    display.drawRoundRect(14, 40, 100, 10, 3, SSD1306_WHITE);
    int w = map(progresso, 0, 100, 0, 96);
    if (w > 0) {
      display.fillRoundRect(16, 42, w, 6, 2, SSD1306_WHITE);
    }

    display.setCursor(88, 12);
    display.print(progresso);
    display.print(F("%"));

    display.display();
    delay(75);
  }

  display.clearDisplay();
  display.drawRoundRect(0, 0, 128, 64, 4, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(18, 22);
  display.print(F("SISTEMA PRONTO!"));
  display.setCursor(14, 38);
  display.print(F("Conectando Wi-Fi"));
  display.display();
  delay(600);
}

// ================= TELAS DO OLED (DESENHADAS PARA O IDOSO) =================
void desenharTelaEspera() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(4);
  display.setCursor(4, 5);
  display.print(getHoraAtualStr());

  display.drawFastHLine(4, 42, 120, SSD1306_WHITE);

  display.setTextSize(1);
  if (totalAlarmes > 0) {
    display.setCursor(4, 49);
    display.print(F("Prox: "));
    display.print(getProximoAlarmeStr());
  } else {
    display.setCursor(16, 49);
    display.print(F("Sem alarmes hoje"));
  }

  display.display();
}

void desenharTelaAlarme() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(8);
  int x = (poteAlarmeAtual < 10) ? 40 : 16;
  display.setCursor(x, 0);
  display.print(poteAlarmeAtual);

  display.display();
}

void desenharTelaConfirmacao() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(3);
  display.setCursor(19, 7);
  display.print(F("MUITO"));

  display.setCursor(28, 35);
  display.print(F("BEM!"));

  display.display();
}

// ================= CONTROLE DE ALARME =================
void iniciarAlarme(int pote) {
  alarmeAtivo = true;
  poteAlarmeAtual = pote;
  passoAlarme = 0;
  tempoAlarmeAnterior = millis();

  setBuzzer(true);
  setLed(true);
  desenharTelaAlarme();

  String msg = String(EMOJI_SIRENE) + " *PILL DISPENSER • HORA DO REMEDIO!*\n";
  msg += "━━━━━━━━━━━━━━━━━━━━\n";
  msg += String(EMOJI_RELOGIO) + " *Horario:* `" + getHoraAtualStr() + "`\n";
  msg += String(EMOJI_PILULA) + " *POTINHO A TOMAR: Nº " + String(poteAlarmeAtual) + "*\n\n";
  msg += String(EMOJI_DEDO) + " _O idoso deve pegar a caixinha " + String(poteAlarmeAtual) + " e apertar o botao._";

  enviarAlertaTelegramComBotao(msg);
}

void confirmarDose() {
  alarmeAtivo = false;
  setBuzzer(false);
  setLed(false);

  dosesConfirmadas++;
  tempoTelaConfirmacao = millis();
  desenharTelaConfirmacao();
  emitirBipe();

  String msg = String(EMOJI_CHECK) + " *DOSE CONFIRMADA!*\n";
  msg += "━━━━━━━━━━━━━━━━━━━━\n";
  msg += String(EMOJI_PILULA) + " *Potinho tomado:* Nº " + String(poteAlarmeAtual) + "\n";
  msg += String(EMOJI_HORA) + " *Horario:* " + getHoraAtualStr() + "\n";
  msg += String(EMOJI_GRAFICO) + " *Total hoje:* " + String(dosesConfirmadas) + " doses registradas\n\n";
  msg += "_Tudo tranquilo por aqui!_ " + String(EMOJI_CORACAO);
  enviarTelegram(msg);
}

void processarAlarmeContinuo() {
  if (!alarmeAtivo) return;

  unsigned long agora = millis();

  if (agora - tempoAlarmeAnterior >= (unsigned long)duracoesPassos[passoAlarme]) {
    tempoAlarmeAnterior = agora;
    passoAlarme = (passoAlarme + 1) % totalPassos;

    bool emitirSom = (passoAlarme % 2 == 0);
    setBuzzer(emitirSom);
    setLed(emitirSom);
  }
}

void checarHorarioAutomatico() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  if (timeinfo.tm_hour == ultimaHoraDisparada && timeinfo.tm_min == ultimoMinutoDisparado) {
    return;
  }

  for (int i = 0; i < totalAlarmes; i++) {
    if (alarmes[i].ativo && alarmes[i].hora == timeinfo.tm_hour && alarmes[i].minuto == timeinfo.tm_min) {
      if (!alarmeAtivo) {
        ultimaHoraDisparada = timeinfo.tm_hour;
        ultimoMinutoDisparado = timeinfo.tm_min;
        iniciarAlarme(alarmes[i].pote);
      }
      break;
    }
  }
}

// ================= COMANDOS DO TELEGRAM =================
void processarComandosTelegram() {
  // NÃO trava o processador no Telegram se o alarme estiver tocando!
  if (alarmeAtivo) return;

  int numNovasMensagens = bot.getUpdates(bot.last_message_received + 1);

  while (numNovasMensagens) {
    for (int i = 0; i < numNovasMensagens; i++) {
      String chat_id = String(bot.messages[i].chat_id);
      if (chat_id != CHAT_ID) continue;

      String tipo = bot.messages[i].type;
      String texto = bot.messages[i].text;
      texto.trim();

      if (tipo == "callback_query" || texto == "/confirmar") {
        if (alarmeAtivo) {
          confirmarDose();
          bot.answerCallbackQuery(bot.messages[i].query_id, "Dose confirmada com sucesso!");
        } else {
          bot.answerCallbackQuery(bot.messages[i].query_id, "Nenhum alarme tocando no momento.");
        }
        continue;
      }

      if (texto.startsWith("/horarios ") || texto.startsWith("/rotina ")) {
        int pos = 0;
        totalAlarmes = 0;

        while ((pos = texto.indexOf(':', pos)) != -1 && totalAlarmes < MAX_ALARMES) {
          int startH = pos - 1;
          while (startH >= 0 && isDigit(texto.charAt(startH))) startH--;
          startH++;
          String hStr = texto.substring(startH, pos);

          int endM = pos + 1;
          while (endM < texto.length() && isDigit(texto.charAt(endM))) endM++;
          String mStr = texto.substring(pos + 1, endM);

          int h = hStr.toInt();
          int m = mStr.toInt();
          int p = totalAlarmes + 1;

          int searchPote = endM;
          while (searchPote < texto.length() && (texto.charAt(searchPote) == ' ' || texto.charAt(searchPote) == 'p' || texto.charAt(searchPote) == 'P')) {
            searchPote++;
          }
          if (searchPote < texto.length() && isDigit(texto.charAt(searchPote))) {
            int endP = searchPote;
            while (endP < texto.length() && isDigit(texto.charAt(endP))) endP++;
            p = texto.substring(searchPote, endP).toInt();
            endM = endP;
          }

          if (p < 1 || p > 20) p = totalAlarmes + 1;

          if (h >= 0 && h <= 23 && m >= 0 && m <= 59) {
            alarmes[totalAlarmes].hora = h;
            alarmes[totalAlarmes].minuto = m;
            alarmes[totalAlarmes].pote = p;
            alarmes[totalAlarmes].ativo = true;
            totalAlarmes++;
          }
          pos = endM;
        }

        ordenarAlarmes();
        salvarAlarmesNaFlash();

        String resp = String(EMOJI_RELOGIO) + " *ROTINA DE HORARIOS ATUALIZADA!*\n";
        resp += "━━━━━━━━━━━━━━━━━━━━\n";
        if (totalAlarmes > 0) {
          for (int a = 0; a < totalAlarmes; a++) {
            resp += String(a + 1) + ". `" + getHoraFormatada(alarmes[a].hora, alarmes[a].minuto) + "` ➔ *Pote " + String(alarmes[a].pote) + "*\n";
          }
          resp += "\n" + String(EMOJI_SINO) + " *Proximo:* `" + getProximoAlarmeStr() + "`\n";
          resp += "Salvo permanentemente na memoria!";
        } else {
          resp += "Formato invalido. Exemplo:\n`/horarios 08:00 1, 14:00 5, 20:00 2`";
        }

        bot.sendMessage(CHAT_ID, resp, "Markdown");
        desenharTelaEspera();
      }
      else if (texto.startsWith("/add ") || texto.startsWith("/alarme ")) {
        if (totalAlarmes >= MAX_ALARMES) {
          bot.sendMessage(CHAT_ID, "Limite de " + String(MAX_ALARMES) + " alarmes atingido! Use `/limpar` para reiniciar.", "Markdown");
        } else {
          String args = texto.substring(texto.indexOf(' ') + 1);
          args.trim();
          int doisPontos = args.indexOf(':');

          if (doisPontos != -1) {
            int h = args.substring(0, doisPontos).toInt();
            int endM = doisPontos + 1;
            while (endM < args.length() && isDigit(args.charAt(endM))) endM++;
            int m = args.substring(doisPontos + 1, endM).toInt();

            int p = totalAlarmes + 1;
            int searchP = endM;
            while (searchP < args.length() && (args.charAt(searchP) == ' ' || args.charAt(searchP) == 'p' || args.charAt(searchP) == 'P')) searchP++;
            if (searchP < args.length() && isDigit(args.charAt(searchP))) {
              p = args.substring(searchP).toInt();
            }

            if (p < 1 || p > 20) p = 1;

            if (h >= 0 && h <= 23 && m >= 0 && m <= 59) {
              alarmes[totalAlarmes].hora = h;
              alarmes[totalAlarmes].minuto = m;
              alarmes[totalAlarmes].pote = p;
              alarmes[totalAlarmes].ativo = true;
              totalAlarmes++;

              ordenarAlarmes();
              salvarAlarmesNaFlash();

              String resp = String(EMOJI_CHECK) + " *HORARIO ADICIONADO!*\n";
              resp += "Alarme: `" + getHoraFormatada(h, m) + "` ➔ *Pote " + String(p) + "*\n";
              resp += String(EMOJI_SINO) + " Proximo alarme: `" + getProximoAlarmeStr() + "`";
              bot.sendMessage(CHAT_ID, resp, "Markdown");
              desenharTelaEspera();
            } else {
              bot.sendMessage(CHAT_ID, "Horario invalido! Exemplo: `/add 08:00 3`", "Markdown");
            }
          }
        }
      }
      else if (texto == "/alarmes" || texto == "/listar") {
        String resp = String(EMOJI_RELOGIO) + " *ROTINA DE REMEDIOS (POTINHOS 1 A 20)*\n";
        resp += "━━━━━━━━━━━━━━━━━━━━\n";
        if (totalAlarmes == 0) {
          resp += "Nenhum alarme programado.\nDefina com: `/add 08:00 3` ou `/horarios 08:00 1, 14:00 5`";
        } else {
          for (int a = 0; a < totalAlarmes; a++) {
            resp += String(a + 1) + ". `" + getHoraFormatada(alarmes[a].hora, alarmes[a].minuto) + "` ➔ *Potinho " + String(alarmes[a].pote) + "*\n";
          }
          resp += "\n" + String(EMOJI_SINO) + " *Proximo:* `" + getProximoAlarmeStr() + "`\n";
          resp += String(EMOJI_HORA) + " *Hora Atual:* `" + getHoraAtualStr() + "`";
        }
        bot.sendMessage(CHAT_ID, resp, "Markdown");
      }
      else if (texto == "/limpar") {
        totalAlarmes = 0;
        salvarAlarmesNaFlash();
        bot.sendMessage(CHAT_ID, String(EMOJI_CHECK) + " *Todos os alarmes foram apagados!*", "Markdown");
        desenharTelaEspera();
      }
      else if (texto.startsWith("/disparar")) {
        int poteTeste = 1;
        int espaco = texto.indexOf(' ');
        if (espaco != -1) {
          poteTeste = texto.substring(espaco + 1).toInt();
          if (poteTeste < 1 || poteTeste > 20) poteTeste = 1;
        }
        iniciarAlarme(poteTeste);
      }
      else if (texto == "/status") {
        String st = String(EMOJI_PILULA) + " *STATUS • PILL DISPENSER*\n";
        st += "━━━━━━━━━━━━━━━━━━━━\n";
        st += String(EMOJI_HORA) + " *Hora Atual:* " + getHoraAtualStr() + "\n";
        st += String(EMOJI_SINO) + " *Proximo Alarme:* `" + getProximoAlarmeStr() + "`\n";
        st += String(EMOJI_GRAFICO) + " *Doses Tomadas Hoje:* " + String(dosesConfirmadas) + "\n";
        st += String(EMOJI_LAMPADA) + " *LED Bancada:* " + String(ledLigado ? "LIGADO" : "DESLIGADO");
        bot.sendMessage(CHAT_ID, st, "Markdown");
      }
      else if (texto.startsWith("/brilho ")) {
        int b = texto.substring(8).toInt();
        if (b < 0) b = 0;
        if (b > 255) b = 255;
        setBrilhoDisplay(b);
        bot.sendMessage(CHAT_ID, "☀️ *Brilho ajustado para:* `" + String(b) + "/255`", "Markdown");
      }
      else if (texto == "/inverter") {
        telaInvertida = !telaInvertida;
        display.invertDisplay(telaInvertida);
        if (telaInvertida) {
          bot.sendMessage(CHAT_ID, "🌓 *Modo Invertido ATIVADO!*\nFundo da tela aceso e letras pretas.", "Markdown");
        } else {
          bot.sendMessage(CHAT_ID, "🌑 *Modo Normal ATIVADO!*\nFundo preto e letras acesas.", "Markdown");
        }
      }
      else if (texto == "/start" || texto == "/ajuda") {
        String ajuda = String(EMOJI_PILULA) + " *CENTRAL DO PILL DISPENSER*\n";
        ajuda += "━━━━━━━━━━━━━━━━━━━━\n\n";
        ajuda += "⏰ `/add 08:00 3` - Adiciona alarme com o numero do pote (1 a 20)\n";
        ajuda += "📋 `/horarios 08:00 1, 14:00 5` - Define rotina completa\n";
        ajuda += "📋 `/alarmes` - Lista os remedios e caixinhas\n";
        ajuda += "☀️ `/brilho 255` - Ajusta brilho da tela (0 a 255)\n";
        ajuda += "🌓 `/inverter` - Alterna entre modo normal e invertido\n";
        ajuda += "🗑️ `/limpar` - Apaga todos os horarios\n";
        ajuda += "📊 `/status` - Relogio, doses e status\n";
        ajuda += "🚨 `/disparar 3` - Testa alarme chamando o pote 3";
        bot.sendMessage(CHAT_ID, ajuda, "Markdown");
      }
    }
    numNovasMensagens = bot.getUpdates(bot.last_message_received + 1);
  }
}

// ================= PÁGINA WEB =================
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Pill Dispenser</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background: #090d16; color: #f1f5f9; display: flex; justify-content: center; align-items: center; min-height: 100vh; padding: 20px; }
    .card { background: #111827; border: 1px solid #1e293b; border-radius: 24px; padding: 28px 24px; width: 100%; max-width: 400px; text-align: center; box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.7); }
    .brand-tag { font-size: 11px; font-weight: 700; letter-spacing: 0.16em; text-transform: uppercase; color: #64748b; margin-bottom: 6px; }
    h1 { font-size: 22px; font-weight: 600; color: #f8fafc; letter-spacing: -0.02em; margin-bottom: 20px; }
    .grid-status { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 18px; }
    .status-tile { background: #0b0f19; border: 1px solid #1e293b; padding: 14px 10px; border-radius: 14px; text-align: center; }
    .tile-label { font-size: 10px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.08em; color: #64748b; margin-bottom: 6px; }
    .tile-val { font-size: 18px; font-weight: 700; color: #f1f5f9; }
    .val-alarm { font-size: 12px; font-weight: 700; padding: 4px 8px; border-radius: 6px; display: inline-block; }
    .st-wait { background: #064e3b33; color: #34d399; border: 1px solid #05966955; }
    .st-ring { background: #7f1d1d44; color: #f87171; border: 1px solid #ef4444; animation: pulse 1s infinite; }
    @keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.6; } }
    .box-schedule { background: #0b0f19; border: 1px solid #1e293b; padding: 16px; border-radius: 16px; margin-bottom: 18px; text-align: left; }
    .box-schedule-title { font-size: 11px; font-weight: 600; text-transform: uppercase; letter-spacing: 0.06em; color: #94a3b8; margin-bottom: 10px; display: flex; justify-content: space-between; }
    .input-row { display: flex; gap: 8px; margin-bottom: 12px; }
    input[type="time"] { background: #1e293b; border: 1px solid #334155; color: #f8fafc; font-size: 15px; font-weight: 600; padding: 10px 12px; border-radius: 10px; outline: none; width: 130px; text-align: center; }
    input[type="time"]::-webkit-calendar-picker-indicator { filter: invert(1); }
    .inp-pote { background: #1e293b; border: 1px solid #334155; color: #f8fafc; font-size: 14px; font-weight: 600; padding: 10px 8px; border-radius: 10px; outline: none; width: 80px; text-align: center; }
    .btn-save { background: #3b82f6; color: white; border: none; padding: 10px 14px; font-size: 13px; font-weight: 600; border-radius: 10px; cursor: pointer; flex-grow: 1; }
    .alarm-list { display: flex; flex-wrap: wrap; gap: 8px; min-height: 28px; }
    .alarm-chip { background: #1e293b; border: 1px solid #334155; padding: 6px 10px; border-radius: 8px; font-size: 12px; font-weight: 600; display: inline-flex; align-items: center; gap: 6px; }
    .alarm-chip b { color: #60a5fa; }
    .alarm-chip span.del { color: #ef4444; cursor: pointer; font-size: 14px; padding-left: 4px; }
    button.btn-action { display: block; width: 100%; border: none; padding: 13px; margin: 8px 0; border-radius: 12px; font-size: 14px; font-weight: 600; cursor: pointer; transition: 0.15s; }
    button.btn-action:active { transform: scale(0.98); }
    .btn-confirm { background: #059669; color: white; }
    .btn-trigger { background: #b91c1c; color: white; }
    .btn-led-toggle { background: #1e293b; color: #cbd5e1; border: 1px solid #334155; }
    .footer { margin-top: 16px; font-size: 11px; color: #475569; }
  </style>
</head>
<body>
  <div class="card">
    <div class="brand-tag">Assistente de Medicacao</div>
    <h1>Pill Dispenser</h1>

    <div class="grid-status">
      <div class="status-tile">
        <div class="tile-label">Relogio</div>
        <div class="tile-val" id="st-hora">--:--</div>
      </div>
      <div class="status-tile">
        <div class="tile-label">Alarme</div>
        <div class="val-alarm st-wait" id="st-alarm">EM ESPERA</div>
      </div>
    </div>

    <div class="box-schedule">
      <div class="box-schedule-title">
        <span>Horarios & Potinhos (1-20)</span>
        <span id="st-proximo" style="color: #60a5fa;">Prox: --:--</span>
      </div>
      <div class="input-row">
        <input type="time" id="inp-hora" value="08:00">
        <input type="number" id="inp-pote" class="inp-pote" min="1" max="20" value="1" title="Numero do Pote">
        <button type="button" class="btn-save" onclick="adicionarHorario()">+ Add</button>
      </div>
      <div class="alarm-list" id="lista-alarmes"></div>
    </div>

    <button type="button" class="btn-action btn-confirm" onclick="chamar('/silenciar')">✅ Confirmar Medicação</button>
    <button type="button" class="btn-action btn-trigger" onclick="chamar('/disparar-alarme')">🚨 Testar Alarme Agora</button>
    <button type="button" class="btn-action btn-led-toggle" onclick="chamar('/toggle-led')">💡 Alternar LED da Bancada</button>

    <div class="footer">Gerenciamento via Telegram: @pill_disp_bot</div>
  </div>

  <script>
    function render(data) {
      document.getElementById('st-hora').textContent = data.hora;
      const alarmEl = document.getElementById('st-alarm');
      if (data.alarme) {
        alarmEl.textContent = 'POTE ' + data.poteAtivo;
        alarmEl.className = 'val-alarm st-ring';
      } else {
        alarmEl.textContent = 'EM ESPERA';
        alarmEl.className = 'val-alarm st-wait';
      }
      document.getElementById('st-proximo').textContent = 'Prox: ' + data.prox;
      const container = document.getElementById('lista-alarmes');
      container.innerHTML = '';
      if (!data.alarmes || data.alarmes.length === 0) {
        container.innerHTML = '<span style="font-size:12px;color:#64748b;">Nenhum horário cadastrado</span>';
      } else {
        data.alarmes.forEach((item, idx) => {
          const chip = document.createElement('div');
          chip.className = 'alarm-chip';
          chip.innerHTML = item.hora + ' ➔ <b>Pote ' + item.pote + '</b> <span class="del" onclick="removerHorario(' + idx + ')">&times;</span>';
          container.appendChild(chip);
        });
      }
    }
    function chamar(url) { fetch(url).then(r => r.json()).then(render); }
    function adicionarHorario() {
      const hora = document.getElementById('inp-hora').value;
      const pote = document.getElementById('inp-pote').value;
      if (!hora) return;
      const partes = hora.split(':');
      chamar('/add-alarme?h=' + partes[0] + '&m=' + partes[1] + '&p=' + pote);
    }
    function removerHorario(idx) { chamar('/del-alarme?idx=' + idx); }
    setInterval(() => { fetch('/status').then(r => r.json()).then(render); }, 1200);
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void enviarStatusJson() {
  String json = "{\"led\":" + String(ledLigado ? "true" : "false") +
                ",\"alarme\":" + String(alarmeAtivo ? "true" : "false") +
                ",\"poteAtivo\":" + String(poteAlarmeAtual) +
                ",\"doses\":" + String(dosesConfirmadas) +
                ",\"hora\":\"" + getHoraAtualStr() + "\"" +
                ",\"prox\":\"" + getProximoAlarmeStr() + "\"" +
                ",\"alarmes\":[";

  for (int i = 0; i < totalAlarmes; i++) {
    json += "{\"hora\":\"" + getHoraFormatada(alarmes[i].hora, alarmes[i].minuto) + "\",\"pote\":" + String(alarmes[i].pote) + "}";
    if (i < totalAlarmes - 1) json += ",";
  }
  json += "]}";

  server.send(200, "application/json", json);
}

void handleAddAlarmeWeb() {
  if (server.hasArg("h") && server.hasArg("m")) {
    int h = server.arg("h").toInt();
    int m = server.arg("m").toInt();
    int p = server.hasArg("p") ? server.arg("p").toInt() : 1;
    if (p < 1 || p > 20) p = 1;

    if (totalAlarmes < MAX_ALARMES && h >= 0 && h <= 23 && m >= 0 && m <= 59) {
      alarmes[totalAlarmes].hora = h;
      alarmes[totalAlarmes].minuto = m;
      alarmes[totalAlarmes].pote = p;
      alarmes[totalAlarmes].ativo = true;
      totalAlarmes++;

      ordenarAlarmes();
      salvarAlarmesNaFlash();
      desenharTelaEspera();
    }
  }
  enviarStatusJson();
}

void handleDelAlarmeWeb() {
  if (server.hasArg("idx")) {
    int idx = server.arg("idx").toInt();
    if (idx >= 0 && idx < totalAlarmes) {
      for (int i = idx; i < totalAlarmes - 1; i++) {
        alarmes[i] = alarmes[i + 1];
      }
      totalAlarmes--;
      salvarAlarmesNaFlash();
      desenharTelaEspera();
    }
  }
  enviarStatusJson();
}

void handleToggleLed() {
  if (!alarmeAtivo) {
    setLed(!ledLigado);
    desenharTelaEspera();
  }
  enviarStatusJson();
}

void handleDispararAlarme() {
  iniciarAlarme(1);
  enviarStatusJson();
}

void handleSilenciar() {
  if (alarmeAtivo) {
    confirmarDose();
  }
  enviarStatusJson();
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(PINO_LED, OUTPUT);
  pinMode(PINO_BUZZER, OUTPUT);
  pinMode(PINO_BOTAO, INPUT_PULLUP);

  digitalWrite(PINO_BUZZER, LOW);
  digitalWrite(PINO_LED, LOW);
  ledLigado = false;
  alarmeAtivo = false;

  carregarAlarmesDaFlash();

  if(!display.begin(SSD1306_SWITCHCAPVCC, ENDERECO_OLED)) {
    Serial.println(F("Falha ao iniciar OLED!"));
  } else {
    setBrilhoDisplay(255);

    setLed(true);
    setBuzzer(true);
    delay(80);
    setLed(false);
    setBuzzer(false);
    delay(90);
    setLed(true);
    setBuzzer(true);
    delay(80);
    setLed(false);
    setBuzzer(false);

    animacaoRemedinhoElegante();
    animacaoIniciandoSistema();
  }

  secured_client.setInsecure();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando ao iPhone");

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 25) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("CONECTADO! IP: ");
    Serial.println(WiFi.localIP());
    configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  }

  server.on("/", handleRoot);
  server.on("/status", enviarStatusJson);
  server.on("/toggle-led", handleToggleLed);
  server.on("/disparar-alarme", handleDispararAlarme);
  server.on("/silenciar", handleSilenciar);
  server.on("/add-alarme", handleAddAlarmeWeb);
  server.on("/del-alarme", handleDelAlarmeWeb);

  server.begin();

  desenharTelaEspera();

  setBuzzer(true);
  delay(80);
  setBuzzer(false);

  if (WiFi.status() == WL_CONNECTED) {
    String boasVindas = String(EMOJI_FOGUETE) + " *PILL DISPENSER ONLINE!*\n";
    boasVindas += "━━━━━━━━━━━━━━━━━━━━\n";
    boasVindas += "Dispositivo pronto na bancada.\n";
    if (totalAlarmes > 0) {
      boasVindas += String(EMOJI_SINO) + " " + String(totalAlarmes) + " alarmes restaurados da memoria!\n";
      boasVindas += "Proximo: `" + getProximoAlarmeStr() + "`\n\n";
    } else {
      boasVindas += "Nenhum alarme salvo. Use `/add 08:00 3` para definir.\n\n";
    }
    boasVindas += "Envie `/ajuda` para ver os comandos.";
    enviarTelegram(boasVindas);
  }
}

// ================= LOOP =================
void loop() {
  server.handleClient();

  processarAlarmeContinuo();

  checarHorarioAutomatico();

  // Atualiza relogio da tela a cada minuto
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    if (timeinfo.tm_min != ultimoMinutoExibido) {
      ultimoMinutoExibido = timeinfo.tm_min;
      if (!alarmeAtivo && tempoTelaConfirmacao == 0) {
        desenharTelaEspera();
      }
    }
  }

  // Consulta o Telegram apenas em repouso (evita travar o ESP32 e ignorar o botão durante o alarme)
  if (!alarmeAtivo && (millis() - ultimoTempoTelegramCheck > 2500)) {
    ultimoTempoTelegramCheck = millis();
    processarComandosTelegram();
  }

  // Limpa tela de confirmacao apos 3 segundos
  if (tempoTelaConfirmacao > 0 && !alarmeAtivo) {
    if (millis() - tempoTelaConfirmacao > 3000) {
      tempoTelaConfirmacao = 0;
      desenharTelaEspera();
    }
  }

  // ================= LEITURA DIRETA E ULTRA-RESPONSIVA DO BOTÃO =================
  // Como usamos INPUT_PULLUP (pino 13 -> botao -> GND):
  // Botao APERTADO = LOW (0)
  // Botao SOLTO = HIGH (1)
  bool botaoEstaApertado = (digitalRead(PINO_BOTAO) == LOW);

  if (botaoEstaApertado && !botaoPressionadoAnterior) {
    if (millis() - ultimoDebounceTime > 40) {
      ultimoDebounceTime = millis();
      botaoPressionadoAnterior = true;

      Serial.println(">>> CLIQUE NO BOTAO DETECTADO! <<<");

      if (alarmeAtivo) {
        // Silencia na hora e registra que tomou o remédio!
        confirmarDose();
      } else {
        // Em repouso: liga/desliga a luz da bancada
        setLed(!ledLigado);
        emitirBipe();
        desenharTelaEspera();
      }
    }
  } else if (!botaoEstaApertado && botaoPressionadoAnterior) {
    if (millis() - ultimoDebounceTime > 40) {
      ultimoDebounceTime = millis();
      botaoPressionadoAnterior = false;
    }
  }
}
