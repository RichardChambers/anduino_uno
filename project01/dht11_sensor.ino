/*
 * dht11 temperature sensor
 *   Wire a DHT11 temperature sensor, three pin micro-PCB package, with
 *   5v, GND, and data to a digital pin
 */

#include <stdint.h>

// uncomment the following define to enable a basic debug output which
// will display the statistics for the reading from the DHT11 in order to
// check if any pulses are being detected by the pin and what is the minimum
// and maximum pulse width.
// Testing with an Arduino Uno we have seen the following:
//    minimum pulse width count -> 2
//    maximum pulse widht count -> 15
// With these statistics, threshold for function onewireReadDHT11() set
// to 5 works great.
//#define CHECK_STATS 1

const int Dht11Pin = 12;       // digital pin on Arduino connected to DHT11 data pin
const int Dht11Threshold = 5;  // onewireReadDHT11() threshold value

// struct for DHT11 communications to allow access to various parameters
// such as number of bytes read, minimum count and maximum count to
// allow fine tuning of the function onewireReadDHT11() and setting the
// threshold for a pulse width for a binary one versus a binary zero.
typedef struct {
  int  iByteCount;
  int  iMinWait;
  int  iMaxWait;
} onewireStats;


int onewireResetWire (int iPin)
{
    pinMode (iPin, OUTPUT);
    digitalWrite (iPin, LOW);
    delay (18);

    digitalWrite (iPin, HIGH);
    delayMicroseconds (40);

    pinMode (iPin, INPUT);
    int iSlaveResponse = digitalRead (iPin);
    delayMicroseconds (410);

    return iSlaveResponse;
}

void  onewireSetup (int iPin)
{
    pinMode (iPin, INPUT_PULLUP);
}

//   DHT11 sensor specific One Wire Protocol functions follow

onewireStats onewireReadDHT11 (int iPin, int iThreshold, unsigned char bitArray[], int iLen)
{
    onewireStats  myStats = {0};
    uint8_t laststate = HIGH;
    uint8_t counter = 0;

    memset (bitArray, 0, iLen);

    //pull pin down to send start signal
    pinMode( iPin, OUTPUT );
    digitalWrite( iPin, LOW );
    delay( 18 );

    //pull pin up and wait for sensor response
    digitalWrite( iPin, HIGH );
    delayMicroseconds( 40 );
    //prepare to read the pin
    pinMode( iPin, INPUT );

    int iCountMax = 0;
    int iCountMin = 100;

    //detect change and read data
    uint8_t j = 0, i = 0;
    do {
        counter = 0;
        while ( digitalRead( iPin ) == laststate )
        {
            counter++;
            delayMicroseconds( 1 );
            if ( counter == 255 )
            {
                break;
            }
        }
        laststate = digitalRead( iPin );

        if ( counter == 255 )
            break;

        //ignore first 3 transitions
        if ( (i >= 4) && (i % 2 == 0) )
        {
            int iIndex = j >> 3;

            if (iIndex >= iLen) break;

            //shove each bit into the storage bytes
            bitArray[iIndex] <<= 1;
            if ( counter > iThreshold )
                bitArray[iIndex] |= 1;
            if (counter > iCountMax) iCountMax = counter;
            if (counter < iCountMin) iCountMin = counter;
            j++;
        }
        i++;
    } while (1);

    myStats.iByteCount = j;
    myStats.iMinWait = iCountMin;
    myStats.iMaxWait = iCountMax;
    return myStats;
}

int GoodCheckSumDht11 (unsigned char dht11[])
{
  return (dht11[4] == (dht11[0] + dht11[1] + dht11[2] + dht11[3]) & 0xff);
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);

  Serial.println ("dht11_sensor: begin.");
  
  onewireSetup (12);
}

void loop() {
  static  int  iLoopCount = 0;  // count loops in order to slow down pinging of DHT11
  
  // put your main code here, to run repeatedly:
  unsigned char  dht11_dat[8];

  iLoopCount++;

  if (iLoopCount % 100000 == 0) {
    onewireStats  myStats = {0};
    
    myStats = onewireReadDHT11 (Dht11Pin, Dht11Threshold, dht11_dat, 8);

#if defined(CHECK_STATS)
    Serial.print (" - stats iByteCount = ");
    Serial.print (myStats.iByteCount);
    Serial.print ("  iMinWait ");
    Serial.print (myStats.iMinWait);
    Serial.print (", iMaxWait ");
    Serial.println (myStats.iMaxWait);
#endif

    if (myStats.iByteCount >= 40 && GoodCheckSumDht11 (dht11_dat)) {
      float f = dht11_dat[2] * 9.0 / 5.0 + 32.0;  // convert temp to fahnrenheit
      
      Serial.print ("Humidity read ");
      Serial.print (dht11_dat[0]);
      Serial.print (".");
      Serial.print (dht11_dat[1]);
      Serial.print ("%  Temp read ");
      Serial.print (dht11_dat[2]);
      Serial.print (".");
      Serial.print (dht11_dat[3]);
      Serial.print ("C (");
      Serial.print (f);
      Serial.println ("F)");
    } else {
      Serial.print ("bad data  bytecount = ");
      Serial.print (myStats.iByteCount);
      Serial.print (", min = ");
      Serial.print (myStats.iMinWait);
      Serial.print (", max = ");
      Serial.println (myStats.iMaxWait);
    }
  }
}
