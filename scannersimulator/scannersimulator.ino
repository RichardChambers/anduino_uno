
/*
 * Scanner simulator built on the previous scale simulator.
 * 
 * This application turns an Arduino into a simulator for a barcode scanner scale
 * by sending a barcode in the proper message format.
 * 
 * The user interface needs the following changes:
 *  - need to handle button event to send a scan to the terminal
 *  - need to show the current PLU that will be sent
 *  - need a way to choose the current PLU
 */
 
byte incoming;
String inBuffer;

// table of scanner data. this determines values returned in a scan response.
// the symChar indicates the symbology used for the barcode
//   - A   UPC-A, length 12
//   - E   UPC-E, length 7
//   - FF  JAN-8, EAN-8, length 9
//   - F   Jan-13, EAN-13, length 13
//   - B1  Code 39
//   - B2  Interleaved 2 of 5
//   - B3  Code 128
//   - ]e0 RSS-14

struct ScannerData {
  char symChar[4];      // string representing symbology such as 'A' for UPC-A
  char barcode[32];     // string of digits from the barcode
};

ScannerData pluList[] = {
  {"A", "070177155766"},          //    Twinings English Afternoon tea 20 bags
  {"A", "070177154240"},          //    Twinings Irish Breakfast tea 20 bags
  {"A", "070177154127"},          //    Twinings Darjeeling tea 20 bags
  {"A", "072310001893"},          //    Bigelow Plantation Mint tea 20 bags
  {"A", "072310001978"},          //    Bigelow Lemon Lift tea 20 bags
  {"A", "072310001053"},          //    Bigelow Constant Comment tea 20 bags
  {"A", "747599303142"},          //    Ghirardelli Squares Dark Chocolate Sea Caramel
  {"A", "046677426002"},          //    4 pack Philips EcoVantage Soft White 40w A19 bulbs
  {"A", "046677426040"},          //    4 pack Philips Soft White 100w A19 bulbs
  {"A", "086854005682"},          //    5.75 oz bottle Laura Lynn Manzanilla Olives with minced pimiento
  {"A", "086854043622"},          //    8.5 oz bottle Laura Lynn extra virgin olive oil
  {"A", "086854042311"},          //    14.5 oz can Laura Lynn diced tomatoes no salt added
  {"A", "041443113421"},          //    14.5 oz can Margaret Holmes Italian Green Beans
  {"A", "039400017066"},          //    16 oz can Bush's Reduced Sodium Garbanzos chick peas
  {"A", "052000011227"},          //    15 oz can Van Camp's Pork and Beans in tomato sauce
  {"A", "029000076501"},          //    16 oz Planters Dry Roasted Peanuts Lightly salted
  {"A", "037000388517"},          //    Charmin Ultra Soft toilet tissue 6 roll Super Mega
  {"A", "037000527787"},          //    Charmin Ultra Soft toilet tissue 6 roll Mega
  {"A", "201404309526"},          //    container of beef loin 1.59lbs @ $5.99 (PLU, 01404 with price $9.52)
  {"A", "200065109001"},          //    container London Broil 2.44lbs @ $3.69 (PLU, 00065 with price $9.00)
  {"A", "206141306005"},          //    container deli lunch $6.00
  {"E", "1215704"}                //    Aquafina bottled water 1L
};

unsigned char s1 = 0x30, s2 = 0x30;   // status byte 1 and status byte 2

struct {
  char *specScan;
  char *specStatus;
  bool  bSendBcc;
} specInUseFmt [] = {
      //weight then units then status
      { "18%s%s\x03", "14%c%c\x03", false},     // NCR 78xx without BCC appended to message
      { "08%s%s\x03%2.2x", "14%c%c\x03", true}  // NCR 78xx with BCC appended to message
};

char *lcdInfoFmt[] = {
      // max of 16 characters for 16x2 LCD module
      "%4.4d.%2.2d%-2.2s %2.2x %2.2x",    // SCP-01 specification for response
      "%2.2d.%3.3d%-2.2s %2.2x %2.2x "    // SCP-02 specification for response
};

enum SpecInUse { Scp_01 = 0, Scp_02 = 1};
SpecInUse  specInUse = Scp_02;

// calculate the BCC or Block Check Character which is a
// checksum on the message to send. It is added to the
// message after the ETX character in the message.
// See https://en.wikipedia.org/wiki/Binary_Synchronous_Communications
//
unsigned char ScannerScaleCalcBCC(unsigned char *puchData, short sLength)
{
    unsigned char   uchBCC = 0;
    short   sIndex;

    for (sIndex = 0; sIndex < sLength; sIndex++) {
        uchBCC ^= *(puchData + sIndex);
    }

    return (uchBCC);
}

//#define USE_LCD         // use the 16x2 LCD as a display
//#define USE_KEYPAD      // use the keypad for input selection
#define USE_BUTTON      // use the button to trigger sending a scan
#define USE_SERIAL      // use Serial for debugging prints


#if defined(USE_LCD)
// A 16x2 LCD can be attached to the Arduino to show
// status information. See format specifier lcdInfoFmt[] above.
//  - d  -> a digit or a decimal point for the current weight setting
//  - u  -> a letter of current weight units:  lb, kg
//  - s  -> a letter of current scale specification
//  - i  -> a symbol representing current input status from keypad see handleKeyPad()
//          R ready for weight request
//          * clear key to restart the data entry sequence
//          # change the units of measurement
//          A set the status byte 1 value (0 - 3)
//          B set the status byte 2 value (0 - 3)
//          C set the specification to be used for response messages
//  - m  -> a letter of a free form message
//
//      00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15
//  0    d  d  d  d  d  d  d  u  u     a  a  b  b  
//  1    i  m  m  m  m  m  m  m  m  m  m  m  m  m  m  m

#include <LiquidCrystal.h>

// In order to use both the LCD and the keypad we will
// use the six analog pins as digital pins.
// Analog pin 0 is digital pin 14, Analog pin 1 is digital pin 15, etc.
LiquidCrystal lcd(14, 15, 16, 17, 18, 19);
#endif

#if defined(USE_BUTTON)
// A button is used as a scan trigger. The button event represents
// an item's barcode being scanned to ring up the item.
//
// The button is expected to be a pushbutton that is wired so
// that when pressed, the pin to which it is attached will see
// HIGH when the pushbutton is pressed and LOW when it is released.
// One side of the pushbutton is wired to 5v and the other pole
// is wired to the sensing pin on the Arduino. There is a pulldown
// resistor of 10KOhm connected between the second pole and ground.
//
// The button has a debouncing delay so that the button state is sensed
// only so often in order to give a person time to press the button and
// release it in order to trigger only a single scan event.

const int buttonPin = 10;    // the pin number the pushbutton is connected to
int buttonState = 0;         // variable for reading the pushbutton status
const unsigned long buttondelay = 250;  // milliseconds of delay for debouncing

#endif

#if defined(USE_KEYPAD)
// A membrane matrix keypad can be attached to the Arduino to allow a simple
// user interface for changing the simulated amount of weight and the units of
// measurement.
//
// The keypad library used in testing is from the Elegoo UNO R3 Project Complete Starter Kit with Arduino Uno:
// || @version 3.1
// || @author Mark Stanley, Alexander Brevig

#include <Keypad.h>

const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns
//define the cymbols on the buttons of the keypads
char hexaKeys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {9, 8, 7, 6}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {5, 4, 3, 2}; //connect to the column pinouts of the keypad

//initialize an instance of class NewKeypad
Keypad customKeypad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);

short lbNdx = 0;    // index for keypad data entry into lb1 and lb2 to change weight
short stNdx = 0;    // set status indicator, 0 no set, 1 set byte 1, 2 set byte 2, 100 set Spec in use

int setLcdIndicator (int c)
{
  char cBuff[4]= {0, 0};
  cBuff[0] = c;
  
#if defined(USE_LCD)
  lcd.setCursor(0,1);    // beginning at column 0, line 1 (begining of first column on second line)
  lcd.print(cBuff);
#endif
#if defined(USE_SERIAL)
  Serial.print("setLcdIndicator: "); 
  Serial.println(cBuff);
#endif
  return 0;
}

int updateLCDInfo (void)
{
    char cBuff[32] = {0};
          
#if defined(USE_LCD)
    lcd.setCursor(1,0);
    lcd.print(cBuff);
#endif
#if defined(USE_SERIAL)
    Serial.println(cBuff);
#endif
    return 0;
}

void handleKeyPad ()
{
  char customKey = customKeypad.getKey();
  
  if (customKey){
    switch (customKey) {
    case '*':     // clear key to restart the data entry sequence
        lbNdx = 0;        // set the weight entry state indicator to allow input
        stNdx = 0;        // set the stNdx state indicator indicating weight entry
        setLcdIndicator('*');
        break;
    case '#':     // change the units of measurement
        stNdx = 0;          // reset the stNdx state indicator as we are done
        lbNdx = 100;        // set the weight entry state indicator to ignore input
        updateLCDInfo();
        setLcdIndicator('R');
        break;
    case 'A':       // set the status byte 1 value (0 - 3)
        stNdx = 1;
        lbNdx = 100;        // set the weight entry state indicator to ignore input
        setLcdIndicator('A');
        break;
    case 'B':       // set the status byte 2 value (0 - 3)
        stNdx = 2;
        lbNdx = 100;        // set the weight entry state indicator to ignore input
       setLcdIndicator('B');
        break;
    case 'C':       // set the specification to be used for response messages
        stNdx = 100;
        lbNdx = 100;        // set the weight entry state indicator to ignore input
        setLcdIndicator('C');
        break;
    case 'D':       // write out current status information
        stNdx = 0;          // reset the stNdx state indicator as we are done
        lbNdx = 100;        // set the weight entry state indicator to ignore input
        updateLCDInfo();
        setLcdIndicator('R');
        break;
    case '0':
        if (stNdx == 100) {
          specInUse = Scp_01;
          stNdx = 0;          // reset the stNdx state indicator as we are done
          lbNdx = 100;        // set the weight entry state indicator to ignore input
          updateLCDInfo();
          setLcdIndicator('R');
          break;
        }
    case '1':
        if (stNdx == 100) {
          specInUse = Scp_02;
          stNdx = 0;          // reset the stNdx state indicator as we are done
          lbNdx = 100;        // set the weight entry state indicator to ignore input
          updateLCDInfo();
          setLcdIndicator('R');
          break;
        }
    case '2':
    case '3':
        if (stNdx == 1) {
          // set status byte 1, bits 0 and 1
          s1 &= 0xfc;   // clear bits 0 and 1
          s1 |= customKey - '0';
        } else if (stNdx == 2) {
          // set status byte 2, bits 0 and 1
          s2 &= 0xfc;   // clear bits 0 and 1
          s2 |= customKey - '0';
        }
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        if (stNdx) {
          // if status byte change or spec change was requested then just ignore this
          stNdx = 0;
          lbNdx = 100;
          updateLCDInfo();
          setLcdIndicator('R');
          break;
        }

        break;  
    }
  }
}
#endif    // defined(USE_KEYPAD)

void handle_command(const String &inCommand) {
    char cBuff[64] = {0};

#if defined(USE_SERIAL)
    Serial.println(inCommand);
#endif
    switch (inCommand[0]) {
      case 0x31:    // status command
#if defined(USE_SERIAL)
        Serial.println("0x31 command");
#endif
        if (inCommand[1] == 0x33) {
          sprintf (cBuff, specInUseFmt[0].specStatus, s1, s2);
        } else {
          sprintf (cBuff, specInUseFmt[0].specScan, pluList[0].symChar, pluList[0].barcode);
        }
        break;
      case 'S':    // status command
      case 's':
        sprintf (cBuff, specInUseFmt[specInUse].specStatus, s1, s2);
        break;
      default:     // unrecognized command
        sprintf (cBuff, "\n?\r\x03");
        break;
    }
    
    Serial.print(cBuff);
#if defined(USE_SERIAL)
    Serial.println("");
#endif
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600); // Turn the serial protocol ON

  delay (1000);

#if defined(USE_LCD)
  updateLCDInfo();
  setLcdIndicator('R');
#endif

#if defined(USE_BUTTON)
  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT);
#endif

}

void loop() {
   // put your main code here, to run repeatedly:

#if defined(USE_KEYPAD)
   handleKeyPad();
#endif

#if defined(USE_BUTTON)
    // read the state of the pushbutton value:
    buttonState = digitalRead(buttonPin);
    
    if (buttonState) {
      delay(buttondelay);
      handle_command("11");
      // Clear the string for the next command
      inBuffer = "";
    }
#endif

   // setup as non-blocking code
   incoming = Serial.read();
   if(incoming < 255) {
         if(incoming == '\n' || incoming == '\r' || incoming == '\x03') {  // newline, carriage return, both, or custom character
            // handle the incoming command
            handle_command(inBuffer);

            // Clear the string for the next command
            inBuffer = "";
        } else{
            // add the character to the buffer
            inBuffer += (char)incoming;
        }
    }
}
