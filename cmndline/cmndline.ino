#include <EEPROM.h>

void setup() {
  // put your setup code here, to run once:
  Serial.begin (19200);

  while (!Serial) {
    ;  // wait for serial port to connect
  }

}

// ------------------------------------------------------------------
//  The following function, getToken(), is used to implement a simple
//  recursive descent parser. This function and the token struct is used
//  to process a command line in a manner similar to standard command shell
//  specifications.
//
//  A command line is expected to be composed of multiple tokens which are
// separated by spaces.

typedef unsigned short USHORT;

struct token {
  char    *first;   // beginning of token
  char    *last;    // last character of token
  USHORT  len;      // count of characters of token
  char    *strEnd;  // points to position after last character of string being parsed
};

enum tokState { TokInitial = 0, TokStringStart, TokString, TokQuoted, TokEnd };
struct token getToken (struct token myTok)
{
  tokState   state = TokInitial;
  char      *line = myTok.last;

 myTok.first = line;

 while (state != TokEnd) {
  switch (*line) {
    case 0:      // this is end of string terminator
      state = TokEnd;
    case ' ':
      if (state != TokQuoted && state != TokInitial) state = TokEnd;
      break;
    case '"':
      if (state != TokQuoted) {
        state = TokQuoted;
        myTok.first = line;
      } else {
        state = TokString;
      }
      break;
    case 1:     // this appears to be same line end
    case '\r':
      state = TokEnd;
      break;
    default:
      switch (state) {
        case TokString:
        case TokQuoted:
          break;
        case TokStringStart:
          state = TokString;
          break;
        default:
          state = TokStringStart;
          myTok.first = line;
          break;
      }
      break;
  }
  if (state == TokEnd) {
    myTok.last = line;
    myTok.len = myTok.last - myTok.first;
    break;
  }
  line++;
 }
  return myTok;
}

// --------------------------------------------------------------------

// Command handlers follow. These are the functions which will handle
// the specific command entered.

struct cmdList {
  char *cmdName;
  int (*pHandler) (struct token x);
};

/*
 * handlerSet - handle the set command.
 * 
 *   The set command is used to set particular provisioning information.
 *   
 *   set device name
 *     Sets the device name to be the text specified by name. The device name
 *     is a text string that identifies the device. It is stored in the EEPROM
 *     at address 0 and
 */
struct DeviceName {
  char  aszDeviceName [24];
};

int handlerSetDevice (struct token tok)
{
  DeviceName deviceName;

  tok = getToken (tok);
  if (tok.first && tok.first < tok.strEnd) {
    // set the specified device name into the EEPROM
    if (tok.len > 23) tok.len = 23;
    strncpy (deviceName.aszDeviceName, tok.first, tok.len);
    deviceName.aszDeviceName[tok.len] = 0;  // end of string
    EEPROM.put (0, deviceName);
  } else {
    // read the device name from EEPROM and print it out
    EEPROM.get (0, deviceName);
    deviceName.aszDeviceName[23] = 0;  // ensure zero terminator to device name
    Serial.println (deviceName.aszDeviceName);
  }
}

int handlerSet (struct token tok)
{
  cmdList setList[] = {
    {"device", handlerSetDevice},
    {  0,  0}
  };

  tok = getToken (tok);
   if (tok.first && tok.first < tok.strEnd) for (int i = 0; setList[i].cmdName; i++) {
     if (strncmp (setList[i].cmdName, tok.first, tok.len) == 0) {
       setList[i].pHandler (tok);
       break;
     }
   }
  return 0;
}

// -----------------------------------------------------------------------------
//
// following is list of supported commands and a pointer to the function
// which handles the command and processes the comnand parameters.
// Each of the commands in this command list may have further arguments which
// the handler for the command must process. This is only a dispatch function
// that determines which handler to hand the command line to based on the
// first token on the command line.

struct cmdList myList [] = {
  {"set", handlerSet},
  { 0, 0}
};

int processCmdLine (char *cmdline, int nBytes)
{
  struct token myTok = {0};
  
  myTok.last = cmdline;
  myTok.strEnd = nBytes + myTok.last;
  
  myTok = getToken (myTok);

#if 0
  // test code to test out the getToken() function to see if it is parsing the
  // input line of text properly.
  if (myTok.first) do {
    Serial.print (myTok.len);
    Serial.print ("  ."); Serial.print ( (int)*myTok.first); Serial.print (". ");
    Serial.println (myTok.first);
    myTok = getToken (myTok);
   } while (myTok.first && myTok.first < myTok.strEnd);
#else
   if (myTok.first && myTok.first < myTok.strEnd) for (int i = 0; myList[i].cmdName; i++) {
     if (strncmp (myList[i].cmdName, myTok.first, myTok.len) == 0) {
       myList[i].pHandler (myTok);
       break;
     }
   }
#endif   
}

// -----------------------------------------------------------------------------

char  cmdline[64];

int cmdlinestate = 0;

void loop() {
  int nBytes;
  
  // put your main code here, to run repeatedly:

  switch (cmdlinestate) {
    case 0:    // initial state so prompt the user
      Serial.print ("cmd> ");
      cmdlinestate = 1;
      break;
    case 1:    // line was entered
      nBytes = Serial.readBytes (cmdline, 63);
      if (nBytes) {
//        Serial.print (" nBytes = "); Serial.println(nBytes);
        cmdline[nBytes] = 0;
        // echo the entered command since IDE Serial Monitor has separate control for text entry.
        Serial.println (cmdline);
        processCmdLine (cmdline, nBytes);
        cmdlinestate = 0;
      }
      break;
  }
}
