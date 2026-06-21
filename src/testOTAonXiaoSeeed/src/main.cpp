#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>

// 外部ファイルからSSIDとパスワードを読み込む
#include "secrets.h"
#include "version.h"

// Wi-Fi設定 (secrets.h のマクロを使用)
const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;

// バージョンとか設定とか
const char* firmwareVersion = FIRMWARE_VERSION;
const char* deviceName = DEVICE_NAME;
String firmwareUrl = FIRMWARE_URL;

// 実験用：ダウンロードするファームウェアのURL
// ※今はダミーURLです。次のフェーズでGitHubのRelease URLに書き換えます。


// シリアル入力バッファ
String inputString = "";

// ==========================================
// OTAアップデートを実行する関数
// ==========================================
void performOTA(String url) {
  Serial.println("\n--- Starting OTA Update ---");
  Serial.print("Target URL: ");
  Serial.println(url);

  // HTTPS通信用のクライアント設定
  WiFiClientSecure client;
  // 今回は実験のため証明書の検証をスキップします（GitHubとの通信に必須のハックです）
  client.setInsecure(); 

  HTTPClient http;

  // GitHub等からのダウンロード（302リダイレクト）を追従する設定（超重要！）
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (http.begin(client, url)) {
    Serial.println("Connected to server. Downloading firmware...");
    
    // HTTP GETリクエストを送信
    int httpCode = http.GET();
    
    // HTTPステータスコードが200(OK)または301(Moved Permanently)の場合に処理を進める
    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
      // ファイルのサイズを取得
      int contentLength = http.getSize();
      Serial.printf("Firmware size: %d bytes\n", contentLength);

      if (contentLength > 0) {
        // OTAの開始（フラッシュメモリに十分な空きがあるかチェック）
        bool canBegin = Update.begin(contentLength);

        if (canBegin) {
          Serial.println("Begin OTA. This may take a minute...");
          
          // ストリーム（HTTPからのデータ）を直接フラッシュメモリに書き込む
          WiFiClient* tcpClient = http.getStreamPtr();
          size_t written = Update.writeStream(*tcpClient);

          if (written == contentLength) {
            Serial.println("Written: " + String(written) + " successfully");
          } else {
            Serial.println("Written only: " + String(written) + "/" + String(contentLength) + ". Retry?");
          }

          // OTAの終了処理と結果確認
          if (Update.end()) {
            Serial.println("OTA Update complete!");
            if (Update.isFinished()) {
              Serial.println("Update successfully completed. Rebooting...");
              delay(2000);
              ESP.restart(); // デバイスを再起動して新しいファームウェアを起動
            } else {
              Serial.println("Update not finished? Something went wrong.");
            }
          } else {
            Serial.println("Error Occurred. Error #: " + String(Update.getError()));
          }
        } else {
          Serial.println("Not enough space to begin OTA");
        }
      } else {
        Serial.println("Content-Length is 0. Cannot proceed.");
      }
    } else {
      Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
      if(httpCode > 0){
          Serial.printf("HTTP Status Code: %d\n", httpCode);
      }
    }
    http.end(); // 接続を閉じる
  } else {
    Serial.println("Failed to connect to server");
  }
}

// ==========================================
// セットアップ
// ==========================================
void setup() {
  Serial.begin(115200);
  
  unsigned long startWait = millis();
  while (!Serial && millis() - startWait < 5000) {
    delay(10);
  }

  Serial.println("\n\n================================");
  Serial.println("--- Booting Device ---");
  Serial.print("Current Firmware Version: ");
  Serial.println(firmwareVersion);

  pinMode(D10, OUTPUT);
  digitalWrite(D10, HIGH); 

  Serial.printf("Connecting to Wi-Fi: %s\n", ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Ready for commands (*IDN? or UPDATE)");
  Serial.println("================================\n");
}

// ==========================================
// メインループ
// ==========================================
void loop() {
  static unsigned long lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 5000) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Wi-Fi disconnected. Reconnecting...");
      WiFi.reconnect();
    }
  }

  // シリアルコンソールからの入力パース
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    
    if (inChar == '\n' || inChar == '\r') {
      if (inputString.length() > 0) {
        inputString.trim();

        // クエリ *IDN? の判定
        if (inputString.equalsIgnoreCase("*IDN?")) {
          Serial.printf("SeeedStudio,%s,000000,%s\n", deviceName, firmwareVersion);
        }
        else if (inputString.equalsIgnoreCase("UPDATE")) {
          if (WiFi.status() == WL_CONNECTED) {
             performOTA(firmwareUrl); // 設定したURLでOTAを開始
          } else {
             Serial.println("Error: Wi-Fi not connected. Cannot perform OTA.");
          }
        }
        else {
          Serial.printf("Unknown Command: [%s]\n", inputString.c_str());
        }
        
        inputString = "";
      }
    } else {
      inputString += inChar;
    }
  }
}