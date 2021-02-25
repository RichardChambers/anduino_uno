
/*
 * Simulator for a Weigh-Tronix scale for point of sale applications.
 *     TITLE: NCI General Serial Communications Protocol
 *     Desc: NCI’s general purpose scale interface.
 *     
 *     Weigh-Tronix  SCP-01  8408-14788-01
 *     SCP-01 (NCI STANDARD, and 3825)
 *     SERIAL COMMUNICATIONS PROTOCOL:
 * 
 * The point of sale application sends a scale request to the Arduino which parses
 * the request, formats a response message, and sends the response message.
 * 
 * NOTE: Support the mandatory commands (request weight, request status, zero the scale) only.
 * 
 * From the Weigh-Tronix documentation of the NCI SCP-01 protocol.
 * 
 * Key to symbols used in the message descriptions following:
 *     <ETX> End of Text character (0x03).
 *     <LF>  Line Feed character (0x0A).
 *     <CR>  Carriage Return character (0x0D).
 *     <SP>  Space (0x20).
 *     x     Weight characters from display including minus sign and out-of range characters.
 *     c     Message/menu (ie non-weight) characters from display.
 *     hh... Two or more status bytes.
 *     uu    Units of measure (lb, kg, oz, g, etc. using ANSI standard abbreviations).
 *     
 * Name: Request weight
 *     Command:  W<CR>
 *     Response: Returns decimal weight (x1), units and status.
 *               <LF>xxxx.xxuu<CR><LF>hh...<CR><ETX>
 *               
 *               Returns lb-oz weight, units plus scale status.
 *               <LF>xxlb<SP>xx.xoz<CR><LF>hh...<CR><ETX>
 *               
 *               Returns contents of display (other than wt) with units and scale status.
 *               <LF>cccccccuu<CR><LF>hh...<CR><ETX
 * 
 * Name: Change units of measure
 *     Command:  U<CR>
 *     Response: Changes units of measure, returns new units and scale status.
 *               <LF>uu<CR><LF>hh...<CR><ETX>
 *               
 * Name: Request status
 *     Command:  S<CR>
 *     Response: Returns scale status.
 *               <LF>hh...<CR><ETX>
 *               
 *               Bit 0 is the least significant bit in the byte while bit 7 is the most significant bit.
 *               Bit   Status Byte 1        Status Byte 2             Status Byte 3 (opt)
 *                0    1 = Scale in motion   1 = Under capacity         00 = Low range
 *                     0 = Stable            0 = Not under capacity     01 = (undefined)
 *                                                                      10 = (undefined)
 *                1    1 = Scale at zero     1 = Over capacity          11 = High range
 *                     0 = Not at zero       0 = Not over capacity
 *                     
 *                2    1 = RAM error         1 = ROM error               1 = Net weight
 *                     0 = RAM okay          0 = ROM okay                0 = Gross weight
 *                     
 *                3    1 = EEPROM error      1 = Faulty calibration      1 = Initial zero error
 *                     0 = EEPROM okay       0 = Calibration okay        0 = Initial zero OK
 *                     
 *                4    Always 1              Always 1                    Always 1
 *                
 *                5    Always 1              Always 1                    Always 1
 *                
 *                6    Always 0              1 = Byte follows            1 = Byte follows
 *                                           0 = Last byte               0 = Last byte
 *                                           
 *                7    Parity                Parity                      Parity
 *                
 *                
 * Name: unrecognized command
 *     Command:  all others
 *     Response: Unrecognized command
 *               <LF>?<CR><ETX>
 *     
 */

 
byte incoming;
String inBuffer;

// scale measurement data. this determines values returned in a weight response.

int   lb1 = 0, lb2 = 250;    // most significant and least signicant parts of weight.
unsigned char s1 = 0x30, s2 = 0x30;   // status byte 1 and status byte 2

enum  ScaleUnits {English, Metric};
ScaleUnits   iUnits = English;

struct {
  char *specWeight;
  char *specStatus;
  short maxMsp;
  short maxLsp;
} specInUseFmt [] = {
      //weight then units then status
      { "\n%4.4d.%2.2d%s\r\n%c%c\r\x03", "\n%c%c\r\x03", 4, 2},    // SCP-01 specification for response
      { "\n%2.2d.%3.3d%s\r\nS%c%c\r\x03", "\nS%c%c\r\x03", 2, 3 }  // SCP-02 specification for response
};

char *lcdInfoFmt[] = {
      // max of 16 characters for 16x2 LCD module
      "%4.4d.%2.2d%-2.2s %2.2x %2.2x",    // SCP-01 specification for response
      "%2.2d.%3.3d%-2.2s %2.2x %2.2x "    // SCP-02 specification for response
};

enum SpecInUse { Scp_01 = 0, Scp_02 = 1};
SpecInUse  specInUse = Scp_02;

int mypow (int baseVal, int expVal)
{
  int iVal = 1;

  if (expVal == 0) {
    iVal = 0;
  } else if (expVal > 0) {
    for ( ; expVal > 0; expVal--) {
      iVal *= baseVal;
    }
  }

  return iVal;
}
void modWeightValues()
{
    int iModVal = 1;

    iModVal = mypow(10, specInUseFmt[specInUse].maxMsp);
    lb1 %= iModVal;
    iModVal = mypow(10, specInUseFmt[specInUse].maxLsp);
    lb2 %= iModVal;
}

#define USE_LCD
#define USE_KEYPAD
//#define USE_SERIAL


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

     if (iUnits == English)
      sprintf (cBuff, lcdInfoFmt[specInUse], lb1, lb2, "LB", s1, s2);
    else
      sprintf (cBuff, lcdInfoFmt[specInUse], lb1, lb2, "KG", s1, s2);
          
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
        lb1 = lb2 = 0;
        setLcdIndicator('*');
        break;
    case '#':     // change the units of measurement
        switch (iUnits) {
          case English: iUnits = Metric; break;
          case Metric:  iUnits = English; break;
          default: iUnits = English; break;
        }
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
          modWeightValues();  // mod the current weight values in case too large for new spec
          updateLCDInfo();
          setLcdIndicator('R');
          break;
        }
    case '1':
        if (stNdx == 100) {
          specInUse = Scp_02;
          stNdx = 0;          // reset the stNdx state indicator as we are done
          lbNdx = 100;        // set the weight entry state indicator to ignore input
          modWeightValues();  // mod the current weight values in case too large for new spec
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

        if (lbNdx < specInUseFmt[specInUse].maxMsp) {
          lb1 *= 10;
          lb1 += customKey - '0';
        } else if (lbNdx < specInUseFmt[specInUse].maxMsp + specInUseFmt[specInUse].maxLsp) {
          lb2 *= 10;
          lb2 += customKey - '0';
         }
        lbNdx++;
        if (lbNdx == specInUseFmt[specInUse].maxMsp + specInUseFmt[specInUse].maxLsp) {
          updateLCDInfo();
          setLcdIndicator('R');
        }
        break;  
    }
  }
}
#endif    // defined(USE_KEYPAD)

void handle_command(String &inCommand) {
    char cBuff[64];

    switch (inCommand[0]) {
      case 'W':    // weight command
      case 'w':
        {
          if (iUnits == English)
            sprintf (cBuff, specInUseFmt[specInUse].specWeight, lb1, lb2, "LB", s1, s2);
          else
            sprintf (cBuff, specInUseFmt[specInUse].specWeight, lb1, lb2, "KG", s1, s2);
        }
        break;
      case 'S':    // status command
      case 's':
      case 'Z':    // zero scale command (zeros scale but response is same as status command)
      case 'z':
        sprintf (cBuff, specInUseFmt[specInUse].specStatus, s1, s2);
        break;
      default:     // unrecognized command
        sprintf (cBuff, "\n?\r\x03");
        break;
    }
    
    Serial.print(cBuff);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600); // Turn the serial protocol ON

  delay (1000);

  updateLCDInfo();
  setLcdIndicator('R');
 
}

void loop() {
   // put your main code here, to run repeatedly:

#if defined(USE_KEYPAD)
   handleKeyPad();
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
