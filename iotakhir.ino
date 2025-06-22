#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <PubSubClient.h>

// -------------------- WiFi & MQTT Configuration --------------------
const char* ssid = "122";          // Ganti dengan SSID WiFi Anda
const char* password = "12345678";   // Ganti dengan password WiFi Anda
const char* mqtt_server = "test.mosquitto.org";  // Ganti dengan alamat MQTT broker Anda
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// MQTT Topics
const char* topic_quantity_display = "crimping/quantity/display";
const char* topic_quantity_plus = "crimping/quantity/plus";
const char* topic_quantity_minus = "crimping/quantity/minus";
const char* topic_quantity_enter = "crimping/quantity/enter";
const char* topic_status = "crimping/status";
const char* topic_progress_percent = "crimping/progress/percent";  // Progress dalam persentase
const char* topic_progress = "crimping/progress"; 
const char* topic_error = "crimping/error";
const char* topic_mode = "crimping/mode";  // Topic untuk mode switching
const char* topic_clear = "crimping/clear";  // Clear dashboard

// -------------------- OLED Configuration --------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// -------------------- LED WS2812b Configuration --------------------
#define LED_PIN     18    // Pin data LED WS2812b
#define LED_COUNT   8    // Jumlah LED
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// -------------------- LED Variables --------------------
unsigned long lastLedUpdate = 0;
const unsigned long ledStandbyInterval = 500;  // Interval kelip kelip hijau (500ms)
const unsigned long ledRunningInterval = 200;  // Interval bergerak merah (200ms)
int currentLedPosition = 0;  // Posisi LED yang sedang menyala saat running
bool ledStandbyState = false;  // Status kelip kelip LED standby

// -------------------- Deklarasi Pin ------------------
const int Buttonganti       = 12;
const int Buttonmax         = 13;  // Button untuk tambah kuantitas
const int Buttonmin         = 14;  // Button untuk kurang kuantitas
const int Buttondone        = 15;  // Button enter/confirm kuantitas

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

// -------------------- Variabel Kuantitas --------------------
int targetQuantity = 1;        // Kuantitas target (default 1)
int currentQuantity = 0;       // Kuantitas yang telah diproses
bool quantitySet = false;      // Status apakah kuantitas sudah diatur
bool systemRunning = false;    // Status sistem berjalan untuk LED
unsigned long lastButtonPress = 0;
const unsigned long debounceDelay = 200;

// -------------------- Input Mode Variables --------------------
enum InputMode {
  MQTT_MODE,
  MANUAL_MODE
};

InputMode currentInputMode = MQTT_MODE;  // Default menggunakan MQTT
unsigned long lastActivity = 0;
const unsigned long timeoutDuration = 30000;  // 30 detik timeout
bool modeChangeRequested = false;

// -------------------- MQTT Variables --------------------
bool mqttConnected = false;
unsigned long lastMqttReconnectAttempt = 0;
const unsigned long mqttReconnectInterval = 5000;
bool systemError = false;
String lastError = "";

// -------------------- Function Declarations --------------------
void setupWiFi();
void setupMQTT();
void callback(char* topic, byte* payload, unsigned int length);
void reconnectMQTT();
void publishStatus(String status);
void publishProgress(int current, int total);
void publishError(String error);
void publishQuantityDisplay(int quantity);
void publishMode(String mode);
void homingInfrared();
void Penutupbotol();
void crimping();
void Infraredakhir();
void stepMotor(int stepPin, int steps, int delayTime);
void kuantitas();
void kuantitasMQTT();
void kuantitasManual();
void updateDisplay();
void updateDisplayMQTT();
void updateDisplayManual();
void displayProgress();
void updateLED();
void setLEDStandby();
void setLEDRunning();
void setLEDOff();
void setLEDError();
void handleSystemError(String errorMessage);
void checkModeSwitch();
void resetQuantitySettings();
void clearMQTTDashboard();

// ================== PERBAIKAN UTAMA: Setup Function ==================
void setup() {
  Serial.begin(9600);

  // ... inisialisasi hardware tetap sama ...
  strip.begin();
  strip.show();
  strip.setBrightness(200);

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Gagal inisialisasi OLED"));
    handleSystemError("OLED Init Failed");
    for(;;);
  }
  
  // Display startup message
  display.clearDisplay();
  display.setTextSize(1.5);
  display.setTextColor(SSD1306_WHITE);
  String line1 = "Crimping dan Penutup";
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(line1, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((128 - w) / 2, 0);
  display.println(line1);
  String line2 = "Otomatis";
  display.getTextBounds(line2, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((128 - w) / 2, 12);
  display.println(line2);
  String line3 = "Siap !!!";
  display.getTextBounds(line3, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((128 - w) / 2, 24);
  display.println(line3);
  display.display();

  // LED startup animation
  for(int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 255));
    strip.show();
    delay(100);
  }
  delay(2000);
  setLEDOff();
  delay(3000);

  // Setup WiFi dan MQTT
  setupWiFi();
  setupMQTT();

  // ... setup pin tetap sama ...
  pinMode(Buttonganti, INPUT_PULLUP);
  pinMode(Buttonmax, INPUT_PULLUP);
  pinMode(Buttonmin, INPUT_PULLUP);
  pinMode(Buttondone, INPUT_PULLUP);
  
  pinMode(Opticalswitch1, INPUT);
  pinMode(Opticalswitch2, INPUT);
  pinMode(Opticalswitch3, INPUT);
  pinMode(Infrared1, INPUT);
  pinMode(Infrared2, INPUT);

  pinMode(EnPin1, OUTPUT); pinMode(StepPin1, OUTPUT); pinMode(DirPin1, OUTPUT);
  pinMode(EnPin2, OUTPUT); pinMode(StepPin2, OUTPUT); pinMode(DirPin2, OUTPUT);
  pinMode(EnPin3, OUTPUT); pinMode(StepPin3, OUTPUT); pinMode(DirPin3, OUTPUT);

  pinMode(Buzzer, OUTPUT);
  pinMode(Relay, OUTPUT);
  digitalWrite(Relay, HIGH);

  digitalWrite(EnPin1, HIGH);
  digitalWrite(EnPin2, HIGH);
  digitalWrite(EnPin3, HIGH);
  
  lastActivity = millis();
  
  // PERBAIKAN: Inisialisasi mode berdasarkan koneksi
  if (WiFi.status() == WL_CONNECTED && mqttConnected) {
    currentInputMode = MQTT_MODE;
    publishStatus("System Ready - MQTT Mode Active (Default)");
    publishMode("MQTT");
    publishQuantityDisplay(targetQuantity);
    Serial.println("Mode Default: MQTT (WiFi & MQTT Connected)");
  } else {
    currentInputMode = MANUAL_MODE;
    Serial.println("Mode Fallback: MANUAL (WiFi/MQTT Failed)");
  }
  
  Serial.println("Sistem Crimping Otomatis Siap!");
  Serial.println("Tekan BUTTON GANTI untuk switch ke mode MANUAL");
}

// ================== PERBAIKAN: Setup WiFi ==================
void setupWiFi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Connecting WiFi...");
  display.println(ssid);
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    
    if (attempts % 5 == 0) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("Connecting WiFi...");
      display.println(ssid);
      display.setCursor(0, 16);
      display.print("Attempt: ");
      display.print(attempts + 1);
      display.println("/30");
      display.display();
    }
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    display.setCursor(0, 8);
    display.print("IP: ");
    display.println(WiFi.localIP());
    display.setCursor(0, 16);
    display.println("Connecting MQTT...");
    display.display();
    delay(2000);
    
  } else {
    Serial.println("WiFi connection failed");
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("WiFi Failed!");
    display.setCursor(0, 8);
    display.println("Check SSID/Password");
    display.setCursor(0, 16);
    display.println("Using Manual Mode");
    display.display();
    delay(3000);
    
    currentInputMode = MANUAL_MODE;
    handleSystemError("WiFi Connection Failed");
  }
}
// ================== PERBAIKAN: MQTT Setup ==================
void setupMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    mqttConnected = false;
    return;
  }
  
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  // Langsung coba connect
  Serial.print("Attempting MQTT connection...");
  
  String clientId = "CrimpingSystem-";
  clientId += String(random(0xffff), HEX);
  
  if (client.connect(clientId.c_str())) {
    Serial.println("connected");
    mqttConnected = true;
    systemError = false;
    
    // Subscribe ke topik yang diperlukan
    client.subscribe(topic_quantity_plus);
    client.subscribe(topic_quantity_minus);
    client.subscribe(topic_quantity_enter);
    
    // Set status awal
    publishStatus("System Initialized - MQTT Mode Ready");
    publishMode("MQTT");
    publishQuantityDisplay(targetQuantity);
    publishProgressPercent(0, 1);
    
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" - will retry in loop");
    mqttConnected = false;
  }
}

// ================== PERBAIKAN: Reconnect MQTT ==================
void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  
  if (!client.connected() && (millis() - lastMqttReconnectAttempt > mqttReconnectInterval)) {
    Serial.print("Attempting MQTT connection...");
    
    String clientId = "CrimpingSystem-";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      mqttConnected = true;
      systemError = false;
      
      // Subscribe ke topik yang diperlukan
      client.subscribe(topic_quantity_plus);
      client.subscribe(topic_quantity_minus);
      client.subscribe(topic_quantity_enter);
      
      // AUTO-SWITCH ke MQTT mode setelah reconnect (jika tidak sedang proses)
      if (!quantitySet && !systemRunning) {
        currentInputMode = MQTT_MODE;
        publishStatus("MQTT Reconnected - Switched to MQTT Mode");
        publishMode("MQTT");
        publishQuantityDisplay(targetQuantity);
        Serial.println("MQTT Reconnected: Auto-switched to MQTT Mode");
      } else {
        publishStatus("MQTT Reconnected - Process Continuing");
      }
      
      publishProgressPercent(currentQuantity, targetQuantity);
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      mqttConnected = false;
      
      // Jika sedang tidak proses dan MQTT gagal, fallback ke manual
      if (!quantitySet && !systemRunning) {
        currentInputMode = MANUAL_MODE;
        handleSystemError("MQTT Connection Failed - Code: " + String(client.state()));
      }
    }
    lastMqttReconnectAttempt = millis();
  }
}

// ================== PERBAIKAN: MQTT Callback ==================
void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.println(message);
  
  // Reset activity timer saat ada aktivitas MQTT
  lastActivity = millis();
  
  // HANYA proses jika dalam mode MQTT dan tidak sedang proses
  if (currentInputMode == MQTT_MODE && !quantitySet && !systemError && !systemRunning) {
    if (String(topic) == topic_quantity_plus) {
      if (targetQuantity < 999) {
        targetQuantity++;
        publishQuantityDisplay(targetQuantity);
        publishMode("MQTT INPUT");
        Serial.print("MQTT: Kuantitas naik menjadi ");
        Serial.println(targetQuantity);
        
        tone(Buzzer, 800);
        delay(100);
        noTone(Buzzer);
      }
    }
    else if (String(topic) == topic_quantity_minus) {
      if (targetQuantity > 1) {
        targetQuantity--;
        publishQuantityDisplay(targetQuantity);
        publishMode("MQTT INPUT");
        Serial.print("MQTT: Kuantitas turun menjadi ");
        Serial.println(targetQuantity);
        
        tone(Buzzer, 600);
        delay(100);
        noTone(Buzzer);
      }
    }
    else if (String(topic) == topic_quantity_enter) {
      quantitySet = true;
      publishStatus("Quantity Confirmed via MQTT: " + String(targetQuantity));
      publishMode("MQTT CONFIRMED");
      Serial.print("MQTT: Kuantitas dikonfirmasi: ");
      Serial.println(targetQuantity);
      
      // LED konfirmasi
      for(int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 255));
      }
      strip.show();
      
      tone(Buzzer, 750);
      delay(500);
      noTone(Buzzer);
      
      setLEDOff();
    }
  }
}

// -------------------- MQTT Publish Functions --------------------
void publishStatus(String status) {
  if (mqttConnected && client.connected()) {
    client.publish(topic_status, status.c_str());
  }
}

void publishProgressPercent(int current, int total) {
  if (mqttConnected && client.connected() && total > 0) {
    int percent = (current * 100) / total;
    client.publish(topic_progress_percent, String(percent).c_str());
    
    // Juga publish progress biasa untuk kompatibilitas
    String progress = String(current) + "/" + String(total);
    client.publish(topic_progress, progress.c_str());
  }
}

void publishError(String error) {
  if (mqttConnected && client.connected()) {
    client.publish(topic_error, error.c_str());
  }
}

void publishQuantityDisplay(int quantity) {
  if (mqttConnected && client.connected()) {
    client.publish(topic_quantity_display, String(quantity).c_str());
  }
}

void publishMode(String mode) {
  if (mqttConnected && client.connected()) {
    client.publish(topic_mode, mode.c_str());
  }
}

// -------------------- Error Handling --------------------
void handleSystemError(String errorMessage) {
  systemError = true;
  lastError = errorMessage;
  
  Serial.print("SYSTEM ERROR: ");
  Serial.println(errorMessage);
  
  // Publish error ke MQTT
  publishError(errorMessage);
  
  // Update LED ke mode error
  setLEDError();
  
  // Beep error pattern
  for (int i = 0; i < 2; i++) {
    tone(Buzzer, 300);
    delay(200);
    noTone(Buzzer);
    delay(100);
  }
}

// -------------------- Loop --------------------
void loop() {
  // Handle MQTT connection
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      reconnectMQTT();
    } else {
      client.loop();
    }
  }
  
  // Check for mode switching
  checkModeSwitch();
  
  // Update LED berdasarkan status sistem
  updateLED();
  
  // 1. Atur kuantitas terlebih dahulu
  if (!quantitySet) {
    systemRunning = false;
    kuantitas();
    return;
  }
  
  // 2. Mulai proses produksi
  systemRunning = true;
  publishStatus("Production Started - Processing " + String(targetQuantity) + " items");
  
  while (currentQuantity < targetQuantity) {
    // Reset status untuk siklus baru
    InfraredDetected = false;
    InfraredDetected2 = false;
    motorDone = false;
    
    Serial.print("=== Memulai Siklus ");
    Serial.print(currentQuantity + 1);
    Serial.print(" dari ");
    Serial.print(targetQuantity);
    Serial.println(" ===");
    
    // PERBAIKAN: Gunakan progress persentase
    publishProgressPercent(currentQuantity, targetQuantity);
    displayProgress();
    
    // 1. Homing dengan infrared sensor
    homingInfrared();
    
    // 2. Motor menuju penutup botol
    Serial.println("Motor menuju posisi penutup botol...");
    publishStatus("Moving to bottle cap position");
    digitalWrite(EnPin1, LOW);
    digitalWrite(DirPin1, HIGH);
    for (int x = 0; x < 3200; x++) {
      digitalWrite(StepPin1, HIGH);  
      delayMicroseconds(250);
      digitalWrite(StepPin1, LOW);
      delayMicroseconds(250);
      
      // Update LED setiap beberapa step untuk efisiensi
      if (x % 100 == 0) {
        updateLED();
        if (mqttConnected) client.loop();
      }
    }
    digitalWrite(EnPin1, HIGH);
    
    // 3. Proses penutup botol
    publishStatus("Processing bottle cap");
    Penutupbotol();
    
    // 4. Motor ke posisi crimping
    Serial.println("Motor menuju posisi crimping...");
    publishStatus("Moving to crimping position");
    digitalWrite(EnPin1, LOW);
    digitalWrite(DirPin1, HIGH);
    for (int x = 0; x < 51200; x++) {
      digitalWrite(StepPin1, HIGH);  
      delayMicroseconds(250);
      digitalWrite(StepPin1, LOW);
      delayMicroseconds(250);
      
      // Update LED setiap beberapa step untuk efisiensi
      if (x % 100 == 0) {
        updateLED();
        if (mqttConnected) client.loop();
      }
    }
    digitalWrite(EnPin1, HIGH);
    
    // 5. Proses crimping
    publishStatus("Processing crimping");
    crimping();
    
    // 6. Motor ke posisi akhir
    Serial.println("Motor menuju posisi akhir...");
    publishStatus("Moving to final position");
    digitalWrite(EnPin1, LOW);
    digitalWrite(DirPin1, HIGH);
    for (int x = 0; x < 12800; x++) {
      digitalWrite(StepPin1, HIGH);  
      delayMicroseconds(250);
      digitalWrite(StepPin1, LOW);
      delayMicroseconds(250);
      
      // Update LED setiap beberapa step untuk efisiensi
      if (x % 100 == 0) {
        updateLED();
        if (mqttConnected) client.loop();
      }
    }
    digitalWrite(EnPin1, HIGH);
    
    // 7. Deteksi infrared akhir
    publishStatus("Final infrared detection");
    Infraredakhir();
    
    digitalWrite(EnPin1, HIGH);
    
    // Increment jumlah yang telah diproses
     currentQuantity++;
    publishProgressPercent(currentQuantity, targetQuantity);  // Update persentase
    
    Serial.print("=== Siklus ");
    Serial.print(currentQuantity);
    Serial.println(" Selesai ===");
    
    if (currentQuantity < targetQuantity) {
      unsigned long delayStart = millis();
      while (millis() - delayStart < 2000) {
        updateLED();
        if (mqttConnected) client.loop();
        delay(50);
      }
    }
  }
  
   // Semua siklus selesai
  systemRunning = false;
  
  Serial.println("=== SEMUA PROSES SELESAI ===");
  publishStatus("All processes completed successfully!");
  publishProgressPercent(currentQuantity, targetQuantity);  // 100%
  
  // ... animasi LED dan buzzer tetap sama ...
  
  // PERBAIKAN: Clear dashboard setelah selesai
  delay(3000);  // Tunggu sebentar agar user bisa melihat status selesai
  clearMQTTDashboard();
  
  // Reset untuk batch baru
  resetQuantitySettings();
  
  unsigned long delayStart = millis();
  while (millis() - delayStart < 5000) {
    updateLED();
    if (mqttConnected) client.loop();
    delay(50);
  }
}


// ================== PERBAIKAN: Reset Function ==================
void resetQuantitySettings() {
  quantitySet = false;
  currentQuantity = 0;
  targetQuantity = 1;
  
  // PRIORITAS: Selalu kembali ke MQTT jika terhubung
  if (WiFi.status() == WL_CONNECTED && mqttConnected && client.connected()) {
    currentInputMode = MQTT_MODE;  // SELALU default ke MQTT
    publishStatus("Ready for new batch - MQTT Mode (Default)");
    publishMode("MQTT");
    publishQuantityDisplay(targetQuantity);
    Serial.println("Reset: Back to MQTT Mode (Default)");
  } else {
    currentInputMode = MANUAL_MODE;
    Serial.println("Reset: Fallback to Manual Mode (No Connection)");
  }
  
  lastActivity = millis();
  systemError = false;
  publishProgressPercent(0, 1);
}


/// ================== PERBAIKAN: Mode Switching yang Lebih Presisi ==================
void checkModeSwitch() {
  static bool buttonPressed = false;
  
  // Deteksi penekanan button dengan debounce yang lebih baik
  if (digitalRead(Buttonganti) == LOW) {
    if (!buttonPressed && (millis() - lastButtonPress) > debounceDelay) {
      buttonPressed = true;
      
      // HANYA switch dari MQTT ke MANUAL jika sedang tidak proses
      if (currentInputMode == MQTT_MODE && !quantitySet && !systemRunning) {
        currentInputMode = MANUAL_MODE;
        lastActivity = millis();
        Serial.println("USER ACTION: Switching to Manual Mode");
        publishStatus("User switched to Manual Input Mode");
        publishMode("MANUAL INPUT");
        
        // Beep konfirmasi switch
        tone(Buzzer, 1000);
        delay(200);
        noTone(Buzzer);
      }
      
      lastButtonPress = millis();
    }
  } else {
    buttonPressed = false; // Reset flag saat button dilepas
  }
  
  // Auto-timeout dari manual ke MQTT (HANYA jika tidak sedang proses)
  if (currentInputMode == MANUAL_MODE && !quantitySet && !systemRunning) {
    if (millis() - lastActivity > timeoutDuration) {
      // Pastikan MQTT masih terhubung sebelum switch
      if (WiFi.status() == WL_CONNECTED && mqttConnected && client.connected()) {
        currentInputMode = MQTT_MODE;
        lastActivity = millis();
        Serial.println("TIMEOUT: Auto-switching back to MQTT Mode");
        publishStatus("Timeout - Auto-switched back to MQTT Mode");
        publishMode("MQTT");
        publishQuantityDisplay(targetQuantity);
        
        // Beep timeout
        tone(Buzzer, 500);
        delay(100);
        noTone(Buzzer);
        delay(100);
        tone(Buzzer, 500);
        delay(100);
        noTone(Buzzer);
      }
    }
  }
}


// -------------------- Quantity Input Functions --------------------
void kuantitas() {
  if (currentInputMode == MQTT_MODE) {
    kuantitasMQTT();
  } else {
    kuantitasManual();
  }
}

void kuantitasMQTT() {
  Serial.println("=== PENGATURAN KUANTITAS - MQTT MODE ===");
  
  // Loop menunggu hingga kuantitas diset melalui MQTT atau switch ke manual
  while (!quantitySet) {
    // Update display untuk mode MQTT
    updateDisplayMQTT();
    
    // Handle MQTT
    if (mqttConnected && client.connected()) {
      client.loop();
    } else if (WiFi.status() == WL_CONNECTED) {
      reconnectMQTT();
    }
    
    // Update LED
    updateLED();
    
    // Check mode switch
    checkModeSwitch();
    
    // Jika mode berubah ke manual, keluar dari fungsi ini
    if (currentInputMode == MANUAL_MODE) {
      return;
    }
    
    delay(50);
  }
  
  // Tampilkan konfirmasi
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("KUANTITAS SET (IoT):");
  display.setTextSize(2);
  display.setCursor(40,15);
  display.print(targetQuantity);
  display.display();
  delay(2000);
}

// ================== PERBAIKAN 5: Manual Mode dengan Status Display ==================
void kuantitasManual() {
  Serial.println("=== PENGATURAN KUANTITAS - MANUAL MODE ===");
  Serial.println("Gunakan Button MAX/MIN untuk mengatur, DONE untuk konfirmasi");
  
  lastActivity = millis(); // Reset timer saat masuk manual mode
  publishMode("MANUAL INPUT"); // PERBAIKAN: Tampilkan status input manual
  
  while (!quantitySet) {
    unsigned long currentTime = millis();
    
    // Update display untuk mode manual
    updateDisplayManual();
    
    // Update LED selama pengaturan kuantitas
    updateLED();
    
    // Handle MQTT untuk menjaga koneksi
    if (mqttConnected && client.connected()) {
      client.loop();
    }
    
    // Check timeout untuk kembali ke MQTT mode
    checkModeSwitch();
    if (currentInputMode == MQTT_MODE) {
      return; // Keluar dari manual mode
    }
    
    // Button MAX - Tambah kuantitas
    if (digitalRead(Buttonmax) == LOW && (currentTime - lastButtonPress) > debounceDelay) {
      if (targetQuantity < 999) { // Batas maksimal 999
        targetQuantity++;
        lastActivity = currentTime; // Reset timeout
        publishMode("MANUAL INPUT"); // PERBAIKAN: Update status saat input
        Serial.print("Kuantitas: ");
        Serial.println(targetQuantity);
        
        // Beep feedback
        tone(Buzzer, 800);
        delay(100);
        noTone(Buzzer);
      }
      lastButtonPress = currentTime;
    }
    
    // Button MIN - Kurang kuantitas
    if (digitalRead(Buttonmin) == LOW && (currentTime - lastButtonPress) > debounceDelay) {
      if (targetQuantity > 1) { // Minimum 1
        targetQuantity--;
        lastActivity = currentTime; // Reset timeout
        publishMode("MANUAL INPUT"); // PERBAIKAN: Update status saat input
        Serial.print("Kuantitas: ");
        Serial.println(targetQuantity);
        
        // Beep feedback
        tone(Buzzer, 600);
        delay(100);
        noTone(Buzzer);
      }
      lastButtonPress = currentTime;
    }
    
    // Button DONE - Konfirmasi kuantitas
    if (digitalRead(Buttondone) == LOW && (currentTime - lastButtonPress) > debounceDelay) {
      quantitySet = true;
      Serial.print("Kuantitas dikonfirmasi (Manual): ");
      Serial.println(targetQuantity);
      publishStatus("Quantity confirmed (Manual): " + String(targetQuantity));
      publishMode("MANUAL CONFIRMED"); // PERBAIKAN: Tampilkan konfirmasi manual
      
      // LED konfirmasi - semua LED biru sebentar
      for(int i = 0; i < LED_COUNT; i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 255)); // Biru
      }
      strip.show();
      
      // Beep konfirmasi panjang
      tone(Buzzer, 750);
      delay(500);
      noTone(Buzzer);
      
      setLEDOff(); // Matikan LED setelah konfirmasi
      
      // Tampilkan konfirmasi
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0,0);
      display.println("KUANTITAS SET:");
      display.setTextSize(2);
      display.setCursor(40,15);
      display.print(targetQuantity);
      display.display();
      delay(2000);
      
      lastButtonPress = currentTime;
    }
    
    delay(50); // Delay kecil untuk stabilitas
  }
}


// -------------------- Display Functions --------------------
void updateDisplayMQTT() {
  display.clearDisplay();
  display.setTextSize(1);
  
  if (systemError) {
    display.setCursor(0, 0);
    display.println("ERROR MQTT:");
    display.setCursor(0, 12);
    display.println(lastError.substring(0, 21));
    if (lastError.length() > 21) {
      display.setCursor(0, 24);
      display.println(lastError.substring(21));
    }
  } else {
    // Status normal - tampilkan mode MQTT
    display.setCursor(0, 0);
    display.println("Mode: MQTT (Default)");
    display.setCursor(0, 12);
    display.print("Kuantitas: ");
    display.print(targetQuantity);
    
    // Status koneksi
    display.setCursor(0, 24);
    if (mqttConnected && client.connected()) {
      display.println("Status: Connected");
    } else {
      display.println("Status: Reconnecting");
    }
  }
  
  display.display();
}

void updateDisplayManual() {
  display.clearDisplay();
  display.setTextSize(1);
  
  // Tampilkan mode manual
  display.setCursor(0, 0);
  display.println("Mode: MANUAL");
  
  display.setCursor(0, 12);
  display.print("Kuantitas: ");
  display.print(targetQuantity);
  
  // Tampilkan instruksi
  display.setCursor(0, 24);
  display.println("MAX/MIN/DONE");
  
  display.display();
}

void displayProgress() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("PROSES BERJALAN:");
  
  display.setTextSize(2);
  display.setCursor(0, 15);
  display.print(currentQuantity + 1);
  display.print("/");
  display.print(targetQuantity);
  
  display.display();
}

// -------------------- LED Control Functions --------------------
void updateLED() {
  unsigned long currentTime = millis();
  
  if (systemError) {
    setLEDError();
  } else if (systemRunning) {
    setLEDRunning();
  } else {
    setLEDStandby();
  }
}

void setLEDStandby() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastLedUpdate >= ledStandbyInterval) {
    ledStandbyState = !ledStandbyState;
    
    for(int i = 0; i < LED_COUNT; i++) {
      if (ledStandbyState) {
        strip.setPixelColor(i, strip.Color(0, 100, 0)); // Hijau redup
      } else {
        strip.setPixelColor(i, strip.Color(0, 0, 0)); // Mati
      }
    }
    strip.show();
    lastLedUpdate = currentTime;
  }
}

void setLEDRunning() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastLedUpdate >= ledRunningInterval) {
    // Clear semua LED
    for(int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
    
    // Set LED yang sedang aktif
    strip.setPixelColor(currentLedPosition, strip.Color(255, 0, 0)); // Merah
    strip.show();
    
    // Pindah ke LED berikutnya
    currentLedPosition = (currentLedPosition + 1) % LED_COUNT;
    lastLedUpdate = currentTime;
  }
}

void setLEDError() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastLedUpdate >= 250) { // Berkedip cepat untuk error
    ledStandbyState = !ledStandbyState;
    
    for(int i = 0; i < LED_COUNT; i++) {
      if (ledStandbyState) {
        strip.setPixelColor(i, strip.Color(255, 0, 0)); // Merah terang
      } else {
        strip.setPixelColor(i, strip.Color(0, 0, 0)); // Mati
      }
    }
    strip.show();
    lastLedUpdate = currentTime;
  }
}
void clearMQTTDashboard() {
  if (mqttConnected && client.connected()) {
    // Clear semua data di dashboard
    client.publish(topic_quantity_display, "0", true);  // retained message
    client.publish(topic_progress, "0/0", true);
    client.publish(topic_progress_percent, "0", true);
    client.publish(topic_status, "System Offline", true);
    client.publish(topic_error, "", true);
    client.publish(topic_mode, "OFFLINE", true);
    client.publish(topic_clear, "1", true);
    
    Serial.println("MQTT Dashboard cleared");
  }
}

void setLEDOff() {
  for(int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 0));
  }
  strip.show();
}

// -------------------- Motor Control Functions --------------------
void homingInfrared() {
  Serial.println("=== HOMING DENGAN INFRARED ===");
  publishStatus("Homing with infrared sensor");
  
  digitalWrite(EnPin1, LOW);
  digitalWrite(DirPin1, LOW); // Arah mundur untuk homing
  
  // Bergerak mundur hingga infrared terdeteksi
  while (digitalRead(Infrared1) == LOW) {
    digitalWrite(StepPin1, HIGH);
    delayMicroseconds(500);
    digitalWrite(StepPin1, LOW);
    delayMicroseconds(500);
    
    // Update LED dan MQTT setiap beberapa step
    static int stepCount = 0;
    stepCount++;
    if (stepCount % 100 == 0) {
      updateLED();
      if (mqttConnected) client.loop();
    }
  }
  
  Serial.println("Homing selesai - Infrared terdeteksi");
  digitalWrite(EnPin1, HIGH);
  
  // Beep konfirmasi homing
  tone(Buzzer, 900);
  delay(150);
  noTone(Buzzer);
}

void Penutupbotol() {
  Serial.println("=== PROSES PENUTUP BOTOL ===");
  
  // Tunggu hingga infrared2 terdeteksi (botol masuk)
  Serial.println("Menunggu botol masuk (Infrared2)...");
  publishStatus("Waiting for bottle - Infrared2");
  
  while (digitalRead(Infrared2) == LOW) {
    updateLED();
    if (mqttConnected) client.loop();
    delay(10);
  }
  
  InfraredDetected2 = true;
  Serial.println("Botol terdeteksi!");
  publishStatus("Bottle detected - Processing cap");
  
  // Nyalakan relay untuk proses penutup
  digitalWrite(Relay, LOW);
  Serial.println("Relay ON - Proses penutup botol");
  
  // Motor 2 turun (penutup botol)
  digitalWrite(EnPin2, LOW);
  digitalWrite(DirPin2, HIGH);
  
  while (digitalRead(Opticalswitch2) == LOW) {
    digitalWrite(StepPin2, HIGH);
    delayMicroseconds(500);
    digitalWrite(StepPin2, LOW);
    delayMicroseconds(500);
    
    // Update LED setiap beberapa step
    static int stepCount = 0;
    stepCount++;
    if (stepCount % 50 == 0) {
      updateLED();
      if (mqttConnected) client.loop();
    }
  }
  
  Serial.println("Motor 2 mencapai batas bawah");
  delay(1000); // Tunggu sebentar untuk proses penutup
  
  // Motor 2 naik kembali
  digitalWrite(DirPin2, LOW);
  
  while (digitalRead(Opticalswitch1) == LOW) {
    digitalWrite(StepPin2, HIGH);
    delayMicroseconds(500);
    digitalWrite(StepPin2, LOW);
    delayMicroseconds(500);
    
    // Update LED setiap beberapa step
    static int stepCount = 0;
    stepCount++;
    if (stepCount % 50 == 0) {
      updateLED();
      if (mqttConnected) client.loop();
    }
  }
  
  Serial.println("Motor 2 kembali ke posisi atas");
  digitalWrite(EnPin2, HIGH);
  
  // Matikan relay
  digitalWrite(Relay, HIGH);
  Serial.println("Relay OFF - Proses penutup selesai");
  
  // Beep konfirmasi selesai penutup
  tone(Buzzer, 1200);
  delay(200);
  noTone(Buzzer);
}

void crimping() {
  Serial.println("=== PROSES CRIMPING ===");
  publishStatus("Crimping process started");
  
  // Motor 3 untuk crimping
  digitalWrite(EnPin3, LOW);
  digitalWrite(DirPin3, HIGH);
  
  // Bergerak hingga optical switch 3 terdeteksi
  while (digitalRead(Opticalswitch3) == LOW) {
    digitalWrite(StepPin3, HIGH);
    delayMicroseconds(400);
    digitalWrite(StepPin3, LOW);
    delayMicroseconds(400);
    
    // Update LED setiap beberapa step
    static int stepCount = 0;
    stepCount++;
    if (stepCount % 50 == 0) {
      updateLED();
      if (mqttConnected) client.loop();
    }
  }
  
  Serial.println("Crimping maksimal tercapai");
  delay(500); // Tahan posisi crimping
  
  // Kembali ke posisi awal
  digitalWrite(DirPin3, LOW);
  for (int x = 0; x < 6400; x++) { // Kembali ke posisi awal
    digitalWrite(StepPin3, HIGH);
    delayMicroseconds(400);
    digitalWrite(StepPin3, LOW);
    delayMicroseconds(400);
    
    // Update LED setiap beberapa step
    if (x % 100 == 0) {
      updateLED();
      if (mqttConnected) client.loop();
    }
  }
  
  digitalWrite(EnPin3, HIGH);
  Serial.println("Crimping selesai");
  publishStatus("Crimping completed");
  
  // Beep konfirmasi crimping selesai
  tone(Buzzer, 1000);
  delay(300);
  noTone(Buzzer);
}

void Infraredakhir() {
  Serial.println("=== DETEKSI INFRARED AKHIR ===");
  publishStatus("Final infrared detection");
  
  // Tunggu hingga produk melewati infrared akhir
  Serial.println("Menunggu produk melewati sensor akhir...");
  
  // Tunggu hingga infrared terdeteksi
  while (digitalRead(Infrared1) == LOW) {
    updateLED();
    if (mqttConnected) client.loop();
    delay(10);
  }
  
  InfraredDetected = true;
  Serial.println("Produk terdeteksi di sensor akhir");
  
  // Tunggu hingga produk tidak terdeteksi lagi (melewati sensor)
  while (digitalRead(Infrared1) == HIGH) {
    updateLED();
    if (mqttConnected) client.loop();
    delay(10);
  }
  
  Serial.println("Produk telah melewati sensor akhir");
  publishStatus("Product passed final sensor");
  
  // Beep konfirmasi produk keluar
  tone(Buzzer, 1500);
  delay(150);
  noTone(Buzzer);
  
  motorDone = true;
  Serial.println("Satu siklus produksi selesai");
}

// -------------------- Helper Functions --------------------
void stepMotor(int stepPin, int steps, int delayTime) {
  for (int i = 0; i < steps; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(delayTime);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(delayTime);
    
    // Update LED dan MQTT setiap beberapa step untuk efisiensi
    if (i % 100 == 0) {
      updateLED();
      if (mqttConnected) client.loop();
    }
  }
}