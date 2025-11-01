#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <TJpg_Decoder.h>
#include <SPI.h>

const char* ssid = "Change with YOUR SSID";
const char* password = "Change with YOUR PASSWORD";
const int websocket_port = 81;

#define TFT_CS D8
#define TFT_RST D4
#define TFT_DC D3
//SDA to D7
//SCL to D5

WebSocketsServer webSocket = WebSocketsServer(websocket_port);
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

#define JPEG_BUFFER_SIZE (32 * 1024)
uint8_t* jpeg_buffer = nullptr;
uint32_t jpeg_buffer_pos = 0;
uint32_t expected_jpeg_size = 0;

unsigned long lastFrameTime = 0;
int frameCount = 0;
unsigned long lastFpsDisplay = 0;

bool isDecoding = false;

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return false;

  tft.startWrite();
  tft.setAddrWindow(x, y, w, h);
  tft.writePixels(bitmap, w * h);
  tft.endWrite();

  return true;
}

void decodeAndShowJPEG() {
  TJpgDec.drawJpg(0, 40, jpeg_buffer, expected_jpeg_size);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      tft.fillScreen(ST77XX_BLACK);
      tft.setCursor(20, 50);
      tft.setTextColor(ST77XX_RED);
      tft.setTextSize(2);
      tft.println("Disconnected");
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        tft.fillScreen(ST77XX_BLACK);
        tft.setCursor(15, 50);
        tft.setTextColor(ST77XX_GREEN);
        tft.setTextSize(2);
        tft.println("Connected");
        frameCount = 0;
        lastFpsDisplay = millis();
        delay(500);
        break;
      }
    case WStype_TEXT:
      {
        String text = String((char*)payload);
        if (text.startsWith("JPEG_FRAME_SIZE:")) {
          expected_jpeg_size = text.substring(16).toInt();
          jpeg_buffer_pos = 0;

          if (expected_jpeg_size > JPEG_BUFFER_SIZE) {
            expected_jpeg_size = 0;
          }
        }
        break;
      }
    case WStype_BIN:
      {
        if (isDecoding) return;
        if (expected_jpeg_size == 0 || !jpeg_buffer) {
          return;
        }

        uint32_t remaining = expected_jpeg_size - jpeg_buffer_pos;
        if (remaining == 0) return;

        size_t copyLen = (length < remaining) ? length : remaining;
        if (jpeg_buffer_pos + copyLen > JPEG_BUFFER_SIZE) {
          copyLen = JPEG_BUFFER_SIZE - jpeg_buffer_pos;
        }

        if (copyLen > 0) {
          memcpy(jpeg_buffer + jpeg_buffer_pos, payload, copyLen);
          jpeg_buffer_pos += copyLen;
        }
        if (jpeg_buffer_pos >= expected_jpeg_size) {
          isDecoding = true;
          decodeAndShowJPEG();
          isDecoding = false;
          expected_jpeg_size = 0;
          jpeg_buffer_pos = 0;
        }
        break;
      }
    default:
      break;
  }
}

void setup() {
  jpeg_buffer = (uint8_t*)malloc(JPEG_BUFFER_SIZE);
  if (!jpeg_buffer) {
    while (true) delay(1000);
  }
  tft.init(240, 280);
  tft.setSPISpeed(50000000);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(tft_output);

  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.setCursor(25, 40);
  tft.println("Connecting");
  tft.setCursor(10, 60);
  tft.println("to WiFi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    tft.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(ST77XX_GREEN);
    tft.setCursor(25, 20);
    tft.println("WiFi OK!");

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(5, 50);
    tft.print("IP: ");
    tft.println(WiFi.localIP());
    tft.setCursor(5, 70);
    tft.print("Port: ");
    tft.println(websocket_port);

    tft.setTextSize(1);
    tft.setTextColor(ST77XX_YELLOW);
    tft.setCursor(25, 100);
    tft.println("Waiting Connection...");
  } else {
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(20, 50);
    tft.setTextColor(ST77XX_RED);
    tft.setTextSize(2);
    tft.println("WiFi Error!");
    while (true) delay(1000);
  }
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  webSocket.loop();
}
