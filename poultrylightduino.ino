/**********************************************************************
  A poultry light dimmer based on WiFly and Julio Terras Arduino Rest Server
 


 For more information check out the GitHub repository at: 
	https://github.com/julioterra/Arduino_Rest_Server/wiki
 
 Sketch and library created by Julio Terra - November 20, 2011
	http://www.julioterra.com/journal


 **********************************************************************/

#include <config_rest.h>
#include <rest_server.h>
#include <SPI.h>
#include <WiFly.h>
#include <avr/eeprom.h>

//#include "Credentials.h"

#include <HIH4030.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include <OneButton.h>

#include <Time.h>
#include <TimeAlarms.h>
#include <Timezone.h>

struct settings_t
{
  int sunrise_min;
  int sunset_min;
} settings;


//Central European Time (Frankfurt, Borlänge)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     //Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       //Central European Standard Time
Timezone CE(CEST, CET);

#define SERVICES_COUNT	7
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


//Light on or off? 
boolean T5lightOn = false;
//Should we dim true ?
boolean T5lightDim = false;
String currenttime = "";
//for digitalsmooth
#define filterSamples   13
float tempSmoothArray [filterSamples];   // array for holding raw sensor values for temp
float humSmoothArray [filterSamples];   // array for holding raw sensor values for hum 

int brightness = 0;

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
      resource_description_t resource_description [SERVICES_COUNT] = {{"dim", 	true, 	{0, 255}}, 
							  {"temp", 	false, 	{-10000, 10000}}, 
							  {"hum", 	false, 	{0, 10000}}, 
                                                          {"sunrise", 	true, 	{0, 1440}}, 
							  {"sunset", 	true, 	{0, 1140}},
                                                          {"alarm", 	true, 	{0, 1}},
                                                          {"light", 	true, 	{0, 1}} 
                                                            };
      request_server.register_resources(resource_description, SERVICES_COUNT);  
}


void setup() {
        Serial.begin(9600);  
	//setup hum
        HIH4030::setup(PIN_HIH4030);
        sensors.begin();
        sensors.setResolution(0, 10);
        //Load temp smooth. 
        int j;
        for (j=0; j<filterSamples; j++){
            sensors.requestTemperatures();
            //Serial.println(sensors.getTempCByIndex(0));
            temp2 = sensors.getTempCByIndex(0);
            temp = digitalSmooth(temp2, tempSmoothArray); 
            readHum(temp2);
        }
        //read from settings sunrise and sunset time in minute. 
        eeprom_read_block((void*)&settings, (void*)0, sizeof(settings));
        //declare pin as outputs.      
        pinMode(T5dim, OUTPUT);
        pinMode(T5relay, OUTPUT);
        

        request_server.resource_set_state("light", 0);
        request_server.resource_set_state("alarm", 1);
        request_server.resource_set_state("sunset", settings.sunset_min);
        request_server.resource_set_state("sunrise", settings.sunrise_min);
        // start the Ethernet connection and the server:
        WiFly.begin();
        //lets wifly settle and set time. 
        delay(1000);
        
            
       //   Serial.print("IP: ");
       //   Serial.println(WiFly.ip());
        
        //sync time with NTP fron wifly
       setSyncInterval(86400);
       setSyncProvider(getNtpTime);
       //setTime((time_t)getNtpTime());
       //while(timeStatus()== timeNotSet){}

       sunriseAlarmId = Alarm.alarmRepeat((settings.sunrise_min/60),(settings.sunrise_min % 60),0, sunrise);
       sunsetAlarmId =  Alarm.alarmRepeat((settings.sunset_min/60),(settings.sunset_min % 60),0, sunset);
       //Alarm.alarmRepeat(21,40,0, syncNTP);
       

//        Serial.println("Start");
//        Serial.println(); 
        
        
        request_server.set_post_with_get(true);
        request_server.set_json_lock(false);
	server.begin();

	

	// register resources with resource_server
	register_rest_server();
        
}

void loop() {
        
//      delay(200);  

        timeNow = now();
        Alarm.delay(1);
        //run dim function<
        dimT5();
        //read temp
        readTemp();
        //read hum
        readHum(temp);       
        
        
        
        WiFlyClient client = server.available();
        // CONNECTED TO CLIENT
	if (client) {
           //   Serial.println("In client.connect");
		while (client.connected()) {
                        
			// get request from client, if available
			if (request_server.handle_requests(client)) {
				updateStuff();                               
                                request_server.respond();	// tell RestServer: ready to respond          
			}
                                                       
			// send data to client, when ready	
			if (request_server.handle_response(client))
                              {
                                 currenttime = currenttime + hour() + ":" + printzeros(minute()) + ":" + printzeros(second()) +" " + year() + "-" + month() + "-" + day();
                                  client.print(currenttime);
                                  currenttime = "";
                                  request_server.print_flash_string(PSTR("\r\n\r\n\r\n"), client);
                                  break;                                      
                              }
                         
		}
		// give the web browser time to receive the data and close connection
                Alarm.delay(5);
		client.stop();
         //       Serial.println("after client stop");
	}
}


void updateStuff()
{
//Update light  
analogWrite(T5dim, request_server.resource_get_state("dim"));

if(request_server.resource_get_state("dim")>0)
{
digitalWrite(T5relay, HIGH);
}
if(request_server.resource_get_state("dim")==0)
{
digitalWrite(T5relay, LOW);
}

if(request_server.resource_updated("sunrise")){
 settings.sunrise_min = request_server.resource_get_state("sunrise");
 eeprom_write_block((const void*)&settings, (void*)0, sizeof(settings));
}

if(request_server.resource_updated("sunset")){
 settings.sunrise_min = request_server.resource_get_state("sunset");
 eeprom_write_block((const void*)&settings, (void*)0, sizeof(settings));
}



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
}



//Not sure why we need this but it looks like it works
unsigned long getNtpTime()
{
  //Serial.println("In syncNTP");
 return CE.toLocal(WiFly.getTime()); 
 
}


String printzeros(int digits){
  if(digits < 10)
   {return (String)"0" + digits;}
  else 
  {  return (String)digits;}
}

void dimT5()
{
  //update dim bright every 14 second this will take aprox 60 min
//Serial.println("in t5dim");
//Serial.println(T5lightDim);
  if ( (millis() - lastMillis > 14000) && T5lightDim){
           
           lastMillis = millis();
  /*    
           Serial.println("in every 14s");
           Serial.print("T5lightDim: ");
           Serial.println(T5lightDim);
           Serial.print("T5lighOn: ");
           Serial.println(T5lightOn);
           Serial.print("Bright: ");
           Serial.println(brightness);
    */  
           if (T5lightOn){ //Light is on and we are still dimming up
      
     //        Serial.println("T5lightOn true");
      
             if( brightness < 255 ){
                  brightness++;
                  request_server.resource_set_state("dim", brightness);
                  analogWrite(T5dim, brightness);
                  if(brightness == 255){
                    //we are done dimming
                    T5lightDim = false;
                  }
                  }
           }
           else  {
      
       //      Serial.println("T5lightOn not true");
               
               if( brightness > 0 ){
               brightness--;
               request_server.resource_set_state("time", 12);
               request_server.resource_set_state("dim", brightness);
               analogWrite(T5dim, brightness);
               if(brightness == 0){
                     //We are  on zero dim level turn off
                     digitalWrite(T5relay, LOW);
                     T5lightDim = false;
               }
           }
        }  
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
request_server.resource_set_state("temp", int(temp*100));

}
void readHum(float TEMP){
   humidity=digitalSmooth(HIH4030::read(PIN_HIH4030, TEMP), humSmoothArray);
   request_server.resource_set_state("hum", int(humidity*100));
   int analogA; 
   analogA = analogRead(0);
   //Serial.println(humidity);
   //Serial.print("libval: ");
   //Serial.println(HIH4030::read(PIN_HIH4030, TEMP));
   //Serial.print("newval: ");
   //Serial.println(((((analogA/1023.)*5)-0.958)/0.0307)/(1.0546-0.00216*TEMP));
   //Serial.print("analog: ");
   //Serial.println(float (analogA/1023.) * 5);
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
