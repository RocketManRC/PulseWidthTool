#include <M5Stack.h>

#define SERIALDEBUG 0

const byte SLAVE_ADDRESS = 42;
  
const byte outPulse = 19; // pulse out for test
const byte inPulse = 5;   // pulse back in for test and interrupt
const byte interruptReceived = 17; // set to ackknowledge that interrupt received, reset in main loop

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;  
volatile uint32_t interruptCount = 0;					          // increment this for every PPS pulse

int fileOK = 1; // if we get a bad file write we will quit writing...

// 0 - Period (falling edge)
// 1 - Pulse Width (falling edge)
// 2 - Inverted Period (rising edge)
// 3 - Inverted Pulse Width (rising edge)
int measureMode = 0;
int pulseMode = 1; // 1 wire or 2 wire pulse mode

// Enters on rising edge
//=======================================
void IRAM_ATTR handleInterrupt()
{
  // NOTE if output pin changed have to edit the line below!
  //REG_WRITE( GPIO_OUT_W1TS_REG, BIT17 );  // Write a 1
  if( pulseMode == 2)
    REG_WRITE( GPIO_OUT_W1TC_REG, BIT17 );  // Write a 0

  portENTER_CRITICAL_ISR( &mux );

  interruptCount++;

  portEXIT_CRITICAL_ISR(&mux);
}

void setMode( int mode )
{
  measureMode = mode;

  M5.Lcd.setCursor( 0, 0 );
  M5.Lcd.setTextSize( 3 );
  M5.Lcd.fillScreen( GREEN );
  M5.Lcd.setTextColor( RED, GREEN );
  M5.Lcd.println();

  switch( mode )
  {
      case 0:
        M5.Lcd.println("     Period");
        M5.Lcd.println("   (Frequency)");
        break;
        
      case 1:
        M5.Lcd.println("   Pulse Width");
        break;
        
      case 2:
        M5.Lcd.println("     Period-");
        M5.Lcd.println("   (Frequency)");
        break;
        
      case 3:
        M5.Lcd.println("   Pulse Width-");
        break;

      default:
        break;       
  }

  M5.Lcd.setTextColor( PURPLE, ORANGE );
  //M5.lcd.setCursor( 220, 210 );
  M5.lcd.setCursor( 130, 210 );
  M5.Lcd.print("Mode");
  M5.lcd.setCursor( 20, 210 );
  if( pulseMode == 1 )
    M5.Lcd.print("1Wire");
  else
    M5.Lcd.print("2Wire");
  M5.Lcd.setTextColor( PURPLE, GREEN );

}

void setup()
{
#if SERIALDEBUG
  Serial.begin( 115200 );
#endif

  M5.begin();
  setMode( 1 );

  Wire.begin();
  Wire.setClock( 100000 );

  pinMode( interruptReceived, OUTPUT );
  digitalWrite( interruptReceived, 1 );
  pinMode( outPulse, OUTPUT );
  pinMode( inPulse, INPUT );

  attachInterrupt( digitalPinToInterrupt(inPulse), handleInterrupt, RISING ); // attaches pin to interrupt on Rising Edge
}

void saveData( String s )
{
  if( fileOK )
  {
    File file = SD.open( "/log.txt", FILE_APPEND );

    if(!file)
    {
      //stringToLCD("Failed to open file for writing!");
#if SERIALDEBUG
      Serial.println("Failed to open file for writing!");
#endif
      fileOK = 0; // no more write attempts after error
      return;
    }

    file.println( s );
  }
}

void test2( void )
{
  Wire.beginTransmission( 42 );

  Wire.write( measureMode ); // I2C register
 
  Wire.endTransmission();

  Wire.requestFrom( 42, 8 );    // request 8 bytes from slave device #8

  int i = 0;
  uint64_t v = 0;
  byte *p = (byte *)&v;


  while( Wire.available() )
  { // note that slave may send less than requested
    byte c = Wire.read(); // receive a byte as character

    if( i <= 7 )
      p[i++] = c;
#if SERIALDEBUG
    Serial.print( c, 16 );         // print the character
#endif
  }

  //double vv = v / 12000.0;
  //double vv = v / 96000.0 - 0.0017; // add or subtract GPS calibration (0.0017 ms/sec)
  double vv = v / 96000.0; // ms
  vv = vv - (0.0017 * vv / 1000.0);

#if SERIALDEBUG
  Serial.println();
  Serial.printf( "%lld\n", v / 12 );
  Serial.printf( "%f ms\n", vv );
#endif

  M5.lcd.setCursor( 0, 100 );
  if( vv < 1.0 )
  {
    M5.Lcd.printf( "     %.3f us      ", vv * 1000.0 );
    M5.lcd.setCursor( 0, 130 );
    if( measureMode == 0 || measureMode == 2 )
      M5.Lcd.printf( "     (%.3f kHz)      ", 1.0 / vv );
  }
  else
  {
    M5.Lcd.printf( "  %f ms  ", vv );
    M5.lcd.setCursor( 0, 130 );
    if( measureMode == 0 || measureMode == 2 )
      M5.Lcd.printf( "    (%.3f Hz)  ", 1000.0 / vv );
  }

  //String s = vv;
}

void loop()
{
  static uint8_t lastIn = 0;
  static unsigned long lastTime = millis();
  static unsigned int counter = 0;
 
  if( digitalRead( inPulse ) == 0 && lastIn == 1 )
  {
    test2();
    //Serial.println( "test2" );
  }

  lastIn = digitalRead( inPulse );

  //Serial.println( lastIn );

  if( millis() - lastTime >= 125 )
  {
    lastTime = millis();

    digitalWrite( outPulse, (counter++ & 3) > 0 );

#if SERIALDEBUG
    Serial.print( interruptCount );
    Serial.print( "  " );
    //Serial.println( interruptReceived );
#endif
  }

  static uint32_t lastInterruptCount = interruptCount;

  if( (interruptCount != lastInterruptCount) && !digitalRead( inPulse ) )
  {
    lastInterruptCount = interruptCount;

    if( pulseMode == 2)
      digitalWrite( interruptReceived, 1 );
  }

  M5.update();

  if( M5.BtnA.wasReleased() )
  {
    if( pulseMode == 1 )
      pulseMode = 2;
    else
    {
      pulseMode = 1; 
      digitalWrite( interruptReceived, 1 );
    }

    setMode( measureMode );   // need to call this to change display (not pretty I know)
  }

  if( M5.BtnC.wasReleased() )
  {
    switch( measureMode )
    {
      case 0:
        setMode( 2 );
        break;

      case 1:
        setMode( 3 );
        break;

      case 2:
        setMode( 1 );
        break;

      case 3:
        setMode( 0 );
        break;

      default:
        setMode( 0 );
        break;
    }
  }
}
