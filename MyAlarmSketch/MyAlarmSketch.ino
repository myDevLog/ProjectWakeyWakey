#include <StopWatch.h>

//libraries so the mega can use way more interrupts
#include <PinChangeInterrupt.h>
#include <PinChangeInterruptBoards.h>
#include <PinChangeInterruptPins.h>
#include <PinChangeInterruptSettings.h>

//Core graphics library
#include <Elegoo_GFX.h>
//Hardware-specific library
#include <Elegoo_TFTLCD.h>
//library for the touchscreen
#include <TouchScreen.h>

//library for the permanent storage of the time-offsets and the alarm-/ slumbertime
#include <EEPROM.h>

//core library for the real time clock
#include <DS3231.h>

//initiates the DS3231 rtc object
DS3231  rtc(SDA, SCL);
//initiates the time-data structure
Time t;

#define YP A3  // must be an analog pin, use "An" notation!
#define XM A2  // must be an analog pin, use "An" notation!
#define YM 9   // can be a digital pin
#define XP 8   // can be a digital pin


#if defined(__SAM3X8E__)
#undef __FlashStringHelper::F(string_literal)
#define F(string_literal) string_literal
#endif


//define shades
#define BLACK           0x0000
#define GREY            0x738E
#define LIGHTGREY       0xE6FB
#define WHITE           0xFFFF
//define 3 base colors
#define RED             0xF000
#define BLUE            0x00FF
#define YELLOW          0xFF00
//define 3 secondary colors
#define GREEN           0x0F00
#define PURPLE          0xF0FF
#define ORANGE          0xFC00



//Touch For New ILI9341 TP
//Has to be adjusted for every display
#define TS_MINX 124 //124
#define TS_MAXX 912 //912
#define TS_MINY 71  //71
#define TS_MAXY 911 //911



// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// For the one we're using, its 300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

TSPoint p;

#define MINPRESSURE 10
#define MAXPRESSURE 1000



#define LCD_CS A3
#define LCD_CD A2
#define LCD_WR A1
#define LCD_RD A0
// optional
#define LCD_RESET A4



//initialise the object which draws the screen
Elegoo_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);



//defines where the offset-vars will be stored in the EEPROM Array
#define ALARMHOURINDEX 2
#define ALARMMININDEX 3
#define SLUMBERHOURINDEX 4
#define SLUMBERMININDEX 5



//defines the outputs of the seven interrupt pins
#define BRIGHTER_IN 52
#define DARKER_IN 53
#define COLORLIGHT_IN 50
#define LIGHTSWITCH_IN 51
#define SCREENWHILESLUMBER_IN 10
#define PLUSPIN_IN 11
#define MINUSPIN_IN 12

#define SCREENPOWER_OUT 18
#define BRIGHTER_OUT 14
#define DARKER_OUT 15
#define COLORLIGHT_OUT 16
#define LIGHTSWITCH_OUT 17

#define SLUMBERTONE_OUT 13

//defines values which dont need to be stored in variables but are frequently used
#define THRESHOLD 3
#define SIZE7SPACE 5
#define NUM7WIDTH 35
#define SIZE7HEIGHT 49
#define SIZE3HEIGHT 21
#define ANGLE 5

#define DEBOUNCETIME 100



//tells which page is open
int pageOpen = 1;

//stores the previous time stamps so the lcd doesnt have to draw the time new every frame
int prevMin = 0;
int prevHour = 0;

//shows which time-field has been highlighted, only one can be highlighted
bool hourHighlighted = false;
bool minHighlighted = false;

//stores if the Screen is currently powered or not
bool screenState = true;
//stores if the light is currently on or off
bool lightState = false;




//draws a black or a white box around the given dimensions in order to highlight / dehighlight it
void highlight(int posX, int posY, int width, int height, bool toggle) {
  if (toggle) {
    tft.drawRoundRect(posX - THRESHOLD * 2, posY - THRESHOLD * 2, width + THRESHOLD * 4, height + THRESHOLD * 4, ANGLE, BLACK);
  }
  else {
    tft.drawRoundRect(posX - THRESHOLD * 2, posY - THRESHOLD * 2, width + THRESHOLD * 4, height + THRESHOLD * 4, ANGLE, WHITE);
  }
}


//draws a Square Box for the back Button
void drawButtonBox(int posX, int posY, int width, int height) {
  tft.fillRoundRect(posX, posY, width, height, ANGLE, LIGHTGREY);
  tft.drawRoundRect(posX, posY, width, height, ANGLE, BLACK);
}


void printText(int posX, int posY, int textSize, String text) {
  tft.setCursor( posX, posY );
  tft.setTextSize( textSize );
  tft.print( text );
}


void showTime(int x, int y, int time_) {
  //override the last print
  tft.fillRect(x, y, NUM7WIDTH * 2 + SIZE7SPACE + THRESHOLD, SIZE7HEIGHT, WHITE);

  //print the tenth digit
  tft.setTextSize(7);
  tft.setCursor(x , y);
  tft.print(time_ / 10);

  //print the single digit
  tft.setCursor(x + NUM7WIDTH + SIZE7SPACE , y);
  tft.print(time_ % 10);
}





//declares the class homeScreen which shows the time
class homeScreen {
  private:
  //gets added onto the rtc time, so the time from the rtc clock doesnt have to be overwritten every time
  int tempHour = 0, tempMin = 0;

  //0 == nothing highlited, 1 == hour highlited, 2 == minute highlited
  int highlighted = 0;

  //declares the variables for the two buttons
  int alarmWidth = 60, slumberWidth = 120, buttonHeight = 20;

  //store where the lines should start
  const int lowerTextStart = 200, higherTextStart = 80;

  //store the dimensions of the two time labels
  const int hourDim[4] = {tft.height() / 2 - (NUM7WIDTH - 1) / 2 + THRESHOLD - (NUM7WIDTH * 2 + THRESHOLD), higherTextStart, NUM7WIDTH * 2 + SIZE7SPACE, SIZE7HEIGHT};
  const int minDim[4] = {tft.height() / 2 + (NUM7WIDTH - 1) / 2 - THRESHOLD, higherTextStart, NUM7WIDTH * 2 + SIZE7SPACE, SIZE7HEIGHT};
  
  //defines the dimensions of the two buttons
  const int alarmButtonDim[4] = {tft.height() / 4 - alarmWidth / 2 - THRESHOLD, lowerTextStart - THRESHOLD, alarmWidth + THRESHOLD * 2, buttonHeight  + THRESHOLD};
  const int slumberButtonDim[4] = {tft.height() / 2 - THRESHOLD, lowerTextStart - THRESHOLD, slumberWidth + THRESHOLD * 2, buttonHeight + THRESHOLD};
  


  //increases the tempMin by one, starts at 0 if over 59
  private: void increaseMin() {
      if ( (tempMin + 1) > 59) { tempMin = 0; }
      else { tempMin++; }
      
      //if the user is modyfieing the time, display it
      this->updateMin(tempMin);
    }


  //decreases the tempMin by one, starts at 60 if under 0
  private: void decreaseMin() {
      if ( (tempMin - 1) < 0) { tempMin = 59; }
      else { tempMin--; }
      
      //if the user is modyfieing the time, display it
      this->updateMin(tempMin);
    }


  //increases the tempHour by one, starts at 0 if over 23
  private: void increaseHour() {
      if ( (tempHour + 1) > 23) { tempHour = 0; }
      else { tempHour++; }
      
      //if the user is modyfieing the time, display it
      this->updateHour(tempHour);
    }


  //decreases the tempHour by one, starts at 24 if under 0
  private: void decreaseHour() {
      if ( (tempHour - 1) < 0) { tempHour = 23; }
      else { tempHour--; }
      
      //if the user is modyfieing the time, display it
      this->updateHour(tempHour);
    }
  
  
  
  public: bool checkHourBounds(int x, int y){
     if(y > hourDim[0] - THRESHOLD && x > hourDim[1] - THRESHOLD && y < hourDim[0] + hourDim[2] + THRESHOLD*2 && x < hourDim[1] + hourDim[3] + THRESHOLD*2){
      return true;
    } else { return false; }
  }

  
  public: bool checkMinBounds(int x, int y){
     if(y > minDim[0] - THRESHOLD && x > minDim[1] - THRESHOLD && y < minDim[0] + minDim[2] + THRESHOLD*2 && x < minDim[1] + minDim[3] + THRESHOLD*2){
      return true;
    } else { return false; }
  }


  //checks if the touch was in the bounds of the alarm button
  public: bool checkAlarmBounds(int x, int y){
    if(y > alarmButtonDim[0] && x > alarmButtonDim[1] && y < alarmButtonDim[0] + alarmButtonDim[2] && x < alarmButtonDim[1] + alarmButtonDim[3]){
      return true;
    } else { return false; }
  }

  
  public: bool checkSlumberBounds(int x, int y){
    if(y > slumberButtonDim[0] && x > slumberButtonDim[1] && y < slumberButtonDim[0] + slumberButtonDim[2] && x < slumberButtonDim[1] + slumberButtonDim[3]){
      return true;
    } else { return false; }
  }

  
  //draws the homeScreen
 public: void drawScreen(){
    pageOpen = 1;
    highlighted = 0;
    int textOffset = 1;

    //resets the screen so new things can be drawn
    tft.fillScreen(WHITE);

    //draws the hour and the minute
    //gets the data from the rtc
    t = rtc.getTime();
    
    this->updateHour(t.hour);
    
    printText(tft.width() / 2 - (NUM7WIDTH - 1) / 2, higherTextStart, 7, ":");
    
    this->updateMin(t.min);

    //draws the alarm button and the text
    drawButtonBox(alarmButtonDim[0], alarmButtonDim[1], alarmButtonDim[2], alarmButtonDim[3]);
    printText(alarmButtonDim[0] + THRESHOLD + textOffset, alarmButtonDim[1] + THRESHOLD + textOffset, 2, "Alarm");

    //draws the slumber button and the text
    drawButtonBox(slumberButtonDim[0], slumberButtonDim[1], slumberButtonDim[2], slumberButtonDim[3]);
    printText(slumberButtonDim[0] + THRESHOLD + textOffset, slumberButtonDim[1] + THRESHOLD + textOffset, 2, "Schlummern");
  }


  //highlights one or none of the time-drawings with a roundRect around it
  public: void setHighlight(int _highlight) {
    //dehighlights both hour and minute
    if (_highlight == highlighted) {
      highlight(hourDim[0], hourDim[1], hourDim[2], hourDim[3], false);
      highlight(minDim[0], minDim[1], minDim[2], minDim[3], false);
      
      highlighted = 0;
      minHighlighted = false;
      hourHighlighted = false;
      
      t = rtc.getTime();

      //save the time on the rtc
      if(t.hour != tempHour || t.min != tempMin){
        rtc.setTime(tempHour, tempMin, t.sec);  
      }

      //now display the proper time
      t = rtc.getTime();

      //start displaying the real time again
      this->updateMin(t.min);
      this->updateHour(t.hour);
    }
     
    //highlights the hour, dehighlights the minute
    else if (_highlight == 1) {
      highlight(hourDim[0], hourDim[1], hourDim[2], hourDim[3], true);
      highlight(minDim[0], minDim[1], minDim[2], minDim[3], false);
      
      highlighted = 1;
      minHighlighted = false;
      hourHighlighted = true;
      
      //portray the current time which can be manipulated
      t = rtc.getTime();
      tempHour = t.hour;
      tempMin = t.min;

      //for the user to know how much he is manipulating the time
      this->updateHour(tempHour);
      this->updateMin(tempMin);
    }
     
    //highlights the minute, dehighlights the hour
    else if (_highlight == 2) {
      highlight(hourDim[0], hourDim[1], hourDim[2], hourDim[3], false);
      highlight(minDim[0], minDim[1], minDim[2], minDim[3], true);
      
      highlighted = 2;
      minHighlighted = true;
      hourHighlighted = false;
      
      //portray the current time which can be manipulated
      t = rtc.getTime();
      tempHour = t.hour;
      tempMin = t.min;

      //for the user to know how much he is manipulating the time
      this->updateHour(tempHour);
      this->updateMin(tempMin);
    }
     
    delay(DEBOUNCETIME);
  }

  
  //overdraws ONLY the hour, saving processing power
  public: void updateHour(int _hour) { showTime(hourDim[0], hourDim[1], _hour); }

  //overdraws ONLY the minute, saving processing power
  public: void updateMin(int _min) { showTime(minDim[0], minDim[1], _min); }


  public: void increase(){
    if (minHighlighted) {
      this->increaseMin();
    }
    else if (hourHighlighted) {
      this->increaseHour();
    }
  }

  public: void decrease(){
    if (minHighlighted) {
      this->decreaseMin();
    }
    else if (hourHighlighted) {
      this->decreaseHour();
    }
  }
};





//declares the class alarmScreen for setting the alarm and slumber time
class setTimeScreen {
  protected:
  //stores the time for the daily alarm / slumber alarm
  int timerMin, timerHour;

  private:
  //0 == nothing highlited, 1 == hour highlited, 2 == minute highlited
  int highlighted = 0;

  int hourAdress, minAdress;
  
  const int startWidth = 30;

  int higherTextStart = tft.width() * 0.25, timeStart = tft.width() * 0.5, lowerTextStart = tft.width() * 0.75;

  //store the dimensions of the two time labels
  const int hourDim[4] = {startWidth, tft.width() / 2 - (SIZE7HEIGHT / 2), NUM7WIDTH * 2 + SIZE7SPACE, SIZE7HEIGHT};
  const int minDim[4] = {startWidth + NUM7WIDTH * 3 + SIZE7SPACE, tft.width() / 2 - (SIZE7HEIGHT / 2), NUM7WIDTH * 2 + SIZE7SPACE, SIZE7HEIGHT};

  const int backButtonDim[4] = {startWidth / 2, startWidth / 2, (startWidth / 3) * 2, (startWidth / 3) * 2};

  
  public:
  int getTimerMin(){ return timerMin; }
  int getTimerHour(){ return timerHour; }

 
  bool checkHourBounds(int x, int y){
    if(x > hourDim[1] && y > hourDim[0] && x < hourDim[1] + hourDim[3] && y < hourDim[0] + hourDim[2]){
      Serial.println("Hour clicked");
      return true;
    } else{ return false; }
  }


  bool checkMinBounds(int x, int y){
    if(x > minDim[1] && y > minDim[0] && x < minDim[1] + minDim[3] && y < minDim[0] + minDim[2]){
      Serial.println("Min clicked");
      return true;
    } else{ return false; }
  }


  bool checkBackBounds(int x, int y){
    if(x > backButtonDim[0] - THRESHOLD && y > backButtonDim[1] - THRESHOLD && x < backButtonDim[0] + backButtonDim[2] + THRESHOLD*2 && y < backButtonDim[1] + backButtonDim[3] + THRESHOLD*2){
      Serial.println("Back clicked");
      return true;
    } else{ return false; }
  }
 
  
  //draws the Screen where the user can change a time set by this class
  void drawScreen(String upperText, String lowerText) {
    bool plusP = false, minusP = false;
      
    //resets the screen and the highlighted areas
    tft.fillScreen(WHITE);
    highlighted = 0;
  
    //draw the back Button
    drawButtonBox(startWidth / 2, startWidth / 2, (startWidth / 3) * 2, (startWidth / 3) * 2);
    tft.fillTriangle(startWidth / 2 + THRESHOLD, startWidth / 2 + (startWidth / 3), startWidth / 2 + (startWidth / 3) * 2 - THRESHOLD * 2, startWidth / 2 + THRESHOLD, startWidth / 2 + (startWidth / 3) * 2 - THRESHOLD * 2, startWidth / 2 + (startWidth / 3) * 2 - THRESHOLD - 1, BLACK);

    //draws the upper text
    printText(startWidth, higherTextStart - (SIZE3HEIGHT / 2), 3, upperText);

    //draws the time
    showTime(hourDim[0], hourDim[1], timerHour);
    printText(startWidth + NUM7WIDTH * 2 + SIZE7SPACE, tft.height() / 2 - (SIZE7HEIGHT / 2), 7, ":");
    showTime(minDim[0], minDim[1], timerMin);

    //draws the lower text
    printText(startWidth, lowerTextStart - (SIZE3HEIGHT / 2), 3, lowerText);
  }


  //highlights one or none of the time-drawings with a roundRect around it
  void setHighlight(int _highlight) {
    //dehighlights both hour and minute
    if (_highlight == highlighted) {
      highlight(hourDim[0], hourDim[1], hourDim[2], hourDim[3], false);
      highlight(minDim[0], minDim[1], minDim[2], minDim[3], false);
      highlighted = 0;
      minHighlighted = false;
      hourHighlighted = false;
    }
    
    //highlights the hour, dehighlights the minute
    else if (_highlight == 1) {
      highlight(hourDim[0], hourDim[1], hourDim[2], hourDim[3], true);
      highlight(minDim[0], minDim[1], minDim[2], minDim[3], false);
      highlighted = 1;
      minHighlighted = false;
      hourHighlighted = true;
    }
    
    //highlights the minute, dehighlights the hour
    else if (_highlight == 2) {
      highlight(hourDim[0], hourDim[1], hourDim[2], hourDim[3], false);
      highlight(minDim[0], minDim[1], minDim[2], minDim[3], true);
      highlighted = 2;
      minHighlighted = true;
      hourHighlighted = false;
    }
    
    delay(DEBOUNCETIME);
  }

  //initiates the addresses for the time, because this class is inherited by other classes
  void initAdress(int _hourAdress, int _minAdress){
    hourAdress = _hourAdress;
    minAdress = _minAdress;

    timerHour = EEPROM.read(hourAdress);
    timerMin = EEPROM.read(minAdress);
  }

  void increaseMinOffset() {
    if ( (timerMin + 1) > 59) { timerMin = 0; }
    else { timerMin++; }
    this->updateMinute();
  }

  void decreaseMinOffset() {
    if ( (timerMin - 1) < 0) { timerMin = 59; }
    else { timerMin--; }
    this->updateMinute();
  }

  void increaseHourOffset() {
    if ( (timerHour + 1) > 23) { timerHour = 0; }
    else { timerHour++; }
    this->updateHour();
  }

  void decreaseHourOffset() {
    if ( (timerHour - 1) < 0) { timerHour = 23; }
    else { timerHour--; }
    this->updateHour();
  }

  //updates ONLY the hour
  void updateHour() { showTime(hourDim[0], hourDim[1], timerHour); }

  //updates ONLY the minute
  void updateMinute() { showTime(minDim[0], minDim[1], timerMin); }

  
  void increase(){
    if (minHighlighted) {
      this->increaseMinOffset();
    }
    else if (hourHighlighted) {
      this->increaseHourOffset();
    }
  }

  void decrease(){
    if (minHighlighted) {
      this->decreaseMinOffset();
    }
    else if (hourHighlighted) {
      this->decreaseHourOffset();
    }
  }
  
};





//declares a subside class of setTimeScreen in order to manage the alarm feature seperately
class alarmScreen : public setTimeScreen {
  private:
  bool wakeActive = true;

  public: 
  void openScreen(String upperText, String lowerText) {
    pageOpen = 2;
    this->drawScreen(upperText, lowerText);
  }

  void resetAlarm(){
    Serial.println("AlarmScreen: Alarm reset");
    wakeActive = true;
  }
  
  //Wake Me UP!!!, Wake Me UP Inside!!!
  bool wakeMeUp() {
    //checks if the time has come to wake the user
    if(t.hour == EEPROM.read(ALARMHOURINDEX) && t.min == EEPROM.read(ALARMMININDEX) && wakeActive){

      Serial.println("Light activated");
      wakeActive = false;
      
      return true;
    }
    else {
      Serial.println("AlarmScreen: Nothing Actiavated");
      return false;
    }
  }
};





//declares a subside class of setTimeScreen in order to manage the slumber feature seperately
class slumberScreen : public setTimeScreen {
  private:
  bool slumberActive = true;

  public:
  void openScreen(String upperText, String lowerText) {
      pageOpen = 3;
      this->drawScreen(upperText, lowerText);
  }

  void resetSlumber(){ 
    Serial.println("Slumber Screen: Slumber reseted");
    slumberActive = true;
  }

  void slumber(alarmScreen &myAlarmScreen){
    
    //if its time to let the user know that its bed time
    if(t.hour == EEPROM.read(SLUMBERHOURINDEX) && t.min == EEPROM.read(SLUMBERMININDEX) && slumberActive){

      Serial.println("SlumberScreen: myAlarmScreen.resetAlarm executed");
      myAlarmScreen.resetAlarm();
      
      //dont annoy the user by making a sound when hes already sleeping and has turned off the lamp
      if(!lightState){
        Serial.println("Slumber Screen: Light already off");
        slumberActive = false;
        return;
      }
      
      Serial.println("Slumber Screen: Noise activated");
      
      //make a noise to let the user know that its bed time
      tone(SLUMBERTONE_OUT, 1480);
      delay(250);
      noTone(SLUMBERTONE_OUT);

    } else {
      Serial.println("Slumber Screen: Nothing Actiavated");
    }
  }
};





enum p {brighter = 0, darker, colorLight, lightSwitch, screenWhileSlumber, plus, minus};

//stores the state of the pins from the user input
class userInput{
  private:
  //stores the states of each pin in the char
  char pins = 0;
  StopWatch watches[7];

  //stores how many times the ligth_out pin has been switched for getting the state of the light
  int lightSwitches = 1;

  private: void timedPinOutput(StopWatch &refWatch, int pinOut, bool currentPin){
    if(refWatch.isRunning() && refWatch.elapsed() > DEBOUNCETIME){
      refWatch.stop();
      refWatch.reset();
      digitalWrite(pinOut, LOW);
      currentPin = 0;
    } else if(!refWatch.isRunning() && currentPin){
      refWatch.start();
      digitalWrite(pinOut, HIGH);
    }
  }

  private: void lightSwitchPinOutput(StopWatch &refWatch, int pinOut, bool currentPin, bool &changeState){
    //the debouncer stops the user from desyncronising the actual state of the lamp with the state the changeState-Variable is referencing
    //otherwise the reed-relay cannot switch quick enough which results in a timing-nightmare
    static StopWatch debouncer;

    //stop the user from spaming the button
    if(!(debouncer.elapsed() > 0 && debouncer.elapsed() < DEBOUNCETIME * 3)){
      if(refWatch.isRunning() && refWatch.elapsed() > DEBOUNCETIME){
        refWatch.stop();
        refWatch.reset();
        digitalWrite(pinOut, LOW);
        currentPin = 0;

        //start the cooldown on the pin
        debouncer.start();      
      
      } else if(!refWatch.isRunning() && currentPin){
        refWatch.start();
        digitalWrite(pinOut, HIGH);
        changeState = !changeState;
        
        //cooldown on the pin is done
        debouncer.stop();
        debouncer.reset();
      } 
    }
  }


  public: void screenPowerPinOutput(StopWatch &refWatch, int pinOut, bool currentPin, bool &changeState){
    //the debouncer stops the user from desyncronising the actual state of the lamp with the state the changeState-Variable is referencing
    //otherwise the reed-relay cannot switch quick enough which results in a timing-nightmare
    static StopWatch debouncer;

    //stop the user from spaming the button
    if(!(debouncer.elapsed() > 0 && debouncer.elapsed() < DEBOUNCETIME)){
      if(refWatch.isRunning() && refWatch.elapsed() > DEBOUNCETIME){
        refWatch.stop();
        refWatch.reset();
        currentPin = 0;

        //start the cooldown on the pin
        debouncer.start();      
      
      } else if(!refWatch.isRunning() && currentPin){
        refWatch.start();

        changeState = !changeState;

        if(changeState){
          	resetScreen();
        } else {
          digitalWrite(pinOut, LOW);
        }
        
        //cooldown on the pin is done
        debouncer.stop();
        debouncer.reset();
      } 
    }
  }
  
  //functions for the interrupts
  public: void togglePin(enum p inputPin){
    pins = pins ^ (1 << inputPin);
  }

  //gets one specific pinbit and sets it to 0
  public: bool getPin(enum p outputPin){
    //get the state of the specified pinbit
    char temporary = pins & (1 << outputPin);
    bool pinstate = (bool)(temporary >> outputPin);

    //set that pinbit to 0
    pins = pins & (255 ^ (1 << outputPin) );

    return pinstate;
  }


  public: void plusInterrupt(homeScreen &myHomeScreen, alarmScreen &myAlarmScreen, slumberScreen &mySlumberScreen) {
    //increment if the homeScreen is Open
    if (pageOpen == 1) {
      myHomeScreen.increase();
    }
    //increment if the alarmScreen is Open
    else if (pageOpen == 2) {
      myAlarmScreen.increase();
    }
    //increment if the slumberScreen is open
    else if (pageOpen == 3) {
      mySlumberScreen.increase();
    }
  }

  public: void minusInterrupt(homeScreen &myHomeScreen, alarmScreen &myAlarmScreen, slumberScreen &mySlumberScreen) {
    //decrement if the homeScreen is Open
    if (pageOpen == 1) {
      myHomeScreen.decrease();
    }
    //decrement if the alarmScreen is Open
    else if (pageOpen == 2) {
      myAlarmScreen.decrease();
    }
    //decrement if the slumberScreen is open
    else if (pageOpen == 3) {
      mySlumberScreen.decrease();
    }
  }

  
  public: void processUserInput(homeScreen &myHomeScreen, alarmScreen &myAlarmScreen, slumberScreen &mySlumberScreen){
    
    //temporary storage, because getPin() sets the respective bit to 0
    bool tempBrighter = this->getPin(brighter), tempDarker = this->getPin(darker);
    
    //cancel opposite user input
    if(tempBrighter && tempDarker){
      tempBrighter = 0;
      tempDarker = 0;
    }

    this->timedPinOutput(watches[0], BRIGHTER_OUT, tempBrighter);
    this->timedPinOutput(watches[1], DARKER_OUT, tempDarker);
    this->timedPinOutput(watches[2], COLORLIGHT_OUT, this->getPin(colorLight));
    this->lightSwitchPinOutput(watches[3], LIGHTSWITCH_OUT, this->getPin(lightSwitch), lightState);

    this->screenPowerPinOutput(watches[4], SCREENPOWER_OUT, this->getPin(screenWhileSlumber), screenState);


    //temporary storage, because getPin() sets the respective bit to 0
    bool tempPlus = this->getPin(plus), tempMinus = this->getPin(minus);
    
    //cancel opposite user input
    if(tempPlus && tempMinus){
      tempPlus = 0;
      tempMinus = 0;
    }
    
    if(tempPlus){
      this->plusInterrupt(myHomeScreen, myAlarmScreen, mySlumberScreen);
      tempPlus = 0;
      delay(DEBOUNCETIME);
    }
    
    if(tempMinus){
      this->minusInterrupt(myHomeScreen, myAlarmScreen, mySlumberScreen);
      tempMinus = 0;
      delay(DEBOUNCETIME);
    }
  }
};





//creates the objects which draw the different screens
homeScreen myHomeScreen;
alarmScreen myAlarmScreen;
slumberScreen mySlumberScreen;
userInput myInput;





//starts the screen after it has been turned off
void resetScreen(){
  digitalWrite(SCREENPOWER_OUT, HIGH);

  //resets the screen
  tft.reset();
  tft.begin(0x9341);
  
  //initialises the basic variables
  tft.setRotation(3);
  tft.setTextColor(RED);
  
  //draws the home screen 
  myHomeScreen.drawScreen();
}





void brighterPressed(){ myInput.togglePin(brighter); }
void darkerPressed(){ myInput.togglePin(darker); }
void colorLightPressed(){ myInput.togglePin(colorLight); }
void lightSwitchPressed(){ myInput.togglePin(lightSwitch); }
void screenWhileSlumberPressed(){ myInput.togglePin(screenWhileSlumber); }
void plusPressed(){ myInput.togglePin(plus); }
void minusPressed(){ myInput.togglePin(minus); }





void setButtonPins(){
  //sets the pins for the interrupts as an input
  pinMode(BRIGHTER_IN, INPUT);
  pinMode(DARKER_IN, INPUT);
  pinMode(COLORLIGHT_IN, INPUT);
  pinMode(LIGHTSWITCH_IN, INPUT);
  pinMode(SCREENWHILESLUMBER_IN, INPUT);
  pinMode(PLUSPIN_IN, INPUT);
  pinMode(MINUSPIN_IN, INPUT);


  //sets the pins for the reaction to the user input
  pinMode(SCREENPOWER_OUT, OUTPUT);
  pinMode(BRIGHTER_OUT, OUTPUT);
  pinMode(DARKER_OUT, OUTPUT);
  pinMode(COLORLIGHT_OUT, OUTPUT);
  pinMode(LIGHTSWITCH_OUT, OUTPUT);

  
  //sets the pin where a button can be pressed to a interrupt
  attachPCINT(digitalPinToPCINT(BRIGHTER_IN), brighterPressed, FALLING);
  attachPCINT(digitalPinToPCINT(DARKER_IN), darkerPressed, FALLING);
  attachPCINT(digitalPinToPCINT(COLORLIGHT_IN), colorLightPressed, FALLING);
  attachPCINT(digitalPinToPCINT(LIGHTSWITCH_IN), lightSwitchPressed, FALLING);
  attachPCINT(digitalPinToPCINT(SCREENWHILESLUMBER_IN), screenWhileSlumberPressed, FALLING);
  attachPCINT(digitalPinToPCINT(PLUSPIN_IN), plusPressed, FALLING);
  attachPCINT(digitalPinToPCINT(MINUSPIN_IN), minusPressed, FALLING);
}





void setup() {
  //starts the serial monitor for debuging
  Serial.begin(9600);

  //sets the interrupts for the user input
  setButtonPins();

  // Initializes the rtc object and gets the data from it
  rtc.begin();
  t = rtc.getTime();

  //sets the updating time to the current time
  prevMin = t.min;
  prevHour = t.hour;

  //initiates the adresses for the location of the index in the EEPROM for the respective Screen
  myAlarmScreen.initAdress(ALARMHOURINDEX, ALARMMININDEX);
  mySlumberScreen.initAdress(SLUMBERHOURINDEX, SLUMBERMININDEX);

  resetScreen();

  pinMode(SLUMBERTONE_OUT, OUTPUT);
  digitalWrite(SLUMBERTONE_OUT, LOW);

  pinMode(13, OUTPUT);
}





void loop() { 
  //process the pressed buttons
  myInput.processUserInput(myHomeScreen, myAlarmScreen, mySlumberScreen);

  //gets the data from the rtc
  t = rtc.getTime();

  //only check the alarms every new minute
  if(prevMin != t.min){
      
      //wake the user and prime the slumberScreen
      if(myAlarmScreen.wakeMeUp()){
        Serial.println("alarmScreen returned true, resetting slumberScreen");
        mySlumberScreen.resetSlumber();
        
        //dont turn on the LED if the user is awake already
        if(!lightState){
          Serial.println("AlarmScreen: Light is already on");
          myInput.togglePin(lightSwitch);
        }
      }

      //tell the user its time to go to Bed and prime the alarmScreen
      mySlumberScreen.slumber(myAlarmScreen);


      //update the minute, but only if the screen is turned off so the update function works
      if(!screenState){
        prevMin = t.min;
      } 
    }
  
  //only check for touches or update the screen if it is turned on
  if(screenState){
    
    //gets the coordinates of the touch
    p = ts.getPoint();
    pinMode(XM, OUTPUT);
    pinMode(YP, OUTPUT);
  
    //updates the minute if the minute is different from the last drawn minute
    if (prevMin != t.min && pageOpen == 1) {
      myHomeScreen.updateMin(t.min);
      prevMin = t.min;
    }
  
    //updates the hour if the hour is different form the last drawn hour
    if (prevHour != t.hour && pageOpen == 1) {
      myHomeScreen.updateHour(t.hour);
      prevHour = t.hour;
    }
    
    
    
    //checks if the user touched the lcd
    if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
  
      //converts the coordinates of the touch into usable values
      p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.height() );
      p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.width() );
  
      Serial.print("X = "); Serial.print(p.x);
      Serial.print("\tY = "); Serial.print(p.y);
      Serial.print("\tPressure = "); Serial.println(p.z);
  
      //check the clickables if the homeScreen is open
      if (pageOpen == 1) {
        
        //checks if the alarm button has been pressed
        if (myHomeScreen.checkAlarmBounds(p.x, p.y)) {
          myAlarmScreen.openScreen("Alarm auf", "gesetzt");
        }
        //checks if the slumber button has been pressed
        else if (myHomeScreen.checkSlumberBounds(p.x, p.y)) {
          mySlumberScreen.openScreen("Schlummern auf", "gesetzt");
        }
        //if the user has tapped on the hour
        else if (myHomeScreen.checkHourBounds(p.x, p.y)) {
          myHomeScreen.setHighlight(1);
        }
        //if the user has tapped on the minute
        else if (myHomeScreen.checkMinBounds(p.x, p.y)) {
          myHomeScreen.setHighlight(2);
        }
      }
      //check the clickables if the alarmScreen is open
      else if (pageOpen == 2) {
        
        //checks if the backbutton in the alarmScreen has been pressed
        if (myAlarmScreen.checkBackBounds(p.x, p.y)) {
          prevHour = t.hour;
          prevMin = t.min;
          
          EEPROM.update(ALARMHOURINDEX, myAlarmScreen.getTimerHour());
          EEPROM.update(ALARMMININDEX, myAlarmScreen.getTimerMin());
          myHomeScreen.drawScreen();
        }
        //if the user has tapped on the hour
        else if (myAlarmScreen.checkHourBounds(p.x, p.y)) {
          myAlarmScreen.setHighlight(1);
        }
        //if the user has tapped on the minute
        else if (myAlarmScreen.checkMinBounds(p.x, p.y)) {
          myAlarmScreen.setHighlight(2);
        }
      }
      //check the clickables if the slumberScreen is open
      else if (pageOpen == 3) {
        //checks if the backbutton in the slumberScreen has been pressed
        if (mySlumberScreen.checkBackBounds(p.x, p.y)) {
          prevHour = t.hour;
          prevMin = t.min;

          EEPROM.update(SLUMBERHOURINDEX, mySlumberScreen.getTimerHour());
          EEPROM.update(SLUMBERMININDEX, mySlumberScreen.getTimerMin());
          myHomeScreen.drawScreen();
        }
        //if the user has tapped on the hour
        else if (mySlumberScreen.checkHourBounds(p.x, p.y)) {
          myAlarmScreen.setHighlight(1);
        }
        //if the user has tapped on the minute
        else if (mySlumberScreen.checkMinBounds(p.x, p.y)) {
          myAlarmScreen.setHighlight(2);
        }
      }
    }
  }
}
