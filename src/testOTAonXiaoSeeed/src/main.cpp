#include <Arduino.h>
#include <WiFi.h>

#include "secrets.h"

// ファームウェア情報の定義
const char* FIRMWARE_VERSION = "v1.0";
const char* DEVICE_NAME = "XIAO_ESP32C3_OTA_TEST";

// Wi-Fi設定 (secrets.h のマクロを使用)
const char* ssid = SECRET_SSID;
const char* password = SECRET_PASS;

// シリアル入力バッファ
String inputString = "";

void setup() {
  // --- 以降のコードは変更なし ---
  Serial.begin(115200);
  
  // シリアルポートが開くまで待つ（タイムアウト付き：最大5秒）
  unsigned long startWait = millis();
  while (!Serial && millis() - startWait < 5000) {
    delay(10);
  }

  // ここから確実に出力されるはずです
  Serial.println("\n\n================================");
  Serial.println("--- Booting Device ---");
  Serial.print("Current Firmware Version: ");
  Serial.println(FIRMWARE_VERSION);

  pinMode(D10, OUTPUT);
  digitalWrite(D10, HIGH); 

  // Wi-Fi接続処理
  Serial.printf("Connecting to Wi-Fi: %s\n", ssid);
  WiFi.begin(ssid, password);
  
  // Wi-Fiが繋がるまで待機して、状態を視覚化する
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // 接続成功時の出力
  Serial.println("\nWi-Fi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println("================================\n");
}

void loop() {
  // Wi-Fiの接続状態を定期的にチェック（ログ用）
  static unsigned long lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 5000) {
    lastWifiCheck = millis();
    if (WiFi.status() == WL_CONNECTED) {
      // 接続完了時はLEDをゆっくり1回点滅させるなどの処理を入れても良いです
    } else {
      Serial.print(".");
    }
  }

  // シリアルコンソールからの入力パース
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    
    // 改行文字（LF: '\n' または CR: '\r'）を受信したらコマンドを評価
    if (inChar == '\n' || inChar == '\r') {
      if (inputString.length() > 0) {
        inputString.trim(); // 前後の不要な空白や改行を削除

        // クエリ *IDN? の判定
        if (inputString.equalsIgnoreCase("*IDN?")) {
          // メーカー名,デバイス名,シリアル番号(今回は固定),ファームウェアバージョン
          Serial.printf("SeeedStudio,%s,000000,%s\n", DEVICE_NAME, FIRMWARE_VERSION);
        }
        else {
          Serial.printf("Unknown Command: [%s]\n", inputString.c_str());
        }
        
        inputString = ""; // バッファをクリア
      }
    } else {
      // 通常の文字はバッファに蓄積
      inputString += inChar;
    }
  }
}