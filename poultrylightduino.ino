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

#define SERVICES_COUNT	10
#define CRLF "\r\n"

WiFlyServer server(80);
time_t prevDisplay = 0;

//my t5 dimmer pin and relay

int T5dim = 9;
int T5relay = 8;
//Light on or off? 
boolean T5lightOn = false;
//Should we dim true ?
boolean T5lightDim = false;

volatile int brightness = 0;

unsigned long lastMillis = 0;

// Create instance of the RestServer
RestServer request_server = RestServer(Serial);


// input and output pin assignments



// method that register the resource_descriptions with the request_server
// it is important to define this array in its own method so that it will
// be discarted from the Arduino's RAM after the registration.
void register_rest_server() {
      resource_description_t resource_description [SERVICES_COUNT] = {{"output_1", 	false, 	{0, 1024}}, 
							  {"output_2", 	false, 	{0, 1024}}, {"output_3", 	false, 	{0, 1024}}, 
							  {"output_4", 	false, 	{0, 1024}}, {"output_5", 	false, 	{0, 1024}}, 
							  {"output_6", 	false, 	{0, 1024}}, {"input_1", 	true, 	{0, 255}}, 
							  {"input_2", 	true, 	{0, 255}}, {"relay", 	true, 	{0, 255}}, 
							  {"input_4", 	true, 	{0, 255}} };
      request_server.register_resources(resource_description, SERVICES_COUNT);  
}


//put Innterupt code here
void MyIntterupt() {
  
  
}

void setup() {
	
        FlexiTimer2::set(1000, MyIntterupt); //Interrupt every second.
        FlexiTimer2::start();
        //declare ping as outputs.      
        pinMode(T5dim, OUTPUT);
        pinMode(T5relay, OUTPUT);
        
        
        // start the Ethernet connection and the server:
        WiFly.begin();

        
            Serial.begin(9600);
       //   Serial.print("IP: ");
       //   Serial.println(WiFly.ip());
    
        //sync time with NTP fron wifly
       setSyncProvider(getNtpTime);
       while(timeStatus()== timeNotSet){} 
      
        Serial.println("the time");
        
        Serial.print(hour());
        printDigits(minute());
        printDigits(second());
        Serial.print(" ");
        Serial.print(year()); 
        Serial.print(" ");
        Serial.print(month());
        Serial.print(" ");
        Serial.print(day());      
        Serial.println(); 
        
        
        request_server.set_post_with_get(true);
	server.begin();

	

	// register resources with resource_server
	register_rest_server();
}

void loop() {
        
        WiFlyClient client = server.available();
  

	// CONNECTED TO CLIENT
	if (client) {
		while (client.connected()) {

			// get request from client, if available
			if (request_server.handle_requests(client)) {
				
				request_server.respond();	// tell RestServer: ready to respond
			}		

			// send data to client, when ready	
			if (request_server.handle_response(client)) break;
		}
		// give the web browser time to receive the data and close connection
		delay(200);
		client.stop();
	}
}




//Not sure why we need this but it looks like it works
unsigned long getNtpTime()
{
 return WiFly.getTime(); 
}


void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}


void dimT5()
{
  //update dim bright every 14 second this will take aprox 60 min

  if ( (millis() - lastMillis > 14000) && T5lightDim){
           lastMillis = millis();
           if (T5lightOn){ //Light is on and we are still dimming up
             if( brightness < 255 ){
                  brightness++;
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
               }
           }
        }  
    }  
  
}


