#include "fidelio_display.h"
#include <SPI.h>

word FidelioDisplay::numbers[] = {0x3F00, 0x0600, 0x5B00, 0x4F00, 0x6600, 0x6D00, 0x7D00, 0x0700, 0x7F00, 0x6F00, 0x0000}; //0..9: where : = empty
byte FidelioDisplay::daddr[]   = {0xC6, 0xC4, 0xC2, 0xC0};

// namespace PT6964 {

FidelioDisplay::FidelioDisplay(int dioPin, int clkPin, int stbPin, uint32_t spiClk) {
  _dioPin = dioPin;
  _clkPin = clkPin;
  _stbPin = stbPin;
  _spiClk = spiClk;
  _dots = false;
  _pm = false;
  _alarm = false;
  displaySPI = new SPIClass();
}



void FidelioDisplay::init() 
{
 
 #ifdef ESP32
  displaySPI->begin(_clkPin, -1, _dioPin, _stbPin);
  pinMode(displaySPI->pinSS(), OUTPUT);  
 #elif defined(AVR328)
  displaySPI->begin();
  pinMode(_stbPin, OUTPUT);
#endif

  delay(250);
  sendCommand(CMD_MODE_WRITE_INCREMENT);  // Default write increment addr
  sendCommand(CMD_DISPLAY_4x13);          // Configure as 4Dig 13Seg
  cls();
  setBright(7);
}

void FidelioDisplay::setBright(int level)
{
  sendCommand(CMD_DISPLAY_ON | (level & CMD_DISPLAY_ON_MASK) );
}

void FidelioDisplay::Off()
{
  sendCommand(CMD_DISPLAY_OFF);
}

void FidelioDisplay::On()
{
  sendCommand(CMD_DISPLAY_ON);
}

void FidelioDisplay::cls()
{
  sendCommand(CMD_MODE_WRITE_INCREMENT);
  spiStart();
  sendByte(CMD_SET_ADDR_0);    // 3: Set address to 00 / first
  for(int i = 0; i < 4; i++) { // clear display memory (4 digits)
    sendWord(0x0000);
  }
  spiStop();
}


 void FidelioDisplay::dots(bool value)
 {
  _dots = value;
 }
 
 void FidelioDisplay::pm(bool value)
 {
  _pm = value;
 }
 
 void FidelioDisplay::alarm(bool value)
 {
  _alarm = value;
 }

 void FidelioDisplay::toogleDots()
 {
  _dots = !_dots;
 }
 
 void FidelioDisplay::tooglePm()
 {
  _pm = !_pm;
 }
 
 void FidelioDisplay::toogleAlarm()
 {
  _alarm = !_alarm;
 }

void FidelioDisplay::write(char *buf)
{
  sendCommand(CMD_MODE_WRITE_FIXED_ADDRESS);
  for (int i = 0; i < 4; i++)  {
    // Serial.println(buf[i]);
    if (buf[i] == 0) break;
    word data = numbers[buf[i] - '0'] + daddr[i];
    if (_dots  && i == 1) data |= 0x8000;
    if (_alarm && i == 2) data |= 0x8000;
    if (_pm    && i == 3) data |= 0x8000;
    spiStart();
    sendWord(data);
    spiStop();
  }
}

void FidelioDisplay::at(byte pos, char digit)
{
  if (pos > 4) return;
  sendCommand(CMD_MODE_WRITE_FIXED_ADDRESS);
  // sendByte(daddr[pos]);    // 3: Set address to 00 / first  
  word data = numbers[digit - '0'] + daddr[pos];
  if (_dots  && pos == 1) data |= 0x8000;
  if (_alarm && pos == 2) data |= 0x8000;
  if (_pm    && pos == 3) data |= 0x8000;
  spiStart();
  sendWord(data);
  spiStop();
}

void FidelioDisplay::draw(byte pos, byte what)
{
  if (pos > 4) return;
  sendCommand(CMD_MODE_WRITE_FIXED_ADDRESS);
  // sendByte(daddr[pos]);    // 3: Set address to 00 / first  
  word data = (what << 8) + daddr[pos];
  if (_dots  && pos == 1) data |= 0x8000;
  if (_alarm && pos == 2) data |= 0x8000;
  if (_pm    && pos == 3) data |= 0x8000;
  spiStart();
  sendWord(data);
  spiStop();
}

void FidelioDisplay::sendByte(byte data) {
  displaySPI->transfer(data);
}

void FidelioDisplay::sendWord(word data) {
  displaySPI->transfer(byte(data & 0xFF));
  displaySPI->transfer(byte(data >> 8));  
}

void FidelioDisplay::spiStart() {
  displaySPI->beginTransaction(SPISettings(_spiClk, LSBFIRST, SPI_MODE1));
  digitalWrite(_stbPin, LOW);
}
void FidelioDisplay::spiStop() {
  digitalWrite(_stbPin, HIGH);
  displaySPI->endTransaction();
}

void FidelioDisplay::sendCommand(byte data) {
  spiStart();
  sendByte(data);
  spiStop();
  delayMicroseconds(1);
}
