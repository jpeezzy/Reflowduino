/*
 * Title: Reflowduino PID Test
 * Author: Timothy Woo
 * Website: www.botletics.com
 * Last modified: 10/25/2017
 * 
 * -----------------------------------------------------------------------------------------------
 * This is an example sketch to test the PID control of the Reflowduino and facilitate choosing the
 * right PID constants. This example sets the desired temperature to a fixed value and controls the
 * oven or hot plate accordingly to match that temperature. See the temperature response and adjust
 * the PID gains accordingly by evaluating parameters such as reponse time, overshoot, and steady-
 * state error which are explained in more detail on my Github tutorial.
 * 
 * Order a Reflowduino at https://www.botletics.com/products/reflowduino
 * Full documentation and design resources at https://github.com/botletics/Reflowdiuno
 * 
 * -----------------------------------------------------------------------------------------------
 * Credits: Special thanks to Brett Beauregard, author of the Arduino PID Library!
 * 
 * -----------------------------------------------------------------------------------------------
 * License: This code is released under the GNU General Public License v3.0
 * https://choosealicense.com/licenses/gpl-3.0/ and appropriate attribution must be
 * included in all redistributions of this code.
 * 
 * -----------------------------------------------------------------------------------------------
 * Disclaimer: Dealing with mains voltages is dangerous and potentially life-threatening!
 * If you do not have adequate experience working with high voltages, please consult someone
 * with experience or avoid this project altogether. We shall not be liable of any damage that
 * might occur form the use of the Reflowduino, and all actions are taken at your own risk.
 */

#include <SoftwareSerial.h> // Library needed for Bluetooth communication
#include <Keyboard.h> // Only if you need the ATmega32u4 to act as a keyboard

// Libraries needed for using MAX31855 thermocouple interface
#include <SPI.h>
#include "Adafruit_MAX31855.h" // https://github.com/adafruit/Adafruit-MAX31855-library

// Library for PID control
#include <PID_v1.h> // https://github.com/br3ttb/Arduino-PID-Library

// Define pins
#define relay 7
#define BT_RX 9
#define BT_TX 10
#define MAX_CS 8 // MAX31855 chip select pin

// Initialize Bluetooth software serial
SoftwareSerial BT = SoftwareSerial(BT_TX,BT_RX); // Reflowduino (RX, TX), Bluetooth (TX, RX)

// Initialize thermocouple with hardware SPI
// Reflowduino uses hardware SPI to save digital pins
Adafruit_MAX31855 thermocouple(MAX_CS);

// Define if you want to enable the keyboard feature to type data into Excel
#define enableKeyboard false

// Define a desired temperature in deg C
#define desiredTemp 28

// Define PID parameters
#define PID_sampleTime 1000
#define Kp_preheat 50
#define Ki_preheat 0.05
#define Kd_preheat 20

// Bluetooth app settings. Define which characters belong to which functions
#define dataChar '*' // App is receiving data from Reflowduino
#define stopChar '!' // App is receiving command to stop reflow process (process finished!)
#define startReflow 'A' // Command from app to "activate" reflow process
#define stopReflow 'S' // Command from app to "stop" reflow process at any time

double temperature, output, setPoint; // Input, output, set point
PID myPID(&temperature, &output, &setPoint, Kp_preheat, Ki_preheat, Kd_preheat, DIRECT);

// Logic flags
bool justStarted = true;
bool reflow = false; // Baking process is underway!
bool preheatComplete = false;
bool soakComplete = false;
bool reflowComplete = false;
bool coolComplete = false;

double T_start; // Starting temperature before reflow process
int windowSize = 2000;
unsigned long sendRate = 2000; // Send data to app every 2s
unsigned long previousMillis = 0;
unsigned long windowStartTime, timer;

void setup() {
  Serial.begin(9600); // This should be different from the Bluetooth baud rate
  BT.begin(57600);

  pinMode(relay, OUTPUT);
  digitalWrite(relay, LOW); // Set default relay state to OFF

  setPoint = desiredTemp;

  myPID.SetOutputLimits(0, windowSize);
  myPID.SetSampleTime(PID_sampleTime);
  myPID.SetMode(AUTOMATIC); // Turn on PID control

  while (!Serial) delay(1); // OPTIONAL: Wait for serial to connect
  Serial.println("*****Reflowduino PID Test*****");

  if (enableKeyboard) Keyboard.begin(); // Only if you want to type data into Excel
}

void loop() {
  /***************************** MEASURE TEMPERATURE *****************************/
  temperature = thermocouple.readCelsius(); // Read temperature
  
  /***************************** REFLOW PROCESS CODE *****************************/
  if (reflow) {
    temperature = thermocouple.readCelsius(); // Read temperature
//  temperature = thermocouple.readFarenheit(); // Alternatively, read in deg F

    // This only runs when you first start the test
    if (justStarted) {
      justStarted = false;
      windowStartTime = millis();
      
      if (isnan(T_start)) {
       Serial.println("Invalid reading, check thermocouple!");
      }
      else {
       Serial.print("Starting temperature: ");
       Serial.print(T_start);
       Serial.println(" *C");
      }
    }
  
    // Compute PID output (from 0 to windowSize) and control relay accordingly
    myPID.Compute(); // This will only be evaluated at the PID sampling rate
    if (millis() - windowStartTime >= windowSize) windowStartTime += windowSize; // Shift the time window
    if (output > millis() - windowStartTime) digitalWrite(relay, HIGH);
    else digitalWrite(relay, LOW);
  }

  /***************************** BLUETOOTH CODE *****************************/
  BT.flush();
  char request = ' ';

  // Send data to the app periodically
  if (millis() - previousMillis > sendRate) {
    previousMillis = millis();
    Serial.print("--> Temperature: "); // The right arrow means it's sending data out
    Serial.print(temperature);
    Serial.println(" *C");
    if (!isnan(temperature)) { // Only send the temperature values if they're legit
      BT.print(dataChar); // This tells the app that it's data
      BT.print(String(temperature)); // Need to cast to String for the app to receive it properly

      if (enableKeyboard && reflow) {
        // Type time and temperature data into Excel on separate columns!
        Keyboard.print((millis()-timer)/1000); // Convert elapsed time from ms to s
        Keyboard.print('\t'); // Tab to go to next column
        Keyboard.print(temperature);
        Keyboard.print('\t');
        Keyboard.print(digitalRead(relay)); // Record the relay state as well!
        Keyboard.println('\n'); // Jump to new row
      }
    }
  }
  
  // Check for an incoming command. If nothing was sent, return to loop()
  if (BT.available() < 1) return;

  request = BT.read();  // Read request
//  Serial.print("REQUEST: "); Serial.println(request); // DEBUG

  if (request == startReflow) { // Command from app to start reflow process
    justStarted = true;
    reflow = true; // Reflow started!
    timer = millis(); // Timer for logging data points
    Serial.println("<-- ***Reflow process started!"); // Left arrow means it received a command
  }
  else if (request == stopReflow) { // Command to stop reflow process
    digitalWrite(relay, LOW); // Turn off appliance and set flag to stop PID control
    reflow = false;
    Serial.println("<-- ***Reflow process aborted!");
  }
  // Add you own functions here and have fun with it!
}
