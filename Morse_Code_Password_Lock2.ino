#include <EEPROM.h>
#include <Servo.h>
#include <SoftwareSerial.h>

/* 
 * Morse Password System with 6 7-Segment Displays, Servo & Notifications
 * Author: Varshith Gudala
 * Arduino: Uno R3
 */

// -------------------------
// Pins
// -------------------------
const int dotButton = 2;
const int dashButton = 3;
const int buzzer = 4;
const int resetButton = 12;
const int servoPin = 13;

// 7-segment pins
const int segmentPins[7] = {5, 6, 7, 8, 9, 10, 11};
const int digitPins[6] = {A0, A1, A2, A3, A4, A5};

// Bluetooth module pins
SoftwareSerial BTSerial(7, 6); // RX, TX

// -------------------------
// Variables
// -------------------------
String morseInput = "";
char enteredPassword[6] = {' ', ' ', ' ', ' ', ' ', ' '};
char savedPassword[6];
int charIndex = 0;
unsigned long lastDigitTime = 0;
int currentDigit = 0;
const int digitRefreshInterval = 2;
bool passwordSet = false;
unsigned long lastInputTime = 0;
const int charPause = 2000;

// Wrong attempts tracking
int wrongAttempts = 0;
const int maxAttempts = 3;

// Servo
Servo lockServo;

// -------------------------
// 7-segment patterns
// -------------------------
byte numbers[10][7] = {
  {1,1,1,1,1,1,0}, {0,1,1,0,0,0,0}, {1,1,0,1,1,0,1}, {1,1,1,1,0,0,1},
  {0,1,1,0,0,1,1}, {1,0,1,1,0,1,1}, {1,0,1,1,1,1,1}, {1,1,1,0,0,0,0},
  {1,1,1,1,1,1,1}, {1,1,1,1,0,1,1}
};
byte letters[26][7] = {
  {1,1,1,0,1,1,1}, {0,0,1,1,1,1,1}, {1,0,0,1,1,1,0}, {0,1,1,1,1,0,1},
  {1,0,0,1,1,1,1}, {1,0,0,0,1,1,1}, {1,0,1,1,1,1,0}, {0,1,1,0,1,1,1},
  {0,1,1,0,0,0,0}, {0,1,1,1,0,0,0}, {0,0,1,0,1,1,1}, {0,0,0,1,1,1,0},
  {1,1,1,0,1,1,0}, {0,0,1,0,1,0,1}, {1,1,1,1,1,1,0}, {1,1,0,0,1,1,1},
  {1,1,1,0,0,1,1}, {0,0,0,0,1,0,1}, {1,0,1,1,0,1,1}, {0,0,0,1,1,1,1},
  {0,1,1,1,1,1,0}, {0,1,1,1,1,0,0}, {0,1,1,0,1,0,1}, {0,1,1,0,1,1,1},
  {0,1,1,1,0,1,1}, {1,1,0,1,1,0,1}
};

// -------------------------
// Morse decoding
// -------------------------
char decodeMorse(String code) {
  if(code == ".-") return 'A'; if(code == "-...") return 'B'; if(code == "-.-.") return 'C';
  if(code == "-..") return 'D'; if(code == ".") return 'E'; if(code == "..-.") return 'F';
  if(code == "--.") return 'G'; if(code == "....") return 'H'; if(code == "..") return 'I';
  if(code == ".---") return 'J'; if(code == "-.-") return 'K'; if(code == ".-..") return 'L';
  if(code == "--") return 'M'; if(code == "-.") return 'N'; if(code == "---") return 'O';
  if(code == ".--.") return 'P'; if(code == "--.-") return 'Q'; if(code == ".-.") return 'R';
  if(code == "...") return 'S'; if(code == "-") return 'T'; if(code == "..-") return 'U';
  if(code == "...-") return 'V'; if(code == ".--") return 'W'; if(code == "-..-") return 'X';
  if(code == "-.--") return 'Y'; if(code == "--..") return 'Z';
  if(code == ".----") return '1'; if(code == "..---") return '2'; if(code == "...--") return '3';
  if(code == "....-") return '4'; if(code == ".....") return '5'; if(code == "-....") return '6';
  if(code == "--...") return '7'; if(code == "---..") return '8'; if(code == "----.") return '9';
  return '?';
}

// -------------------------
// EEPROM functions
// -------------------------
void savePassword() { for(int i=0;i<6;i++) EEPROM.write(i, enteredPassword[i]); }
void loadPassword() { for(int i=0;i<6;i++) savedPassword[i] = EEPROM.read(i); }
bool checkPassword() { for(int i=0;i<6;i++) if(savedPassword[i]!=enteredPassword[i]) return false; return true; }

// -------------------------
// Buzzer & Servo
// -------------------------
void wrongPassword() {
  tone(buzzer,1000); delay(1500); noTone(buzzer);
  wrongAttempts++;
  BTSerial.print("Alert: Wrong password attempt! Attempt ");
  BTSerial.println(wrongAttempts);
  if(wrongAttempts >= maxAttempts) {
    BTSerial.println("Alert: Too many wrong attempts!");
    wrongAttempts = 0;
  }
}

void correctPassword() {
  for(int i=0;i<2;i++){ tone(buzzer,1500); delay(200); noTone(buzzer); delay(100); }
  BTSerial.println("Alert: Correct password entered!");
  wrongAttempts = 0;

  // Servo unlock
  lockServo.write(180); // unlock
  delay(1000); 
  lockServo.write(0);   // lock again
}

// -------------------------
// Reset input
// -------------------------
void resetInput() { charIndex=0; morseInput=""; for(int i=0;i<6;i++) enteredPassword[i]=' '; }

// -------------------------
// 7-segment display
// -------------------------
void displayChar(char c, int digit) {
  for(int i=0;i<6;i++) digitalWrite(digitPins[i],LOW);
  byte pattern[7];
  if(c>='0' && c<='9') for(int i=0;i<7;i++) pattern[i]=numbers[c-'0'][i];
  else if(c>='A' && c<='Z') for(int i=0;i<7;i++) pattern[i]=letters[c-'A'][i];
  else for(int i=0;i<7;i++) pattern[i]=0;
  for(int i=0;i<7;i++) digitalWrite(segmentPins[i],pattern[i]);
  digitalWrite(digitPins[digit],HIGH);
  delay(5);
  digitalWrite(digitPins[digit],LOW);
}

void refresh7SegmentDisplay() {
  if(millis()-lastDigitTime>=digitRefreshInterval) {
    lastDigitTime=millis();
    for(int i=0;i<6;i++) digitalWrite(digitPins[i],LOW);
    char c = enteredPassword[currentDigit];
    byte pattern[7];
    if(c>='0'&&c<='9') for(int i=0;i<7;i++) pattern[i]=numbers[c-'0'][i];
    else if(c>='A'&&c<='Z') for(int i=0;i<7;i++) pattern[i]=letters[c-'A'][i];
    else for(int i=0;i<7;i++) pattern[i]=0;
    for(int i=0;i<7;i++) digitalWrite(segmentPins[i],pattern[i]);
    digitalWrite(digitPins[currentDigit],HIGH);
    currentDigit++; if(currentDigit>=6) currentDigit=0;
  }
}

// -------------------------
// Setup
// -------------------------
void setup() {
  pinMode(dotButton, INPUT_PULLUP);
  pinMode(dashButton, INPUT_PULLUP);
  pinMode(buzzer, OUTPUT);
  pinMode(resetButton, INPUT_PULLUP);

  for(int i=0;i<7;i++) pinMode(segmentPins[i], OUTPUT);
  for(int i=0;i<6;i++) pinMode(digitPins[i], OUTPUT);

  Serial.begin(9600);
  BTSerial.begin(9600);

  lockServo.attach(servoPin);
  lockServo.write(0);

  loadPassword();
  if(savedPassword[0]==0){ passwordSet=false; Serial.println("Set new password"); }
  else{ passwordSet=true; Serial.println("Enter password"); }
}

// -------------------------
// Loop
// -------------------------
void loop() {
  // Reset button
  if(digitalRead(resetButton)==LOW){
    Serial.println("Resetting password...");
    for(int i=0;i<6;i++) EEPROM.write(i,0);
    passwordSet=false;
    resetInput();
    Serial.println("Enter new password");
    delay(1000);
  }

  // Morse input
  if(digitalRead(dotButton)==LOW){ morseInput+="."; Serial.print("."); lastInputTime=millis(); delay(250);}
  if(digitalRead(dashButton)==LOW){ morseInput+="-"; Serial.print("-"); lastInputTime=millis(); delay(250);}

  // Decode char
  if(morseInput.length()>0 && millis()-lastInputTime>charPause){
    char decoded = decodeMorse(morseInput);
    enteredPassword[charIndex] = decoded;
    Serial.print(" -> "); Serial.println(decoded);
    charIndex++;
    morseInput="";
    if(charIndex==6){
      if(!passwordSet){ savePassword(); Serial.println("Password Saved"); passwordSet=true; }
      else{
        loadPassword();
        if(checkPassword()){ Serial.println("ACCESS GRANTED"); correctPassword(); }
        else{ Serial.println("ACCESS DENIED"); wrongPassword(); }
      }
      resetInput();
    }
  }

  refresh7SegmentDisplay();
}