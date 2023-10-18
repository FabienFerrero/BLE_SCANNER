/*
   Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleScan.cpp
   Ported to Arduino ESP32 by Evandro Copercini
*/

#define PERIOD      10    // Period of scanning
#define ID_array    500  // Define current BLE ID array max size limit
#define WL_array    2000  // Define White list array max size of the array

//#define LORA    1      // Define if LoRa packet are sent to network

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

// Define LoRaWan OTAA credential

static const String devEUI = "70B3D57ED0061CBD";
static const String appEUI = "0000000000000000";
static const String appkey = "66C2C8CD4E20FA591592EE7F5C381F0E";

// Define UART Pin for RA3172
static const int rxPin = 20; 
static const int txPin = 21; 

// Parameters for BLE counting 
const int nullscan = 2; // number of undetected BLE scan to remove a device from the list
const int id_max_count=20; //number of continuous detection to move in the white list
const int margin_WL = 40; // margin added to the counter when a device is added to white list
const int inc_margin_WL = 5; // margin added to the counter when a device is added to white list   

int scanTime = 5; //In seconds
BLEScan* pBLEScan;

// Define arrays to store BLE ID and counter informations
static BLEAddress* id = (BLEAddress*)malloc(ID_array * sizeof(BLEAddress)); // array of BLEAddress detected
static BLEAddress* whitelist = (BLEAddress*)malloc(ID_array * sizeof(BLEAddress)); // array of BLEAddress detected


static int8_t cnt[ID_array]; // counter of presence for the BLEAddress detected
static int8_t undet[ID_array]; // counter of presence for the BLEAddress detected
static int8_t cntWL[WL_array]; // counter of presence for the BLEAddress detected
static int rssi[ID_array]; // last RSSI for the BLEAddress detected


static int id_num = 0; // number of BLE adress currently detected and not removed 
static int scan_num = 0; // number of BLE adress detected on the last scan
static int scan_new = 0; // number of new BLE adress detected on the last scan
static int scan_del = 0; // number of BLE address deleted on the last scan
static int cnt_wl = 0; // number of BLE address in the white list

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
     // Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
    }
};

void setup() {
  Serial.begin(115200);
  pinMode(txPin, OUTPUT);
  pinMode(rxPin, INPUT);

  pinMode(4, OUTPUT); // LED
  digitalWrite(4, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(200);                       // wait for a second
  digitalWrite(4, LOW);    // turn the LED off by making the voltage LOW
  delay(200);  

  #ifdef LORA
  pinMode(10, OUTPUT); //Rak enable  
  pinMode(1, OUTPUT); // GNSS enable
  digitalWrite(1, LOW);   // switch off GPS
  digitalWrite(10, HIGH); // Switch on RAK

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
  delay(1000);
   #endif
  

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(200);
  pBLEScan->setWindow(200);  // less or equal setInterval value
}

void loop() {
  // put your main code here, to run repeatedly:
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  
  checklist(foundDevices,id,cnt,undet,whitelist); // update the counters

  process_list(id,cnt,undet,whitelist); // clean the list with undetected device
   
  foundDevices.dump();

Serial.printf(" Short List of detected device \n");
int k=id_num;
if(id_num>10){
  k=10;}
  else{
    k=id_num;
    }
  
  
//  for (int i=0;i<id_num;i++){
for (int i=0;i<k;i++){
Serial.printf(" %x : ", *(id + i));
Serial.print(cnt[i]);
Serial.print(" : ");
Serial.print(undet[i]);
Serial.print(" RSSI: ");
Serial.println(rssi[i]);
delay(10);
  }

int m=cnt_wl;
if(cnt_wl>10){
  m=10;}
  else{
    m=cnt_wl;
    }  

  
Serial.printf(" List of White listed device \n");
  for (int i=0;i<m;i++){
Serial.printf(" %x : ", *(whitelist + i));
Serial.println(cntWL[i]);
delay(10);
  }  
  
 pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory

  Serial.print("Total counter: ");
  Serial.print(id_num);
   Serial.print(" Devices scan: ");
  Serial.print(scan_num);
   Serial.print(" Devices new: ");
  Serial.print(scan_new);
   Serial.print(" Devices del: ");
  Serial.print(scan_del);
   Serial.print(" Devices wl: ");
  Serial.println(cnt_wl);
  Serial.println("");
  delay(10);

   #ifdef LORA
     sendLora();
     #endif
     
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
void checklist(BLEScanResults foundDevices, BLEAddress* id, int8_t* cnt,int8_t* undet, BLEAddress* whitelist) {
  Serial.print("Devices found: ");
  scan_num = foundDevices.getCount();
  Serial.println(scan_num);  
for (int i=0;i<scan_num;i++){
    //Serial.printf("Advertised Device: %x RSSI :%i \n", foundDevices.getDevice(i).getAddress(),  foundDevices.getDevice(i).getRSSI()); 
    boolean flag=false;
    BLEAddress ble_id = foundDevices.getDevice(i).getAddress();
    //if(sizeof(ble_id) != 6){
    //  Serial.printf("Device with %d byte: %x \n",sizeof(ble_id), ble_id);
    //}

     if (checkWL(ble_id,whitelist)){ // if device exist in white list
      flag=true;
     // Serial.printf("Device in WL: %x \n", *(id + id_num));
         }
     else {

     for(int j=0;j<id_num;j++){ // check if the device already exist
        if(*(id + j) == ble_id){
       //    Serial.printf("Device exist: %x \n", *(id + id_num)); 
        flag = true; // The device exist
        cnt[j]=cnt[j]+1; // increment the counter
        undet[j]=0; //reset undetect counter
        rssi[j]= foundDevices.getDevice(i).getRSSI();
        if(cnt[j] > id_max_count){
          undet[j]=nullscan+1; //set to undetect to remove from list on the next check pass
           }
        }
            
      } //end for

 if(flag==false){ // The device do not exist, enter in the list      
        *(id + id_num) = foundDevices.getDevice(i).getAddress();
        cnt[id_num]=1; // set the counter
        rssi[id_num]= foundDevices.getDevice(i).getRSSI();
        undet[id_num]=0; // set undetect counter to 0
    //   Serial.printf("New Device detected: %x RSSI:", *(id + id_num));
    //   Serial.println(rssi[id_num]); 
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
   for (int i=0;i<cnt_wl ;i++){
    cntWL[i]=cntWL[i]-1; // decrease white list counter before new scan
  }
  // Reset state counters
  scan_num = 0; // number of BLE adress detected on the last scan
  scan_new = 0; // number of new BLE adress detected on the last scan
  scan_del = 0; // number of BLE adress deleted on the last scan  
}

// Function counter
// Update counter and BLE adress list
 
void process_list(BLEAddress* id, int8_t* cnt,int8_t* undet, BLEAddress* whitelist) {
  int j=0;
  int i=0;  
  while (j < id_num) { // process ID list
     *(id+j)=*(id+j+i); // copy next device ID in the list to current block in the array
      cnt[j]=cnt[j+i]; // copy next device counter in the list to current unit in the array
      undet[j]=undet[j+i]; // copy next device undetect counter in the list to current block in the array

    if (undet[j] > nullscan) { // if the device has not been detected for a nullscan time
       append_payload(j);
       appendWL(j, id,whitelist); // move device in white list 
    // Serial.printf("Remove Device : %x \n", *(id + j));      
      id_num=id_num-1; // decrease array length by 1 unit
      i++;      
      scan_del++; // increase delete counter
    }
    else{  // move to next unit  
    j++;
    }
  }

// process white list, if a device not scan for a long time, remove from the white list

  j=0;
  i=0;
  while (j < cnt_wl) { // process White list, if a device not scan for a long time, remove from the white list
   *(whitelist+j)=*(whitelist+j+i); // copy next device ID in the list to current block in the array
      cntWL[j]=cntWL[j+i]; // copy next device counter in the list to current unit in the array
    if (cntWL[j]==0) {
    cnt_wl=cnt_wl-1; 
   i++;
 //  Serial.printf("Remove from white list : %x \n", *(whitelist+j));
    }
    else {   
    j++;
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

void appendWL(int j, BLEAddress* id, BLEAddress* whitelist){
   if(checkWL(*(id + j), whitelist)==false){
    *(whitelist+cnt_wl) = *(id + j);
    cntWL[cnt_wl]=margin_WL; // define a high number to get some margin
    cnt_wl++;
   // Serial.printf("New device in White list : %x \n", *(id + j));    
   }
}

//////////////////////////////

 boolean checkWL(BLEAddress BLE, BLEAddress* whitelist) {
  int i=0;
  boolean flag = false;
  while (i < cnt_wl) {
    if (*(whitelist+i)==BLE){
      if(cntWL[i]<50){
      cntWL[i]=cntWL[i]+inc_margin_WL;}
     // Serial.printf("Device already in White list : %x \n", *(whitelist + i));
      flag=true;
      }
     i++; 
  }
  return flag;
 }
