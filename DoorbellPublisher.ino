/* System wide includes */
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "SSD1306Wire.h"

/* Local includes */
#include "images.h"
#include "config.h"

/* Function prototypes */
void setup_wifi(void);
void setup_mqtt(void);
void setup_offset_voltage(void);
void drawBrokenLink(void);
float countsToMillivolts(int nCounts);

/* Global instances */
SSD1306Wire display(0x3c, SDA, SCL);
WiFiClient espClient;
PubSubClient client(espClient);
IPAddress serverIp;

/* Other global variables */
volatile uint8_t g_nTimeout = TIMEOUT_RESET_VAL;
volatile float g_fSensorOffsetVoltage = 0.0f;

void setup()
{
    Serial.begin(SERIAL_BAUDRATE);
    Serial.println();

    display.init();
    display.flipScreenVertically();

    setup_wifi();
    setup_mqtt();

    /* Offset voltage measurement at the end due to powerbank issues */
    delay(2000);
    setup_offset_voltage();  
}

void setup_offset_voltage(void)
{
    int nConversionValue = 0;
    float fMilliVolts = 0.0f;
    float fMilliVoltsAccu = 0.0f;
    float fMilliVoltsAvg = 0.0f;
    const uint16_t nNumSamples = 4096;
    char sDisplayText[32];

    for( uint16_t k = 0; k < nNumSamples; k++ )
    {
        nConversionValue = analogRead(ANALOG_INPUT_PIN);
        fMilliVolts = countsToMillivolts(nConversionValue);
        fMilliVoltsAccu += fMilliVolts;
    }
    fMilliVoltsAvg = fMilliVoltsAccu/nNumSamples;
    snprintf(sDisplayText, 31, "%.2f", fMilliVoltsAvg);

    g_fSensorOffsetVoltage = fMilliVoltsAvg;

    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 12, sDisplayText);
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 34, "mV average.");
    display.display();

    delay(3000);
}

void mqtt_reconnect()
{
    bool bBroken = false;

    // Loop until we're reconnected
    while( !client.connected() )
    {
        Serial.print("Attempting MQTT connection...");

        // Attempt to (re-)connect
        if( client.connect("DoorbellPublisher", "", "") )
        {
            Serial.println("connected");
        }
        else
        {
            bBroken = true;
            display.clear();
            drawBrokenLink();
            display.display();

            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" trying again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }

    if( bBroken )
    {
        /* connection was broken (could not directly be re-established; should remove the broken link symbol */
        display.clear();
        display.display();
    }
}

void setup_wifi()
{
    delay(10);

    display.clear();
    drawWifiDisconnected();
    display.display();
    delay(2000);

    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to WiFi with SSID: \"");
    Serial.print(WIFI_SSID);
    Serial.println("\"");
  
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
  
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 20, "WiFi connected!");
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 40, WiFi.localIP().toString() );
    display.display();

    delay(3000);
}

void setup_mqtt(void)
{
    client.setServer(MQTT_SERVER_IP, MQTT_PORT);

    if (!client.connected())
    {
        mqtt_reconnect();
    }
}

void drawBell(bool bOnOff)
{
    // see http://blog.squix.org/2015/05/esp8266-nodemcu-how-to-create-xbm.html
    // on how to create xbm files

    if( bOnOff )
    {
        display.drawXbm(34, 2, doorbell_width, doorbell_height, doorbell_bits);
    }
    else
    {
        display.drawXbm(34, 2, doorbell_noring_width, doorbell_noring_height, doorbell_noring_bits);
    }
}

void drawBrokenLink(void)
{
    display.drawXbm(34, 2, broken_link_width, broken_link_height, broken_link_bits);
}

void drawWifiDisconnected(void)
{
    display.drawXbm(34, 2, wifi_disconnected_width, wifi_disconnected_height, wifi_disconnected_bits);
}

void drawHeart(void)
{
    display.drawXbm(118, 2, heart_width, heart_height, heart_bits);
}

void loop()
{
    static uint8_t nProgressBarVal = 100;
    static int nConversionValue = 0;
    static bool bDetected = false;
    static bool bDetectedOld = false;
    static unsigned int nCommTrigger = 0;

    if (!client.connected())
    {
        mqtt_reconnect();
    }
    client.loop();

    nConversionValue = analogRead(ANALOG_INPUT_PIN);
    bDetected = detectDoorbell( countsToMillivolts(nConversionValue) );

    if( bDetectedOld != bDetected )
    {
        if( bDetected ) /* detect rising edge */
        {
            //display.drawString(64, 20, "Ding-dong start!");
            client.publish(MQTT_TOPIC_PROD, MQTT_MSG_DOORBELL);
            
            for(int k=0; k<10; k++)
            {
                display.clear();
                drawBell(true);
                display.display();
                //setNeopixelColor(NEOPIXEL_COLOR_BLUE);
                delay(1000);
              
                display.clear();
                drawBell(false);
                display.display();
                //setNeopixelColor(NEOPIXEL_COLOR_BLACK);
                delay(1000);
            }

            // force next heartbeat immediately after
            g_nTimeout = 1;
        }
    }
    bDetectedOld = bDetected;

    if( 0 == nCommTrigger )
    {
        /* only decrement when timeout has not already occured, else uint underflow */
        if( g_nTimeout > 0 )
        {
            g_nTimeout--;
            Serial.print("Time until next heartbeat: ");
            Serial.println(g_nTimeout);
    
            /* detect transition: timeout reached */
            if( 0 == g_nTimeout )
            {
                Serial.println("Timed out: sending heartbeat!");
    
                client.publish(MQTT_TOPIC_CONNECTION, MQTT_MSG_HEARTBEAT);
                Serial.println("Sent heartbeat.");
    
                /* auto-reset timeout */
                g_nTimeout = TIMEOUT_RESET_VAL;
            }
        }

        Serial.println("Update progress bar.");
        nProgressBarVal = g_nTimeout*100/TIMEOUT_RESET_VAL;
        display.clear();
        display.drawProgressBar(1, 1, 110, 6, nProgressBarVal);
        display.display();
    }
    
    nCommTrigger++;
    if( nCommTrigger >= 1000/SENSOR_LOOP_DELAY )
    {
        nCommTrigger = 0;
    }

    delay(SENSOR_LOOP_DELAY);
}

float countsToMillivolts(int nCounts)
{
    // For the Arduino Uno, its a 10 bit unsgined value (i.e. a range of 0..1023).
    // Its measurement range is from 0..5 volts.
    // This yields a resolution between readings of: 5 volts / 1024 units or approx. 4.9 mV per LSB
    return nCounts * 4.882813f;
}

bool detectDoorbell(float fVoltage)
{
    const float fThreshold = SENSOR_THRESHOLD;
    static float fSignal;
    static int nCnt = 0;
    static int nCntDown = 0;
    static bool bDetected = false;
    static bool bDetectedOld = false;

    fSignal = fVoltage - g_fSensorOffsetVoltage;

    /* prepare triggering */
    if( abs(fSignal) > fThreshold )
    {
        nCnt++; // increase likeliness of occurence
    }
    else
    {
        nCnt--; // decrease likeliness of occurence, but never fall below zero
        if( nCnt < 0 )
        {
            nCnt = 0;
        }
    }

    if( nCnt > SENSOR_CNT_LIMIT )
    {
        // detected
        bDetected = true;

        // keep output true for specific time
        nCntDown = SENSOR_CNT_DOWN;
    }

    if( nCntDown > 0 )
    {
        nCntDown--;
        if( nCntDown == 0 )
        {
            // no longer keep output true
            bDetected = false;
        }
    }

#if DEBUG
//    Serial.print("V: ");
//    Serial.print( fVoltage );
//    Serial.print(", ");
    Serial.print("|Sig|: ");
    Serial.print( abs(fSignal) );
    Serial.print(", Count: ");
    Serial.print( nCnt );
    Serial.print(", Out: ");
    Serial.print( bDetected ? 300 : 0 ); /* use scale factor of 300 for display alignment in Serial Monitor */
    Serial.println("");
#endif

    return bDetected;
}

