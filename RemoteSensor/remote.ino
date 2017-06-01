/*
  Remote Temerature Station
  Copyright (C) 2015 by Jason K Jackson jake1164 at hotmail.com
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <SPI.h>
#include <LowPower.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP085_U.h>
#include <DHT_U.h>
#include <DHT.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <RF24_config.h>
#include "header.h"
#include <MemoryFree.h>
#define DHTPIN 4  
#define DHTTYPE DHT22
#define CE_PIN 9
#define CSN_PIN 10
#define PHOTOCELL_PIN 0
#define RAW_V_PIN A1
#define CHARGE_PIN 5
#define DONE_PIN 6
#define DEBUG

/* Sleeping via the 8s watchdog (not accurate, may drift)
   Popular cycle to min conversion:
   7cycles = 1min, 15 = 2min, 30 = 4min
*/
#define PROGRAM_DELAY 3 // number of cycles to delay sleeping to aid in programming.
#define MEASURE_EVERY 10
#define SEND_EVERY 7
#define WARMUP_DELAY 2000 // How long to wait before reading temp from DH22

bool sleep_ready;
// single pipe used to transmit only.
//const uint64_t pipe = 0xE8E8F0F0E1LL;
const uint64_t pipes[2] = { 0xF0F0F0F0E1LL, 0xF0F0F0F0D1LL };

RF24 radio(CE_PIN, CSN_PIN);
unsigned int interval = 0;
unsigned int measures = 0;

struct measure dht22_temperatures = { 0, 0, 0, 0 };
struct measure dht22_humidities = { 0, 0, 0, 0 };
struct measure bmp180_temperatures = { 0, 0, 0, 0 };
struct measure bmp180_pressures = { 0, 0, 0, 0 };
struct measure promini_voltages = { 0, 0, 0, 0 };
struct measure light_levels = { 0, 0, 0, 0 };

DHT_Unified dht22(DHTPIN, DHTTYPE);
Adafruit_BMP085_Unified bmp180 = Adafruit_BMP085_Unified(10085);

bool bmp_ready;

int serial_putc( char c, FILE *) {
  Serial.write(c);
  return c;
}

void printf_begin(void){
  fdevopen( &serial_putc, 0);
}

void blink(int times) {
  int i = 0;
  for(i;i < times; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);    
  }  
}


/*
 * dewPoint
 * Calculates the dew point
 * delta max - 0.6544 wrt dewPoint()
 * reference http://en.wikipedia.org/wiki/dewpoint
 *
 * @param temperature float temperature in celsius
 * @param humidity float humidity in %
 * @return float
 */
float dewPoint(float temperature, float humidity) {
  float a = 12.271;
  float b = 237.7; 
  float temp = (a * temperature) / (b + temperature) + log(humidity / 100);
  float Td = (b * temp) / (a - temp);
  return Td;
}

float fahrenheit(double celsius){
  return 1.8 * celsius + 32; 
}

/*
 * record
 * Records the value into a measure structure
 *
 * @param measure reading
 * @param float value
 * @return void
 */
float record(measure &reading, float value) {
  reading.sum = reading.sum + value;
  reading.count = reading.count + 1; 
  Serial.print("reading sum / cnt: "); Serial.print(reading.sum); Serial.print(" / "); Serial.println(reading.count);
  if (reading.count == 1) {
    reading.minimum = value;
    reading.maximum = value;
  } else {
    reading.minimum = min(reading.minimum, value);
    reading.maximum = max(reading.maximum, value);
  }
}

/*
 * calculateAverage
 * Calculate the average of all the stored values in the passed in measure structure
 *
 * @param measure reading
 * @return float
 */
float calculateAverage(measure &reading) {
  if (reading.count > 1){
    return (reading.sum / reading.count);
  } else {
    return reading.sum;
  }
}

/*
 * reset
 * Resets all values
 *
 * @return void
 */
void reset(measure &reading) {
  reading.count = 0;
  reading.sum = 0;
}

/*
 * resetAll
 * Resets all averages
 *
 * @return void
 */
void resetAll() {
  reset(dht22_humidities);
  reset(dht22_temperatures);
  reset(dht22_humidities);
  reset(bmp180_temperatures);
  reset(bmp180_pressures);
  reset(promini_voltages);
  reset(light_levels);
}

/*
 * readAll
 * Reads all the values and stores them into a measurement structure.
 *
 * @return void
 */
void readAll() {
  // DHT22 needs to settle
  delay(WARMUP_DELAY);
  
  sensors_event_t event;
  dht22.temperature().getEvent(&event);
  if(!isnan(event.temperature)) {    
    Serial.print("Temperature Reading: "); Serial.println(event.temperature);
    record(dht22_temperatures, event.temperature);
  } 
  
  Serial.println();
  dht22.humidity().getEvent(&event);
  if(!isnan(event.relative_humidity)){
    Serial.print("Humidity Reading: "); Serial.println(event.relative_humidity);    
    record(dht22_humidities, event.relative_humidity);
  }
  
  bmp180.getEvent(&event);
  Serial.println();
  if(event.pressure){
    Serial.print("BP Reading: "); Serial.println(event.pressure);
    // To convert to inHg mulitply by 0.02953337    
    record(bmp180_pressures, event.pressure * 0.02953337); 
  

    float temperature;
    bmp180.getTemperature(&temperature);
    record(bmp180_temperatures, temperature);
    Serial.print("BMP180 Temperature Reading: "); Serial.println(temperature);
  }  

  Serial.println();
  //float voltage = readVoltage();
  int sensorValue = analogRead(RAW_V_PIN);
  float voltage = sensorValue * (3.3 / 1023) * 2;
  record(promini_voltages, voltage);
  Serial.print("Voltage Reading: ");
  Serial.println(voltage);
  
  Serial.println();
  int photocell = analogRead(PHOTOCELL_PIN);
  record(light_levels, photocell);
  Serial.print("Photocell Sensor Reading: "); Serial.println(photocell);
}

/*
 * sendAll
 * Gets averages and sends all data through UART link
 *
 * @return void
 */
void sendAll() {
  txMeasureStruct measurements;
  measurements.header='*';
  measurements.temperature_dht = fahrenheit(calculateAverage(dht22_temperatures));
  measurements.temperature_bmp = fahrenheit(calculateAverage(bmp180_temperatures));
  measurements.humidity = calculateAverage(dht22_humidities);
  measurements.pressure = calculateAverage(bmp180_pressures);
  measurements.voltage = calculateAverage(promini_voltages);
  measurements.lightlevel = calculateAverage(light_levels);
  measurements.dewpoint = fahrenheit(dewPoint(measurements.temperature_dht, measurements.humidity));
  measurements.done = !digitalRead(DONE_PIN);
  measurements.charge = !digitalRead(CHARGE_PIN);
  if(measurements.done == true && measurements.charge == true){
    measurements.done = false;
    measurements.charge = false;
  }
  Serial.println("Calculated Averages: ");
  Serial.print("avg dht: "); Serial.println(measurements.temperature_dht);
  Serial.print("avg bmp: "); Serial.println(measurements.temperature_bmp);
  Serial.print("avg hum: "); Serial.println(measurements.humidity);
  Serial.print("avg pres: "); Serial.println(measurements.pressure);
  Serial.print("avg volt: "); Serial.println(measurements.voltage);
  Serial.print("avg light: "); Serial.println(measurements.lightlevel);
  Serial.print("charge: "); Serial.println(measurements.charge);
  Serial.print("done: "); Serial.println(measurements.done);
  radio.stopListening();
  Serial.println("Sending measurements over RF24 device");
  // transmit the structure
  Serial.println(sizeof(txMeasureStruct));
  bool ok = radio.write(&measurements, sizeof(txMeasureStruct));  
  //bool ok = radio.writeAckPayload(1, &measurements, sizeof(txMeasurementStruct));
  Serial.println();
  if(ok)    
    Serial.println(">>>> send PASS");
  else
    Serial.println(">>>> send FAIL");
  Serial.println();
  blink(1);    
  // power down the radio to save power.
  // Note: radio.write wakes the radio up.
  //radio.powerDown();
}

void displaySensorDetails(void)
{
  sensor_t sensor;
  bmp180.getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" hPa");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" hPa");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" hPa");  
  Serial.println("------------------------------------");
  Serial.println("");
  delay(500);
}


/*
 * setup
 *
 * @return void
 */
void setup() {
  Serial.begin(9600);
  printf_begin();
  Serial.println("Starting up Remote Weather Station");
  pinMode(DONE_PIN, INPUT);
  pinMode(CHARGE_PIN, INPUT);
  
  dht22.begin();
  
  // Setup the rf24 for TX.
  radio.begin();    
  //radio.setChannel(0x4c);
  radio.setChannel(70);
  radio.setAutoAck(0);
  radio.setRetries(15,15);
  radio.setPALevel(RF24_PA_LOW);
  //radio.setDataRate(RF24_250KBPS);
  radio.enableDynamicPayloads();
  radio.openWritingPipe(pipes[1]);
  radio.openReadingPipe(1, pipes[0]);
  radio.startListening();
  radio.stopListening();
  radio.printDetails();

  
  // check if the BMP180 is readable
  bmp_ready = (bool)bmp180.begin();  
  sleep_ready = false;
  pinMode(PHOTOCELL_PIN, INPUT);
  displaySensorDetails();
  blink(5);
}

/*
 * loop
 *
 * @return void
 */
void loop() {
  
  if(sleep_ready){
    // Power down for 8 seconds after the programming delay has been satisfied.
    Serial.println("Sleeping for 8 seconds");
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);  
  }
  
  interval++;
  // Outer loop will check if we need to take a measurement
  if(interval % MEASURE_EVERY == 0){
    Serial.println("\nTaking measurements");
    readAll(); // Take all measurements updating the measure structs 

    interval = 0; // reset the interval
    measures++;
    // How many measurements to take before we send
    if(measures % SEND_EVERY == 0){
      Serial.println("Sending measurements via RF24");
      sendAll();
      resetAll();
      measures = 0;
    }

    // once satisfied we will start the sleep routine.
    if(!sleep_ready && measures == PROGRAM_DELAY) {
      Serial.println("Turning sleep mode on");
      //sleep_ready = true;
    }
    Serial.print("freeMemory: "); Serial.println(freeMemory());
  }
}
