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
//#include "Credentials.h"

#include <HIH4030.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include <FlexiTimer2.h>
#include <Time.h>
#include <TimeAlarms.h>

#define SERVICES_COUNT	7
#define CRLF "\r\n"

WiFlyServer server(80);
time_t prevDisplay = 0;
time_t timeNow= 0;

//my t5 dimmer pin and relay

int T5dim = 9;
int T5relay = 8;
//Light on or off? 
volatile boolean T5lightOn = false;
//Should we dim true ?
volatile boolean T5lightDim = false;
String currenttime = "";

volatile int brightness = 0;

unsigned long lastMillis = 0;

// Create instance of the RestServer
RestServer request_server = RestServer(Serial);


// input and output pin assignments



// method that register the resource_descriptions with the request_server
// it is important to define this array in its own method so that it will
// be discarted from the Arduino's RAM after the registration.
void register_rest_server() {
      resource_description_t resource_description [SERVICES_COUNT] = {{"dim", 	true, 	{0, 255}}, 
							  {"temp", 	false, 	{-100, 100}}, 
							  {"hum", 	false, 	{0, 100}}, 
                                                          {"sunrise", 	true, 	{0, 1024}}, 
							  {"sunset", 	true, 	{0, 1024}},
                                                          {"time", 	true, 	{0, 1024}},
                                                          {"light", 	true, 	{0, 1}} 
                                                            };
      request_server.register_resources(resource_description, SERVICES_COUNT);  
}


//put Innterupt code here
void MyIntterupt() {
  //update and check dim every second. 
  dimT5();
  Alarm.delay(0);
  
}

void setup() {
	
        
        //declare ping as outputs.      
        pinMode(T5dim, OUTPUT);
        pinMode(T5relay, OUTPUT);
        Serial.begin(9600);

        request_server.resource_set_state("light", 0);
        // start the Ethernet connection and the server:
        WiFly.begin();
        //lets wifly settle and set time. 
        delay(1000);
        
            
       //   Serial.print("IP: ");
       //   Serial.println(WiFly.ip());
        
        //sync time with NTP fron wifly
       setSyncInterval(60);
       setSyncProvider(getNtpTime);
       //setTime((time_t)getNtpTime());
       //while(timeStatus()== timeNotSet){} 
       Alarm.alarmRepeat(6,00,0, sunrise);
       Alarm.alarmRepeat(10,00,0, sunset);
       //Alarm.alarmRepeat(21,40,0, syncNTP);
       Alarm.timerRepeat(60, Repeats);


        Serial.println("Start");
        Serial.println(); 
        
        
        request_server.set_post_with_get(true);
	server.begin();

	

	// register resources with resource_server
	register_rest_server();
        //FlexiTimer2::set(1000, MyIntterupt); //Interrupt every second.
        //FlexiTimer2::start();
}

void loop() {
        
//      delay(200);  
        WiFlyClient client = server.available();
        timeNow = now();
        Alarm.delay(1000);
        dimT5();
	// CONNECTED TO CLIENT
	if (client) {
              Serial.println("In client.connect");
		while (client.connected()) {
                        
			// get request from client, if available
			if (request_server.handle_requests(client)) {
				
				request_server.respond();	// tell RestServer: ready to respond
                                
                                if(request_server.resource_updated("light")){
                                  T5lightOn = request_server.resource_get_state("light");
                                   if(T5lightOn) {
                                   T5lightDim = true;
                                   digitalWrite(T5relay, HIGH);
                                   }
                                   if(!T5lightOn) {
                                   T5lightDim = true;
                                   }
                                  }
                                  
                                  
                                  
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
//delay(200);
		client.stop();
                Serial.println("after client stop");
	}
}




//Not sure why we need this but it looks like it works
unsigned long getNtpTime()
{
  Serial.println("In syncNTP");
 return WiFly.getTime() + 7200; 
 
}


void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
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
  //         Serial.println("in every 14s");
           if (T5lightOn){ //Light is on and we are still dimming up
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
           if (!T5lightOn){
               if( brightness > 0 ){
               brightness--;
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
  
  Serial.println("Alarm: - turn lights on");    
}
void sunset(){
  T5lightOn = false;
  T5lightDim = true;
  
  
  Serial.println("Alarm: - turn lights off");    
}

void Repeats(){
  Serial.println("second timer");         
}

void syncNTP(){
  Serial.println("In syncNTP");  
  setTime((time_t)getNtpTime());
  Serial.println("after set snyncNTP");  
}

