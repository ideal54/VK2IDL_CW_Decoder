/***********************************************************************
   WB7FHC's Simple Morse Code Decoder v. 2.0 [03/15/15]
   (c) 2015, Budd Churchward - WB7FHC
   This is an Open Source Project
   http://opensource.org/licenses/MIT
   
   Search YouTube for 'WB7FHC' to see several videos of this project
   as it was developed.
   
   MIT license, all text above must be included in any redistribution
   **********************************************************************
   *
   ############################################################################
   MODIFIED by VK2IDL on April 2020 with the following changes:
   - Removed the speaker and sidetone adjustments as I didnt need them.
   - ALtered button functions:
      - Right Button now only activates the auto-sweep of the tone filter.
      - Left button now only adjusts the QRM filter setting.
      - Added additional switch to enable live activation of the Farnsworth setting.
   - Fixed bug that could cause the system to lock up after auto-sweep
     due to excessive 'startDownTime' and 'startUpTime' values. These variables
     are now reset via the resetDefaults() function whenever an auto-sweep has occurred.
   - Altered start up and operational display to continuously show important 
     variable settings on the top line during operation. Text scrolling is now 
     limited to the bottom 3 lines.
   - Faster tone decoder sweep by setting smaller range limits.

   
    ############################################################################
   
   The program will automatically adjust to the speed of code that
   is being received. The first few characters may come out wrong while it
   homes in on the speed. The software tracks the speed of the sender's dahs to make
   its adjustments. The more dahs you receive at the beginning
   the sooner it locks into solid copy.

   If you are not seeing solid copy, press the SWEEP button which will retune 
   the tone decoder and clear old variables from the software.
   
   This project is built around the 20x4 LCD display. The sketch includes
   funtions for word wrap and scrolling. If a word extends beyond the 20
   column line, it will drop down to the next line. When the bottom line
   is filled, all lines will scroll up one row and new text will continue
   to appear at the bottom.
   
   This version makes use of the 4 digit parallel method of driving the
   display.
  
*********************************************************************/

//////////////////////////////////////////////////
#include <EEPROM.h>
int addr = 0;

#include <LiquidCrystal_I2C.h>  // include LCD library
#include <Wire.h>               // Include the Wire library for Serial access and I2C control

#include <SPI.h>                // Include I2C bus library

#define DEBUG 1                 // Remove '//' to turn monitor debugging on

#define myCall "VK2IDL"         // Callsign of User
#define Vers "2.7"              // Version number

#define LCDCOLS 20              // Using 20x4 LCD
#define LCDROWS 4

#define MAXSWEEP 240           // Tone decoder chip cycles around if we go too high with the digital pot
#define MINSWEEP 150           // Tone decoder starts at this setting
#define SWEEPCOUNTBUMP 10      // How much more we will delay checking for tone match after each sweep

#define farnsBUTTON 5            // Pin for Farnsworth Switch
#define sweepBUTTON 6          // Pin for 567 Decoder tuning Switch
#define filterBUTTON 7         // Pin for QRN Filter Switch
#define signalPIN 8            // 567 Tone Decoder output connects here
                               
int signalPinState = 1;        // Stores the state of the signal pin

#define slaveSelectPIN 10      // Connected to Pin 1 CS (Chip Select) of MPC41010

int myMax = 0;                 // Used to capture highest value that decodes tone
int myMin = 255;               // Used to capture lowest value that decodes tone

int ToneSet = 201;             // A value bumped up or down by the Rotary Encoder

int oldToneSet = ToneSet;      // So we can tell when the value has changed
int pitchDir = 1;              // Determines direction of Rotary Encoder to adjust the LM567 filter frequency

// Define LCD Pinout Configuration
const int  en = 2, 
           rw = 1, 
           rs = 0, 
           d4 = 4, 
           d5 = 5, 
           d6 = 6, 
           d7 = 7, 
           bl = 3;

const int i2c_addr = 0x27;     // Define LCD I2C bus address 

// Define LCD object from library usig Pinout Configuration variables
LiquidCrystal_I2C lcd(i2c_addr, en, rw, rs, d4, d5, d6, d7, bl, POSITIVE);  

// Variables for Rotary Encoder - No longer included in the scematic
const byte inputCLK = 3;     // Encoder pin A goes here so we can use interrupt
const byte inputDT = 4;      // Encoder pin B
const byte clickPin = 2;     // Push button pin here, thought I might use interrupt but now I'm not
boolean myClick = false;     // Goes true when we press the knob in
int currentStateCLK;         // Holds the current state of the Rotary Encoder CLK pin 
int previousStateCLK;        // Holds the previous state of the Rotary Encoder CLK pin
boolean toneFlag = false;    // Flag determines if the ToneSet has been changed

/*  The variable 'noiseFilter' is a value used to filter out noise spikes coming from 
 *  the decoder chip. The value of 'noiseFilter' will be the number of milliseconds 
 *  we wait after reading pin 8 before reading it again to see if we have a valid 
 *  key down or key up.
*/
int noiseFilter = 4;         // Default value

int oldNoiseFilter = noiseFilter;   // Used to see if the value has changed

int sweepCount = 0;          // Used to slow down the auto-tune sweep each time 
                             // it cycles without success

/////////////////////////////////////////////////////////////////////////////////////////
// The following variables store values collected and calculated from millis()
// For various timing functions

long pitchTimer = 0;     // Keep track of how long since right button was pressed 
                         // so we can reverse direction of pitch change
long downTime = 0;       // How long the tone was on in milliseconds
long upTime = 0;         // How long the tone was off in milliseconds

long startDownTime = 0;  // Arduino's internal timer when tone first comes on
long startUpTime = 0;    // Arduino's internal timer when tone first goes off
long lastChange = 0;     // Keep track of when we make changes 

long lastDahTime = 0;    // Length of last dah in milliseconds
long lastDitTime = 0;    // Length of last dit in milliseconds

// The following values will auto adjust to the incoming Morse speed
long averageDah = 100;   // A dah should be 3 times as long as a dit
long fullWait = 6000;    // The time between letters
long waitWait = 6000;    // The time between dits and dahs
long newWord = 0;        // The time between words
long dit = 10;           // Initial dit value is defined as 10 milliseconds
int Farns = 2;           // Used to calculate the Farnsworth space between words
int FarnsTime = 0;       // Toggle value for Farnsworth setting
int oldFarns = 2;        // Holds previous value of Farns
                          
////////////////////////////////////////////////////////////////////////////////////////////
// These variables handle line scrolling and word wrap on the LCD panel                   //
int LCDline = 3;            // Keeps track of which line we're printing on                //
int lineEnd = LCDCOLS + 1;  // One more than number of characters across display          //
int letterCount = 0;        // Keeps track of how may characters were printed on the line //
int lastWordCount = 0;      // Keeps track of how may characters are in the current word  //
int lastSpace = 0;          // Keeps track of the location of the last 'space'            //
//                                                                                        //
// The next line stores the text that we are currently printing on a line,                //
// The characters in the current word,                                                     //
// Our top line of text,                                                                  //
// Our second line of text,                                                               //
// and our third line of text                                                             //
// For a 20x4 display these are all 20 characters long                                    //
char currentLine[] = "12345678901234567890";                                              //
char    lastWord[] = "                    ";                                              //
char       line1[] = "                    ";                                              //
char       line2[] = "                    ";                                              //
char       line3[] = "                    ";                                              //
////////////////////////////////////////////////////////////////////////////////////////////

boolean ditOrDah = true;         // We have either a full dit or a full dah
boolean characterDone = true;    // A full character has been received
boolean justDid = true;          // Makes sure we only print one space during long gaps

int myBounce = 2;                // Used as a short delay between key up and down
int myCount = 0;

int wpm = 0;             // We'll print the sender's speed on the LCD when we scroll
int myNum = 0;           // We will turn dits and dahs into a binary number stored here

/////////////////////////////////////////////////////////////////////////////////
// Now here is the 'Secret Sauce'
// The Morse Code is embedded into the binary version of the numbers from 2 - 63
// The place a letter appears here matches myNum that we parsed out of the code
// #'s are miscopied characters
char mySet[] ="##TEMNAIOGKDWRUS##QZYCXBJP#L+FVH09#8###7#####/-61#######2###3#45";
char lcdChar = ' ';       // We will store the actual character decoded here


void setup() {
  lcd.begin(LCDCOLS, LCDROWS);      // Define rows and columns for the LCD display
  lcd.clear();                      // Clear the LCD

  Serial.begin(9600);               // Start serial monitor

  pinMode(signalPIN, INPUT_PULLUP);    // Reads the tone decoder chip
  pinMode(filterBUTTON, INPUT_PULLUP);  // Reads the left button (QRN Filter)
  pinMode(sweepBUTTON, INPUT_PULLUP); // Reads the right button (Sweep)
  
  pinMode(farnsBUTTON,INPUT_PULLUP);     // Reads the Farnsworth button

  pinMode (inputCLK,INPUT_PULLUP);     // CLK pin on Rotary Encoder
  pinMode (inputDT,INPUT_PULLUP);      // DT pin on Rotary Encoder
  pinMode(clickPin,INPUT_PULLUP);      // Knob-press pin on Rotary Encoder

  // Read the initial state of inputCLK on the Rotary Encoder
  previousStateCLK = digitalRead(inputCLK);
  
  if(noiseFilter > 8) noiseFilter = 4; // Initialize the noise filter at 4 ms
  
  SPI.begin();                 // The SPI bus is used to set the value of the 
                               // digital pot MPC41010 and talk to the LCD 
  delay(1000);
  pinMode (slaveSelectPIN, OUTPUT);  // Enables the MPC41010

  if(ToneSet > 245) ToneSet = 122;  // Initialize this value if it has not been stored yet      
  digitalPotWrite(ToneSet);
  
  printHeader();                // Display the Into message 
  printFarnsStatus();          // Display the current farns Value
#ifdef DEBUG                          // Print debug text on the monitor
  Serial.begin(9600);
  Serial.print("CW Decoder ");  // Display Intro Message on the monitor
  Serial.println(Vers);         // Print SW version on the monitor
#endif

LCDline = 3;                  // Set the line for incoming Morse text
lcd.setCursor(0,3);           // Set the cursor start position for incoming Morse Text
}

///////////////////////////////////////////////////////////////////
// This function sends the value to the digital pot on the SPI bus
// Parameter: 0-245
int digitalPotWrite(int value)
{
  digitalWrite(slaveSelectPIN, LOW);
  SPI.transfer(0x11);
  SPI.transfer(value);
  digitalWrite(slaveSelectPIN, HIGH);
}

/////////////////////////////////////////////////////////////////////////////////
// This function prints the Splash text on LCD
void printHeader(){
  lcd.clear();           // Get rid of any garbage that might appear on startup
  lcd.setCursor(3,0);             // Set cursor position
  lcd.print("CW DECODER ");       // Display Intro Message on the LCD
  lcd.print(Vers);                // Display software version on the LCD
  lcd.setCursor(7,1);             // Set cursor position
  lcd.print(myCall);              // Display User Callsign
  delay(3000);                    // Wait 3 seconds
 
  lcd.clear();
  printToneSet();                 // Display the Toneset (LM567 Tuning) at the top of the LCD
  printQRNf();                    // Display the QRN filter value at the top of the LCD
  printMorseSpeed();              // Display Morse Speed
 
  //lcd.setCursor(0,LCDROWS - 2);
  justDid = true;     // So we don't start with a space at the beginning of the line
}

void loop() 
{
   // ========= Process incoming Morse Code from the LM567 or Key at signalPIN 8 ==========
   signalPinState = digitalRead(signalPIN); // What is the tone decoder doing?
   if (!signalPinState) keyIsDown();        // LOW, or 0, means tone is being decoded
   if (signalPinState) keyIsUp();           // HIGH, or 1, means no tone is there
   
   // Now check to see if a button is pressed
   if (!digitalRead(filterBUTTON)) changeFilter();  // The left button adjusts the QRN Filter        
   if (!digitalRead(sweepBUTTON)) sweep();         // The right button auto-tunes the LM567 filter  
   if (!digitalRead(farnsBUTTON)) getFarns();         // The Farns button Increments the Farns setting  

} //======== End of Main Loop ========
 
// Reset all important variables after delays or changes that effect timings
void resetDefaults()
{
  downTime = 0;         // Clear Input timers
  upTime = 0;           // Clear Input timers
  startDownTime = 0;    // Clear Input timers
  startUpTime = 0;      // Clear Input timers
  dit = 10;             // We start by defining a dit as 10 milliseconds
  averageDah = 100;     // A dah should be 3 times as long as a dit
  fullWait = 6000;      // The time between letters
  waitWait = 6000;      // The time between dits and dahs
  myMax = 150;          // Sweep End Stop
  myMin = 240;          // Sweep End Stop
  LCDline = 3;          // Place to start printing text
}

// Sweeps the LM567 filter to tune the LM567 to the incoming Morse tone frequency ======
void sweep()
{
  sweepCount = 0;
  oldToneSet = ToneSet;
  lcd.clear();
  lcd.print("Sweep");
  myMin = 220;              // Reduced range to speed up sweep
  myMax = 160;              // Reduced range to speed up sweep

 
  // We keep calling the sweep methods as long as our
  // best minimum is greater than our best maximum 
  while (myMin > myMax)
   {
      sweepCount = 10;                                          
      #ifdef DEBUG                              // Print debug text on the monitor
        Serial.print("\nsweepCount=");
        Serial.println(sweepCount);
      #endif
    
      sweepUp();
      if (!digitalRead(sweepBUTTON)) sweep(); // Allow user to force a new sweep
    
      sweepDown();
      if (!digitalRead(sweepBUTTON)) sweep(); // Allow user to force a new sweep
   }
   
    // Find a value that is half way between the min and max
    ToneSet = (myMax - myMin)/2;
    ToneSet = ToneSet + myMin;
    digitalPotWrite(ToneSet);             // send this value to the digital pot
    
    #ifdef DEBUG                          // Print debug text on the monitor
      Serial.print("\nTone Parked at:");
      Serial.println(ToneSet);
    #endif
    
    lcd.clear();              // Clear the LCD
    resetDefaults();          // Update all the variables affect by the sweep delay
    printToneSet();           // Display the Toneset Value
    printQRNf();              // Display the QRN Filter value
    printWPM();               // Display the Morse Speed
    printFarnsStatus();       // Display the Farnsworth Status
}

void sweepUp()
{
  #ifdef DEBUG                      // Print debug text on the monitor  
    Serial.println("\nSweep Up");  
  #endif
  
  lcd.clear();               // Clear the LCD
  lcd.print("Sweep Up");

  // Sweeps the digital potentiometer which reading the output of the LM567
  for (ToneSet = MINSWEEP; ToneSet <= MAXSWEEP; ToneSet += 1)
    {
      digitalPotWrite(ToneSet);         // Write a value to the digital Pot
      delay(sweepCount);                // Wait
      if (!digitalRead(signalPIN))              // Read the output of the LM567
      {
        if (ToneSet > myMax)            // Check for maximum sweep value reached
        {
          myMax = ToneSet;              // Set value
          
          #ifdef DEBUG                  // Print debug text on the monitor
            Serial.print("myMax=");
            Serial.println(myMax);
          #endif
          
          lcd.print('.');               // Show our hits on the display
        }
      }
    }
 }
 
void sweepDown()
{
  #ifdef DEBUG                          // Print debug text on the monitor  
    Serial.println("\nSweep Down");  
  #endif

  lcd.clear();                      // Clear the LCD
  lcd.print("Sweep Down");
 
  for (ToneSet = MAXSWEEP; ToneSet >= MINSWEEP; ToneSet -= 2)

    {
      digitalPotWrite(ToneSet);         // Write a value to the digital Pot
      delay(sweepCount);                // Wait
      if (!digitalRead(signalPIN))      // Read the output of the LM567
      {
        if (ToneSet < myMin)            // Check for limit
        {
          myMin=ToneSet;                // Set value
          
          #ifdef DEBUG                  // Print debug text on the monitor
            Serial.print("myMin=");
            Serial.println(myMin);
          #endif
          
          lcd.print('.');               // Show our hits on the display
        }
     }
   }
}


void keyIsDown() 
{
   if (noiseFilter) delay(noiseFilter);
   signalPinState = digitalRead(signalPIN); // What is the tone decoder doing?
   
   if (signalPinState) return;              // Return if nothing is happening
   
   if (startUpTime>0)
   {  
     // We only need to do this once, when the key first goes down
     startUpTime=0;    // clear the 'Key Up' timer
   }
   // If we haven't already started our timer, do it now
   if (startDownTime == 0)
   {
       startDownTime = millis();  // Get the Arduino's current clock time
   }

     characterDone=false; // We're still building a character
     ditOrDah=false;      // The key is still down we're not done with the tone
     delay(myBounce);     // Take a short breath here
     
   if (myNum == 0) 
   {      // myNum will equal zero at the beginning of a character
      myNum = 1;          // This is our start bit  - it only does this once per letter
   }
 }
 
void keyIsUp() 
{
   if (noiseFilter) delay(noiseFilter);       // Apply noise filter delay
   signalPinState = digitalRead(signalPIN);   // What is the tone decoder doing?
   
   if (!signalPinState) return;
       
   
   // If we haven't already started our timer, do it now
   if (startUpTime == 0)
   {
    startUpTime = millis();
   }
   
   // Find out how long we've gone with no tone
   // If it is twice as long as a dah print a space
   
   upTime = millis() - startUpTime;
   
  if (upTime<20) return;
  
   // Farnsworth setting.
   if (upTime > (averageDah*Farns)) 
   {    
      printSpace();
   }
   
   // Only do this once after the key goes up
   if (startDownTime > 0)
   {
     downTime = millis() - startDownTime;  // how long was the tone on?
     startDownTime=0;      // clear the 'Key Down' timer
   }
 
   if (!ditOrDah) 
   {   
     // We don't know if it was a dit or a dah yet
      shiftBits();    // Let's go find out! And do our Magic with the bits
   }

    // If we are still building a character ...
    if (!characterDone) 
    {
       // Are we done yet?
       if (upTime > dit) 
       { 
         // BINGO! we're done with this one  
         printCharacter();       // Go figure out what character it was and print it       
         characterDone=true;     // We got him, we're done here
         myNum=0;                // This sets us up for getting the next start bit
       }
         downTime=0;             // Reset our keyDown counter
    }
}
   
   
void shiftBits() 
{
  // we know we've got a dit or a dah, let's find out which
  // then we will shift the bits in myNum and then add 1 or not add 1
  
  if (downTime < dit/3) return;  // Ignore my keybounce
  myNum = myNum << 1;            // shift bits left
  ditOrDah = true;               // We will know which one in two lines 
  
  
  // If it is a dit we add 1. If it is a dah we do nothing!
  if (downTime < dit) 
  {
     myNum++;                   // add one because it is a dit
  } 
  else 
  {
    // The next three lines handle the automatic speed adjustment:
    averageDah = (downTime+averageDah) / 2;  // running average of dahs
    dit = averageDah / 3;                    // normal dit would be this
    dit = dit * 2;          // Double it to get the threshold between dits and dahs
  }
}

void printCharacter() 
{           
  justDid = false;       // OK to print a space again after this
 
  // Punctuation marks will make a BIG myNum
  if (myNum > 63) 
  {  
    printPunctuation();   // The value we parsed is bigger than our character array
                          // It is probably a punctuation mark so go figure it out.
    return;               // Go back to the main loop(), we're done here.
  }
  lcdChar = mySet[myNum]; // Find the letter in the character set
  sendToLCD();            // Go figure out where to put in on the display
}

void printSpace() {
  if (justDid) return;    // only one space, no matter how long the gap
  justDid = true;         // so we don't do this twice
  Farns = 2 + FarnsTime;  // Confirm latest Farns value
  Serial.print("farns = ");
  Serial.println(Farns);

  lastWordCount=0;               // start counting length of word again
  currentLine[letterCount]=' ';  // add a space to the variable that stores the current line
  lastSpace=letterCount;         // keep track of this, our last, space
  
  // Now we need to clear all the characters out of our last word array
  for (int i=0; i<20; i++) 
  {
    lastWord[i]=' ';
  }
   
  lcdChar=' ';                  // This is going to go to the LCD 
  
  // We don't need to print the space if we are at the very end of the line
  if (letterCount < 20) 
  { 
    sendToLCD();                // go figure out where to put it on the display
  }
}

void printPunctuation() 
{
  // Punctuation marks are made up of more dits and dahs than
  // letters and numbers. Rather than extend the character array
  // out to reach these higher numbers we will simply check for
  // them here. This function only gets called when myNum is greater than 63
  
  // Thanks to Jack Purdum for the changes in this function
  // The original uses if then statements and only had 3 punctuation
  // marks. Then as I was copying code off of web sites I added
  // characters we don't normally see on the air and the list got
  // a little long. Using 'switch' to handle them is much better.


  switch (myNum) 
  {
    case 71:
      lcdChar = ':';
      break;
    case 76:
      lcdChar = ',';
      break;
    case 84:
      lcdChar = '!';
      break;
    case 94:
      lcdChar = '-';
      break;
    case 97:
      lcdChar = 39;    // Apostrophe
      break;
    case 101:
      lcdChar = '@';
      break;
    case 106:
      lcdChar = '.';
      break;
    case 115:
      lcdChar = '?';
      break;
    case 246:
      lcdChar = '$';
      break;
    case 122:
      lcdChar = 's';
      sendToLCD();
      lcdChar = 'k';
      break;
    default:
      lcdChar = '#';    // Should not get here
      break;
  }
  sendToLCD();          // Go figure out where to put it on the display
}

void sendToLCD()
{
  // Do this only if the character is a 'space'
  if (lcdChar > ' ')
  {
    lastWord[lastWordCount] = lcdChar; // Store the space at the end of the array
    if (lastWordCount < lineEnd - 1) 
    {
      lastWordCount++;   // Only bump up the counter if we haven't reached the end of the line
    }
  }
  currentLine[letterCount] = lcdChar; // Now store the character in our current line array
 
  letterCount++;                     // We're counting the number of characters on the line

  // If we have reached the end of the line we will go do some chores
  if (letterCount == lineEnd) 
  {
    newLine();  // Check for word wrap and get ready for the next line
    return;     // We don't need to do anything more here
  }
  
  lcd.print(lcdChar); // Print our character at the current cursor location
  printMorseSpeed();  // Dispay the current morse speed at the top of the LCD
}

//////////////////////////////////////////////////////////////////////////////////////////
// The following functions handle word wrapping and line scrolling for a 4 line display //
//////////////////////////////////////////////////////////////////////////////////////////

void newLine() 
{
  // sendToLCD() will call this routine when we reach the end of the line
  if (lastSpace == 0)
  {
    // We just printed an entire line without any spaces in it.
    // We cannot word wrap this one so this character has to go at 
    // the beginning of the next line.
    
    // First we need to clear all the characters out of our last word array
    for (int i=0; i<20; i++) 
    {
      lastWord[i]=' ';
    }
     
     lastWord[0]=lcdChar;  // store this character in the first position of our next word
     lastWordCount=1;      // set the length to 1
  }
  truncateOverFlow();      // Trim off the first part of a word that needs to go on the next line
  
  linePrep();              // Store the current line so we can move it up later
  reprintOverFlow();       // Print the truncated text and space padding on the next line 
  printMorseSpeed();       // Print the current Morse Speed value
}
  
void printMorseSpeed()
{
  // Calculate Morse Speed
  wpm = dit/2;
  wpm = 1400 / wpm ;
  if (wpm > 60) wpm = 20;        // Limit the size of wpm

  printToneSet();               // Display Toneset
  printQRNf();                  // Display QRN Filter
  printWPM();                   // Display morse speed in WPM

  lcd.setCursor(letterCount, LCDline); // Restore last text cursor position
}

void truncateOverFlow()
{
  // Our word is running off the end of the line so we will chop it off at the 
  // last space and put it at the beginning of the next line
  
  if (lastSpace==0) {return;}  // Don't do this if there was no space in the last line
  
  // Move the cursor to the place where the last space was printed on the current line
  lcd.setCursor(lastSpace,LCDline);
  
  letterCount = lastSpace;    // Change the letter count to this new shorter length
  
  // Print 'spaces' over the top of all the letters we don't want here any more
  for (int i = lastSpace; i < 20; i++) 
  {
     lcd.print(' ');         // This space goes on the display
     currentLine[i] = ' ';   // This space goes in our array
  }
}


void linePrep()
{
     LCDline++;           // This is our line number, we make it one higher
     
     // What we do next depends on which line we are moving to
     // The first three cases are pretty simple because we working on a cleared
     // screen. When we get to the bottom, though, we need to do more.
     switch (LCDline) 
     {
     case 1:
       // Line 1 is reserved for displaying our settings so nothing to do here
       break;
     case 2:
       // We just finished line 1
       // We are going to move the contents of our current line into the line1 array
       for (int j=0; j<20; j++)
       {
         line1[j] = currentLine[j];
       }
        break;
     case 3:
       // We just finished line 2
       // We are going to move the contents of our current line into the line2 holding bin
       for (int j=0; j<20; j++)
       {
         line2[j] = currentLine[j];
       }
       break;
     case 4:
       // We just finished line 3
       // We are going to move the contents of our current line into the line3 holding bin
       for (int j=0; j<20; j++)
       {
         line3[j] = currentLine[j];
       }
       // This is our bottom line so we will keep coming back here
       LCDline = 3;  
       
       myScroll();  // Move everything up a line so we can do the bottom one again
       break;
   }
}

void myScroll()
{
  // We will move each line of text up one row
  
  int i = 0;  // We will use this variables in all our for-loops
  
  // We discard our Line1 text because the top line of the LCD is reserved 
  // for displaying our settings so Morse text is not allowed here 
  
  // Move everything stored in our line2 array into our line1 array
  for (i = 0; i < 20; i++) 
  {
    line1[i] = line2[i];
  }
  
  lcd.setCursor(0,1);      // Move the cursor to the beginning of the second line
  lcd.print(line1);        // Print the new line1 here
 
  // Move everything stored in our line3 array into our line2 array
  for (i = 0; i < 20; i++) 
  {
    line2[i]=line3[i];
  }
  lcd.setCursor(0,2);      // Move the cursor to the beginning of the third line
  lcd.print(line2);        // Print the new line2 here
 
  // Move everything stored in our currentLine array into our line3 array
  for (i = 0; i < 20; i++) 
  {
    line3[i] = currentLine[i];
  }

}

void reprintOverFlow()
{
  // Here we put the word that wouldn't fit at the end of the previous line
  // Back on the display at the beginning of the new line
  
  // Load up our current line array with what we have so far
   for (int i = 0; i < 20; i++) 
   {
     currentLine[i] = lastWord[i];
   } 

  lcd.setCursor(0, LCDline);              // Move the cursor to the beginning of our new line 
  lcd.print(lastWord);                    // Print the stuff we just took off the previous line
  letterCount = lastWordCount;            // Set up our character counter to match the text
  lcd.setCursor(letterCount, LCDline); 
  lastSpace=0;                            // Clear the last space pointer
  lastWordCount=0;                        // Clear the last word length
}

// ====== Allows QRN filter setting to be adjusting using the LEFT BUTTON =======
void changeFilter()
{
  noiseFilter++;                      // Increment the filter value
  delay(300);                         // Delay to prevent key bounce
  if (noiseFilter>8) noiseFilter=0;   // wrap value around if necessary

//  lcd.setCursor(6,0);                 // Set the cursor on the LCD
//  lcd.print("fl=");                   // Display the new QRN filter value
//  lcd.print(noiseFilter);

  printQRNf(); 
  // Store new filter value
  oldNoiseFilter = noiseFilter;       // Update storage flag

  // Restore previous cursor position for continued display of incoming text
  lcd.setCursor(letterCount, LCDline);   // Restore previous text-cursor position
}


// Print the Toneset value (LM567 tuning value) at the top of the LCD
void printToneSet()
{
  lcd.setCursor(5,0);           // Set cursor position
  lcd.print("T=");              // Display Toneset
  lcd.print(ToneSet);
  lcd.setCursor(letterCount, LCDline);   // Restore previous text-cursor position
}

// Prints the QRN filter value at the top of the LCD
void printQRNf()
{
  lcd.setCursor(0,0);           // Set cursor position
  lcd.print("fl=");             // Display QRN Filter
  lcd.print(noiseFilter);
  lcd.setCursor(letterCount, LCDline);   // Restore previous text-cursor position
}


// Prints the Morse Speed in WPM at the top of the LCD
void printWPM()
{
  lcd.setCursor(14,0);          // Set cursor position
  lcd.print("WPM=");            // Display Morse Speed
  lcd.print(wpm);
  lcd.setCursor(letterCount, LCDline);   // Restore previous text-cursor position
}


void getFarns()
{
  FarnsTime = FarnsTime + 2;          // Increment the Farns Timing variable
  delay(300);                         // Wait to avoid keybounce
  if (FarnsTime > 4) FarnsTime = 0;   // Wrap FarnsTime limits between 0 and 4
  Farns = 2 + FarnsTime;              // Set the new Farns value
  printFarnsStatus();                 // Display the new setting  
}


// Prints the Farnsworth Status at the top of the LCD
void printFarnsStatus()
{
  lcd.setCursor(11,0);                // Set cursor position
  lcd.print("F");                     // Display Farnsworth Value

  // Print the Farnsworth Setting based on the value of FarnsTime
  switch (FarnsTime)
  {
  case 0:
    lcd.print("O");
    break;
  case 2:
    lcd.print("8");
    break;
  case 4:
    lcd.print("5");
    break;
  }
  lcd.setCursor(letterCount, LCDline);   // Restore previous text-cursor position
}
