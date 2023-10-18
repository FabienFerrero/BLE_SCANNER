/*
   Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleScan.cpp
   Ported to Arduino ESP32 by Evandro Copercini
*/

#define PERIOD      50
#define ID_array    300
#define WL_array    1000

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

// Fefine LoRaWan OTAA credential

String devEUI = "70B3D57ED0061CBD";
String appEUI = "0000000000000000";
String appkey = "66C2C8CD4E20FA591592EE7F5C381F0E";

// Define UART Pin for RA3172
int rxPin = 20; 
int txPin = 21;    

int scanTime = 5; //In seconds
BLEScan* pBLEScan;

// Define arrays to store BLE ID and counter informations
BLEAddress* id = (BLEAddress*)malloc(ID_array * sizeof(BLEAddress)); // array of BLEAddress detected
BLEAddress* whitelist = (BLEAddress*)malloc(ID_array * sizeof(BLEAddress)); // array of BLEAddress detected


int8_t cnt[ID_array]; // counter of presence for the BLEAddress detected
int8_t undet[ID_array]; // counter of presence for the BLEAddress detected
int8_t cntWL[WL_array]; // counter of presence for the BLEAddress detected
int rssi[300]; // last RSSI for the BLEAddress detected


uint8_t id_num = 0; // number of BLE adress currently detected and not removed
uint8_t scan_num = 0; // number of BLE adress detected on the last scan
uint8_t scan_new = 0; // number of new BLE adress detected on the last scan
uint8_t scan_del = 0; // number of BLE address deleted on the last scan
int cnt_wl = 0; // number of BLE address in the white list

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
  delay(200);
  mySerial1.println("AT+NJM=1"); // Set OTAA
  delay(200);
  mySerial1.println("AT+CLASS=A"); // Set ClassA
  delay(200);
  mySerial1.println("AT+BAND=4");// Set EU868
  delay(200);
  mySerial1.println("AT+DR=5");// Set SF7  -  DR5
  delay(200);
    mySerial1.printf("AT+DEVEUI=");
    mySerial1.println(devEUI);
  delay(200);
  mySerial1.printf("AT+APPEUI=");
  mySerial1.println(appEUI);
  delay(200);
  mySerial1.printf("AT+APPKEY=");
  mySerial1.println(appkey);
  delay(200);
      while (mySerial1.available()){
      Serial.write(mySerial1.read()); // read it and send it out Serial (USB)
    }
  
  mySerial1.println("AT+JOIN=1:0:10:8"); // Join network
  delay(200);
  
  
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

Serial.printf(" List of detected device \n");
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
  
  // Sleep 50s
  delay(PERIOD*1000);
  
//  gpio_deep_sleep_hold_en(); // hold GPIO 
//  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUO_UART); // disable wake up source
//  //esp_sleep_enable_timer_wakeup(); 
//  esp_sleep_enable_timer_wakeup(10000000); // 50 sec


  
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
        if(cnt[j]==100){
          appendWL(j);
           }
        }
            
      } //end for
    if(flag==0){ // The device do not exist, enter in the list
       if (checkWL(i)){ // if device exist in white list
        *(id + id_num) = foundDevices.getDevice(i).getAddress();
        cnt[id_num]=1; // increment the counter
        rssi[id_num]= foundDevices.getDevice(i).getRSSI();
        undet[id_num]=0; // set undetect counter to 0             
        id_num++; //iterate the number of device detected
       }
       else {
        *(id + id_num) = foundDevices.getDevice(i).getAddress();
        cnt[id_num]=1; // increment the counter
        rssi[id_num]= foundDevices.getDevice(i).getRSSI();
        undet[id_num]=0; // set undetect counter to 0
        Serial.printf("New Device detected: %x \n", *(id + id_num)); 
        scan_new++;              
        id_num++; //iterate the number of device detected
       }        
    } 
  } // end for 
}

// Function counter
// Update counter and BLE address list
 
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
       appendWL(j);
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

/////////////////////////////////////////////////////////

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

/////////////////////////////////////

void sendLora(){

unsigned char mydata[5];
mydata[0] = (char) id_num; // actual counter
mydata[1] = (char) scan_num; // last scan counter
mydata[2] = (char) scan_new; // new device from last scan 
mydata[3] = (char) scan_del; // deleted device from last scan
mydata[4] = (char) cnt_wl; // white list counter

char str[32] = "";
array_to_string(mydata, 5, str);
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

//////////////////////////////////////

void appendWL(int j){
  int i=0;
  while (i < cnt_wl) {
    if (*(whitelist+i)==*(id + j)){
      cntWL[i]=cntWL[i]+cnt[j];
      i=9999;
      }
     else{
      i++;
      }  
  }
   if(i==cnt_wl){
    *(whitelist+i) = *(id + j);
    cntWL[i]=cnt[j];
    cnt_wl++;
   }
}

//////////////////////////////
 boolean checkWL(int j) {
  int i=0;
  while (i < cnt_wl) {
    if (*(whitelist+i)==*(id + j)){
      cntWL[i]=cntWL[i]+cnt[j];
      i=9999;
      }
     else{
      i++;
      }  
  }
  if(i==cnt_wl){
    return false;    
    }
    else {
      return true;
      } 
 }
