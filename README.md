poultrylightduino
=================

A wireless poultry light sunrise sunset dimmer with temp and humidity sensors for Arduino 


Hardware
========

 * Arduino UNO
 * T5 light source for example  2x28W http://biltema.se/sv/Bygg/Belysning-och-lampor/Belysning-inomhus/Lysrorsarmatur-T5-IP65-D-Markt-44790/
 * HF dimmable ballast e.g OSRAM 2 x 28w 54w T5 DIMMABLE HF BALLAST 1-10v DIMMABLE QTi 2x28/54 DIM
 * Onewire temp sensor or any temp sensor i use https://www.sparkfun.com/products/11050
 * HUM sensor i use https://www.sparkfun.com/products/9569
 * Wireless i use the WiFLY shield https://www.sparkfun.com/products/9954 (but the library is not that good)
 * an OPAMP circut to convert arudiono PWMM analog out from 5v to 0-10V that is needed to the Ballast. 
 * A good 250V relay circut. e.g https://www.sparkfun.com/products/11042 Warning you need to know what your are doing! I separated the relay from the breadbord for some extra safety. 
 * A push button switch
 * Connection cabels
 * 12V powersource to the Arduino. 
 * 
 
