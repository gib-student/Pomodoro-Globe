// For display
#include <LiquidCrystal_I2C.h>
// For Rotary Encoder
#include "AiEsp32RotaryEncoder.h"
#include "Arduino.h"
#include <arduino-timer.h>

// Rotary encoder defines
#define ROTARY_ENCODER_A_PIN 19
#define ROTARY_ENCODER_B_PIN 16
#define ROTARY_ENCODER_BUTTON_PIN 18
#define ROTARY_ENCODER_VCC_PIN -1
#define ROTARY_ENCODER_STEPS 4

// System states
#define WAITING 0
#define COUNTING 1
#define PAUSED 2
#define BUZZING 3

// Buzzer pin
#define BUZZER_PIN 17
#define CHANNEL 0

// Globals
auto timer = timer_create_default();

// System states
int state           = WAITING;
int hours           = 0;
int minutes         = 0;
int seconds         = 60;
int rotary_input    = 0;
int time_changed    = true;
bool new_text       = true;
bool button_pressed = false;
static unsigned long lastTimePressed = 0;  // for debouncing

LiquidCrystal_I2C lcd(0x27, 16, 2);  // set the LCD address to 0x3F for a 16 chars and 2 line display
AiEsp32RotaryEncoder rotaryEncoder = AiEsp32RotaryEncoder(ROTARY_ENCODER_A_PIN, ROTARY_ENCODER_B_PIN, ROTARY_ENCODER_BUTTON_PIN, ROTARY_ENCODER_VCC_PIN, ROTARY_ENCODER_STEPS);

void IRAM_ATTR readEncoderISR() {
  rotaryEncoder.readEncoder_ISR();
}

void alarm() {
  ledcWriteTone(CHANNEL, 600);  // Generate a 1000 Hz tone
  delay(1000);                  // Wait for 1 second
  ledcWriteTone(CHANNEL, 0);    // Stop the tone
  delay(1000);                  // Wait for 1 second
}

void poll_rotary_input() {
  if (rotaryEncoder.encoderChanged()) {
    time_changed = true;
    rotary_input = rotaryEncoder.readEncoder() ;
  }
}

void set_hours_minutes() {
  hours = rotary_input / 60;
  minutes = rotary_input % 60;
}

void display_waiting_message() {
  if (new_text) {
    lcd.setCursor(0, 0);
    lcd.print("Timer duration: ");
    new_text = false;
  }
  display_time();
}

void display_counting_message() {
  if (new_text) {
    lcd.setCursor(0, 0);
    lcd.print("Time remaining: ");
    new_text = false;
  }
  display_time();
}

void display_paused_message() {
  if (new_text) {
    lcd.setCursor(0, 0);
    lcd.print("Paused          ");
    new_text = false;
  }
  display_time();
}

void display_time() {
  // If the time hasn't changed, then just return
  if (!time_changed)  {
    return;
  }
  else {
    // Display hours
    lcd.setCursor(0, 1);
    if (hours == 0) {
      lcd.print("00");
      lcd.print(" hour  ");
    } else if (hours == 1) {
      lcd.print("0");
      lcd.print(hours);
      lcd.print(" hour  ");
    } else if (hours < 10) {
      lcd.print("0");
      lcd.print(hours);  // print num hours
      lcd.print(" hours ");
    } else {
      lcd.print(hours);
      lcd.print(" hours ");
    }

    // Display minutes
    if (minutes == 0) {
      lcd.print("00");
      lcd.print(" mins");
    } else if (minutes == 1) {
      lcd.print("0");
      lcd.print(minutes);
      lcd.print(" min ");
    }
    else if (minutes < 10) {
      lcd.print("0");
      lcd.print(minutes);
      lcd.print(" mins");
    } else {
      lcd.print(minutes);
      lcd.print(" mins");
    }
    // Once we've updated the time, reset the time_changed variable
    time_changed = false;
  }
}

void rotary_encoder_button_ISR() {
  if (millis() - lastTimePressed < 500) {
    return;
  }
  lastTimePressed = millis();
  button_pressed = true;
}

bool every_minute(void *) {
  time_changed = true;
  // Decrement minutes
  minutes --;
  // If there is still an hour on the clock, then decrement hours and reset
  // minute
  if (minutes <= -1 && hours > 0) {
    hours --;
    minutes = 59;
  }
  return true;
}

bool end_timer(void *) {
  state = BUZZING;
  timer.cancel();
  return false;
}

void handle_button_push() {
  if (button_pressed) {
    // If the button has been pressed, and we are in the WAITING state, and the 
    // user has entered positive time, then change to the COUNTING state
    if (state == WAITING && hours + minutes > 0) {
      set_hours_minutes();  // Determine the parameters of the timer
      unsigned long millis = (hours * 60 * 60 * 1000) + (minutes * 60 * 1000);
      timer.in(millis, end_timer);

      state = COUNTING; // Change system state
      new_text = true;  // new text will need to be displayed when we change
                        // states
    } else if (state == COUNTING) {
      state = PAUSED; // If we are already counting and the button has been 
      new_text = true;// pressed, change to the PAUSED state
    } else if (state == PAUSED) {
      state = COUNTING;// If we are paused and the button is pressed, change to 
      new_text = true; // COUNTING state
    } else if (state == BUZZING) {
      state = WAITING; // If buzzing and the button is pressed, change 
      new_text = true; // back to WAITING
    }
    // There is one small case which one of these cases address, and that is
    // the case that they press the button and there is no time on the clock.
    // In that case, we do nothing, and simply wait for them to put positive
    // time on the clock.
    button_pressed = false; // After we have handled all the cases,
                            // lower the flag.
  }

}

void handle_system_state() {
  // Check system state and do what needs to be done for that state.
  if (state == WAITING) {
    // In this state we want to just wait for the user to press the button
    // for the first time after they've entered a valid input
    poll_rotary_input();
    // and update the screen according to the time
    set_hours_minutes();
    display_waiting_message();
  } else if (state == COUNTING) {
    // If we are in the counting state, then we simply want to advance the timer
    timer.tick();
    // and update the screen
    display_counting_message();
  } else if (state == PAUSED) {
    // If the timer has been paused, then we will not advance the timer, and
    // instead we do nothing but show a "Paused" message
    display_paused_message();
  } else if (state == BUZZING) {
    // If the timer is now buzzing, we will just buzz and wait for them to 
    // press the button, which will reset the timer.
    alarm();  // buzz the alarm
  }
}

void setup() {
  /* For display */
  lcd.init();
  lcd.clear();
  lcd.backlight();  // Make sure backlight is on

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

  //rotaryEncoder.disableAcceleration(); //acceleration is now enabled by default - disable if you dont need it
  rotaryEncoder.setAcceleration(250);  //or set the value - larger number = more accelearation; 0 or 1 means disabled acceleration

  // Set up buzzer
  ledcSetup(CHANNEL, 5000, 8);         // Setup LEDC channel with a frequency of 5000Hz and 8-bit resolution
  ledcAttachPin(BUZZER_PIN, CHANNEL);  // Attach the buzzer pin to the LEDC channel

  // Enable interrupt for rotary encoder button
  attachInterrupt(ROTARY_ENCODER_BUTTON_PIN, rotary_encoder_button_ISR, RISING);

  // Setup timer
  timer.every(60000, every_minute); // update the timer every minute

  // Display opening message
  display_waiting_message();
}

void loop() {
  handle_button_push();
  handle_system_state();
}
