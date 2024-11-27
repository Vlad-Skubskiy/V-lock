#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>

#define LOCK_TIMEOUT  1000
#define MAX_TAGS        3
#define BUZZER_PIN      3
#define RED_LED_PIN     4
#define GREEN_LED_PIN   5
#define RST_PIN         6
#define CS_PIN          7
#define BTN_PIN         8
#define DOOR_PIN        9
#define RELAY_PIN       10
#define EE_START_ADDR   0
#define EE_KEY        100

MFRC522 rfid(CS_PIN, RST_PIN);

#define DECLINE 0
#define SUCCESS 1
#define OPEN_SOUND 4
#define CLOSE_SOUND 5

bool isOpen(void) { return digitalRead(DOOR_PIN); }

bool locked = true;
uint8_t savedTags = 0;

void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();

  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(DOOR_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  bool needClear = false;
  if (digitalRead(BTN_PIN) == LOW) {
    uint32_t start = millis();
    while (digitalRead(BTN_PIN) == LOW) {
      if (millis() - start >= 3000) {
        needClear = true;
        indicate(DECLINE);
        break;
      }
    }
  }

  if (needClear || EEPROM.read(EE_START_ADDR) != EE_KEY) {
    for (uint16_t i = 0; i < EEPROM.length(); i++) EEPROM.write(i, 0x00);
    EEPROM.write(EE_START_ADDR, EE_KEY);
  } else {
    savedTags = EEPROM.read(EE_START_ADDR + 1);
  }


  if (savedTags > 0) {
    if (isOpen()) {
      ledSetup(SUCCESS);
      locked = false;
      unlock();
    } else {
      ledSetup(DECLINE);
      locked = true;
      lock();
    }
  } else {
    ledSetup(SUCCESS);
    locked = false;
    unlock();
  }
}

void loop() {
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    int16_t tagIndex = foundTag(rfid.uid.uidByte, rfid.uid.size);
    if (locked) { 
      indicate(DECLINE);
    } if (tagIndex >= 0) { 
      if (locked) {
        indicate(SUCCESS);
        unlock();
        locked = false;
      } else {
        indicate(SUCCESS);
        lock(); 
        locked = true;
      }
    } else {
      indicate(DECLINE); 
    }
    rfid.PICC_HaltA();
  }
}

void ledSetup(bool state) {
  if (state) {
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
  } else {
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, HIGH);
  }
}

void indicate(uint8_t signal) {
  ledSetup(signal);
  switch (signal) {
    case DECLINE:
      Serial.println("DECLINE");
      tone(BUZZER_PIN, 5000, 3000); 
      return;
    case SUCCESS:
      Serial.println("SUCCESS");
      tone(BUZZER_PIN, 890, 330); 
      return;
    case OPEN_SOUND: 
      Serial.println("OPEN SOUND");
      tone(BUZZER_PIN, 3000, 100);
      return;
    case CLOSE_SOUND: 
      Serial.println("CLOSE SOUND");
      tone(BUZZER_PIN, 200, 150);
      return;
  }
}

void lock(void) {
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("lock");
  indicate(CLOSE_SOUND);
}

void unlock(void) {
  digitalWrite(RELAY_PIN, HIGH);
  Serial.println("unlock");
  indicate(OPEN_SOUND);
}

int16_t foundTag(uint8_t *tag, uint8_t size) {
  uint8_t buf[8];
  uint16_t address;
  for (uint8_t i = 0; i < savedTags; i++) {
    address = (i * 8) + EE_START_ADDR + 2;
    EEPROM.get(address, buf);
    if (compareUIDs(tag, buf, size)) return address;
  }
  return -1;
}

bool compareUIDs(uint8_t *in1, uint8_t *in2, uint8_t size) {
  for (uint8_t i = 0; i < size; i++) {
    if (in1[i] != in2[i]) return false;
  }
  return true;
}
