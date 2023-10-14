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

int scanTime = 3; //In seconds
BLEScan* pBLEScan;

BLEAddress* id = (BLEAddress*)malloc(300); // array of BLEAddress detected
int8_t cnt[300]; // counter of presence for the BLEAddress detected
int8_t undet[300]; // counter of presence for the BLEAddress detected
int rssi[300]; // last RSSI for the BLEAddress detected

int id_num = 0; // number of BLE adress detected




class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
     // Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Scanning...");

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
  
  checklist(foundDevices);
  
  
  foundDevices.dump();

Serial.printf(" List of detected device with counter \n");
  for (int i=0;i<id_num;i++){
Serial.printf(" %x : ", *(id + i));
Serial.print(cnt[i]);
Serial.print(" : ");
Serial.print(undet[i]);
Serial.print(" RSSI: ");
Serial.println(rssi[i]);
  }
  
  Serial.println("Scan done!");
  pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory

  Serial.print("Devices counter: ");
  Serial.print(id_num);


  counter();
  delay(10000);
}


// Function check list
// Compare the new BLE scan with detected BLE list
// Increment counter and append BLE list if new device detected 
void checklist(BLEScanResults foundDevices) {
  Serial.print("Devices found: ");
  int count = foundDevices.getCount();
  Serial.println(count);
for (int i=0;i<count;i++){
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
        Serial.printf("New Device detected: %x \n", *(id + id_num));               
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

}
