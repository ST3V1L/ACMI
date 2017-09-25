/*
Automated Blinds Controller
*/

#include <SPI.h>
#include <Ethernet.h>
#include <Time.h>
#include <AFMotor.h>
#include <SD.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0D, 0x0D, 0xE0 };
IPAddress ip(192,168,0,10); //<<< ENTER YOUR IP ADDRESS HERE!!!

// Initialise the Ethernet server library
// with the IP address and port you want to use 
// (port 80 is default for HTTP):
EthernetServer server(80);
 
byte mem0=0; 
byte mem1=0; //variables to set transition state
byte TemPin=0;  //Input Pin for Temperature sensor
byte photocellPin=1; //Input Pin for Photocell
byte stepperRevolution=200; //number of steps taking for stepper to make on revolution. 
byte limitRev=5;//number of revolutions needed to open or close blind. 
byte NumberOfRevsFor=0; //Counter for number of revolutions completed by stepper motor going forward.
byte NumberOfRevsBack=limitRev; //Counter for number of revolutions completed by stepper motor in reverse.
AF_Stepper motor(200,2); //200 step/rev motor connect to M3 and M4 on motor shield 
File myFile; 
String value;
byte SDreadAtStart=0;
byte openHour,closeHour,ctltemp, OvRCtl=0, timeHour, timeMin; 
int Luminosity; //variable which sensor luminosity reading is compared against. 

void setup()
{  
   Serial.begin(9600); 
   pinMode(10, OUTPUT); //set up reading and writing to SD card
   SD.begin(4);    
   motor.setSpeed(250); //set motor 200 rpm.
   // start the Ethernet connection and the server:
   Ethernet.begin(mac, ip);    
   server.begin();  
   //SDreader("BlindCtl/Settings/Settings.txt"); //read values in from Time file. Hour and minute values are saved temporarily saved to Luminosity and ctltemp
   //setTime(timeHour,timeMin,0,10,7,2012); //set current time on arduino   
   delay(1000); //Give ethernet and sensors time to set up.   
}



void loop()//main 
{    
  byte SDread;   
  SDread=EthernetClients();   //checks to see if client is connected  
        
  if(SDreadAtStart==0||SDread==1)
  {            
      SDreader("BlindCtl/Settings/Settings.txt");   //reads values from Settings.txt
      setTime(timeHour,timeMin,0,10,7,2012);  
      SDreadAtStart=1;     
  }      
  //Serial.println(Luminosity);           
  motors(Luminosity,ctltemp,openHour,closeHour);    
}

void motors(int luminosity, byte ctltemp, byte openHour, byte closeHour)//(byte Open, byte Close)
{
 // Serial.println(F("motors is started"));
  time_t t=now();//set current time to variable t
  byte curTime=hour(t); //saves the current hour
  Serial.println(curTime); 
  //String readFile, value; 
  analogRead(TemPin);
  delay(10);
  int temRead=analogRead(TemPin); //reads in from temperature sensor
  float temVoltage=temRead*5.0;
  temVoltage/=1024.0;   
  float temperature=(temVoltage-0.5)*100;
  delay(10); //delay to let voltage settle before reading from photocell
  
  analogRead(photocellPin);
  delay(10);
  int photocellReading=analogRead(photocellPin);//reads the photocell value
  delay(10);
  if(OvRCtl==1)//checks for variable that indicates if blinds should auto open and then sets requirements to low. 
  {
    luminosity=200; 
    ctltemp=temperature+5;
    openHour=0;
    closeHour=24; 
  }
  else if(OvRCtl==2)//checks variable to see if override for close is enables and then sets requirements high. 
  {
    luminosity=999; 
    ctltemp=25;
    openHour=0;
    closeHour=24;    
  }
  else
  {
    if(luminosity<=200||luminosity>=1000)//error checks to make sure user has not entered values outside of scope.
      luminosity=600; 
    if(ctltemp<=5||ctltemp>=60)
      ctltemp=25; 
    if(openHour<=(-1)||openHour>=25)
      openHour=13;
   if(closeHour<=(-1)||closeHour>=25)
      closeHour=20;
  } 
  
  if(mem0==0&&mem1==0)//State one where blinds are closed
  { 
   // Serial.print("Blinds are closed \n");
   if((photocellReading>=luminosity)&&(curTime>=openHour)&&(curTime<closeHour)&&(temperature<=ctltemp))//If all three of these conidtions are met, blinds will move into opening state. 
    { 
      mem0=1;
    }
  }
  
  else if(mem0==1&&mem1==0)//Moving forward state
  { 
    //Serial.print("Blinds are opening \n");    
    if(NumberOfRevsFor==limitRev)//checks to see if blinds are fully open and if so, switches to fully open state.  
    {          
     mem1=1;
     mem0=1; 
    }     
    else if((photocellReading>=luminosity)&&(curTime>=openHour)&&(curTime<closeHour)&&(temperature<=ctltemp))//&&(NumberOfRevsFor!=TravelForward))//confirms that opening condition is still present and continues to move motor forward
    {
      motor.step(stepperRevolution, FORWARD, DOUBLE); 
      motor.release();
      NumberOfRevsFor++;      
      NumberOfRevsBack--;
          
    } 
    else if(temperature>ctltemp||curTime>=closeHour||photocellReading<luminosity)//switches state to move back in closing condition has become true
    {      
      mem1=1;
      mem0=0; 
    }     
      
  }
  
  else if(mem0==1&&mem1==1)//Full Open position
  {
   // Serial.print("Blinds are open \n");
    if(temperature>ctltemp||curTime>=closeHour||photocellReading<luminosity)//condition to move into closing state
    {
      mem1=1;
      mem0=0; 
    }    
  }
  
  else if(mem0==0&&mem1==1)//Blinds closing state
  {  
   //Serial.print("Blinds are closing \n"); 
        
    if(NumberOfRevsBack==limitRev)//switches to closed state if blinds are fully closed. 
    {
     mem1=0;
     mem0=0; 
    }     
    else if((temperature>ctltemp||curTime>=closeHour||photocellReading<luminosity))//&&NumberOfRevsBack!=TravelBack)//checks for closing conditions and then proceeds to close blinds. 
    {
      motor.step(stepperRevolution, BACKWARD, DOUBLE);
     motor.release(); 
      NumberOfRevsBack++;
      NumberOfRevsFor--;
        
    } 
    else if(photocellReading>=luminosity&&curTime>=openHour&&curTime<closeHour&&temperature<=ctltemp)//if conditions for blinds opening become true, switches to blind open state
    {       
      mem1=0;
      mem0=1; 
    }
  }
  Serial.print(mem0);
  Serial.print(mem1);
  return; 
}

byte EthernetClients()//web clients that takes what the user sets and saves them to teh SD card
{
  boolean stopreading=0,variablePresent=0,storevalue=0,timeOn=0,clientC=0, equalsCount=0, SDread=0;
  String readURL;
  delay(10); 
  EthernetClient client = server.available(); 
  //Serial.println("Eth");  
  if (client) 
  {    
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    //Serial.println("Client available"); 
    Serial.println(F("Client")); 
    
    while (client.connected()) {
      if (client.available()) {
       
        char c = client.read();
        if(c=='?')
        {
          variablePresent=1;        
        }        
                
        if (c=='z'){//stops reading into the string once end of header is reached. 
          stopreading=1;}
        else if(c!='z' && stopreading==0&&variablePresent==1)//reads in GET from the HTTP header
        {
          readURL+=c;
        //Serial.println(readURL);        
        }
        
        //delay(1000);
     if(stopreading==1&&variablePresent==1&&storevalue==0)//reads stored GET header to extract variables
     { 
       
      for(int i=0; i<=(readURL.length()-1); i++)
      {        
        if(readURL[i]=='=')
        {
          int j=i+1;
          while(readURL[j]!='&'&&readURL[j]!='x')
          {
             value+=readURL[j];
             j++;
          }           
         
         if(value=="Open")
           OvRCtl=1;  
         if(value=="Close")
           OvRCtl=2;  
         
         if(equalsCount==0&&value=="Time"){
            timeOn=1;
            OvRCtl=0;}
         else if(value!="Open"&&value!="Close"&&equalsCount==0){
          //timeOn=0; 
           SD.remove("BlindCtl/Settings/Settings.txt");
           myFile= SD.open("BlindCtl/Settings/Settings.txt",FILE_WRITE);          
           myFile.print(F("Luminx"));
           myFile.print(value); 
           myFile.print("z");
           myFile.close();   
          }
         if(equalsCount==1)
        {    
          myFile= SD.open("BlindCtl/Settings/Settings.txt",FILE_WRITE);      
          myFile.print(F("Tempx"));
          myFile.print(value); 
          myFile.print("z");
          myFile.close(); 
        }  
         if(equalsCount==2)
        {   
           myFile= SD.open("BlindCtl/Settings/Settings.txt",FILE_WRITE);        
          myFile.print(F("OpenHourx"));
          myFile.print(value); 
          myFile.print("z"); 
          myFile.close(); 
        }
        if(equalsCount==3)
        {         
          myFile= SD.open("BlindCtl/Settings/Settings.txt",FILE_WRITE);  
          myFile.print(F("CloseHourx"));
          myFile.print(value); 
          myFile.print("z"); 
          myFile.close();
        } 
        if(equalsCount==4)
        {         
          myFile= SD.open("BlindCtl/Settings/Settings.txt",FILE_WRITE);  
          myFile.print(F("Hourx"));
          myFile.print(value); 
          myFile.print("z"); 
          myFile.close();
        }
        if(equalsCount==5)
        {         
          myFile= SD.open("BlindCtl/Settings/Settings.txt",FILE_WRITE);  
          myFile.print(F("Minx"));
          myFile.print(value); 
          myFile.print("z"); 
          myFile.close();
        }
             
         equalsCount++;
          value="";                  
       }
      }
      storevalue=1; 
     }     
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
     if (c == '\n' && currentLineIsBlank) 
     {          
          // send a standard http response header
       client.println(F("HTTP/1.1 200 OK"));
       client.println(F("Content-Type: text/html"));
       client.println();         
       //Any variables cannot contain the characters of z or x! Last variable values must end with xz.
       client.println(F("<div align='center'"));
       client.println(F("<h1>Windows Blinds Client V 1.0</h1>"));
       client.println(F("</div>"));
       // client.println("<div align='center'");
       if(timeOn==0)  
       {         
          client.println(F("<form name='input' action='192.168.0.10' method='get'>"));
          client.println(F("<input type='radio' name='action' value='Openxz' /> Open"));
          client.println(F("<input type='radio' name='action' value='Closexz' /> Close"));
          client.println(F("<input type='radio' name='action' value='Timexz' /> Time"));           
          client.println(F("</br>"));
          client.println(F("<input type='submit' value='Submit'/>"));
          client.println(F("</form>"));
       }
       else if(timeOn==1)
       {
          client.println(F("<form name='input' action='192.168.0.10' method='get'>"));            
          client.println(F("Luminosity:<input type='text' name='Hour' value='600'/>"));
          client.println(F("Temp(close):<input type='text' name='Minutes' value='26'/>"));
          client.println(F("</br>"));            
          client.println(F("Open Blinds At*(hour):<input type='text' name='openTime'/>"));
          client.println(F("Close Blinds At(hour):<input type='text' name='closeTime'/>"));
          client.println(F("</br>"));
          client.println(F("Current Hour:<input type='text' name='Hour'/>"));
          client.println(F("Current Min:<input type='text' name='Min'/>"));
          client.println(F("<input type=hidden name='action' value='xz' />"));
          client.println(F("<input type='submit' value='Submit'/>"));            
          client.println(F("</form>"));
          client.println(F("</br>"));                         
        }                     
          

      break;
      }
      if (c == '\n') {
          // you're starting a new line
         currentLineIsBlank = true;
      } 
      else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
      }
    }
      
      
    // if (loopcounter<=readURL.length())
    //loopcounter++;     
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    SDread=1; 
    //Serial.print('Broswer is closed'); 
  }
  return SDread; 
}

void SDreader(const char* fileName)//function to read the values from teh SD card into variables
{
  byte equalsCount=0;
  String readURL; 
 //Serial.println('SD'); 
  myFile= SD.open(fileName);      

       while(myFile.available())//reads the entire file in a String
      { 
        char c=myFile.read(); 
        readURL+=c; 
      }
      myFile.close();
      //Serial.println(readURL);
      for(int i=0; i<=(readURL.length()); i++)//extracts values from file that are between x and z
      {
        
      if(readURL[i]=='x')
       {         
         int j=i+1;
         i=j; 
         while(readURL[j]!='z')
         {
           value+=readURL[j];
           j++;
         }            
       //Serial.println(value);
       if(equalsCount==0)
          {
            char string_as_char[value.length()+1];
            value.toCharArray(string_as_char,value.length()+1);         
            Luminosity=atoi(string_as_char);
            //Serial.println(Luminosity);            
          }
          if(equalsCount==1)
          {
            char string_as_char[value.length()+1];
            value.toCharArray(string_as_char,value.length()+1);         
            ctltemp=atoi(string_as_char);
            //Serial.println(Luminosity);            
          }
           if(equalsCount==2)
          {
            char string_as_char[value.length()+1];
            value.toCharArray(string_as_char,value.length()+1);         
            openHour=atoi(string_as_char);
            //Serial.println(Luminosity);            
          }
          
          if(equalsCount==3)
          {
            char string_as_char[value.length()+1];
            value.toCharArray(string_as_char,value.length()+1);         
            closeHour=atoi(string_as_char);             
          }
          
          if(equalsCount==4)
          {
            char string_as_char[value.length()+1];
            value.toCharArray(string_as_char,value.length()+1);         
            timeHour=atoi(string_as_char);             
          }
          if(equalsCount==5){
            char string_as_char[value.length()+1];
            value.toCharArray(string_as_char,value.length()+1);         
            timeMin=atoi(string_as_char);          
          }          
             
         equalsCount++;
          value="";                  
       }
      }
      //Serial.print(Luminosity);
      return; 
}

