
// Pre-compiler check to ensure the correct board is selected
#if !defined(ESP32)
#error This code is intended for an ESP32 board. Please select an ESP32 board from the Tools > Board menu.
#endif

// ======================================================================
// PENGATURAN HARDWARE & ANIMASI
// ======================================================================

// 1. PIN UNTUK KOMUNIKASI UART DENGAN ESP-A
#define UART2_RX_PIN 17 // Terhubung ke TX (Pin 17) di ESP-A
#define UART2_TX_PIN 16 // Terhubung ke RX (Pin 16) di ESP-A

// 2. PIN GPIO UNTUK 16 LED ANDA
const int ledPins[16] = {
  13, 12, 14, 27, // Blok 1
  26, 25, 33, 32, // Blok 2
  15, 2, 4, 0,    // Blok 3
  18, 5, 19, 23    // Blok 4
};
const int NUM_LEDS = sizeof(ledPins) / sizeof(ledPins[0]);

// 3. PENGATURAN PWM (LEDC)
const int PWM_FREQ = 5000;
const int PWM_RESOLUTION = 8;

// 4. PENGATURAN ANIMASI
const int NUM_ANIMATION_BLOCKS = 4;
const int LEDS_PER_BLOCK = 4;
const uint8_t brightnessLevels[LEDS_PER_BLOCK] = {10, 80, 150, 255};

// ======================================================================
// VARIABEL STATUS
// ======================================================================
enum PumpStatus {
  PUMP_OFF = 0,
  PUMP_STARTING = 1,
  PUMP_ON = 2,
  PUMP_SHUTTING_DOWN = 3
};

volatile PumpStatus pumpPrimaryStatus = PUMP_OFF;
// Variabel lain tidak diubah, hanya pump1 yang mengontrol animasi ini
// volatile float pressurizerPressure = 0.0;
// volatile PumpStatus pumpSecondaryStatus = PUMP_OFF;
// volatile PumpStatus pumpTertiaryStatus = PUMP_OFF;

String uartBuffer = "";
bool newDataAvailable = false;

unsigned long lastAnimationTime = 0;
int animationSpeedDelay = 1000;
int masterPosition = 0;

// --- FUNGSI PARSING DATA ---
void parseData(String data) {
  data.remove(0, 1); // Hapus '<'
  data.remove(data.length() - 1); // Hapus '>'

  int lastIndex = 0;
  int separatorIndex = data.indexOf(';', lastIndex);
  
  while (separatorIndex != -1) {
    String pair = data.substring(lastIndex, separatorIndex);
    int colonIndex = pair.indexOf(':');
    if (colonIndex != -1) {
      String key = pair.substring(0, colonIndex);
      String value = pair.substring(colonIndex + 1);
      
      if (key == "pump1") {
        pumpPrimaryStatus = (PumpStatus)value.toInt();
        // Kita hanya butuh status pump1 untuk visualizer ini
        // jadi kita bisa hentikan parsing lebih awal jika mau
      }
    }
    lastIndex = separatorIndex + 1;
    separatorIndex = data.indexOf(';', lastIndex);
  }
  // Tidak perlu parse data terakhir karena kita hanya fokus pada pump1
}


// --- FUNGSI KONTROL LED ---
// Fungsi ini tidak lagi diperlukan karena kita akan menggunakan metode buffer
/*
void clearAllLeds() {
  for (int i = 0; i < NUM_LEDS; i++) {
    ledcWrite(i, 0);
  }
}
*/

// --- PERUBAHAN: FUNGSI BARU UNTUK 'MENGGAMBAR' KE BUFFER MEMORI ---
void drawLedBlockToBuffer(int startPos, uint8_t* buffer) {
  for (int i = 0; i < LEDS_PER_BLOCK; i++) {
    int currentLedIndex = (startPos + i) % NUM_LEDS;
    buffer[currentLedIndex] = brightnessLevels[i];
  }
}


// --- FUNGSI UTAMA ---
void setup() {
  Serial.begin(115200);
  Serial.println("ESP-E (Visualizer) Ready. Waiting for data from ESP-A...");

  Serial2.begin(115200, SERIAL_8N1, UART2_RX_PIN, UART2_TX_PIN);

  for (int i = 0; i < NUM_LEDS; i++) {
    ledcSetup(i, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(ledPins[i], i);
    ledcWrite(i, 0); // Pastikan semua LED mati saat startup
  }
}


void loop() {
  // 1. Cek dan baca data dari UART
  if (Serial2.available() > 0) {
    uartBuffer = Serial2.readStringUntil('
');
    newDataAvailable = true;
  }

  // 2. Jika ada data baru, parse datanya
  if (newDataAvailable) {
    if (uartBuffer.startsWith("<") && uartBuffer.endsWith(">")) {
      parseData(uartBuffer);
    }
    uartBuffer = ""; // Reset buffer
    newDataAvailable = false;
  }

  // 3. Tentukan kecepatan animasi berdasarkan status pompa primer
  switch (pumpPrimaryStatus) {
    case PUMP_OFF:
      // Matikan semua LED sekali saja jika status berubah menjadi OFF
      if (masterPosition != -1) { 
        for(int i=0; i<NUM_LEDS; i++) ledcWrite(i, 0);
        masterPosition = -1; // Tandai bahwa LED sudah dimatikan
      }
      return; 
    
    // --- PERUBAHAN: NILAI DELAY DIPERLAMBAT LAGI ---
    case PUMP_STARTING:
      animationSpeedDelay = 500; // Sebelumnya 200
      break;
      
    case PUMP_ON:
      animationSpeedDelay = 200;  // Sebelumnya 80
      break;
      
    case PUMP_SHUTTING_DOWN:
      animationSpeedDelay = 600; // Sebelumnya 250
      break;
  }
  
  // Jika pompa baru menyala dari kondisi OFF, reset posisi
  if (masterPosition == -1) masterPosition = 0;

  // 4. Jalankan animasi berdasarkan timer
  if (millis() - lastAnimationTime >= animationSpeedDelay) {
    lastAnimationTime = millis();

    // --- PERUBAHAN UTAMA UNTUK MENGHILANGKAN KEDIPAN ---
    
    // 1. Siapkan 'kanvas' virtual di memori, dan isi dengan nilai 0 (mati)
    uint8_t nextLedState[NUM_LEDS] = {0};

    // 2. 'Gambar' semua blok ke kanvas virtual tersebut, bukan langsung ke LED fisik
    for (int i = 0; i < NUM_ANIMATION_BLOCKS; i++) {
      int startPos = (masterPosition + (i * LEDS_PER_BLOCK)) % NUM_LEDS;
      drawLedBlockToBuffer(startPos, nextLedState);
    }
    
    // 3. Terapkan hasil dari kanvas ke LED fisik dalam satu proses cepat
    for (int i = 0; i < NUM_LEDS; i++) {
        ledcWrite(i, nextLedState[i]);
    }
    
    // 4. Majukan posisi master untuk frame berikutnya
    masterPosition++;
    if (masterPosition >= NUM_LEDS) {
      masterPosition = 0;
    }
  }
}
//Saya telah mengubah nilai jeda untuk setiap status agar animasinya berjalan jauh lebih lambat. Anda bisa mengubah kembali angka-angka ini jika ingin kecepatan yang berbe