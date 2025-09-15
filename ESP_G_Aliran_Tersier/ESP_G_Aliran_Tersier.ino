
// Pre-compiler check to ensure the correct board is selected
#if !defined(ESP32)
#error This code is intended for an ESP32 board. Please select an ESP32 board from the Tools > Board menu.
#endif

// ======================================================================
// PENGATURAN HARDWARE & ANIMASI
// ======================================================================

// 1. PIN UNTUK KOMUNIKASI UART (Menerima dari ESP-F)
#define UART2_RX_PIN 17
#define UART2_TX_PIN 16

// 2. PIN GPIO UNTUK 16 LED ANDA (Asumsi rangkaian sama dengan E & F)
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
// Menggunakan gradasi kecerahan yang sama dengan kode final Anda
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

// Variabel ini sekarang menyimpan status pompa tersier
volatile PumpStatus activePumpStatus = PUMP_OFF;

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
  int separatorIndex = 0;
  
  // PERUBAHAN UTAMA: Sekarang kita harus mencari sampai ke "pump3"
  while ((separatorIndex = data.indexOf(';', lastIndex)) != -1) {
    String pair = data.substring(lastIndex, separatorIndex);
    int colonIndex = pair.indexOf(':');
    if (colonIndex != -1) {
      String key = pair.substring(0, colonIndex);
      String value = pair.substring(colonIndex + 1);
      
      if (key == "pump3") {
        activePumpStatus = (PumpStatus)value.toInt();
        return; 
      }
    }
    lastIndex = separatorIndex + 1;
  }

  // Juga periksa bagian terakhir dari string, karena mungkin tidak diakhiri dengan ';'
  String lastPair = data.substring(lastIndex);
  int colonIndex = lastPair.indexOf(':');
    if (colonIndex != -1) {
      String key = lastPair.substring(0, colonIndex);
      String value = lastPair.substring(colonIndex + 1);
      if (key == "pump3") {
        activePumpStatus = (PumpStatus)value.toInt();
      }
    }
}

// --- FUNGSI KONTROL LED ---
void drawLedBlockToBuffer(int startPos, uint8_t* buffer) {
  for (int i = 0; i < LEDS_PER_BLOCK; i++) {
    int currentLedIndex = (startPos + i) % NUM_LEDS;
    buffer[currentLedIndex] = brightnessLevels[i];
  }
}

// --- FUNGSI UTAMA ---
void setup() {
  Serial.begin(115200);
  Serial.println("ESP-G (Visualizer Aliran Tersier) Ready. Waiting for data...");

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

  // 3. Tentukan kecepatan animasi berdasarkan status pompa tersier
  switch (activePumpStatus) {
    case PUMP_OFF:
      if (masterPosition != -1) { 
        for(int i=0; i<NUM_LEDS; i++) ledcWrite(i, 0);
        masterPosition = -1;
      }
      return; 
    
    // Menggunakan kecepatan yang sama persis dengan kode final Anda
    case PUMP_STARTING:
      animationSpeedDelay = 500;
      break;
      
    case PUMP_ON:
      animationSpeedDelay = 250;
      break;
      
    case PUMP_SHUTTING_DOWN:
      animationSpeedDelay = 600;
      break;
  }
  
  if (masterPosition == -1) masterPosition = 0;

  // 4. Jalankan animasi berdasarkan timer
  if (millis() - lastAnimationTime >= animationSpeedDelay) {
    lastAnimationTime = millis();
    
    uint8_t nextLedState[NUM_LEDS] = {0};

    for (int i = 0; i < NUM_ANIMATION_BLOCKS; i++) {
      int startPos = (masterPosition + (i * LEDS_PER_BLOCK)) % NUM_LEDS;
      drawLedBlockToBuffer(startPos, nextLedState);
    }
    
    for (int i = 0; i < NUM_LEDS; i++) {
        ledcWrite(i, nextLedState[i]);
    }
    
    masterPosition++;
    if (masterPosition >= NUM_LEDS) {
      masterPosition = 0;
    }
  }
}
