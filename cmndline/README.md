## Sample structure for command line shell for Arduino Uno

This folder contains a simple command line shell for the Arduino Uno. This is meant to be a simple
example that provides a structure for future enhancements and changes.

This command shell supports the following commands:
  set
  The set command is used to set various settings. The following command examples show the supported
  set of arguments for the set command.
  
  set device [name]
  If [name] is specified then set the name data area of the EEPROM to the specified string.
  if [name] is not specified then print out the current value of the name data area of the EEPROM.
  
  For this shell, the name data area of the EEPROM begins at address 0 and is 24 bytes long.
