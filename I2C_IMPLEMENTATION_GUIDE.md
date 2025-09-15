# Panduan Implementasi I2C untuk Simulator PLTN

Dokumen ini adalah panduan langkah demi langkah untuk menghubungkan beberapa mikrokontroler ESP (sebagai *slave*) ke Raspberry Pi (sebagai *master*) menggunakan protokol komunikasi I2C.

## 1. Konsep Dasar

Kita akan menggunakan arsitektur Master-Slave:
- **Master (Guru):** Raspberry Pi. Memulai dan mengontrol semua komunikasi.
- **Slave (Murid):** Setiap ESP. Memiliki alamat unik dan hanya mengirim data ketika diminta oleh Master.
- **Bus:** Dua kabel (SDA dan SCL) yang digunakan bersama oleh semua perangkat untuk komunikasi.

## 2. Kebutuhan Proyek

### Hardware:
- 1x Raspberry Pi (model apapun dengan GPIO header)
- Beberapa unit ESP (ESP32/ESP8266)
- Kabel Jumper (secukupnya)
- 1x Breadboard (sangat direkomendasikan)
- 2x Resistor 4.7kΩ (sebagai *pull-up*)

### Software & Library:
- **Di Raspberry Pi:**
  - Raspberry Pi OS
  - `i2c-tools` (untuk deteksi)
  - `python3-smbus2` (library Python untuk I2C)
- **Di ESP:**
  - Arduino IDE atau PlatformIO
  - Library `Wire.h` (sudah terpasang secara default)

## 3. Skema Rangkaian

Ini adalah bagian paling krusial. **Ketelitian sangat penting.**

1.  **Samakan Ground:** Hubungkan pin **GND** dari Raspberry Pi dan **SEMUA** pin **GND** dari setiap ESP ke jalur ground (biru) pada breadboard. **INI WAJIB**.
2.  **Jalur SDA:** Hubungkan pin **SDA** (biasanya GPIO 2) dari Raspberry Pi ke satu baris di breadboard. Kemudian, hubungkan pin **SDA** dari semua ESP ke baris yang sama.
3.  **Jalur SCL:** Hubungkan pin **SCL** (biasanya GPIO 3) dari Raspberry Pi ke baris lain di breadboard. Kemudian, hubungkan pin **SCL** dari semua ESP ke baris yang sama.
4.  **Pasang Resistor Pull-up:**
    - Hubungkan satu resistor 4.7kΩ dari jalur **SDA** ke jalur **3.3V**.
    - Hubungkan satu resistor 4.7kΩ dari jalur **SCL** ke jalur **3.3V**.

```
     +--------------------------------------------------+
     | RASPBERRY PI (MASTER)                            |
     |                                                  |
     |   3.3V o--------------------------------------+  |
     |        |                                     |  |
     |        |    +----------+    +----------+     |  |
     |        +----| 4.7k RES |----+ 4.7k RES |-----+  |
     |        |    +----------+    +----------+     |  |
     |        |         |               |          |  |
     |    SCL o---------+---------------+----------+  |
     |        |                         |             |
     |    SDA o-------------------------+             |
     |        |                                       |
     |    GND o----------------------------------+    |
     +--------|----------------------------------|----+
              |                                  |
     +--------|------------------+      +--------|------------------+
     | ESP 1 (SLAVE 0x08)        |      | ESP 2 (SLAVE 0x09)        |
     |                           |      |                           |
     | SCL o---------------------+      | SCL o---------------------+
     | SDA o---------------------+      | SDA o---------------------+
     | GND o---------------------+      | GND o---------------------+
     +---------------------------+      +---------------------------+
```

## 4. Langkah Implementasi

### Bagian 1: Sisi Slave (Setiap ESP)

Setiap ESP harus diprogram dengan **alamat I2C yang unik**.

**Daftar Alamat:**
| Perangkat              | Alamat I2C |
| ---------------------- | ---------- |
| ESP_E_Aliran_Primer    | `0x08`     |
| ESP_F_Aliran_Sekunder  | `0x09`     |
| ESP_G_Aliran_Tersier   | `0x0A`     |
| ... (dan seterusnya)   | ...        |

**Contoh Kode Template untuk `ESP_E_Aliran_Primer`:**
```cpp
// Sertakan library Wire untuk I2C
#include <Wire.h>

// Tentukan alamat UNIK untuk perangkat ini
#define I2C_ADDR 0x08

// Variabel untuk menyimpan data yang akan dikirim
volatile int sensorValue = 0;
const int sensorPin = 34; // Contoh pin untuk sensor/potensiometer

// Ini adalah fungsi "interrupt handler".
// Fungsi ini akan dipanggil secara otomatis ketika Master meminta data.
void requestEvent() {
  // Kirim nilai sensor. Karena int (16-bit) lebih besar dari satu byte (8-bit),
  // kita kirim dalam dua bagian: high byte dan low byte.
  byte high = highByte(sensorValue);
  byte low = lowByte(sensorValue);
  
  Wire.write(high); // Kirim bagian pertama
  Wire.write(low);  // Kirim bagian kedua
}

void setup() {
  // Mulai I2C bus sebagai Slave dengan alamat yang telah ditentukan
  Wire.begin(I2C_ADDR);
  
  // Daftarkan `requestEvent` sebagai fungsi handler
  Wire.onRequest(requestEvent); 
  
  // Setup lain-lain
  Serial.begin(115200);
  pinMode(sensorPin, INPUT);
}

void loop() {
  // Di loop utama, tugas kita hanya membaca sensor dan memperbarui variabel.
  // Pengiriman data akan diurus oleh `requestEvent` secara otomatis.
  sensorValue = analogRead(sensorPin);
  
  // Delay untuk stabilitas
  delay(100);
}
```
*Upload kode ini ke setiap ESP, pastikan untuk mengubah nilai `#define I2C_ADDR` pada setiap ESP.*

### Bagian 2: Sisi Master (Raspberry Pi)

1.  **Aktifkan I2C di OS:**
    - Buka terminal, jalankan `sudo raspi-config`.
    - Pergi ke `Interface Options` -> `I2C`.
    - Pilih `Yes` untuk mengaktifkan.
    - Reboot jika diminta.

2.  **Verifikasi Koneksi:**
    - Setelah semua perangkat terhubung dan menyala, jalankan perintah ini di terminal Pi:
      ```bash
      i2cdetect -y 1
      ```
    - Anda akan melihat sebuah tabel. Jika rangkaian benar, alamat-alamat ESP Anda (`08`, `09`, `0a`, dst.) akan muncul di tabel tersebut. Jika tidak muncul, periksa kembali rangkaian kabel Anda.

3.  **Buat Script Python:**
    - Buat file baru, misalnya `i2c_reader.py`.
    - Isi dengan kode berikut:

    ```python
    import time
    from smbus2 import SMBus

    # Definisikan alamat-alamat slave Anda
    ESP_DEVICES = {
        "aliran_primer": 0x08,
        "aliran_sekunder": 0x09,
        "aliran_tersier": 0x0A,
    }

    print("Membaca data dari ESP via I2C...")

    # Gunakan 'with' untuk memastikan bus ditutup dengan benar
    try:
        with SMBus(1) as bus:
            while True:
                # Loop melalui setiap perangkat yang terdaftar
                for name, address in ESP_DEVICES.items():
                    try:
                        # Minta 2 byte data dari alamat yang dituju
                        # Format: read_i2c_block_data(alamat, offset, jumlah_byte)
                        data_block = bus.read_i2c_block_data(address, 0, 2)
                        
                        # Gabungkan 2 byte kembali menjadi satu nilai integer
                        # (byte pertama << 8) | byte kedua
                        value = (data_block[0] << 8) | data_block[1]
                        
                        print(f"Data dari {name.replace('_', ' ').title()} (0x{address:02x}): {value}")

                    except OSError:
                        # Tangani error jika perangkat tidak merespons
                        print(f"Gagal membaca dari {name} di alamat 0x{address:02x}")

                print("-" * 30)
                time.sleep(1) # Jeda 1 detik sebelum membaca lagi

    except FileNotFoundError:
        print("Error: I2C bus tidak ditemukan. Pastikan I2C sudah diaktifkan di raspi-config.")
    except KeyboardInterrupt:
        print("\nProgram dihentikan.")

    ```

## 5. Menjalankan Sistem

1.  Pastikan semua ESP sudah menyala dan menjalankan kode slave-nya.
2.  Jalankan script Python di Raspberry Pi: `python3 i2c_reader.py`.
3.  Anda akan melihat output data dari setiap ESP di terminal Pi Anda, yang diperbarui setiap detik.
4.  Dari sini, Anda bisa mengintegrasikan logika pembacaan data ini ke dalam aplikasi visualisasi Anda (misalnya yang dibuat dengan Pygame).

```