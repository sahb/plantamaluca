/*
  PlantaMaluca v0.1
  =================

  by Rafael Sahb (rafael.sahb@gmail.com)
  
  Arduino gardening system allowing:
  - collect data on soil moisture and light intensity over http
  - control a water pump attached to a PSU to water the plants
  - set new inferior and superior limits to water the plants over http



  - TO BE USED WITH AN ARDUINO MEGA -

  
  - Based on https://github.com/gordonturner/ArduinoWebServerTemperatureSensor
 
  
*/

#include <SPI.h>

// Use "UIPEthernet.h" if you use ENC28J60 shields
// Use "Ethernet.h" if you use compatible shields
#include <UIPEthernet.h>


//Set 1 if you want to use static IP
#define STATICIP 1

#if STATICIP
// IP address, will depend on your local network.
IPAddress ip(192,168,25,55);
#endif




// Ethernet Shield MAC address, use value on back of shield.
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0D, 0x10, 0x5A };



// Initialize the Ethernet server library, use port 80 for HTTP.
EthernetServer server(80);

// String for reading from client
String request = String(100);
String parsedRequest = "";



// Light Sensor (A0)
#define LDRPIN 0
int ldrValue = 0;


// Soil Moisture Sensor (A1)
#define SOILPIN 1
int soilValue = 0;


// Relay controlling PSU
#define PSURELAYPIN 13
int psuRelayValue = 1; // 1 means relay off

// Relay controlling water pump
#define WATERRELAYPIN 12
int waterRelayValue = 1; // 1 means relay off

// The inferior limit threshold to start watering
int moistureInferiorLimit = 1000;

// the superior limit threshold to stop watering (flooding!)
int moistureSuperiorLimit = 500;

// interval in millisseconds for self monitoring when idle
unsigned long monitorInterval = 10000;

// interval in millisseconds for self monitoring when watering
unsigned long wateringMonitorInterval = 500;

// timer used for millis() to run the self monitor
unsigned long previousMillisMain = 0;

// timer used for millis() to run the self monitor when watering
unsigned long previousMillisWatering = 0;


// NOTE: Serial debugging code has been disabled.

void setup() 
{
  // Open Serial communications and wait for port to open.
  Serial.begin(9600);

  Serial.println("Welcome to the PlantaMaluca project");
 
  Serial.println("Setting up Ethernet");

  // Start the Ethernet connection and the server.

#if STATICIP
  int statusConnection = Ethernet.begin(mac);
#else
  int statusConnection = Ethernet.begin(mac,ip);
#endif
  
  if(statusConnection!=1) {
    Serial.println("Error connecting device to network!");
  }
  server.begin();
  
  Serial.print("Server is at ");
  Serial.println(Ethernet.localIP());

  // setting up initial state of the relays
  Serial.print("Configuring relays... ");
  pinMode(PSURELAYPIN, OUTPUT);
  pinMode(WATERRELAYPIN, OUTPUT);
  digitalWrite(PSURELAYPIN,1);
  digitalWrite(WATERRELAYPIN,1);

  Serial.println("done");
  Serial.println("-- All set! We are good to go! --");
}


void loop() 
{
  
  // Listen for incoming clients.
  EthernetClient client = server.available();

  //Dealing with incoming http requests
  if (client) 
  {
    Serial.println("Client connected");
  
    // An http request ends with a blank line.
    boolean currentLineIsBlank = true;

    // Reading sensors
    readSensors();
    
    
    while (client.connected()) 
    {
      if (client.available()) 
      {
        char c = client.read();
        
        // Read http request.
        if (request.length() < 100) 
        {
          request += c; 
        } 
      
        // If you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply.
        if (c == '\n' && currentLineIsBlank) 
        {
          Serial.println("Finished reading request.");
          Serial.println("http request: '" + request + "'");
          
          // Response looks like:
          // GET /?format=JSONP HTTP/1.1
          // Host: 192.168.2.70
          // User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X

          if (request.startsWith("GET /?format=")) 
          {
            parsedRequest = request.substring(request.indexOf('format=')+1, request.indexOf("HTTP")-1);
            Serial.println("Parsed request: '" + parsedRequest + "'");

            //Setting dummy response
            String dummy = "CASAS BAHIA";
            String error ="";
          
          
            if( parsedRequest == "XML" )
            {
              sendXmlResponse(client, dummy, error);
            }
            else if(parsedRequest.startsWith("JSONP"))
            {
              // Parse callback argument.
              String callback = parsedRequest.substring(parsedRequest.indexOf('callback=')+1, parsedRequest.length());
              callback = callback.substring(0, callback.indexOf('&'));
            
              Serial.println("Using callback: " + callback);
            
              sendJsonpResponse(client, dummy, error, callback);
            }
            else
            {
              // Default to JSON.
              sendJsonResponse(client, dummy, error);
            }
          } 
          else if (request.startsWith("GET /?action="))
          {
            parsedRequest = request.substring(request.indexOf('action=')+1, request.indexOf("HTTP")-1);
            Serial.println("Parsed request: '" + parsedRequest + "'");

            //Setting default response
            String error = "";

            if(parsedRequest.startsWith("newsup"))
            {
              // Parse value argument.
              String newValue = parsedRequest.substring(parsedRequest.indexOf('value=')+1, parsedRequest.length());
              newValue = newValue.substring(0, newValue.indexOf('&'));
            
              Serial.println("Using value: " + newValue);

              // setting the new value
              moistureSuperiorLimit = newValue.toInt();
            
              sendJsonResponse(client, dummy, error);
            } else if(parsedRequest.startsWith("newinf"))
            {
              // Parse value argument.
              String newValue = parsedRequest.substring(parsedRequest.indexOf('value=')+1, parsedRequest.length());
              newValue = newValue.substring(0, newValue.indexOf('&'));
            
              Serial.println("Using value: " + newValue);

              // setting the new value
              moistureInferiorLimit = newValue.toInt();
            
              sendJsonResponse(client, dummy, error);
            } else {
              error = "invalid action provided";

              sendJsonResponse(client, dummy, error);
            }
          }
          
          break;
        }
        
        if (c == '\n') 
        {
          // Character is a new line.
          currentLineIsBlank = true;
        } 
        else if (c != '\r') 
        {
          // Character is not a new line or a carriage return.
          currentLineIsBlank = false;
        }
        
      }
      
    }
    
    // Give the web browser time to receive the data.
    delay(1);
    
    // Close the connection:
    client.stop();
    
    Serial.println("Client disconnected");
  }






  //Auto check routine
  unsigned long currentMillis = millis();

  if(waterRelayValue == 0)
  {
    if(currentMillis - previousMillisWatering > wateringMonitorInterval)
    {
      previousMillisWatering = currentMillis;
      readSensors();

      if(soilValue < moistureSuperiorLimit)
      {
        stopWatering();
      }
    }
  }
  
  if(currentMillis - previousMillisMain > monitorInterval)
  {
    previousMillisMain = currentMillis;
    readSensors();

    //checking if soil moisture is below inferior threshold
    if(soilValue > moistureInferiorLimit){
      if(waterRelayValue == 1)
      {
        startWatering(); 
      }
      
      previousMillisWatering = currentMillis;
    }
  }
  



  
  // Reset the request.
  request = "";
  parsedRequest = "";
}


/*
 *
 */
void sendXmlResponse(EthernetClient client, String dummy, String error)
{
  if(error == "")
  {
    Serial.println("Sending XML response.");
    
    // Send a standard http response header.
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/xml");
    client.println("Connnection: close");
    client.println();
    
    // Send XML body.
    client.println("<?xml version=\"1.0\"?>");
    client.println("<xml>");
    client.println("<dummy>");
    client.println(dummy);
    client.println("</dummy>");            
    client.println("<LDR>");
    client.println(ldrValue);
    client.println("</LDR>");
    client.println("<soil>");
    client.println(soilValue);
    client.println("</soil>");
    client.println("<sup_limit>");
    client.println(moistureSuperiorLimit);
    client.println("</sup_limit>");
    client.println("<inf_limit>");
    client.println(moistureInferiorLimit);
    client.println("</inf_limit>");
    client.println("</xml>");
  }
  else
  {
    Serial.println("Sending XML error response.");
    
    // Send a standard http response header.
    client.println("HTTP/1.1 400 BAD REQUEST");
    client.println("Content-Type: text/xml");
    client.println("Connnection: close");
    client.println();
    
    // Send XML body.
    client.println("<?xml version=\"1.0\"?>");
    client.println("<xml>");  
    client.println("<dummy>");
    client.println(error);
    client.println("</dummy>");        
    client.println("</xml>");            
  }
}


/*
 *
 */
void sendJsonpResponse(EthernetClient client, String dummy, String error, String callback)
{
  
  if(error == "")
  {
    Serial.println("Sending JSONP response.");
    
    // Send a standard http response header.
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connnection: close");
    client.println();
    
    // Send JSONP body.
    client.println(callback + "({");
    client.print("  \"dummy\": \"");
    client.print(dummy);
    client.println("\",");
    client.print("  \"ldr\": \"");
    client.print(ldrValue);
    client.println("\",");
    client.print("  \"soil\": \"");
    client.print(soilValue);
    client.println("\",");
    client.print("  \"inf_limit\": \"");
    client.print(moistureInferiorLimit);
    client.println("\",");
    client.print("  \"inf_limit\": \"");
    client.print(moistureSuperiorLimit);
    client.println("\"");
    client.print("});");
  }
  else
  {
    Serial.println("Sending JSONP error response.");
    
    // Send a standard http response header.
    client.println("HTTP/1.1 400 BAD REQUEST");
    client.println("Content-Type: application/json");
    client.println("Connnection: close");
    client.println();
    
    // Send JSONP body.
    client.println(callback + "({");
    client.println("  \"error\": \"" + error + "\"");
    client.println("})");     
  }
}


/*
 *
 */
void sendJsonResponse(EthernetClient client, String dummy, String error)
{
  if(error == "")
  {
    Serial.println("Sending JSON response.");
    
    // Send a standard http response header.
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: application/json");
    client.println("Connnection: close");
    client.println();
    
    // Send JSON body.
    client.println("{");
    client.print("  \"dummy\": \"");
    client.print(dummy);
    client.println("\",");
    client.print("  \"ldr\": \"");
    client.print(ldrValue);
    client.println("\",");
    client.print("  \"soil\": \"");
    client.print(soilValue);
    client.println("\",");
    client.print("  \"inf_limit\": \"");
    client.print(moistureInferiorLimit);
    client.println("\",");
    client.print("  \"inf_limit\": \"");
    client.print(moistureSuperiorLimit);
    client.println("\"");
    client.println("}");
  }
  else
  {
    Serial.println("Sending JSON error response.");
    
    // Send a standard http response header.
    client.println("HTTP/1.1 400 BAD REQUEST");
    client.println("Content-Type: application/json");
    client.println("Connnection: close");
    client.println();
    
    // Send JSON body.
    client.println("{");
    client.println("  \"error\": \"" + error + "\"");        
    client.println("}");            
  }
}

void readSensors() {
  Serial.println("Reading sensors...");
  
  //Reading LDR
  ldrValue = analogRead(LDRPIN);
  Serial.print("Reading LDR ----- ");
  Serial.println(ldrValue);

  //Reading Soil Moisture
  soilValue = analogRead(SOILPIN);
  Serial.print("Reading Soil ---- ");
  Serial.println(soilValue);
  Serial.println();
}

void startWatering() {
  Serial.println("Watering sequence initiated...");
  psuRelayValue = 0;
  waterRelayValue = 0;
  Serial.print("Turning on PSU ----------- ");
  digitalWrite(PSURELAYPIN,0);
  //giving time to turn on
  delay(1000);
  Serial.println("OK");
  Serial.print("Turning on water pump ---- ");
  digitalWrite(WATERRELAYPIN,0);
  Serial.println("OK");
  
  // pretending to be cult %)
  Serial.println();
  Serial.println("   \"Apres moi le deluge\" %)");
  Serial.println();
  Serial.println();
}

void stopWatering() {
  Serial.println("Terminating watering sequence...");
  psuRelayValue = 1;
  waterRelayValue = 1;
  Serial.print("Turning off water pump ---- ");
  digitalWrite(WATERRELAYPIN,1);
  Serial.println("OK");
  //giving time to turn off
  delay(1000);
  Serial.print("Turning off PSU ----------- ");
  digitalWrite(PSURELAYPIN,1);
  Serial.println("OK");
  

  // pretending to be cult %)
  Serial.println();
  Serial.println("   \"I'll be back\" %)");
  Serial.println();
  Serial.println();
}


