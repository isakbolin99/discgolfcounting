/*
Project Name: discgolfcounting
Description: main code to upload to the microcontroller (arduino pro mini) controlling the system.
Author: Isak Bolin
Date: December 8, 2021

This sketch controls the footfall counter by reading data from the ultrasonic sensor.
Every hour it controls the NB-IoT module to send data to the cloud-based SQL-database.
*/

//Include libraries and declare variables

//Interrupts
//The PIR sensor is used to wake up the microcontroller from sleep mode to save power.
#include "LowPower.h"
#define PIRPin 2
#define alarmPin 3
bool PIR = false;

//Time
#include <TimeLib.h>
#include <DS3232RTC.h>
int timme; //hour in swedish

//JSN-sr04t (Ultrasonic sensor)
#define jsnPowerPin 4
int trigPin = A3;
int echoPin = A2;
long duration;
int distance;
int groupCount = 0;
int count = 0;
int count1 = 0;
int count2 = 0;
int count3 = 0;
int count4 = 0;
int count5 = 0;
int count6 = 0;
int totCount = 0;
int maxDistance = 240;
int breakDistance = 210;
int upCounter1Value = 100; //50ms delay in countingloop which will give a time of up to 20s between two people in one group.
int upCounter2Value = 15; //0.75s hold on reading after one person walks by.
int upCounter1;
int upCounter2;

//SIM7020E (NB-IoT module)
#include <String.h>
#include <SoftwareSerial.h>
SoftwareSerial gprsSerial(8,9);
#define PWRPin 7
int simPowerPin = A1;

//Temperature (DS18B20)
#include <OneWire.h>
#include <DallasTemperature.h>
#define tempDataPin 5
#define tempPowerPin 6
float tempValue;
float temp = 0;
float sendTemp = 0;
OneWire oneWire(tempDataPin);  
DallasTemperature sensors(&oneWire);


//Battery voltage
#define batteryPowerPin 10
int batteryDataPin = A0;
float voltageValue = 0;
float batteryVoltage = 0;
float arduinoVoltage = 335;
int sendVoltage = 0;


void setup() 
{
  sensors.begin();  //Startup Dallas library
  Serial.begin(9600);
  gprsSerial.begin(9600); //For SIM7020E communication
  
  //Configure pins
  pinMode(PIRPin, INPUT);
  pinMode(alarmPin, INPUT);
  pinMode(tempPowerPin, OUTPUT);
  pinMode(batteryPowerPin, OUTPUT);
  pinMode(PWRPin, OUTPUT);
  pinMode(simPowerPin, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(jsnPowerPin, OUTPUT);

  digitalWrite(jsnPowerPin, LOW);
  digitalWrite(simPowerPin, LOW);
  digitalWrite(tempPowerPin, LOW);
  digitalWrite(batteryPowerPin, LOW);

  //Uncomment of you want to configure the time
   /*
    tmElements_t tm;
   // the next few lines set the clock to the correct hour, minute, and second.  Remember 24 hour format so 4pm = hour 16 
    tm.Hour = 12;               
    tm.Minute = 59;
    tm.Second = 00;
   
   // set the correct date on the RTC
    tm.Day = 3;
    tm.Month = 5;
    tm.Year = 2022-1970; // in order to set the year correctly, just change the 2019 and leave the “- 1970” to get the correct offset
    
    RTC.write(tm);    // write the date and time to the RTC
   */   

  //For setting up alarms
  RTC.setAlarm(ALM1_MATCH_DATE, 0, 0, 0, 1);
  RTC.setAlarm(ALM2_MATCH_DATE, 0, 0, 0, 1);
  RTC.alarm(ALARM_1);
  RTC.alarm(ALARM_2);
  RTC.alarmInterrupt(ALARM_1, false);
  RTC.alarmInterrupt(ALARM_2, false);
  RTC.squareWave(SQWAVE_NONE);

  time_t t;
  t = RTC.get(); 
  timme = hour(t);
  RTC.setAlarm(ALM1_MATCH_MINUTES , 0, 0, 0, 0); // Setting alarm 1 to go off in the amount of minutes that we have the time interval constant set to 
  RTC.alarm(ALARM_1); // clear the alarm flag
  RTC.squareWave(SQWAVE_NONE); // configure the INT/SQW pin for "interrupt" operation (disable square wave output)
  RTC.alarmInterrupt(ALARM_1, true); // enable interrupt output for Alarm 1
  attachInterrupt(0, wakeUp0, RISING);
}

//The main loop just wait for an interrupt triggered from either the RTC module or the PIR sensor which means that a person is close by
void loop() 
{
  interrupt();
}


/*
  Function: interrupt()
  Description: Function that sets the alarm on the RTC and interrupt from the PIR sensor.
  An interrupt can either be triggered on a regular base by the RTC or when a person is
  nearby by a person.
*/
void interrupt()
{
  RTC.setAlarm(ALM1_MATCH_MINUTES, 0, 0, 0, 0);
  // clear the alarm flags
  RTC.alarm(ALARM_1);
  attachInterrupt(0, wakeUp0, RISING);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  detachInterrupt(0);
  detachInterrupt(1);

  //Start to count people if the PIR sensor senses a person
  if (PIR == true)
  {
    counting();
    PIR = false;
  }
  timeRead(); 
}

/*
  Function: wakeUp0
  Description: ISR that runs after a PIR interrupt
*/
void wakeUp0()
{
 PIR = true;
}

/*
  Function: timeRead()
  Description: Reads the time each time the microcontroller is woken up from
  sleep mode. If there is a new hour, the battery voltage is measured, the
  outside temperature is measured and the data is sent via the NB-IoT network.
*/
void timeRead()
{
  time_t t;
  t = RTC.get(); 
 
  if (timme != hour(t))
  {
    for (int i = 7; i <= 23; i++)
    {
      if (hour(t) == i)
      {
        measureVoltage();
        measureTemp();
        sendData();
      }
    }
    timme = hour(t);
  }
}

/*
  Function: counting()
  Description: Starts the ultrasonic sensor and counts the number of people
  walking past the sensor based on the distance reading.
*/
void counting()
{
  digitalWrite(jsnPowerPin, HIGH);
  upCounter1 = 0;
  upCounter2 = 10;
  
  while (upCounter1 < upCounter1Value)
  {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(20);
    digitalWrite(trigPin, LOW);
    duration = pulseIn(echoPin, HIGH);
    distance = duration*0.034/2;
    if (distance > maxDistance)
    {
      distance = maxDistance;
    }
    if ((distance < breakDistance) && (upCounter2 >= upCounter2Value))
    {
      groupCount++;
      count++;
      totCount++;
      upCounter1 = 0;
      upCounter2 = 0;
      //Serial.println(count);
    }
    upCounter1++;
    upCounter2++;
    Serial.print(distance);
    Serial.print(" ");
    Serial.println(count);
    delay(50); 
  }
  digitalWrite(jsnPowerPin, LOW);
  if (groupCount == 1)
  {
     count1 = count1 + 1;
  }
  else if (groupCount == 2)
  {
     count2 = count2 + 1;
  }
  else if (groupCount == 3)
  {
     count3 = count3 + 1;
  }
  else if (groupCount == 4)
  {
     count4 = count4 + 1;
  }
  else if (groupCount == 5)
  {
     count5 = count5 + 1;
  }
  else if (groupCount >= 6)
  {
     count6 = count6 + 1;
  }
  groupCount = 0;
}

/*
  Function: sendData()
  Description: Starts the NB-IoT module and sends data via CHPPT.
*/
void sendData()
{
  digitalWrite(PWRPin, HIGH);
  delay(100);
  digitalWrite(simPowerPin, HIGH);
  delay(100);
  digitalWrite(PWRPin, LOW);
  delay(1000);
  digitalWrite(PWRPin, HIGH);
  delay(10000); //Starting up SIM7020E

  gprsSerial.println("AT*MCGDEFCONT=\"IP\",\"lpwa.telia.iot\"");
  delay(2000);
  gprsSerial.println("AT+CHTTPCREATE=\"http://discgolfcounting.com\"");
  delay(2000);
  gprsSerial.println("AT+CHTTPCON=0");
  delay(2000);
  gprsSerial.println("AT+CHTTPSEND=\"0\",\"1\",\"/7020test.php?r="+String(count)+"&1="+String(count1)+"&2="+String(count2)+"&3="+String(count3)+"&4="+String(count4)+"&5="+String(count5)+"&6="+String(count6)+"&temp="+String(sendTemp)+"&v="+String(sendVoltage)+"&t="+String(totCount)+"\"");
  delay(2000);
  digitalWrite(simPowerPin, LOW);
  delay(100);
  digitalWrite(PWRPin, LOW);
  count = 0;
  count1 = 0;
  count2 = 0;
  count3 = 0;
  count4 = 0;
  count5 = 0;
  count6 = 0;
}

*/
  Function: measureTemp()
  Description: Measures the outside temperature with the DS18B20 sensor.
  5 temperature readings are taken during one second to output an average.
*/
void measureTemp()
{
  digitalWrite(tempPowerPin, HIGH);
  delay(200);
  for (int i = 0; i <= 4; i++)
  {
    sensors.requestTemperatures();
    tempValue = tempValue + sensors.getTempCByIndex(0);
    delay(200);
  }
  digitalWrite(tempPowerPin, LOW);
  temp = (tempValue/5);
  sendTemp = temp;
  tempValue = 0;
}

/*
  Function: measureVoltage()
  Description: Estimates the battery voltage by using an analog input on the arduino.
*/
void measureVoltage()
{
  digitalWrite(batteryPowerPin, HIGH);
  delay(200);  
  for (int i = 0; i <= 4; i++)
  {
    voltageValue = voltageValue + analogRead(batteryDataPin);
    delay(200);
  }
  digitalWrite(batteryPowerPin, LOW);
  batteryVoltage = (voltageValue/5)/1023*arduinoVoltage*2;
  sendVoltage = round(batteryVoltage);
  voltageValue = 0;
}
