## Read a DHT11 Temperature Sensor

Using a DHT11 humidity and temperature sensor mounted on a micro-PCB which came with my Raspberry Pi Starter Kit from Osoyoo,
I wanted to test using the sensor with an Arduino.

I used a breadboard hat plugged into the Arduino Uno and inserted the DHT11 sensor into the breadboard. The sensor
has three pins: GND, Data, and VCC. I connected GND and VCC to the ground and 5v pins and then connected Data to
digital pin 12.

I then reused a function I wrote for the DHT11 communications for the Raspberry Pi in an Arduino sketch with the
Arduino IDE.

My environment is a Dell Windows 10 laptop that has the Arduino IDE installed. I connect to the Arduino using a
USB A to B cable that came with my Arduino experiment kit. In my case I can use the Serial Monitor with the
test application.

The DHT11 humidity and temperature sensor uses a very simple variation of
the [1 Wire communication protocol](https://en.wikipedia.org/wiki/1-Wire). The protocol for the DHT11 sensor
uses the width of digital pulses, how long a pin is held LOW with the normal state being HIGH, to send bits as electrical
pulses through a connecting wire. The data the sensor provides is a set of two byte values for the humidity and
another two byte values for the temperature in Celsius. The first of the two byte values is the most significant
part of the value, the value to the left of the decimal point. The second of the two byte values is the least
significant part of the value, the value to the right of the decimal point.

We print out the data by printing the first byte of the pair followed by a text character of a decimal point
followed by the seconnd byte of the pair.

## Notes on the experiment

I first did this experiment with a Rasberry Pi 3 running Raspbian. The core function used to communicate
with the DHT11 sensor to trigger the sensor to send data and reading the data is the same function for
the Arduino as for the Raspberry Pi.

However there is a threhold argument to the function which indicates how long is a pulse width for a binary
one. While a pulse width threshold of 30 was needed for the Raspberry Pi, for the Arduino a pulse width
threshold of 5 provides good data.

The Arduino communication also appears much more reliable probably due to the Raspberry Pi with Raspbian
has some variability in the timers used for the pulse width counting.
