#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <driver/i2s.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define SDA_PIN 19
#define SCL_PIN 18
#define BUTTON_PIN 14

#define I2S_MIC_WS  21
#define I2S_MIC_SCK 22
#define I2S_MIC_SD  20

#define I2S_SPK_DOUT 3
#define I2S_SPK_BCLK 5
#define I2S_SPK_LRC  4

const char* ssid = "PAMELLA"; // Ganti bagian ini, hanya bisa menggunakan Hotspot handphone 
const char* password = "MAMIH020279"; //password wifi
const char* serverIP = "192.168.1.18"; //IP Wifi
const int serverPort = 5000;

#define RECORD_SECONDS 3
#define SAMPLE_RATE 16000
#define RECORD_SIZE (SAMPLE_RATE * 2 * RECORD_SECONDS)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
uint8_t* audioBuffer;

void showText(String text) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(text);
  display.display();
}

void drawFaceIdle() {
  display.clearDisplay();
  display.fillCircle(40, 25, 8, WHITE);
  display.fillCircle(40, 25, 4, BLACK);
  display.fillCircle(88, 25, 8, WHITE);
  display.fillCircle(88, 25, 4, BLACK);
  display.drawLine(50, 45, 78, 45, WHITE);
  display.display();
}

void drawFaceListening() {
  display.clearDisplay();
  display.fillCircle(40, 25, 10, WHITE);
  display.fillCircle(40, 25, 5, BLACK);
  display.fillCircle(88, 25, 10, WHITE);
  display.fillCircle(88, 25, 5, BLACK);
  display.drawCircle(64, 47, 5, WHITE);
  display.display();
}

void drawFaceThinking() {
  display.clearDisplay();
  display.fillCircle(40, 25, 8, WHITE);
  display.fillRect(32, 17, 16, 8, BLACK);
  display.fillCircle(88, 25, 8, WHITE);
  display.fillRect(80, 17, 16, 8, BLACK);
  display.drawLine(50, 47, 64, 44, WHITE);
  display.drawLine(64, 44, 78, 47, WHITE);
  display.display();
}

void connectWiFi() {
  showText("Connecting WiFi...");
  WiFi.disconnect(true);
  delay(2000);
  WiFi.mode(WIFI_STA);
  delay(500);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 60) {
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    showText("WiFi OK!\nIP:\n" + WiFi.localIP().toString());
    Serial.println("WiFi OK");
    Serial.println(WiFi.localIP());
  } else {
    showText("WiFi Failed!\nRestarting...");
    delay(2000);
    ESP.restart();
  }

  delay(1500);
}

void setupMic() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 512,
    .use_apll = false
  };

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_MIC_SCK,
    .ws_io_num = I2S_MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SD
  };

  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);

  showText("Mic OK");
  delay(1000);
}

void setupSpeaker() {
  i2s_driver_uninstall(I2S_NUM_0);

  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
  };

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_SPK_BCLK,
    .ws_io_num = I2S_SPK_LRC,
    .data_out_num = I2S_SPK_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
}

void recordAudio() {
  showText("Listening...\nSpeak now!");
  delay(300);
  drawFaceListening();

  int32_t rawSample;
  int16_t sample16;
  size_t bytesRead = 0;
  int totalBytes = 0;

  unsigned long startTime = millis();

  while (millis() - startTime < (RECORD_SECONDS * 1000)) {
    i2s_read(I2S_NUM_0, &rawSample, sizeof(rawSample), &bytesRead, portMAX_DELAY);

    if (bytesRead > 0 && totalBytes < RECORD_SIZE) {
      sample16 = rawSample >> 14;
      audioBuffer[totalBytes++] = sample16 & 0xff;
      audioBuffer[totalBytes++] = (sample16 >> 8) & 0xff;
    }
  }

  showText("Record done!\nBytes:\n" + String(totalBytes));
  delay(800);
}

void playWavFromHTTP(HTTPClient &http) {
  setupSpeaker();

  WiFiClient *stream = http.getStreamPtr();
  showText("Speaking...");

  uint8_t header[44];
  int headerRead = 0;
  unsigned long startWait = millis();

  while (headerRead < 44 && millis() - startWait < 8000) {
    if (stream->available()) {
      header[headerRead++] = stream->read();
    } else {
      delay(1);
    }
  }

  if (headerRead < 44) {
    showText("WAV header error");
    delay(2000);
    i2s_driver_uninstall(I2S_NUM_0);
    setupMic();
    return;
  }

  uint8_t buffer[512];
  size_t bytesWritten;
  unsigned long lastData = millis();

  while (http.connected() || stream->available()) {
    int availableBytes = stream->available();

    if (availableBytes > 0) {
      int bytesRead = stream->readBytes(buffer, min(availableBytes, 512));

      if (bytesRead > 0) {
        lastData = millis();
        i2s_write(I2S_NUM_0, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
      }
    } else {
      delay(5);
    }

    if (millis() - lastData > 5000) {
      break;
    }
  }

  delay(300);
  i2s_driver_uninstall(I2S_NUM_0);
  setupMic();
}

void sendAudio() {
  showText("Thinking...");
  delay(500);
  drawFaceThinking();

  if (WiFi.status() != WL_CONNECTED) {
    showText("WiFi lost!");
    connectWiFi();
    return;
  }

  HTTPClient http;
  String url = "http://" + String(serverIP) + ":" + String(serverPort) + "/transcribe";

  http.begin(url);
  http.setReuse(false);
  http.addHeader("Content-Type", "audio/wav");
  http.setTimeout(180000);

  uint8_t wav[44];
  int dataSize = RECORD_SIZE;
  int fileSize = dataSize + 36;
  int byteRate = SAMPLE_RATE * 2;

  wav[0]='R'; wav[1]='I'; wav[2]='F'; wav[3]='F';
  wav[4]=fileSize&0xff; wav[5]=(fileSize>>8)&0xff; wav[6]=(fileSize>>16)&0xff; wav[7]=(fileSize>>24)&0xff;
  wav[8]='W'; wav[9]='A'; wav[10]='V'; wav[11]='E';
  wav[12]='f'; wav[13]='m'; wav[14]='t'; wav[15]=' ';
  wav[16]=16; wav[17]=0; wav[18]=0; wav[19]=0;
  wav[20]=1; wav[21]=0;
  wav[22]=1; wav[23]=0;
  wav[24]=SAMPLE_RATE&0xff; wav[25]=(SAMPLE_RATE>>8)&0xff; wav[26]=(SAMPLE_RATE>>16)&0xff; wav[27]=(SAMPLE_RATE>>24)&0xff;
  wav[28]=byteRate&0xff; wav[29]=(byteRate>>8)&0xff; wav[30]=(byteRate>>16)&0xff; wav[31]=(byteRate>>24)&0xff;
  wav[32]=2; wav[33]=0;
  wav[34]=16; wav[35]=0;
  wav[36]='d'; wav[37]='a'; wav[38]='t'; wav[39]='a';
  wav[40]=dataSize&0xff; wav[41]=(dataSize>>8)&0xff; wav[42]=(dataSize>>16)&0xff; wav[43]=(dataSize>>24)&0xff;

  uint8_t* wavData = (uint8_t*)malloc(44 + RECORD_SIZE);

  if (!wavData) {
    showText("RAM error!");
    http.end();
    return;
  }

  memcpy(wavData, wav, 44);
  memcpy(wavData + 44, audioBuffer, RECORD_SIZE);

  showText("Sending...");
  Serial.println("POSTING...");
  Serial.println(url);

  int httpCode = http.POST(wavData, 44 + RECORD_SIZE);

  free(wavData);

  Serial.print("HTTP CODE = ");
  Serial.println(httpCode);

  if (httpCode < 0) {
    Serial.print("HTTP ERROR = ");
    Serial.println(http.errorToString(httpCode));
    showText("HTTP Error:\n" + String(httpCode));
    delay(3000);
    http.end();
    drawFaceIdle();
    return;
  }

  if (httpCode == 200) {
    String result = http.getString();
    Serial.println("POST RESULT: " + result);

    http.end();

    if (result == "OK") {
      HTTPClient speakHttp;
      String speakUrl = "http://" + String(serverIP) + ":" + String(serverPort) + "/speak";

      showText("Get audio...");
      Serial.println("GET SPEAK...");
      Serial.println(speakUrl);

      speakHttp.begin(speakUrl);
      speakHttp.setReuse(false);
      speakHttp.setTimeout(180000);

      int speakCode = speakHttp.GET();

      Serial.print("SPEAK CODE = ");
      Serial.println(speakCode);

      if (speakCode == 200) {
        playWavFromHTTP(speakHttp);
      } else {
        showText("Speak Error:\n" + String(speakCode));
        delay(3000);
      }

      speakHttp.end();
    } else {
      showText("Server:\n" + result);
      delay(3000);
    }
  } else {
    showText("HTTP Error:\n" + String(httpCode));
    delay(3000);
    http.end();
  }

  drawFaceIdle();
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    while (true);
  }

  showText("AI Pocket\nStarting...");
  delay(1000);

  connectWiFi();

  audioBuffer = (uint8_t*)malloc(RECORD_SIZE);

  if (!audioBuffer) {
    showText("RAM kurang!");
    while (true);
  }

  setupMic();
  drawFaceIdle();
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50);

    if (digitalRead(BUTTON_PIN) == LOW) {
      recordAudio();
      sendAudio();

      while (digitalRead(BUTTON_PIN) == LOW) {
        delay(10);
      }
    }
  }

  delay(100);
}