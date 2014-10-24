// ----------------------------------------------------------------------------
// Rotary Encoder Driver with _acceleration
// Supports Click, DoubleClick, Long Click
//
// (c) 2010 karl@pitrich.com
// (c) 2014 karl@pitrich.com
// 
// Timer-based rotary encoder logic by Peter Dannegger
// http://www.mikrocontroller.net/articles/Drehgeber
// ----------------------------------------------------------------------------

#include "ClickEncoder.h"

// ----------------------------------------------------------------------------
// _button configuration (values for 1ms timer service calls)
//
#define ENC_buttonINTERVAL    10  // check _button every x milliseconds, also debouce time
#define ENC_DOUBLECLICKTIME  600  // second click within 600ms
#define ENC_HOLDTIME        1200  // report held _button after 1.2s

// ----------------------------------------------------------------------------
// _acceleration configuration (for 1000Hz calls to ::service())
//
#define ENC_ACCEL_TOP      3072   // max. _acceleration: *12 (val >> 8)
#define ENC_ACCEL_INC        25
#define ENC_ACCEL_DEC         2

// ----------------------------------------------------------------------------

#if ENC_DECODER != ENC_NORMAL
#  ifdef ENC_HALFSTEP
     // decoding _table for hardware with flaky notch (half resolution)
     const int8_t ClickEncoder::_table[16] _attribute_((_progmem_)) = { 
       0, 0, -1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, -1, 0, 0 
     };    
#  else
     // decoding _table for normal hardware
     const int8_t ClickEncoder::_table[16] _attribute_((_progmem_)) = { 
       0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0 
     };    
#  endif
#endif

// ----------------------------------------------------------------------------

ClickEncoder::ClickEncoder(uint8_t A, uint8_t B, uint8_t _buttonPin, uint8_t _stepsPerNotch, bool active)
  : _doubleClickEnabled(true), _accelerationEnabled(true),
    _delta(0), _last(0), _acceleration(0),
    _button(Open), _steps(_stepsPerNotch),
    _pinA(A), _pinB(B), _pinBTN(_buttonPin), _pinsActive(active)
{
  PinMode configType = (_pinsActive == LOW) ? INPUT_PULLUP : INPUT;
  pinMode(_pinA, configType);
  pinMode(_pinB, configType);
  pinMode(_pinBTN, configType);
  
  if (digitalRead(_pinA) == _pinsActive) {
    _last = 3;
  }

  if (digitalRead(_pinB) == _pinsActive) {
    _last ^=1;
  }
}

// ----------------------------------------------------------------------------
// call this every 1 millisecond via timer ISR
//
void ClickEncoder::service(void)
{
  bool moved = false;
  unsigned long now = millis();

  if (_accelerationEnabled) { // decelerate every tick
    _acceleration -= ENC_ACCEL_DEC;
    if (_acceleration & 0x8000) { // handle overflow of MSB is set
      _acceleration = 0;
    }
  }

#if ENC_DECODER == ENC_FLAKY
  _last = (_last << 2) & 0x0F;

  if (digitalRead(_pinA) == _pinsActive) {
    _last |= 2;
  }

  if (digitalRead(_pinB) == _pinsActive) {
    _last |= 1;
  }

  uint8_t tbl = pgm_read_byte(&_table[_last]); 
  if (tbl) {
    _delta += tbl;
    moved = true;
  }
#elif ENC_DECODER == ENC_NORMAL
  int8_t curr = 0;

  if (digitalRead(_pinA) == _pinsActive) {
    curr = 3;
  }

  if (digitalRead(_pinB) == _pinsActive) {
    curr ^= 1;
  }
  
  int8_t diff = _last - curr;

  if (diff & 1) {            // bit 0 = step
    _last = curr;
    _delta += (diff & 2) - 1; // bit 1 = direction (+/-)
    moved = true;    
  }
#else
# error "Error: define ENC_DECODER to ENC_NORMAL or ENC_FLAKY"
#endif

  if (_accelerationEnabled && moved) {
    // increment accelerator if encoder has been moved
    if (_acceleration <= (ENC_ACCEL_TOP - ENC_ACCEL_INC)) {
      _acceleration += ENC_ACCEL_INC;
    }
  }

  // handle _button
  //
#ifndef WITHOUT_button
  static uint16_t keyDownTicks = 0;
  static uint8_t doubleClickTicks = 0;
  static unsigned long _last_buttonCheck = 0;

  if (_pinBTN > 0 // check _button only, if a pin has been provided
      && (now - _last_buttonCheck) >= ENC_buttonINTERVAL) // checking _button is sufficient every 10-30ms
  { 
    _last_buttonCheck = now;
    
    if (digitalRead(_pinBTN) == _pinsActive) { // key is down
      keyDownTicks++;
      if (keyDownTicks > (ENC_HOLDTIME / ENC_buttonINTERVAL)) {
        _button = BUTTON_HELD;
      }
    }

    if (digitalRead(_pinBTN) == !_pinsActive) { // key is now up
      if (keyDownTicks /*> ENC_buttonINTERVAL*/) {
        if (_button == Held) {
          _button = BUTTON_RELEASED;
          doubleClickTicks = 0;
        }
        else {
          #define ENC_SINGLECLICKONLY 1
          if (doubleClickTicks > ENC_SINGLECLICKONLY) {   // prevent trigger in single click mode
            if (doubleClickTicks < (ENC_DOUBLECLICKTIME / ENC_buttonINTERVAL)) {
              _button = BUTTON_DOUBLE_CLICKED;
              doubleClickTicks = 0;
            }
          }
          else {
            doubleClickTicks = (_doubleClickEnabled) ? (ENC_DOUBLECLICKTIME / ENC_buttonINTERVAL) : ENC_SINGLECLICKONLY;
          }
        }
      }

      keyDownTicks = 0;
    }
  
    if (doubleClickTicks > 0) {
      doubleClickTicks--;
      if (--doubleClickTicks == 0) {
        _button = BUTTON_OPEN;
      }
    }
  }
#endif // WITHOUT_button

}

// ----------------------------------------------------------------------------

int16_t ClickEncoder::getValue(void)
{
  int16_t val;
  
  val = _delta;

  if (_steps == 2) _delta = val & 1;
  else if (_steps == 4) _delta = val & 3;
  else _delta = 0; // default to 1 step per notch
  
  if (_steps == 4) val >>= 2;
  if (_steps == 2) val >>= 1;

  int16_t r = 0;
  int16_t accel = ((_accelerationEnabled) ? (_acceleration >> 8) : 0);

  if (val < 0) {
    r -= 1 + accel;
  }
  else if (val > 0) {
    r += 1 + accel;
  }

  return r;
}

// ----------------------------------------------------------------------------

#ifndef WITHOUT_button
ButtonType ClickEncoder::get_button(void)
{
  ButtonType ret = _button;
  if (_button != BUTTON_HELD) {
    _button = BUTTON_OPEN; // reset
  }
  return ret;
}
#endif
