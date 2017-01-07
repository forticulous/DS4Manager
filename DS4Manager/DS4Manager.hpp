//
//  DS4Manager.hpp
//  DS4Manager
//

#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDLib.h>

struct mouse_report_t {
    uint8_t buttons;
    int8_t x;
    int8_t y;
};

class DS4Manager {
public:
    DS4Manager() {
        ioHidManagerRef = nullptr;
        virtualHidConnection = IO_OBJECT_NULL;
    }
    
    ~DS4Manager() {
        closeHidManager();
        destroyVirtualDevice();
    }
    
    void start();
    
    void sendVirtualDeviceReport(mouse_report_t mouse);
private:
    void setupHidManager();
    void openHidManager();
    void closeHidManager();
    
    void loadVirtualDevice();
    void createVirtualDevice();
    void destroyVirtualDevice();
    
    const int vid = 1356;
    const int pid = 1476;
    const char* virtualHidServiceName = "it_unbit_foohid";
    const char* virtualDeviceName = "Foohid Virtual Mouse";
    const char* virtualDeviceSn = "SN 123456";
    const int FOOHID_CREATE = 0;
    const int FOOHID_DESTROY = 1;
    const int FOOHID_SEND = 2;
    
    IOHIDManagerRef ioHidManagerRef; // reads data from controller
    io_connect_t virtualHidConnection; // connection to virtual mouse
};
