# anduino_uno

This is a repository for lesson projects with the Elegoo UNO R3 Project Complete Starter Kit with Arduino Uno.

Some projects are duplicates of projects with the Osoyoo Raspberry Pi Starter Kit as in some cases I reuse the
source code from a project I first did with the Raspberry Pi.

In addition to the Elegoo kit with its various pieces of hardware and the Arduino Uno the Arduino IDE is
also required to compile programs and upload them to the Arduino. The Arduino IDE contains
additional and useful tools such as the Serial Monitor which provides a way to see error messages
and other printed output text.

The Arduino Uno is a true micro-controller, unlike the Raspberry Pi 3 which is a full featured computer
that runs a Linux distro. The Arduino has a different memory organization with a 32KB flash RAM and a
much smaller 2KB dynamic RAM for variables unlike the Raspberry Pi 3 which has 1GB of dynamic RAM.

The Arduino Uno does not have an operating system such as Linux though there are frameworks from
places which provide the rudiments for multi-threading. The basic program structure is a user
application that provides an entry point or starting point function which is called by the
Arduino runtime periodically. The user application function is called by the runtime, it
performs its task, and then returns and then the runtime calls the user function again
in a loop.
