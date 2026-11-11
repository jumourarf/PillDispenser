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

// ================= CONFIGURAÇÕES DO TELEGRAM =================                                                                                                                         ª°º
#define BOT_TOKEN "BOT_TOKEN_AQUI"
#define CHAT_ID   "CHAT_ID_AQUI"

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);

// ================= REDE DO SEU IPHONE =================
const char* WIFI_SSID = "SEU_WIFI"; 
const char* WIFI_PASS = "SUA_SENHA";

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

  // Telegram configurado para NÃO enviar alerta no disparo,
  // enviando apenas no boot e na confirmação da dose tomada.
}

void confirmarDose() {
  alarmeAtivo = false;
  setBuzzer(false);
  setLed(false);

  dosesConfirmadas++;
  tempoTelaConfirmacao = millis();
  desenharTelaConfirmacao();
  emitirBipe();

  String msg = "Dose confirmada!\nPote " + String(poteAlarmeAtual) + " • " + getHoraAtualStr();
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
  int numNovasMensagens = bot.getUpdates(bot.last_message_received + 1);

  while (numNovasMensagens) {
    for (int i = 0; i < numNovasMensagens; i++) {
      String chat_id = String(bot.messages[i].chat_id);
      if (chat_id != CHAT_ID) continue;

      String tipo = bot.messages[i].type;
      String texto = bot.messages[i].text;
      texto.trim();

      // 1. CONFIRMAR DOSE
      if (tipo == "callback_query" || texto == "/confirmar") {
        confirmarDose();
        continue;
      }

      // 2. DEFINIR ROTINA COMPLETA (/horarios)
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

        String resp = "Rotina atualizada!\n";
        if (totalAlarmes > 0) {
          for (int a = 0; a < totalAlarmes; a++) {
            resp += String(a + 1) + ". " + getHoraFormatada(alarmes[a].hora, alarmes[a].minuto) + " • Pote " + String(alarmes[a].pote) + "\n";
          }
          resp += "\nProximo: " + getProximoAlarmeStr();
        } else {
          resp += "Formato invalido. Exemplo:\n/horarios 08:00 1, 14:00 2, 20:00 3";
        }

        bot.sendMessage(CHAT_ID, resp, "");
        desenharTelaEspera();
      }

      // 3. ADICIONAR ALARME INDIVIDUAL (/adicionar ou /add)
      else if (texto.startsWith("/adicionar ") || texto.startsWith("/add ") || texto.startsWith("/alarme ")) {
        if (totalAlarmes >= MAX_ALARMES) {
          bot.sendMessage(CHAT_ID, "Limite de " + String(MAX_ALARMES) + " alarmes atingido!", "");
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

              String resp = "Alarme adicionado!\n" + getHoraFormatada(h, m) + " • Pote " + String(p);
              bot.sendMessage(CHAT_ID, resp, "");
              desenharTelaEspera();
            } else {
              bot.sendMessage(CHAT_ID, "Horario invalido! Exemplo: /adicionar 08:00 1", "");
            }
          }
        }
      }

      // 4. LISTAR ALARMES (/alarmes)
      else if (texto == "/alarmes" || texto == "/listar") {
        String resp = "Alarmes agendados:\n";
        if (totalAlarmes == 0) {
          resp += "Nenhum alarme programado.";
        } else {
          for (int a = 0; a < totalAlarmes; a++) {
            resp += String(a + 1) + ". " + getHoraFormatada(alarmes[a].hora, alarmes[a].minuto) + " • Pote " + String(alarmes[a].pote) + "\n";
          }
          resp += "\nProximo: " + getProximoAlarmeStr();
        }
        bot.sendMessage(CHAT_ID, resp, "");
      }

      // 5. STATUS DO DISPOSITIVO (/status)
      else if (texto == "/status") {
        String st = "Status: Online\n";
        st += "Hora atual: " + getHoraAtualStr() + "\n";
        st += "Proximo alarme: " + getProximoAlarmeStr() + "\n";
        st += "Doses tomadas hoje: " + String(dosesConfirmadas);
        bot.sendMessage(CHAT_ID, st, "");
      }

      // COMANDO OCULTO / BANCADA (/disparar)
      else if (texto.startsWith("/disparar")) {
        int poteTeste = 1;
        int espaco = texto.indexOf(' ');
        if (espaco != -1) {
          poteTeste = texto.substring(espaco + 1).toInt();
          if (poteTeste < 1 || poteTeste > 20) poteTeste = 1;
        }
        iniciarAlarme(poteTeste);
        bot.sendMessage(CHAT_ID, "Alarme disparado!\nPote " + String(poteTeste), "");
      }

      // AJUDA / START
      else if (texto == "/start" || texto == "/ajuda") {
        String ajuda = "Comandos do Pill Dispenser:\n\n";
        ajuda += "/status - Verifica status do aparelho\n";
        ajuda += "/alarmes - Lista os horarios agendados\n";
        ajuda += "/confirmar - Confirma a dose tomada\n";
        ajuda += "/adicionar 08:00 1 - Adiciona horario e pote\n";
        ajuda += "/horarios 08:00 1, 14:00 2 - Define rotina completa";
        bot.sendMessage(CHAT_ID, ajuda, "");
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
  <title>Pill Dispenser - Painel do Cuidador</title>
  <style>
    :root {
      --bg-main: #0c1322;
      --card-bg: #151f33;
      --card-inner: #0f172a;
      --primary: #3b82f6;
      --primary-hover: #2563eb;
      --success: #10b981;
      --success-bg: rgba(16, 185, 129, 0.15);
      --danger: #ef4444;
      --danger-bg: rgba(239, 68, 68, 0.18);
      --warning: #f59e0b;
      --border: #223250;
      --text: #f8fafc;
      --text-muted: #94a3b8;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background: var(--bg-main); color: var(--text); display: flex; justify-content: center; padding: 24px 16px; min-height: 100vh; }
    .container { width: 100%; max-width: 480px; display: flex; flex-direction: column; gap: 16px; }
    .header { background: var(--card-bg); border: 1px solid var(--border); border-radius: 20px; padding: 16px 20px; display: flex; align-items: center; justify-content: space-between; }
    .patient-info { display: flex; align-items: center; gap: 12px; cursor: pointer; user-select: none; padding: 4px 8px 4px 4px; border-radius: 14px; transition: background 0.2s; }
    .patient-info:hover { background: rgba(255, 255, 255, 0.05); }
    .avatar { width: 46px; height: 46px; background: linear-gradient(135deg, #3b82f6, #8b5cf6); border-radius: 50%; display: flex; align-items: center; justify-content: center; font-size: 24px; position: relative; }
    .avatar-edit-badge { position: absolute; bottom: -2px; right: -2px; background: #1e293b; border: 1px solid #475569; border-radius: 50%; width: 18px; height: 18px; font-size: 10px; display: flex; align-items: center; justify-content: center; }
    .patient-meta h2 { font-size: 16px; font-weight: 700; color: #fff; }
    .patient-meta p { font-size: 11px; color: #38bdf8; }
    .clock-pill { background: var(--card-inner); border: 1px solid var(--border); padding: 6px 12px; border-radius: 999px; font-size: 14px; font-weight: 700; color: #38bdf8; display: flex; align-items: center; gap: 6px; }
    .modal-overlay { display: none; position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0,0,0,0.75); backdrop-filter: blur(4px); z-index: 999; align-items: center; justify-content: center; padding: 16px; }
    .modal-overlay.open { display: flex; }
    .modal-content { background: #131d31; border: 1px solid #293d62; border-radius: 20px; padding: 24px; width: 100%; max-width: 380px; text-align: center; }
    .modal-title { font-size: 16px; font-weight: 700; color: #fff; margin-bottom: 16px; }
    .avatar-picker { display: flex; justify-content: center; gap: 16px; margin-bottom: 18px; }
    .avatar-opt { width: 56px; height: 56px; border-radius: 50%; background: var(--card-inner); border: 2px solid var(--border); font-size: 28px; display: flex; align-items: center; justify-content: center; cursor: pointer; transition: all 0.2s; }
    .avatar-opt.selected { border-color: #38bdf8; background: #0284c7; transform: scale(1.08); }
    .modal-input { width: 100%; background: var(--card-inner); border: 1px solid var(--border); color: #fff; font-size: 15px; font-weight: 600; padding: 12px 14px; border-radius: 12px; outline: none; margin-bottom: 18px; text-align: center; }
    .modal-btns { display: flex; gap: 10px; }
    .btn-modal { flex: 1; padding: 12px; border-radius: 10px; font-size: 14px; font-weight: 700; cursor: pointer; border: none; }
    .btn-modal-save { background: var(--primary); color: #fff; }
    .btn-modal-cancel { background: #1e293b; color: #94a3b8; }
    .status-banner { background: var(--card-bg); border: 2px solid var(--border); border-radius: 20px; padding: 20px; text-align: center; transition: all 0.3s ease; position: relative; overflow: hidden; }
    .status-banner.normal { border-color: rgba(16, 185, 129, 0.4); background: linear-gradient(180deg, rgba(16, 185, 129, 0.08) 0%, var(--card-bg) 100%); }
    .status-banner.ringing { border-color: var(--danger); background: linear-gradient(180deg, var(--danger-bg) 0%, var(--card-bg) 100%); animation: alertPulse 1.2s infinite alternate; }
    @keyframes alertPulse { 0% { box-shadow: 0 0 15px rgba(239, 68, 68, 0.3); } 100% { box-shadow: 0 0 30px rgba(239, 68, 68, 0.7); } }
    .banner-icon { font-size: 38px; margin-bottom: 6px; }
    .banner-title { font-size: 18px; font-weight: 800; margin-bottom: 4px; }
    .status-banner.normal .banner-title { color: #34d399; }
    .status-banner.ringing .banner-title { color: #f87171; }
    .banner-subtitle { font-size: 14px; color: var(--text-muted); }
    .banner-action-btn { margin-top: 14px; background: var(--success); color: white; border: none; padding: 12px 20px; border-radius: 12px; font-size: 15px; font-weight: 700; cursor: pointer; display: none; width: 100%; box-shadow: 0 4px 14px rgba(16, 185, 129, 0.4); }
    .status-banner.ringing .banner-action-btn { display: block; }
    .card { background: var(--card-bg); border: 1px solid var(--border); border-radius: 20px; padding: 22px; }
    .card-title { font-size: 15px; font-weight: 700; color: #fff; display: flex; align-items: center; gap: 8px; margin-bottom: 16px; }
    .form-group { margin-bottom: 16px; }
    .form-label { font-size: 12px; font-weight: 600; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.05em; margin-bottom: 8px; display: flex; justify-content: space-between; align-items: center; }
    .form-label span.hint { font-size: 11px; text-transform: none; color: #38bdf8; font-weight: 600; }
    .input-text { width: 100%; background: var(--card-inner); border: 1px solid var(--border); color: #fff; font-size: 15px; font-weight: 600; padding: 12px 14px; border-radius: 12px; outline: none; }
    .time-shortcuts { display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px; margin-bottom: 10px; }
    .btn-chip { background: var(--card-inner); border: 1px solid var(--border); color: #cbd5e1; padding: 8px 4px; border-radius: 10px; font-size: 11px; font-weight: 700; cursor: pointer; text-align: center; }
    .btn-chip.active { background: #1e3a8a; border-color: var(--primary); color: #60a5fa; }
    .custom-time-row { display: flex; align-items: center; gap: 10px; }
    input[type="time"] { flex: 1; background: var(--card-inner); border: 1px solid var(--border); color: #fff; font-size: 16px; font-weight: 700; padding: 11px 14px; border-radius: 12px; outline: none; text-align: center; }
    input[type="time"]::-webkit-calendar-picker-indicator { filter: invert(1); cursor: pointer; }
    .pote-selector { display: grid; grid-template-columns: repeat(5, 1fr); gap: 6px; margin-top: 6px; }
    .pote-btn { background: var(--card-inner); border: 1.5px solid var(--border); color: var(--text-muted); border-radius: 10px; padding: 8px 0; font-size: 13px; font-weight: 800; cursor: pointer; text-align: center; display: flex; flex-direction: column; align-items: center; gap: 1px; }
    .pote-btn span.label { font-size: 8px; font-weight: 600; opacity: 0.7; }
    .pote-btn:hover { border-color: var(--primary); color: #fff; }
    .pote-btn.selected { background: #0284c7; border-color: #38bdf8; color: #fff; box-shadow: 0 0 10px rgba(56, 189, 248, 0.4); transform: scale(1.04); }
    .btn-submit { width: 100%; background: var(--primary); color: white; border: none; padding: 14px; border-radius: 14px; font-size: 15px; font-weight: 700; cursor: pointer; display: flex; align-items: center; justify-content: center; gap: 8px; margin-top: 14px; }
    .schedule-list { display: flex; flex-direction: column; gap: 10px; }
    .schedule-item { background: var(--card-inner); border: 1px solid var(--border); border-radius: 14px; padding: 14px 16px; display: flex; align-items: center; justify-content: space-between; }
    .item-left { display: flex; align-items: center; gap: 14px; }
    .time-badge { background: #1e293b; border: 1px solid #334155; color: #38bdf8; font-size: 15px; font-weight: 800; padding: 8px 12px; border-radius: 10px; min-width: 68px; text-align: center; }
    .med-details h4 { font-size: 14px; font-weight: 700; color: #fff; }
    .med-details p { font-size: 12px; color: var(--text-muted); margin-top: 2px; display: flex; align-items: center; gap: 6px; }
    .pote-tag { background: #0369a1; color: #e0f2fe; font-weight: 700; font-size: 11px; padding: 2px 6px; border-radius: 6px; }
    .btn-delete { background: transparent; border: none; color: #64748b; font-size: 18px; cursor: pointer; padding: 8px; border-radius: 8px; }
    .btn-delete:hover { color: var(--danger); }
    .empty-state { text-align: center; padding: 24px; color: var(--text-muted); font-size: 13px; }
    .dev-mode-wrapper { margin-top: 6px; display: flex; flex-direction: column; align-items: center; gap: 10px; }
    .btn-toggle-dev { background: transparent; border: 1px dashed #334155; color: #64748b; padding: 6px 14px; border-radius: 999px; font-size: 11px; font-weight: 600; cursor: pointer; display: inline-flex; align-items: center; gap: 6px; }
    .dev-panel { display: none; width: 100%; background: #0b0f19; border: 1px solid #1e293b; border-radius: 16px; padding: 16px; text-align: left; }
    .dev-panel.open { display: block; }
    .dev-panel-title { font-size: 11px; font-weight: 700; text-transform: uppercase; color: #f59e0b; margin-bottom: 12px; }
    .dev-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    .btn-test { background: var(--card-inner); border: 1px solid var(--border); color: #cbd5e1; padding: 10px; border-radius: 10px; font-size: 12px; font-weight: 700; cursor: pointer; display: flex; align-items: center; justify-content: center; gap: 6px; }
    .btn-test.alarm-btn { color: #f87171; border-color: rgba(239, 68, 68, 0.3); }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <div class="patient-info" onclick="abrirModalEdicao()" title="Clique para editar paciente">
        <div class="avatar" id="avatar-display">
          <span id="avatar-emoji">👵</span>
          <span class="avatar-edit-badge">✏️</span>
        </div>
        <div class="patient-meta">
          <h2><span id="nome-paciente-display">Dona Maria</span></h2>
          <p>Dispensador Inteligente • Editar</p>
        </div>
      </div>
      <div class="clock-pill">
        <span>⏰</span>
        <span id="relogio-topo">--:--</span>
      </div>
    </div>

    <!-- Modal Editar Paciente -->
    <div class="modal-overlay" id="modal-edicao">
      <div class="modal-content">
        <div class="modal-title">Identificação do Paciente</div>
        <div class="avatar-picker">
          <div class="avatar-opt selected" id="opt-idosa" onclick="selecionarAvatar('👵')">👵</div>
          <div class="avatar-opt" id="opt-idoso" onclick="selecionarAvatar('👴')">👴</div>
        </div>
        <input type="text" id="modal-input-nome" class="modal-input" placeholder="Nome do Paciente" maxlength="30">
        <div class="modal-btns">
          <button type="button" class="btn-modal btn-modal-cancel" onclick="fecharModalEdicao()">Cancelar</button>
          <button type="button" class="btn-modal btn-modal-save" onclick="salvarPaciente()">Salvar</button>
        </div>
      </div>
    </div>

    <div class="status-banner normal" id="status-card">
      <div class="banner-icon" id="status-icon">✅</div>
      <div class="banner-title" id="status-title">Tudo sob controle</div>
      <div class="banner-subtitle" id="status-sub">Carregando status...</div>
      <button class="banner-action-btn" id="btn-confirmar-dose" onclick="confirmarDoseTomada()">
        ✅ Confirmar que o Remédio foi Tomado
      </button>
    </div>

    <div class="card">
      <div class="card-title"><span>➕ Programar Remédio</span></div>
      <div class="form-group">
        <label class="form-label"><span>1. Nome do Medicamento</span></label>
        <input type="text" id="inp-nome" class="input-text" placeholder="Ex: Losartana 50mg, Dipirona, Vitamina...">
      </div>
      <div class="form-group">
        <label class="form-label"><span>2. Horário de Tomar</span></label>
        <div class="time-shortcuts">
          <button class="btn-chip" onclick="selecionarHorarioAtalho('07:00', this)">☀️ Manhã<br>07:00</button>
          <button class="btn-chip" onclick="selecionarHorarioAtalho('12:00', this)">🍽️ Almoço<br>12:00</button>
          <button class="btn-chip" onclick="selecionarHorarioAtalho('16:00', this)">☕ Tarde<br>16:00</button>
          <button class="btn-chip" onclick="selecionarHorarioAtalho('20:00', this)">🌙 Noite<br>20:00</button>
        </div>
        <div class="custom-time-row">
          <input type="time" id="inp-horario" value="08:00">
        </div>
      </div>
      <div class="form-group">
        <label class="form-label">
          <span>3. Em qual Potinho você guardou?</span>
          <span class="hint" id="pote-selecionado-texto">Pote 1 selecionado</span>
        </label>
        <div class="pote-selector" id="pote-grid"></div>
      </div>
      <button type="button" class="btn-submit" onclick="adicionarMedicamento()">💾 Salvar no Aparelho</button>
    </div>

    <div class="card">
      <div class="card-title"><span>📋 Programação de Hoje</span></div>
      <div class="schedule-list" id="lista-medicamentos"></div>
    </div>

    <div class="dev-mode-wrapper">
      <button type="button" class="btn-toggle-dev" onclick="toggleModoDev()">
        <span>🛠️ Modo Desenvolvedor</span>
        <span id="dev-icon-arrow">▼</span>
      </button>
      <div class="dev-panel" id="dev-panel">
        <div class="dev-panel-title">⚙️ Ferramentas de Teste e Hardware</div>
        <div class="dev-grid">
          <button type="button" class="btn-test alarm-btn" onclick="simularDisparo()">🚨 Testar Alarme</button>
          <button type="button" class="btn-test" id="btn-led" onclick="alternarLed()">💡 Luz da Gaveta (Off)</button>
        </div>
      </div>
    </div>
  </div>

  <script>
    let pacienteNome = localStorage.getItem('pill_paciente_nome') || 'Dona Maria';
    let pacienteAvatar = localStorage.getItem('pill_paciente_avatar') || '👵';
    let avatarTemp = pacienteAvatar;

    function carregarDadosPaciente() {
      document.getElementById('nome-paciente-display').textContent = pacienteNome;
      document.getElementById('avatar-emoji').textContent = pacienteAvatar;
    }

    function abrirModalEdicao() {
      avatarTemp = pacienteAvatar;
      document.getElementById('modal-input-nome').value = pacienteNome;
      atualizarSelecaoAvatarModal();
      document.getElementById('modal-edicao').classList.add('open');
    }

    function fecharModalEdicao() {
      document.getElementById('modal-edicao').classList.remove('open');
    }

    function selecionarAvatar(av) {
      avatarTemp = av;
      atualizarSelecaoAvatarModal();
    }

    function atualizarSelecaoAvatarModal() {
      document.getElementById('opt-idosa').className = 'avatar-opt ' + (avatarTemp === '👵' ? 'selected' : '');
      document.getElementById('opt-idoso').className = 'avatar-opt ' + (avatarTemp === '👴' ? 'selected' : '');
    }

    function salvarPaciente() {
      const novoNome = document.getElementById('modal-input-nome').value.trim();
      if (novoNome) pacienteNome = novoNome;
      pacienteAvatar = avatarTemp;
      localStorage.setItem('pill_paciente_nome', pacienteNome);
      localStorage.setItem('pill_paciente_avatar', pacienteAvatar);
      carregarDadosPaciente();
      fecharModalEdicao();
      chamar('/status');
    }

    let poteSelecionado = 1;
    let alarmesRemotos = [];

    function toggleModoDev() {
      const panel = document.getElementById('dev-panel');
      const arrow = document.getElementById('dev-icon-arrow');
      const estaAberto = panel.classList.toggle('open');
      arrow.textContent = estaAberto ? '▲' : '▼';
    }

    function renderPotinhos() {
      const grid = document.getElementById('pote-grid');
      grid.innerHTML = '';
      const ordemFisica = [
        1,  5,  9, 13, 17,
        2,  6, 10, 14, 18,
        3,  7, 11, 15, 19,
        4,  8, 12, 16, 20
      ];
      ordemFisica.forEach(i => {
        const btn = document.createElement('div');
        btn.className = 'pote-btn ' + (poteSelecionado === i ? 'selected' : '');
        btn.onclick = () => { poteSelecionado = i; renderPotinhos(); };
        btn.innerHTML = '<span class="label">POTE</span>' + i;
        grid.appendChild(btn);
      });
      document.getElementById('pote-selecionado-texto').textContent = 'Pote ' + poteSelecionado + ' selecionado';
    }

    function selecionarHorarioAtalho(hora, btnElement) {
      document.getElementById('inp-horario').value = hora;
      document.querySelectorAll('.time-shortcuts .btn-chip').forEach(b => b.classList.remove('active'));
      if (btnElement) btnElement.classList.add('active');
    }

    function render(data) {
      document.getElementById('relogio-topo').textContent = data.hora || '--:--';

      const banner = document.getElementById('status-card');
      const icon = document.getElementById('status-icon');
      const title = document.getElementById('status-title');
      const sub = document.getElementById('status-sub');

      if (data.alarme) {
        banner.className = 'status-banner ringing';
        icon.textContent = '🚨';
        title.textContent = 'HORA DO REMÉDIO!';
        sub.textContent = 'Entregar medicação do POTE ' + data.poteAtivo + ' para ' + pacienteNome + '!';
      } else {
        banner.className = 'status-banner normal';
        icon.textContent = '✅';
        title.textContent = 'Tudo em ordem com ' + pacienteNome;
        sub.textContent = data.prox && data.prox !== '--:--' 
          ? 'Próximo remédio: ' + data.prox + ' • ' + data.doses + ' doses confirmadas hoje'
          : 'Nenhum alarme ativo no momento';
      }

      const container = document.getElementById('lista-medicamentos');
      container.innerHTML = '';
      alarmesRemotos = data.alarmes || [];

      if (alarmesRemotos.length === 0) {
        container.innerHTML = '<div class="empty-state">Nenhum remédio cadastrado ainda.<br>Use o formulário acima para adicionar.</div>';
      } else {
        alarmesRemotos.forEach((item, idx) => {
          const row = document.createElement('div');
          row.className = 'schedule-item';
          row.innerHTML = 
            '<div class="item-left">' +
              '<div class="time-badge">' + item.hora + '</div>' +
              '<div class="med-details">' +
                '<h4>Medicação Programada</h4>' +
                '<p><span class="pote-tag">Pote ' + item.pote + '</span></p>' +
              '</div>' +
            '</div>' +
            '<button class="btn-delete" title="Excluir" onclick="removerMedicamento(' + idx + ')">🗑️</button>';
          container.appendChild(row);
        });
      }

      const btnLed = document.getElementById('btn-led');
      btnLed.textContent = data.led ? '💡 Luz da Gaveta (LIGADA)' : '💡 Luz da Gaveta (Off)';
    }

    function chamar(url) { fetch(url).then(r => r.json()).then(render).catch(e => console.log(e)); }

    function adicionarMedicamento() {
      const hora = document.getElementById('inp-horario').value;
      if (!hora) return;
      const partes = hora.split(':');
      chamar('/add-alarme?h=' + partes[0] + '&m=' + partes[1] + '&p=' + poteSelecionado);
      if (poteSelecionado < 20) poteSelecionado++; else poteSelecionado = 1;
      renderPotinhos();
      document.getElementById('inp-nome').value = '';
    }

    function removerMedicamento(idx) {
      if (confirm('Deseja remover este alarme?')) {
        chamar('/del-alarme?idx=' + idx);
      }
    }

    function confirmarDoseTomada() { chamar('/silenciar'); }
    function simularDisparo() { chamar('/disparar-alarme'); }
    function alternarLed() { chamar('/toggle-led'); }

    carregarDadosPaciente();
    renderPotinhos();
    chamar('/status');
    setInterval(() => { chamar('/status'); }, 1500);
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
    String boasVindas = "Pill Dispenser iniciado!\nStatus: Online";
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