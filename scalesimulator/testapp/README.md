## Simple console app to test Arduino scale simulator

This is a simple Visual Studio 2019 C++ console application to test the Arduino scale simulator.

The user interface has single character commands that causes the application to create the appropriate
request message as documented in the Weigh-Tronix SCP-01 serial communications protocol documentation,
send the command through a COM port that transports data through a USB serial communications connection
to the Arduino which then creates a response message from a set of parameters.

The parameter values in the Arduino can be changed through a user interface based on a membrane matrix keypad
whose 8 pin connector is attached to 8 data pins of the Arduino and read using a keypad library.

The response message from the Arduino is displayed on the console window the app is running in.

The available commands are:
 - w  request a weight
 - s  request a status
 - z  zero the scale
 - p  close current port and open one specified (syntax p n where n is serial port number)
 - h  display the list of commands (help)
 - e or x  exit the application
 
 
