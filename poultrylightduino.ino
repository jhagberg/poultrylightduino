/**********************************************************************
 * A poultry light dimmer based on WiFly and Julio Terras Arduino Rest Server
 * 
 * 
 * 
 * For more information check out the GitHub repository at: 
 * 	https://github.com/julioterra/Arduino_Rest_Server/wiki
 * 
 * Sketch and library created by Julio Terra - November 20, 2011
 * 	http://www.julioterra.com/journal
 * 
 * 
 **********************************************************************/

#include <config_rest.h>
#include <rest_server.h>
#include <SPI.h>
#include <WiFly.h>
#include <avr/eeprom.h>

#include <FlexiTimer2.h>
#include "OneButton.h"

//#include "Credentials.h"

#include <HIH4030.h>
#include <OneWire.h>
#include <DallasTemperature.h>


#include <Time.h>
#include <TimeAlarms.h>
#include <Timezone.h>

struct settings_t
{
  time_t sunrise_min;
  time_t sunset_min;
} 
settings;


//Central European Time (Frankfurt, Borlänge)
TimeChangeRule CEST = {
  "CEST", Last, Sun, Mar, 2, 120};     //Central European Summer Time
TimeChangeRule CET = {
  "CET ", Last, Sun, Oct, 3, 60};       //Central European Standard Time
Timezone CE(CEST, CET);

#define SERVICES_COUNT	6
#define CRLF "\r\n"

#define ONE_WIRE_BUS 7
#define PIN_HIH4030 A0

WiFlyServer server(80);
time_t prevDisplay = 0;
time_t timeNow= 0;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

float humidity;
float temp;
float temp2;
//my t5 dimmer pin and relay
int T5dim = 9;
int T5relay = 8;

//int for button press dim loop
volatile int bpress = 1;
volatile int bpress2 = 1;
OneButton button(A1, true);

//Light on or off? 
volatile boolean T5lightOn = false;
boolean firstreq = true;
//Should we dim true ?
volatile boolean T5lightDim = false;
volatile boolean ButtonDim = false;
String currenttime = "";
//for digitalsmooth
#define filterSamples   13
float tempSmoothArray [filterSamples];   // array for holding raw sensor values for temp
float humSmoothArray [filterSamples];   // array for holding raw sensor values for hum 

unsigned long lastMillis = 0;
unsigned long lastMillis2 = 0;

// Create instance of the RestServer
RestServer request_server = RestServer(Serial);

AlarmID_t sunriseAlarmId;  
AlarmID_t sunsetAlarmId;

// input and output pin assignments


// method that register the resource_descriptions with the request_server
// it is important to define this array in its own method so that it will
// be discarted from the Arduino's RAM after the registration.
void register_rest_server() {
  resource_description_t resource_description [SERVICES_COUNT] = {
    {
      "dim", 	true, 	{
        0, 255      }
    }
    , 
    {
      "temp", 	false, 	{
        -10000, 10000      }
    }
    , 
    {
      "hum", 	false, 	{
        0, 10000      }
    }
    , 
    {
      "sunrise", 	true, 	{
        0, 1440      }
    }
    , 
    {
      "sunset", 	true, 	{
        0, 1440      }
    }
    ,
    {
      "alarm", 	true, 	{
        0, 1      }
    } 
  };
  request_server.register_resources(resource_description, SERVICES_COUNT);  
}


void setup() {
  
  Serial.begin(9600);  
  Serial.println("PING");
 
 // start the Ethernet connection and the server:
  WiFly.begin();
  //lets wifly settle and set time. 
  delay(1000);

  server.begin();
    
  //read from settings sunrise and sunset time in minute. BEFORE SETTING IN SERVER
  eeprom_read_block((void*)&settings, (void*)0, sizeof(settings));
  Serial.println(settings.sunrise_min);
  Serial.println(settings.sunset_min);
  
  //REST Settings
  request_server.set_post_with_get(true);
  request_server.set_json_lock(false);
  
  

  // register resources with resource_server
  register_rest_server();

  //Set start values
  request_server.resource_set_state("alarm", 1);
  request_server.resource_set_state("sunset", settings.sunset_min);
  request_server.resource_set_state("sunrise", settings.sunrise_min);

  //setup hum
  HIH4030::setup(PIN_HIH4030);
  sensors.begin();
  sensors.setResolution(0, 10);


  //Load temp smooth. 
  for (int j=0; j<filterSamples; j++){
    sensors.requestTemperatures();

    temp2 = sensors.getTempCByIndex(0);
    //Serial.println(temp2);
    //Serial.println(j);
    temp = digitalSmooth(temp2, tempSmoothArray); 
    readHum(temp2);
  }
  
 

  //declare pin as outputs.      
  pinMode(T5dim, OUTPUT);
  pinMode(T5relay, OUTPUT);




  //sync time with NTP fron wifly
  setSyncInterval(86400);
  setSyncProvider(getNtpTime);
  
  //Set Alarms sunrise and sunset
  sunriseAlarmId = Alarm.alarmRepeat((settings.sunrise_min*60), sunrise);
  sunsetAlarmId =  Alarm.alarmRepeat((settings.sunset_min*60), sunset);


  //Onebutton lib functions
  
  button.attachDoubleClick(doubleclick);
  button.attachClick(singleclick);
  button.attachPress(buttonpress);
  
  
  
  //timer interupt to check if button is pressed
  FlexiTimer2::set(1,1/1000, myInterupt); //Interrupt every 1/1000 milisec
  FlexiTimer2::start();
   

}

void loop() {

  //check for Alarms 
  Alarm.delay(1);
  //run dim function
  dimT5();
  //read temp
  readTemp();
  //read hum
  readHum(temp);

//Da WiFly shit or magic
  WiFlyClient client = server.available();
  // CONNECTED TO CLIENT
  if (client) {
Serial.println("after client con");
    while (client.connected()) {

      // get request from client, if available
      if (request_server.handle_requests(client)) {
          //all states is always updated first time requested after power cycle
          if(firstreq)
          {
            firstreq =false;
          }
          else
          {
          updateStuff();                               
          }
        request_server.respond();	// tell RestServer: ready to respond          
        
      }

      // send data to client, when ready	
      if (request_server.handle_response(client))
      {
        //Send current time extra garbage on the page. 
        currenttime = currenttime + hour() + ":" + printzeros(minute()) + ":" + printzeros(second()) +" " + year() + "-" + month() + "-" + day();
        client.print(currenttime);
        currenttime = "";
        request_server.print_flash_string(PSTR("\r\n\r\n\r\n"), client);
        break;                                      
      }

    }
    // give the web browser time to receive the data and close connection

    Alarm.delay(10);

    
    client.stop();
    Serial.println("after client stop");
  }
}

void myInterupt() {
  button.tick();
  if( (digitalRead(A1) == 0 ) && ButtonDim)
  {
    dimFast();
  }
  else{
    ButtonDim =false;
  }
  
}

void doubleclick() {

  if(request_server.resource_get_state(0)>0)
  {
    //Serial.println("doubleclick sunset");
    sunset();
  }
  else
  {
    //Serial.println("doubleclick sunrise");
    sunrise();
    analogWrite(T5dim,1);
    request_server.resource_set_state(0,1);
  }

} // doubleclick

void singleclick() {
  if(request_server.resource_get_state(0)==0)
  {
    //Serial.println("click ON");
    analogWrite(T5dim,1);
    request_server.resource_set_state(0,1);
    digitalWrite(T5relay, HIGH);
    T5lightOn = true;
  }
  else
  {
    //serial.println("Click Off");
    analogWrite(T5dim,0);
    request_server.resource_set_state(0,0);
    digitalWrite(T5relay, LOW);
    T5lightOn = false;
    T5lightDim = false;
  }


} 

void buttonpress() {

  ButtonDim = true;
  
  //serialprintln("In press");
  dimFast();

} 


void dimFast() {

  if ( (millis() - lastMillis2 > 50) ){

    lastMillis2 = millis();
  request_server.resource_set_state(0,request_server.resource_get_state(0)+bpress2);
  analogWrite(T5dim,request_server.resource_get_state(0) );
  //Serial.println(request_server.resource_get_state("dim")); 
  if (request_server.resource_get_state(0) == 255) bpress2 = -1;             // switch direction at peak
  if (request_server.resource_get_state(0) == 1) bpress2 = 1;             // switch direction at peak
  }
}


void updateStuff()
{
   if(request_server.resource_updated("alarm"))
    {
     if(request_server.resource_get_state("alarm") == 0){
    Alarm.disable(sunriseAlarmId);
    Alarm.disable(sunsetAlarmId);
     }
     else {
    Alarm.enable(sunriseAlarmId);
    Alarm.enable(sunsetAlarmId);
       
     }
    }
  
  //Update light  
  if(request_server.resource_updated(0))
    {
    analogWrite(T5dim, request_server.resource_get_state(0));
    }
    
  if(request_server.resource_updated(0) && request_server.resource_get_state(0)>0)
  {
    digitalWrite(T5relay, HIGH);
  }
  if(request_server.resource_updated(0) && request_server.resource_get_state(0)==0)
  {
    digitalWrite(T5relay, LOW);
  }

  if(request_server.resource_get_state("sunrise") != settings.sunrise_min){
    settings.sunrise_min = request_server.resource_get_state("sunrise");
    eeprom_write_block((const void*)&settings, (void*)0, sizeof(settings));
    Alarm.write(sunriseAlarmId, time_t(settings.sunrise_min*60));
    //Serial.println("in sunrise update");
    //Serial.println(Alarm.read(sunriseAlarmId));

  }

  if(request_server.resource_get_state("sunset") != settings.sunset_min){
    settings.sunset_min = request_server.resource_get_state("sunset");
    eeprom_write_block((const void*)&settings, (void*)0, sizeof(settings));
    Alarm.write(sunsetAlarmId, time_t(settings.sunset_min*60));
    //Serial.println(Alarm.read(sunsetAlarmId));
  }


  /*
if(request_server.resource_updated("light")){
   T5lightOn = request_server.resource_get_state("light");
   if(T5lightOn) {
   T5lightDim = true;
   digitalWrite(T5relay, HIGH);
   }
   else  {
   T5lightDim = true;
   }
   }
   */
}



//Not sure why we need this but it looks like it works
unsigned long getNtpTime()
{
  //Serial.println("In syncNTP");
  return CE.toLocal(WiFly.getTime()); 

}


String printzeros(int digits){
  if(digits < 10)
  {
    return (String)"0" + digits;
  }
  else 
  {  
    return (String)digits;
  }
}

void dimT5()
{
  //update dim bright every 14 second this will take aprox 60 min
  if ( (millis() - lastMillis > 14000) && T5lightDim)
    {
    lastMillis = millis();
    if (T5lightOn)
        { //Light is on and we are still dimming up
        bpress = 1;
        if(request_server.resource_get_state(0) == 255) T5lightDim = false;
        }
        
    else  
      {
      if(request_server.resource_get_state(0) > 0 )
        {
        bpress = -1;             
        }
       if(request_server.resource_get_state(0) == 0)
         {
         digitalWrite(T5relay, LOW);
         T5lightDim = false;
         }
        
      }  
    
    request_server.resource_set_state(0,request_server.resource_get_state(0)+bpress);
    analogWrite(T5dim,request_server.resource_get_state(0) );
  }  

}

void sunrise(){
  T5lightOn = true;
  T5lightDim = true;
  digitalWrite(T5relay, HIGH);

  //Serial.println("Alarm: - turn lights on");
}

void sunset(){
  T5lightOn = false;
  T5lightDim = true;
  //Serial.println("Alarm: - turn lights off");  
}


void syncNTP(){
  //Serial.println("In syncNTP");  
  setTime((time_t)getNtpTime());
  //Serial.println("after set snyncNTP");  
}

void readTemp(){
  sensors.requestTemperatures();

  //Serial.println(sensors.getTempCByIndex(0));
  temp = digitalSmooth(sensors.getTempCByIndex(0), tempSmoothArray); 
  request_server.resource_set_state(1, int(temp*100));

}
void readHum(float TEMP){
  humidity=digitalSmooth(HIH4030::read(PIN_HIH4030, TEMP), humSmoothArray);
  request_server.resource_set_state(2, int(humidity*100));
  int analogA; 
  analogA = analogRead(0);
  /*
  Serial.println(humidity);
  Serial.print("libval: ");
  Serial.println(HIH4030::read(PIN_HIH4030, TEMP));
  Serial.print("newval: ");
  Serial.println(((((analogA/1023.)*5)-0.958)/0.0307)/(1.0546-0.00216*TEMP));
  Serial.print("analog: ");
  Serial.println(float (analogA/1023.) * 5);
  */
}

float digitalSmooth(float rawIn, float *sensSmoothArray){     // "int *sensSmoothArray" passes an array to the function - the asterisk indicates the array name is a pointer
  int j, k, top, bottom; 
  float temp;
  float total;
  static int i;
  // static int raw[filterSamples];
  static float sorted[filterSamples];
  boolean done;

  i = (i + 1) % filterSamples;    // increment counter and roll over if necc. -  % (modulo operator) rolls over variable
  sensSmoothArray[i] = rawIn;                 // input new data into the oldest slot

  // Serial.print("raw = ");

  for (j=0; j<filterSamples; j++){     // transfer data array into anther array for sorting and averaging
    sorted[j] = sensSmoothArray[j];
  }

  done = 0;                // flag to know when we're done sorting              
  while(done != 1){        // simple swap sort, sorts numbers from lowest to highest
    done = 1;
    for (j = 0; j < (filterSamples - 1); j++){
      if (sorted[j] > sorted[j + 1]){     // numbers are out of order - swap
        temp = sorted[j + 1];
        sorted [j+1] =  sorted[j] ;
        sorted [j] = temp;
        done = 0;
      }
    }
  }

  /*
  for (j = 0; j < (filterSamples); j++){    // print the array to debug
   Serial.print(sorted[j]); 
   Serial.print("   "); 
   }
   Serial.println();
   */

  // throw out top and bottom 15% of samples - limit to throw out at least one from top and bottom
  bottom = max(((filterSamples * 15)  / 100), 1); 
  top = min((((filterSamples * 85) / 100) + 1  ), (filterSamples - 1));   // the + 1 is to make up for asymmetry caused by integer rounding
  k = 0;
  total = 0;
  for ( j = bottom; j< top; j++){
    total += sorted[j];  // total remaining indices
    k++; 
    // Serial.print(sorted[j]); 
    // Serial.print("   "); 
  }

  //  Serial.println();
  //  Serial.print("average = ");
  //  Serial.println(total/k);
  return total / k;    // divide by number of samples
}

