#include <Wire.h>

// -------------------- Deklarasi Pin ------------------
const int Buttonganti       = 12;
const int Buttonmax         = 13;
const int Buttonmin         = 14;
const int Buttondone        = 15;

const int Opticalswitch1    = 22;  // Atas/bawah (Motor 2)
const int Opticalswitch2    = 21;
const int Opticalswitch3    = 20;  // Crimping (Motor 3)
const int Infrared1         = 27;
const int Infrared2         = 26;

// Stepper motor 1
const int EnPin1   = 2;
const int StepPin1 = 1;
const int DirPin1  = 0;

// Stepper motor 2
const int EnPin2   = 11;
const int StepPin2 = 9;
const int DirPin2  = 8;

// Stepper motor 3
const int EnPin3   = 7;
const int StepPin3 = 6;
const int DirPin3  = 3;

// Output
const int Buzzer = 17;
const int Relay  = 19;

// -------------------- Variabel Status --------------------
bool InfraredDetected = false;
bool InfraredDetected2 = false;
bool motorDone = false;

// -------------------- Function Declarations --------------------
void homingInfrared();
void Penutupbotol();
void crimping();
void Infraredakhir();
void stepMotor(int stepPin, int steps, int delayTime);

// -------------------- Setup --------------------
void setup() {
  Serial.begin(9600);

  // Input buttons
  pinMode(Buttonganti, INPUT_PULLUP);
  pinMode(Buttonmax, INPUT_PULLUP);
  pinMode(Buttonmin, INPUT_PULLUP);
  pinMode(Buttondone, INPUT_PULLUP);
  
  // Input sensors - Optical switch tanpa pullup karena menggunakan logika HIGH = tertekan
  pinMode(Opticalswitch1, INPUT);
  pinMode(Opticalswitch2, INPUT);
  pinMode(Opticalswitch3, INPUT);
  pinMode(Infrared1, INPUT);
  pinMode(Infrared2, INPUT);

  // Output motor
  pinMode(EnPin1, OUTPUT); pinMode(StepPin1, OUTPUT); pinMode(DirPin1, OUTPUT);
  pinMode(EnPin2, OUTPUT); pinMode(StepPin2, OUTPUT); pinMode(DirPin2, OUTPUT);
  pinMode(EnPin3, OUTPUT); pinMode(StepPin3, OUTPUT); pinMode(DirPin3, OUTPUT);

  // Output buzzer & relay
  pinMode(Buzzer, OUTPUT);
  pinMode(Relay, OUTPUT);
  digitalWrite(Relay, HIGH);   // Relay dimulai dalam kondisi MATI

  // Matikan motor di awal
  digitalWrite(EnPin1, HIGH);
  digitalWrite(EnPin2, HIGH);
  digitalWrite(EnPin3, HIGH);
  
  Serial.println("Sistem Crimping Otomatis Siap!");
}

// -------------------- Loop --------------------
void loop() {
  // Reset status untuk siklus baru
  InfraredDetected = false;
  InfraredDetected2 = false;
  motorDone = false;
  
  Serial.println("=== Memulai Siklus Baru ===");
  
    // 1. Homing dengan infrared sensor
  homingInfrared();
  
  // 2. Motor menuju penutup botol
  Serial.println("Motor menuju posisi penutup botol...");
  digitalWrite(EnPin1, LOW);
  digitalWrite(DirPin1, HIGH);
  for (int x = 0; x < 8700; x++) {
    digitalWrite(StepPin1, HIGH);  
    delayMicroseconds(250);
    digitalWrite(StepPin1, LOW);
    delayMicroseconds(250); 
  }
  digitalWrite(EnPin1, HIGH);
  
   // 3. Proses penutup botol
   Penutupbotol();

   // 4. Motor ke posisi crimping
   Serial.println("Motor menuju posisi crimping...");
  digitalWrite(EnPin1, LOW);
  digitalWrite(DirPin1, HIGH);
  for (int x = 0; x < 103000; x++) {
    digitalWrite(StepPin1, HIGH);  
    delayMicroseconds(250);
    digitalWrite(StepPin1, LOW);
    delayMicroseconds(250); 
  }
  digitalWrite(EnPin1, HIGH);
  delay (10000);
  // // 5. Proses crimping
  // crimping();
  
  // // 6. Motor ke posisi akhir
  // Serial.println("Motor menuju posisi akhir...");
  // digitalWrite(EnPin1, LOW);
  // digitalWrite(DirPin1, HIGH);
  // for (int x = 0; x < 7200; x++) {
  //   digitalWrite(StepPin1, HIGH);  
  //   delayMicroseconds(350);
  //   digitalWrite(StepPin1, LOW);
  //   delayMicroseconds(350); 
  // }
  // digitalWrite(EnPin1, HIGH);
  
  // // 7. Deteksi infrared akhir
  // Infraredakhir();
  
  // // 8. Motor kembali ke posisi awal
  // Serial.println("Motor kembali ke posisi awal...");
  // digitalWrite(DirPin1, HIGH); // Tetap HIGH untuk arah yang sama
  // digitalWrite(EnPin1, LOW);
  // for (int x = 0; x < 3600; x++) {
  //   digitalWrite(StepPin1, HIGH);  
  //   delayMicroseconds(350);
  //   digitalWrite(StepPin1, LOW);
  //   delayMicroseconds(350); 
  // }
  // digitalWrite(EnPin1, HIGH);
  
  // Serial.println("=== Siklus Selesai ===");
  // delay(2000); // Jeda sebelum siklus berikutnya
}

// -------------------- Functions --------------------
void homingInfrared() {
  Serial.println("Memulai homing dengan infrared sensor...");
  
  while (!InfraredDetected) {
    if (digitalRead(Infrared1) == LOW) {
      digitalWrite(EnPin1, HIGH);
      InfraredDetected = true;
      Serial.println("Objek terdeteksi! Motor berhenti.");
      delay(1000);
    } else {
      digitalWrite(EnPin1, LOW);
      digitalWrite(DirPin1, HIGH);
      digitalWrite(StepPin1, HIGH);
      delayMicroseconds(450);
      digitalWrite(StepPin1, LOW);
      delayMicroseconds(450);
    }
  }
}

void Penutupbotol() {
  Serial.println("Memulai proses penutup botol...");
  
  // Aktifkan relay untuk penutup botol
  digitalWrite(Relay, LOW);
  Serial.println("Relay aktif - Proses penutup botol");
  delay(1000); // Waktu untuk proses penutup botol
  
  // Matikan relay
  digitalWrite(Relay, HIGH);
  Serial.println("Relay dimatikan - Penutup botol selesai");
  motorDone = true;
}

void crimping() {
  Serial.println("Memulai proses crimping...");

  // --- 1. Cek posisi TOP untuk Motor 2 (Optical Switch 1) ---
  if (digitalRead(Opticalswitch1) == HIGH) {
    Serial.println("Posisi TOP sudah tertekan - Motor 2 di posisi TOP");
  } else {
    Serial.println("Mencari posisi TOP (Motor 2)...");
    digitalWrite(DirPin2, LOW);  // Arah ke atas
    digitalWrite(EnPin2, LOW);
    while (digitalRead(Opticalswitch1) == LOW) {
      stepMotor(StepPin2, 1, 100);
    }
    digitalWrite(EnPin2, HIGH);
    Serial.println("Posisi TOP ditemukan.");
  }

  delay(2000);  // Jeda 2 detik

  // --- 2. Gerak Motor 2 ke posisi BOTTOM (Optical Switch 2) ---
  Serial.println("Mencari posisi BOTTOM...");
  digitalWrite(DirPin2, HIGH);  // Arah ke bawah
  digitalWrite(EnPin2, LOW);
  while (digitalRead(Opticalswitch2) == LOW) {
    stepMotor(StepPin2, 1, 100);
  }
  digitalWrite(EnPin2, HIGH);
  Serial.println("Posisi BOTTOM ditemukan.");

  delay(1000);

  // --- 3. Proses CRIMPING dengan Motor 3 (turunkan dengan torsi besar) ---
Serial.println("CRIMPING dimulai - Motor 3 ke bawah dengan torsi besar...");
digitalWrite(DirPin3,LOW);  // Turun
digitalWrite(EnPin3, LOW);    // Aktifkan driver

// Bergerak sampai menyentuh Optical Switch 3
while (digitalRead(Opticalswitch3) == LOW) {
  stepMotor(StepPin3, 1, 850);  // Langkah demi langkah
}

Serial.println("Crimping selesai - Optical Switch 3 aktif.");
digitalWrite(EnPin3, HIGH);   // Matikan motor sementara

// Tambahkan delay 3 detik
delay(3000);

// --- 4. Motor 3 kembali ke atas sejauh 3600 langkah ---
Serial.println("Motor 3 kembali ke atas (3600 langkah)...");
digitalWrite(DirPin3, HIGH);   // Arah berlawanan (naik)
digitalWrite(EnPin3, LOW);    // Aktifkan kembali motor
stepMotor(StepPin3, 3000, 350);  // Jalankan 3600 langkah dengan delay 350µs
digitalWrite(EnPin3, HIGH);   // Matikan motor
Serial.println("Motor 3 selesai kembali ke atas.");


  // --- 5. Motor 2 kembali ke posisi TOP (Optical Switch 1) ---
  Serial.println("Motor 2 kembali ke posisi TOP...");
  digitalWrite(DirPin2, LOW);  // Naik
  digitalWrite(EnPin2, LOW);
  while (digitalRead(Opticalswitch1) == LOW) {
    stepMotor(StepPin2, 1, 100);
  }
  digitalWrite(EnPin2, HIGH);
  Serial.println("Motor 2 kembali ke posisi TOP.");

  Serial.println("=== Proses crimping lengkap selesai! ===");
}


void Infraredakhir() {
  Serial.println("Deteksi infrared akhir...");
  
  while (!InfraredDetected2) {
    if (digitalRead(Infrared2) == LOW) {
      digitalWrite(EnPin1, HIGH);
      InfraredDetected2 = true;
      Serial.println("Objek terdeteksi di akhir - Proses selesai");
      
      // Aktifkan buzzer sebagai indikator selesai
      tone(Buzzer, 1000);
      delay(1000);
      noTone(Buzzer);
      Serial.println("Buzzer dimatikan.");
      
    } else {
      // Jika tidak ada objek terdeteksi, beri sinyal error
      digitalWrite(EnPin1, HIGH);
      tone(Buzzer, 500); // Nada lebih rendah untuk error
      delay(500);
      noTone(Buzzer);
      delay(500);
      Serial.println("Menunggu objek di posisi akhir...");
    }
  }
}
void stepMotor(int stepPin, int steps, int delayTime) {
  for (int i = 0; i < steps; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(delayTime);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(delayTime);
  }
}
