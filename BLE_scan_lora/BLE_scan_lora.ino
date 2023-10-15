/*
   Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleScan.cpp
   Ported to Arduino ESP32 by Evandro Copercini
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#if CONFIG_PM_ENABLE
        esp_pm_config_esp32_t pm_config = {
                .max_freq_mhz = 80,
                .min_freq_mhz = 40,
    #if CONFIG_FREERTOS_USE_TICKLESS_IDLE
                .light_sleep_enable = true
    #endif
        };
        ESP_ERROR_CHECK( esp_pm_configure(&pm_config) );
    #endif

// ESP32 C3 SERIAL1 (second UART)
HardwareSerial mySerial1(1);


// Fefine LoRaWan ABP credential
String devAddr = "260B7039";
String nwkkey = "A1B199EDB9A19216EB738B724BE98ECF";
String appkey = "EE54EB30DC0F35653E06204DD5A86C92";

// Define UART Pin for RA3172
int rxPin = 20; 
int txPin = 21;    

int scanTime = 5; //In seconds
BLEScan* pBLEScan;

// Define arrays to store BLE ID and counter informations
BLEAddress* id = (BLEAddress*)malloc(300); // array of BLEAddress detected
int8_t cnt[300]; // counter of presence for the BLEAddress detected
int8_t undet[300]; // counter of presence for the BLEAddress detected
int rssi[300]; // last RSSI for the BLEAddress detected

uint8_t id_num = 0; // number of BLE adress currently detected and not removed
uint8_t scan_num = 0; // number of BLE adress detected on the last scan
uint8_t scan_new = 0; // number of new BLE adress detected on the last scan
uint8_t scan_del = 0; // number of BLE adress deleted on the last scan

int nullscan = 2; // number of undetected BLE scan to remove a device from the list

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
     // Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
    }
};

void setup() {
  Serial.begin(115200);
  pinMode(txPin, OUTPUT);
  pinMode(rxPin, INPUT);

  pinMode(10, OUTPUT); //Rak enable
  pinMode(4, OUTPUT); // LED
  pinMode(1, OUTPUT); // GNSS enable
  digitalWrite(1, LOW);   // switch off GPS
  digitalWrite(10, HIGH); // Switch on RAK
  digitalWrite(4, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(200);                       // wait for a second
  digitalWrite(4, LOW);    // turn the LED off by making the voltage LOW
  delay(200);  

  mySerial1.begin(115200, SERIAL_8N1, rxPin, txPin);

  Serial.println("Setup at command");
  mySerial1.println("AT+NWM=1"); // Set LoRaWan
  delay(100);
  mySerial1.println("AT+NJM=0"); // Set ABP
  delay(100);
  mySerial1.println("AT+CLASS=A"); // Set ClassA
  delay(100);
  mySerial1.println("AT+BAND=4");// Set EU868
  delay(100);
  mySerial1.println("AT+DR=5");// Set SF7  -  DR5
  delay(100);
  Serial.printf("Dev ADR = %s \n", devAddr);
  mySerial1.printf("AT+DEVADDR=%s\n",devAddr);
  delay(100);
  mySerial1.printf("AT+NWKSKEY=%s\n",nwkkey);
  delay(100);
  mySerial1.printf("AT+APPSKEY=%s\n",appkey);
  delay(100);
      while (mySerial1.available()){
      Serial.write(mySerial1.read()); // read it and send it out Serial (USB)
    }
  delay(1000);

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(false); //active scan uses more power, but get results faster
  pBLEScan->setInterval(200);
  pBLEScan->setWindow(200);  // less or equal setInterval value
}

void loop() {
  // put your main code here, to run repeatedly:
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  
  checklist(foundDevices); // update the counters

  process_list(); // clean the list with undetected device
   
  foundDevices.dump();

Serial.printf(" List of detected device with counter \n");
  for (int i=0;i<id_num;i++){
Serial.printf(" %x : ", *(id + i));
Serial.print(cnt[i]);
Serial.print(" : ");
Serial.print(undet[i]);
Serial.print(" RSSI: ");
Serial.println(rssi[i]);
delay(100);
  }
  
 pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory

  Serial.print("Devices counter: ");
  Serial.println(id_num);
  Serial.println("");
  delay(100);

  sendLora();
  counter();
  
  delay(50000);
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////



// Function check list
// Compare the new BLE scan with detected BLE list
// Increment counter and append BLE list if new device detected 
void checklist(BLEScanResults foundDevices) {
  Serial.print("Devices found: ");
 scan_num = foundDevices.getCount();
  Serial.println(scan_num);
for (int i=0;i<scan_num;i++){
    //Serial.printf("Advertised Device: %x RSSI :%i \n", foundDevices.getDevice(i).getAddress(),  foundDevices.getDevice(i).getRSSI()); 
    int flag=0;
      for(int j=0;j<=id_num;j++){ // check if the device already exist
        if(*(id + j) == foundDevices.getDevice(i).getAddress()){
        flag = 1; // The device exist
        cnt[j]=cnt[j]+1; // increment the counter
        undet[j]=0; //reset undetect counter
        rssi[j]= foundDevices.getDevice(i).getRSSI();
        }    
      } //end for
    if(flag==0){ // The device do not exist, enter in the list
        *(id + id_num) = foundDevices.getDevice(i).getAddress();
        cnt[id_num]=1; // increment the counter
        rssi[id_num]= foundDevices.getDevice(i).getRSSI();
        undet[id_num]=0; // set undetect counter to 0
        Serial.printf("New Device detected: %x \n", *(id + id_num)); 
        scan_new++;              
        id_num++; //iterate the number of device detected        
    } 
  } // end for 
}

// Function counter
// Update counter and BLE adress list
 
void counter() {
  for (int i=0;i<id_num;i++){
    undet[i]++; // increment undet counter before new scan
  }
  // Reset state counters
  scan_num = 0; // number of BLE adress detected on the last scan
  scan_new = 0; // number of new BLE adress detected on the last scan
  scan_del = 0; // number of BLE adress deleted on the last scan
  
}


// Function counter
// Update counter and BLE adress list
 
void process_list() {
  int j=0;
  int i=0;
  int num=id_num;
  
  while (j < num) {

    if (undet[j] > nullscan) {
       append_payload(j);
       j++;
       id_num=id_num-1;
       Serial.printf("Remove Device : %x \n", *(id + j));
       scan_del++;
    }
    else{
    *(id + i)=*(id + j);
    cnt[i]=cnt[j];
    undet[i]=undet[j];
    j++;
    i++;
    }
  }
}

// Function append_payload
// Update payload with BLE address and duration
 
void append_payload(int i) {
  
}



void array_to_string(byte array[], unsigned int len, char buffer[])
{
    for (unsigned int i = 0; i < len; i++)
    {
        byte nib1 = (array[i] >> 4) & 0x0F;
        byte nib2 = (array[i] >> 0) & 0x0F;
        buffer[i*2+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
        buffer[i*2+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
    }
    buffer[len*2] = '\0';
}

void sendLora(){

unsigned char mydata[4];
mydata[0] = (char) id_num; // actual counter
mydata[1] = (char) scan_num; // last scan counter
mydata[2] = (char) scan_new; // new device from last scan 
mydata[3] = (char) scan_del; // deleted device from last scan

char str[32] = "";
array_to_string(mydata, 4, str);
Serial.println(str);

mySerial1.printf("AT+SEND=3:");
mySerial1.println(str);
      while (mySerial1.available()){
      Serial.write(mySerial1.read()); // read it and send it out Serial (USB)
    }
  
   delay(3000);
  if (mySerial1.available())
  { // If anything comes in Serial1 (pins 4 & 5)
    while (mySerial1.available())
      Serial.write(mySerial1.read()); // read it and send it out Serial (USB)
  }
  delay(100);
  Serial.println("AT set complete with downlink");
  
}
