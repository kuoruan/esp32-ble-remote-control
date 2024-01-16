#include <NimBLECharacteristic.h>
#include <NimBLEServer.h>

#include <string>

class BleRemote {
   public:
    void init(const char *deviceName);
    void loop();
    bool isConnected();

    void startAdvertising();
    void stopAdvertising();

   private:
    std::string _deviceName;
    bool _isConnected = false;
    bool _isAdvertising = false;
    NimBLEServer *_server;
    void fillAdvertisingData(NimBLEAdvertisementData &advertisingData);
};
