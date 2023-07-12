// For display
#include <LiquidCrystal_I2C.h>
// For Rotary Encoder
#include "AiEsp32RotaryEncoder.h"
#include "Arduino.h"

// Rotary encoder defines
#define ROTARY_ENCODER_A_PIN 19
#define ROTARY_ENCODER_B_PIN 16
#define ROTARY_ENCODER_BUTTON_PIN 18
#define ROTARY_ENCODER_VCC_PIN -1
#define ROTARY_ENCODER_STEPS 4

// Buzzer pin
#define BUZZER_PIN 17
#define CHANNEL 0

LiquidCrystal_I2C lcd(0x27, 16, 2);  // set the LCD address to 0x3F for a 16 chars and 2 line display
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);

/* For Rotary encoder */
void rotary_onButtonClick() {
  static unsigned long lastTimePressed = 0;  // Soft debouncing
  if (millis() - lastTimePressed < 500) {
    return;
  }
  lastTimePressed = millis();
  Serial.print("button pressed ");
  Serial.print(millis());
  Serial.println(" milliseconds after restart");
}

void rotary_loop() {
  //dont print anything unless value changed
  if (rotaryEncoder.encoderChanged()) {
    Serial.print("Value: ");
    Serial.println(rotaryEncoder.readEncoder());  
  }
  if (rotaryEncoder.isEncoderButtonClicked()) {
    rotary_onButtonClick();
  }
}

bool get_rotary_button_input() {
  if (rotaryEncoder.isEncoderButtonClicked()) {
    rotary_onButtonClick();
    return true;
  }
  return false;
}

int get_rotary_input() {
  //dont print anything unless value changed
  Serial.print("Rotary input: ");
  int value = rotaryEncoder.readEncoder();
  int hours = int(value) / 60;
  int minutes = int(value) % 60;
  Serial.print(value);
  Serial.print(" hours: ");
  Serial.print(hours);
  Serial.print(" minutes: ");
  Serial.println(minutes);

  return value;
}

void IRAM_ATTR readEncoderISR() {
  rotaryEncoder.readEncoder_ISR();
}

void setup() {
  /* For display */
  lcd.init();
  lcd.clear();
  lcd.backlight();  // Make sure backlight is on

  // Print a message on both lines of the LCD.
  // lcd.setCursor(2, 0);  //Set cursor to character 2 on line 0
  // lcd.print("Hello world!");

  // lcd.setCursor(2, 1);  //Move cursor to character 2 on line 1
  // lcd.print("LCD Tutorial");

  /* For rotary encoder */
  Serial.begin(115200);

  //we must initialize rotary encoder
  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);
  //set boundaries and if values should cycle or not
  bool circleValues = false;
  // Max value will be 1439 because max time setting will be 23 hours and 59 minutes,
  // which is 1439 minutes
  rotaryEncoder.setBoundaries(0, 1439, circleValues);  //minValue, maxValue, circleValues true|false (when max go to min and vice versa)

  /*Rotary acceleration introduced 25.2.2021.
  * in case range to select is huge, for example - select a value between 0 and 1000 and we want 785
  * without accelerateion you need long time to get to that number
  * Using acceleration, faster you turn, faster will the value raise.
  * For fine tuning slow down.
  */
  //rotaryEncoder.disableAcceleration(); //acceleration is now enabled by default - disable if you dont need it
  rotaryEncoder.setAcceleration(250);  //or set the value - larger number = more accelearation; 0 or 1 means disabled acceleration

  // Set up buzzer
  ledcSetup(CHANNEL, 5000, 8);         // Setup LEDC channel with a frequency of 5000Hz and 8-bit resolution
  ledcAttachPin(BUZZER_PIN, CHANNEL);  // Attach the buzzer pin to the LEDC channel
}

void display_time(int hours, int minutes) {
  // Display hours
  lcd.setCursor(0, 1);  // beginning of row 2
  lcd.print("  ");
  lcd.setCursor(0, 1);
  if (hours == 0) {
    lcd.print("00");
  }
  else if (hours < 10) {
    lcd.print("0");
    lcd.print(hours);     // print num hours
  }
  else {
    lcd.print(hours);
  }
  lcd.print(" hours");

  // Display minutes
  lcd.setCursor(9, 1);
  lcd.print("  ");
  lcd.setCursor(9, 1);
  if (minutes == 0) {
    lcd.print("00");
  }
  else if (minutes < 10) {
    lcd.print("0");
    lcd.print(minutes);
  }
  else {
    lcd.print(minutes);
  }
  lcd.print(" mins");
  
}

// Declare input value from  user
int input = 0;
void loop() {
  /* Step 1: Display a prompt asking for time */
  lcd.setCursor(0, 0);
  lcd.print("Timer duration:");
  if (rotaryEncoder.encoderChanged()) {
    input = get_rotary_input();
  }

  int hours = input / 60;
  int minutes = input % 60;

  // Display time that they choose
  display_time(hours, minutes);
  
  // Proceed only if time is on the clock
  if (hours + minutes > 0) {
    /* Start timer if button was pressed */
    if (get_rotary_button_input()) {
      int seconds = 0;  // even though user won't see seconds, we need to keep
      // track of them as they add up to minutes

      // Start timer
      // Clear LCD for new text
      lcd.clear();
      bool time_expired = false;
      // Count down until the timer has expired, but pause when they press
      // the pause button
      while (!time_expired) {
        // Display remaining time
        lcd.print("Time remaining:");
        display_time(hours, minutes); // display time remaining
        bool paused = get_rotary_button_input();
        if (paused) {
          lcd.clear();
          lcd.print("  Timer paused");
          display_time(hours, minutes);
          while (paused) {
            paused = get_rotary_button_input(); // wait til unpaused
          }
          // Display time remaining again after timer has been un-paused
          lcd.clear();
          lcd.print("Time remaining:");
          display_time(hours, minutes);
        }
        // Otherwise, countdown if there is time remaining on the clock
        else if (hours + minutes > 0){
          // decrement the timer by one second
          delay(950);
          seconds ++;
          // While counting down, check if hours or minutes need to be 
          // decremented
          if (seconds >= 60) {
            minutes --;
            seconds = 0;
            if (minutes <= 0 && hours >= 1) {
              hours --;
              minutes = 59;
            }
          }
        }
        // If no time is remaining, then sound the buzzer and wait for button 
        // press to stop the buzzer
        else {
          time_expired = true;
          bool alarm = true;
          while (alarm){
            ledcWriteTone(CHANNEL, 600);  // Generate a 1000 Hz tone
            delay(1000);                  // Wait for 1 second
            alarm = !get_rotary_button_input();
            ledcWriteTone(CHANNEL, 0);    // Stop the tone
            delay(1000);                  // Wait for 1 second
            alarm = !get_rotary_button_input();  // We check twice because they
            // might press the button at any time
          }
          lcd.clear();  // clear the display in preparation for new screen
        }
      }
    }
  }
}
