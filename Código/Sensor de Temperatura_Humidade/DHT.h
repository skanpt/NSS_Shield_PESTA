#ifndef DHT_H
#define DHT_H

#if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif

class DHT {
  public:
   DHT(uint8_t pin);
   void begin(uint8_t usec=55);
   float readTemperature(bool S=false, bool force=false);
   float readHumidity(bool force=false);
   bool read(bool force=false);

 private:
  uint8_t dados[5];
  uint8_t _pin;
  uint32_t _lastreadtime, _maxcycles;
  bool _lastresult;
  uint8_t pullTime;

  uint32_t count_pulse_time(bool level);

};

class InterruptLock {
  public:
   InterruptLock() {
#if !defined(ARDUINO_ARCH_NRF52)  
    noInterrupts();
#endif
   }
   ~InterruptLock() {
#if !defined(ARDUINO_ARCH_NRF52)  
    interrupts();
#endif
   }
};

#endif
