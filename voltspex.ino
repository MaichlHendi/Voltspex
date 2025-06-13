#include "BLEDevice.h"
#include <stdio.h>
#include <unistd.h>

// Server name
#define bleServerName "BDM"

// BLE Service UUID
static BLEUUID bleServiceUUID("0000fff0-0000-1000-8000-00805f9b34fb");

// BLE Characteristics UUID
// #ifdef value
static BLEUUID valueCharacteristicUUID("0000fff4-0000-1000-8000-00805f9b34fb");
// #else
//  static BLEUUID valueCharacteristicUUID("0000fff4-0000-1000-8000-00805f9b34fb");
// #endif

// Flags stating if should begin connecting and if the connection is up
static boolean doConnect = false;
static boolean doScan = true;

// Address of the peripheral device. Address will be found during scanning
static BLEAddress *pServerAddress;

// Characteristic that we want to read
static BLERemoteCharacteristic* valueCharacteristic;

// Activate notify
const uint8_t notificationOn[] = {0x1, 0x0};
const uint8_t notificationOff[] = {0x0, 0x0};

// Variable to store value
uint16_t value;

int units = 0;
int function, scale, decimal;

float measurement;

char scaleType;
char unitType[5];
char unit;

static boolean testConnect = true;

BLEScan* pBLEScan;

// Flag to check wether new value readings are available
boolean newValue = false;


// Outputs the measurement units
static void printUnits(int scale, int function) {

    switch (scale) {
        case 1:
            scaleType = 'n';
            break;

        case 2:
            scaleType = 'u';
            break;

        case 3:
            scaleType = 'm';
            break;

        case 5:
            scaleType = 'k';
            break;

        case 6:
            scaleType = 'M';
            break;
    }

    switch (function) {

        case 0:
            strcpy(unitType, "Vdc");
            break;

        case 1:
            strcpy(unitType, "Vac");
            break;

        case 2:
            strcpy(unitType, "Adc");
            break;

        case 3:
            strcpy(unitType, "Aac");
            break;

        case 4:
            strcpy(unitType, "\u2126");
            break;

        case 5:
            strcpy(unitType, "F");
            break;

        case 6:
            strcpy(unitType, "Hz");
            break;

        case 7:
            strcpy(unitType, "%%");
            break;

        case 8:
            strcpy(unitType, "°C");
            break;

        case 9:
            strcpy(unitType, "°F");
            break;

        case 10:
            strcpy(unitType, "V");
            break;

        case 11:
            strcpy(unitType, "Ohms");
            break;

        case 12:
            strcpy(unitType, "hFE");
            break;
    }

    switch (function) {

      case 0:
        unit = 'V';
        break;
      
      case 1:
        unit = 'V';
        break;
      
      case 2:
        unit = 'A';
        break;

      case 3:
        unit = 'A';
        break;

      case 4:
        unit = '\u2126';
        break;
    }
}


static void displayReading(uint16_t* reading) {

  function = (reading[0] >> 6) & 0x0f;
  scale = (reading[0] >> 3) & 0x07;
  decimal = reading[0] & 0x07;

  printUnits(scale, function);

  // Extract and convert measurement value
  if (reading[2] < 0x7fff) {
      measurement = (float)reading[2] / pow(10.0, decimal);
  } else {
      measurement = -1 * (float)(reading[2] & 0x7fff) / pow(10.0, decimal);
  }
}


// Callback function that gets called, when another device's advertisment has been received
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {

  void onResult(BLEAdvertisedDevice advertisedDevice) {
    
    if (strcmp(advertisedDevice.getName().c_str(), bleServerName) == 0) {
      advertisedDevice.getScan()->stop();  // Scan can be stopped, we found what we are looking for
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());  // Address of advertiser is the one we need
      doConnect = true;  // Set indicator, stating that we are ready to connect
      Serial.println("Device found. Connecting!");
      advertisedDevice.getScan()
          
    }
  }
};


class MyClientCallback : public BLEClientCallbacks {

  void onConnect(BLEClient* pClient) {

    Serial.println("Connected to Device...");

  }

  void onDisconnect(BLEClient* pClient) {

    Serial.println("Disconnected from device...\n");

    // Serial.println("Attempting Reconnection...");
    doScan = true;
  
  }
};


// Connect to the BLE Sever that has the name. Service, and Characteristics
bool connectToServer(BLEAddress pAddress) {

  BLEClient* pClient = BLEDevice::createClient();

  pClient->setClientCallbacks(new MyClientCallback());
  pClient->connect(pAddress);

  // Obtain a reference to the service we are after in the remote BLE server
  BLERemoteService* pRemoteService = pClient->getService(bleServiceUUID);

  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(bleServiceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  
  }

  // Obtain a reference to the characteristics in the service of the remote BLE server
  valueCharacteristic = pRemoteService->getCharacteristic(valueCharacteristicUUID);

  if (valueCharacteristic == nullptr) {

    Serial.print("Failed to find our characteristic UUID");
    pClient->disconnect();
    return false;
  
  }

  // Assign callback functions for the Characteristics
  valueCharacteristic->registerForNotify(valueNotifyCallback);
  return true;

}


// When the BLE Server sends a new temperature reading with the notify property
static void valueNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  
  // Store multimeter value
  if ((length == 6) && (pData[1] >= 0xf0)) {
    displayReading((uint16_t*)pData);
  }
  newValue = true;

}


void printReadings() {
  
  if (measurement == 0.00) {
    Serial.print("Value: ");
    Serial.println("Overload");
  } 
  else {
    Serial.print("Unit Type: ");
    Serial.print(unitType);
    Serial.print("  |  Value: ");
    Serial.print(measurement);
    Serial.print(scaleType);
    Serial.println(unit);
  }
}


void setup() {

  // Start serial communication
  Serial.begin(9600);
  Serial.println("Starting ESP32 BLE Client application...");

  // Init BLE device
  BLEDevice::init("");

  pBLEScan = BLEDevice::getScan();

}


void loop() {

  if (doScan) {

    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setActiveScan(true);
    pBLEScan->start(0);
    doScan = false;
  
  }

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToServer(*pServerAddress)) {
      // Serial.println("We are now connected to the BLE Server.");

      // Activate the Notify property of each Characteristic
      valueCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
    }
    doConnect = false;
  }
  // if new value readings are available, print in serial monitor
  if (newValue) {
    newValue = false;
    printReadings();
  }
  delay(1000);  // Delay a second between loops.

}
