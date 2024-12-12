#include <WiFi.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "time.h"

// Konfigurasi WiFi
const char* ssid = "Ga";
const char* password = "plmjuhvfrdzaq";

// Konfigurasi Telegram Bot
const String botToken = "7566897277:AAHcfFxQIWG5c9oJEiZtBpQuBtlsRDzPAdM";
const String chatID = "993712456";

// Konfigurasi DS18B20
#define ONE_WIRE_BUS 5 // Pin untuk sensor DS18B20
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Konfigurasi Ultrasonik
#define TRIG_PIN 19
#define ECHO_PIN 18

// Konfigurasi Relay
#define RELAY_PIN 21 // Relay yang terhubung ke mini pump
const int distanceThreshold = 20; // Batas jarak untuk kontrol otomatis pompa

// Konfigurasi Servo
int servoPin = 26;        // Pin GPIO untuk servo
int pulseWidth = 0;       // Durasi pulsa PWM (dalam mikrodetik)
int minPulse = 500;       // Durasi pulsa minimum (0 derajat)
int maxPulse = 2400;      // Durasi pulsa maksimum (90 derajat)

// Objek HTTPClient
HTTPClient http;

// Variabel untuk melacak pembaruan Telegram terbaru
long lastUpdateId = 0;

// Status pompa
bool manualControl = false; // Kontrol manual aktif
bool pumpState = false;     // Status pompa (ON/OFF)

// Informasi NTP (Network Time Protocol)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 25200; // Offset GMT+7 (dalam detik)
const int daylightOffset_sec = 0; // Tidak ada daylight saving

void setup() {
  Serial.begin(115200);

  // Inisialisasi sensor DS18B20
  pinMode(ONE_WIRE_BUS, INPUT_PULLUP);
  sensors.begin();

  // Inisialisasi sensor ultrasonik
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Inisialisasi relay
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Pastikan relay mati (sesuaikan jika perlu)

  // Koneksi ke WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // Kirim pesan awal
  sendTelegramMessage("Bot terhubung. Kirim /cek untuk membaca suhu atau gunakan /pompa_on dan /pompa_off untuk kontrol pompa manual.");

  // Inisialisasi NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Time synchronized.");

  // Inisialisasi Servo
  pinMode(servoPin, OUTPUT);
}

void loop() {
  // Cek perintah Telegram
  checkTelegramCommand();

  // Cek waktu dan kendalikan servo otomatis
  checkTimeAndMoveServo();

  // Logika otomatis hanya aktif jika manualControl tidak diaktifkan
  if (!manualControl) {
    // Baca jarak dari sensor ultrasonik
    long duration = measureDistance();
    float distance = duration * 0.034 / 2; // Konversi ke cm

    Serial.print("Jarak: ");
    Serial.print(distance);
    Serial.println(" cm");

    // Logika kontrol pompa otomatis
    if (distance <= distanceThreshold && !pumpState) {
      Serial.println("Pompa otomatis menyala!");
      digitalWrite(RELAY_PIN, HIGH); // Aktifkan relay (pompa menyala)
      pumpState = true;
    } else if (distance > distanceThreshold && pumpState) {
      Serial.println("Pompa otomatis mati!");
      digitalWrite(RELAY_PIN, LOW); // Matikan relay (pompa mati)
      pumpState = false;
    }
  }

  delay(500); // Tunggu 500 ms sebelum membaca lagi
}

// Fungsi untuk mengirim pesan ke Telegram
void sendTelegramMessage(String message) {
  if (WiFi.status() == WL_CONNECTED) { // Pastikan WiFi terhubung
    String url = "https://api.telegram.org/bot" + botToken + "/sendMessage?chat_id=" + chatID + "&text=" + message;
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.println("Pesan terkirim: " + message);
    } else {
      Serial.println("Gagal mengirim pesan. Error: " + String(http.errorToString(httpCode)));
    }
    http.end(); // Tutup koneksi
  } else {
    Serial.println("WiFi tidak terhubung!");
  }
}

// Fungsi untuk memeriksa perintah Telegram
void checkTelegramCommand() {
  if (WiFi.status() == WL_CONNECTED) { // Pastikan WiFi terhubung
    String url = "https://api.telegram.org/bot" + botToken + "/getUpdates?offset=" + String(lastUpdateId + 1);
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode > 0) {
      String response = http.getString();
      Serial.println("Respons dari Telegram: ");
      Serial.println(response);

      // Cek perintah "/cek"
      if (response.indexOf("\"text\":\"/cek\"") != -1) {
        Serial.println("Perintah /cek diterima!");

        // Baca suhu dari sensor DS18B20
        sensors.requestTemperatures();
        float temperature = sensors.getTempCByIndex(0);

        // Format dan kirim pesan suhu
        String message;
        if (temperature != DEVICE_DISCONNECTED_C) {
          message = "Suhu saat ini: " + String(temperature) + " °C";
          Serial.println("Suhu: " + String(temperature) + " °C");
        } else {
          message = "Sensor suhu tidak terhubung!";
          Serial.println("Sensor suhu tidak terhubung!");
        }

        sendTelegramMessage(message);
      }

      // Cek perintah "/pompa_on"
      if (response.indexOf("\"text\":\"/pompa_on\"") != -1) {
        manualControl = true; // Aktifkan kontrol manual
        pumpState = true;     // Set status pompa ke ON
        digitalWrite(RELAY_PIN, HIGH); // Aktifkan relay
        sendTelegramMessage("Pompa telah dihidupkan secara manual.");
      }

      // Cek perintah "/pompa_off"
      if (response.indexOf("\"text\":\"/pompa_off\"") != -1) {
        manualControl = true; // Aktifkan kontrol manual
        pumpState = false;    // Set status pompa ke OFF
        digitalWrite(RELAY_PIN, LOW); // Matikan relay
        sendTelegramMessage("Pompa telah dimatikan secara manual.");
      }

      // Cek perintah "/otomatis"
      if (response.indexOf("\"text\":\"/otomatis\"") != -1) {
        manualControl = false; // Nonaktifkan kontrol manual
        sendTelegramMessage("Kontrol otomatis pompa diaktifkan.");
      }

      // Perbarui lastUpdateId
      int updateIndex = response.lastIndexOf("\"update_id\":");
      if (updateIndex != -1) {
        String updateIdString = response.substring(updateIndex + 12, response.indexOf(",", updateIndex));
        lastUpdateId = updateIdString.toInt();
      }
    } else {
      Serial.println("Gagal mendapatkan pembaruan. Error: " + String(http.errorToString(httpCode)));
    }
    http.end(); // Tutup koneksi
  } else {
    Serial.println("WiFi tidak terhubung!");
  }
}

// Fungsi untuk mengukur jarak
long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  return pulseIn(ECHO_PIN, HIGH); // Durasi pantulan
}

// Fungsi untuk menggerakkan servo ke sudut tertentu
void moveServo(int angle) {
  pulseWidth = map(angle, 0, 180, minPulse, maxPulse);

  for (int i = 0; i < 50; i++) {  // Kirim pulsa selama 20 ms
    digitalWrite(servoPin, HIGH);
    delayMicroseconds(pulseWidth);
    digitalWrite(servoPin, LOW);
    delayMicroseconds(20000 - pulseWidth);
  }

  Serial.print("Servo moved to: ");
  Serial.print(angle);
  Serial.println(" degrees");
}

// Fungsi untuk cek waktu dan kendalikan servo otomatis
void checkTimeAndMoveServo() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    // Tampilkan waktu saat ini di Serial Monitor
    Serial.printf("Current time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    // Cek apakah waktu saat ini jam 6 pagi untuk menggerakkan servo
    if (timeinfo.tm_hour == 6 && timeinfo.tm_min == 0) {
      moveServo(90);  // Menggerakkan servo ke posisi 90 derajat
      delay(3000);   // Tunggu 1 menit sebelum menggerakkan servo lagi
      motoServo(0);
    }
  }
}
