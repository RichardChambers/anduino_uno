
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

int   lb1 = 0, lb2 = 26;    // most significant and least signicant parts of weight.
unsigned char s1 = 0x30, s2 = 0x30;   // status byte 1 and status byte 2

enum  ScaleUnits {English = 0, Metric = 1};
ScaleUnits   iUnits = English;
char *pUnits[] = {"LB", "KG" };

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
short stNdx = 0;    // set status indicator, 0 no set, 1 set byte 1, 2 set byte 2

enum Protocol_set {Protocol_SCP_01 = 0, Protocol_SCP_02 = 1};

Protocol_set cProtocol = Protocol_SCP_01;

struct {
  short  sMsbLen;
  short  sMsbMod;
  short  sLsbLen;
  short  sLsbMod;
  char  *fmt_weight;
  char  *fmt_status;
} Protocol_fmt [] = {
  {4, 10000, 2, 100, "\n%4.4d.%2.2d%s\r\n%c%c\r\x03", "\n%c%c\r\x03"},   // SCP-01 response message for weight and status
  {2, 100, 3, 1000, "\n%2.2d.%3.3d%s\r\nS%c%c\r\x03", "\nS%c%c\r\x03"}  // SCP-02 response message for weight and status
};

void handleKeyPad ()
{
  char customKey = customKeypad.getKey();
  
  if (customKey){
    switch (customKey) {
    case '*':     // clear key to restart the data entry sequence
        lbNdx = 0;
        lb1 = lb2 = 0;
        break;
    case '#':     // change the units of measurement
        switch (iUnits) {
          case English: iUnits = Metric; break;
          case Metric:  iUnits = English; break;
          default: iUnits = English; break;
        }
        break;
    case 'A':       // set the status byte 1 value (0 - 3)
        stNdx = 1;
        break;
    case 'B':       // set the status byte 2 value (0 - 3)
        stNdx = 2;
        break;
    case 'C':       // change the serial protocol SCP-01, SCP-02, etc.
        stNdx = 100;
        break;
    case '0':
        if (stNdx == 100) {
          cProtocol = Protocol_SCP_01;
          stNdx = 0;
          break;
        }
    case '1':
        if (stNdx == 100) {
          cProtocol = Protocol_SCP_02;
          stNdx = 0;
          break;
        }
    case '2':
    case '3':
        if (stNdx == 1) {
          // set status byte 1, bits 0 and 1
          s1 &= 0xfc;   // clear bits 0 and 1
          s1 |= customKey - '0';
          stNdx = 0;
         break;
        } else if (stNdx == 2) {
          // set status byte 2, bits 0 and 1
          s2 &= 0xfc;   // clear bits 0 and 1
          s2 |= customKey - '0';
          stNdx = 0;
          break;
        }
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        if (stNdx) {
          // if status byte change was requested then just ignore this
          // and ensure no other digits entered will affect current weight setting.
          stNdx = 0;
          lbNdx = 100;
          break;
        }

        if (lbNdx < Protocol_fmt[cProtocol].sMsbLen) {
          lb1 *= 10;
          lb1 += customKey - '0';
        } else if (lbNdx < Protocol_fmt[cProtocol].sMsbLen + Protocol_fmt[cProtocol].sLsbLen) {
          lb2 *= 10;
          lb2 += customKey - '0';
        }
        lbNdx++;
        break;  
    }
  }
}

void handle_command(String &inCommand) {
    char cBuff[64];

    switch (inCommand[0]) {
      case 'W':    // weight command
      case 'w':
        sprintf (cBuff, Protocol_fmt[cProtocol].fmt_weight,
            (lb1 % Protocol_fmt[cProtocol].sMsbMod), (lb2 % Protocol_fmt[cProtocol].sLsbMod),
            pUnits[iUnits], s1, s2);
        break;
      case 'S':    // status command
      case 's':
      case 'Z':    // zero scale command (zeros scale but response is same as status command)
      case 'z':
        sprintf (cBuff, Protocol_fmt[cProtocol].fmt_status, s1, s2);
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

  // Serial.println("Program started.");
}

void loop() {
   // put your main code here, to run repeatedly:
   
   handleKeyPad();
  
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
