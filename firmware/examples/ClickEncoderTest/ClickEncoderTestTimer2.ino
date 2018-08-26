#define WITH_LCD 1

#include <ClickEncoder.h>
#include <TimerOne.h>

#ifdef WITH_LCD
#include <LiquidCrystal.h>

#define LCD_RS       8
#define LCD_RW       9
#define LCD_EN      10
#define LCD_D4       4
#define LCD_D5       5
#define LCD_D6       6
#define LCD_D7       7

#define LCD_CHARS   20
#define LCD_LINES    4

LiquidCrystal lcd(LCD_RS, LCD_RW, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
#endif

ClickEncoder *encoder;
int16_t last, value;

ISR(TIMER2_OVF_vect)        // interrupt service routine 
{
  TCNT2 = timer2_counter;   // preload timer
  encoder->service(); 
}

#ifdef WITH_LCD
void displayAccelerationStatus() {
  lcd.setCursor(0, 1);  
  lcd.print("Acceleration ");
  lcd.print(encoder->getAccelerationEnabled() ? "on " : "off");
}
#endif

void setup() {
  Serial.begin(9600);
  encoder = new ClickEncoder(A1, A0, A2);

#ifdef WITH_LCD
  lcd.begin(LCD_CHARS, LCD_LINES);
  lcd.clear();
  displayAccelerationStatus();
#endif

  noInterrupts();                   // disable all interrupts
  TCCR2B = 0;
  timer2_counter = 6;               // preload timer 1ms
  TCNT2 = timer2_counter;           // preload timer
  TCCR2B |=  (1<<CS21) | (1<<CS20); // 64 prescaler 
  TIMSK2 |= (1 << TOIE2);           // enable timer overflow interrupt
  interrupts();                     // enable all interrupts
  
  last = -1;
}

void loop() {  
  value += encoder->getValue();
  
  if (value != last) {
    last = value;
    Serial.print("Encoder Value: ");
    Serial.println(value);
#ifdef WITH_LCD
    lcd.setCursor(0, 0);
    lcd.print("         ");
    lcd.setCursor(0, 0);
    lcd.print(value);
#endif
  }
  
  ClickEncoder::Button b = encoder->getButton();
  if (b != ClickEncoder::Open) {
    Serial.print("Button: ");
    #define VERBOSECASE(label) case label: Serial.println(#label); break;
    switch (b) {
      VERBOSECASE(ClickEncoder::Pressed);
      VERBOSECASE(ClickEncoder::Held)
      VERBOSECASE(ClickEncoder::Released)
      VERBOSECASE(ClickEncoder::Clicked)
      case ClickEncoder::DoubleClicked:
          Serial.println("ClickEncoder::DoubleClicked");
          encoder->setAccelerationEnabled(!encoder->getAccelerationEnabled());
          Serial.print("  Acceleration is ");
          Serial.println((encoder->getAccelerationEnabled()) ? "enabled" : "disabled");
#ifdef WITH_LCD
          displayAccelerationStatus();
#endif
        break;
    }
  }    
}
