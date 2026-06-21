#include <Arduino.h>
#include <WiFi.h>

// 外部ファイルからSSIDとパスワードを読み込む
#include "version.h"

// Wi-Fi設定 (secrets.h のマクロを使用)
String ssid     = "";
String password = "";

// バージョンとか設定とか
const char* firmwareVersion = FIRMWARE_VERSION;
const char* deviceName = DEVICE_NAME;
String firmwareUrl = FIRMWARE_URL;

// シリアル入力バッファ
String inputString = "";

// ==========================================
// シリアルから1行（改行まで）を読み込む関数
// ==========================================
String readSerialInput() {
  String str = "";
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') {
        if (str.length() > 0) {
          return str; // 改行が来たら文字列を返す
        }
      } else {
        str += c; // 1文字ずつ追加
      }
    }
    delay(10); // WDTリセット防止
  }
}

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

  // 起動するたびに必ずシリアル入力を求める
  Serial.println("\n[Wi-Fi Setup Mode]");
  Serial.println("Please enter Wi-Fi SSID:");
  ssid = readSerialInput();
  Serial.println("SSID received.");

  Serial.println("Please enter Wi-Fi Password:");
  password = readSerialInput();
  Serial.println("Password received.");

  // 入力された情報でWi-Fi接続開始
  Serial.printf("Connecting to Wi-Fi: %s\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), password.c_str());
  
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