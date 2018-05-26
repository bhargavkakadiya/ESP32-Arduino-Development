/*********************************************************************
 *  Sending data to web database using GET request on one core while                                            
 *  broadcasting a string message using BLE on other
 *                                                                   
 *  By Bhargav kakadiya                                                   
 *********************************************************************/
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

int TempPin = A18;    
int heartpin = A19;   

TaskHandle_t Task1;

const char* ssid = "master";
const char* password = "abcd1234";
const char* host = "maverick.000webhostapp.com";  //host address or IP

double alpha = 0.75;
int period = 200;
int sensorValue = 0;
double change = 0.0;

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
char txValue[21] = "Maverick 1 reporting";    //String to be broadcasted
//String length is of 20 but array length is 21 because we must include null character at end

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks: public BLEServerCallbacks {
void onConnect(BLEServer* pServer) {
deviceConnected = true;
};

void onDisconnect(BLEServer* pServer) {
  deviceConnected = false;
}
};

class MyCallbacks: public BLECharacteristicCallbacks {
void onWrite(BLECharacteristic *pCharacteristic) {
std::string rxValue = pCharacteristic->getValue();

  if (rxValue.length() > 0) {
    Serial.println("*********");
    Serial.print("Received Value: ");
    for (int i = 0; i < rxValue.length(); i++)
      Serial.print(rxValue[i]);

    Serial.println();
    Serial.println("*********");
  }
}
};


void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println("");
  Serial.println("WiFi Connected");
  Serial.println("IP Address");
  Serial.println(WiFi.localIP());
  Serial.println("Netmask");
  Serial.println(WiFi.subnetMask());

xTaskCreatePinnedToCore(
    BLEtask,            /* Task function. */
    "BLEtask",          /* name of task. */
    8000,                     /* Stack size of task */
    NULL,                     /* parameter of the task */
    1,                        /* priority of the task */
    &Task1,                   /* Task handle to keep track of created task */
    0);     

  // Create the BLE Device
BLEDevice::init("UART Service");

// Create the BLE Server
BLEServer *pServer = BLEDevice::createServer();
pServer->setCallbacks(new MyServerCallbacks());

// Create the BLE Service
BLEService *pService = pServer->createService(SERVICE_UUID);

// Create a BLE Characteristic
pCharacteristic = pService->createCharacteristic(
CHARACTERISTIC_UUID_TX,
BLECharacteristic::PROPERTY_NOTIFY
);

pCharacteristic->addDescriptor(new BLE2902());

BLECharacteristic *pCharacteristic = pService->createCharacteristic(
CHARACTERISTIC_UUID_RX,
BLECharacteristic::PROPERTY_WRITE
);

pCharacteristic->setCallbacks(new MyCallbacks());

// Start the service
pService->start();

// Start advertising
pServer->getAdvertising()->start();
Serial.println("Waiting a client connection to notify...");

}

void loop() {
  //reading temp
  sensorValue = analogRead(TempPin);
  float mv = ( sensorValue / 1024.0) * 5000;
  float temp = mv / 10;         //temperature value

  //reading heartbeat
  static double oldValue = 0;
  static double oldChange = 0;
  int rawValue = analogRead (heartpin);                             // Reading the sensors values
  double value = alpha * oldValue + (1 - alpha) * rawValue;         // calculating values using the formula
  double heart = ((value / 25) + 35);     //heartbeat value
  oldValue = value;
  delay(200);
  
  Serial.print("Connecting to ");
  Serial.println(host);
  WiFiClient client;

  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  
  String url = "/api/insert.php?temp=" + String(temp) + "&heart=" + String(heart);
  Serial.print("Requesting URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "Connection: close\r\n\r\n");
  delay(500);

  while (client.available()) {
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }

  Serial.println();
  Serial.println("closing connection");
  delay(1500);
}

void BLEtask(void * parameter){
  while(true) {
    if (deviceConnected) {
Serial.printf("*** Sent Value: %d ***\n", txValue);
pCharacteristic->setValue((unsigned char*)txValue, 20);
pCharacteristic->notify();
}
delay(1000);
  }
}

