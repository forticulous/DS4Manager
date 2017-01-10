//
//  DS4Manager.cpp
//  DS4Manager
//

#include <iostream>

#include "DS4Manager.hpp"

static void Handle_DeviceMatchingCallback(void* context, IOReturn result, void* sender, IOHIDDeviceRef iOHIDDeviceRef);
static void Handle_DeviceRemovalCallback(void* context, IOReturn result, void* sender, IOHIDDeviceRef iOHIDDeviceRef);
static void Handle_InputReportCallback(void* context, IOReturn result, void* sender, IOHIDReportType type, uint32_t reportID, uint8_t *report, CFIndex reportLength);

unsigned char report_descriptor[] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x02,                    // USAGE (Mouse)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x09, 0x01,                    //   USAGE (Pointer)
    0xa1, 0x00,                    //   COLLECTION (Physical)
    0x05, 0x09,                    //     USAGE_PAGE (Button)
    0x19, 0x01,                    //     USAGE_MINIMUM (Button 1)
    0x29, 0x03,                    //     USAGE_MAXIMUM (Button 3)
    0x15, 0x00,                    //     LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //     LOGICAL_MAXIMUM (1)
    0x95, 0x03,                    //     REPORT_COUNT (3)
    0x75, 0x01,                    //     REPORT_SIZE (1)
    0x81, 0x02,                    //     INPUT (Data,Var,Abs)
    0x95, 0x01,                    //     REPORT_COUNT (1)
    0x75, 0x05,                    //     REPORT_SIZE (5)
    0x81, 0x03,                    //     INPUT (Cnst,Var,Abs)
    0x05, 0x01,                    //     USAGE_PAGE (Generic Desktop)
    0x09, 0x30,                    //     USAGE (X)
    0x09, 0x31,                    //     USAGE (Y)
    0x15, 0x81,                    //     LOGICAL_MINIMUM (-127)
    0x25, 0x7f,                    //     LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //     REPORT_SIZE (8)
    0x95, 0x02,                    //     REPORT_COUNT (2)
    0x81, 0x06,                    //     INPUT (Data,Var,Rel)
    0xc0,                          //   END_COLLECTION
    0xc0                           // END_COLLECTION
};

void DS4Manager::start() {
    std::cout << "Load virtual hid device" << std::endl;
    
    loadVirtualDevice();
    createVirtualDevice();
    
    std::cout << "Starting..." << std::endl;
    
    setupHidManager();
    
    IOHIDManagerScheduleWithRunLoop(ioHidManagerRef, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    
    std::cout << "Scheduled HID manager with run loop" << std::endl;
    
    openHidManager();
    
    std::cout << "Opened HID manager" << std::endl;
    
    std::cout << "Waiting for device to connect" << std::endl;
    
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 15, true);
    
    std::cout << "Done waiting for device to connect" << std::endl;
}

void DS4Manager::setupHidManager() {
    ioHidManagerRef = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDManagerOptionNone);
    
    if (CFGetTypeID(ioHidManagerRef) != IOHIDManagerGetTypeID()) {
        std::cout << "Failed to create HID Manager" << std::endl;
        exit(1);
    }
    
    std::cout << "Created HID Manager" << std::endl;
    
    {
        CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        
        if (dict == NULL) {
            std::cout << "Failed to create dictionary" << std::endl;
            exit(1);
        }
        
        {
            CFNumberRef vidRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vid);
            CFDictionarySetValue(dict, CFSTR(kIOHIDVendorIDKey), vidRef);
            CFRelease(vidRef);
        }
        {
            CFNumberRef pidRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &pid);
            CFDictionarySetValue(dict, CFSTR(kIOHIDProductIDKey), pidRef);
            CFRelease(pidRef);
        }
        
        IOHIDManagerSetDeviceMatching(ioHidManagerRef, dict);
        
        std::cout << "Told HID manager what kind of device to look for" << std::endl;
        
        CFRelease(dict);
    }
    
    IOHIDManagerRegisterDeviceMatchingCallback(ioHidManagerRef, Handle_DeviceMatchingCallback, this);
    IOHIDManagerRegisterDeviceRemovalCallback(ioHidManagerRef, Handle_DeviceRemovalCallback, this);
    IOHIDManagerRegisterInputReportCallback(ioHidManagerRef, Handle_InputReportCallback, this);
    
    std::cout << "Registered callbacks" << std::endl;
}

void DS4Manager::openHidManager() {
    IOReturn ret = IOHIDManagerOpen(ioHidManagerRef, kIOHIDOptionsTypeNone);
    
    if (ret != kIOReturnSuccess) {
        std::cout << "Failed to open HID manager" << std::endl;
        exit(1);
    }
}

void DS4Manager::closeHidManager() {
    if (ioHidManagerRef == nullptr) {
        return;
    }
    
    IOReturn ret = IOHIDManagerClose(ioHidManagerRef, kIOHIDOptionsTypeNone);
    
    if (ret != kIOReturnSuccess) {
        std::cout << "Failed to close HID manager" << std::endl;
    }
}

void DS4Manager::loadVirtualDevice() {
    io_service_t service;
    io_iterator_t iterator;
    
    // Get a reference to the IOService
    kern_return_t ret = IOServiceGetMatchingServices(kIOMasterPortDefault, IOServiceMatching(virtualHidServiceName), &iterator);
    
    if (ret != KERN_SUCCESS) {
        std:: cout << "Unable to access virtual hid driver" << std::endl;
        exit(1);
    }
    
    // Iterate till success
    int found = 0;
    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        ret = IOServiceOpen(service, mach_task_self(), 0, &virtualHidConnection);
        
        if (ret == KERN_SUCCESS) {
            found = 1;
            break;
        }
        
        IOObjectRelease(service);
    }
    IOObjectRelease(iterator);
    
    if (!found) {
        std::cout << "Unable to open virtual hid driver" << std::endl;
        exit(1);
    }
}

void DS4Manager::createVirtualDevice() {
    // Fill up the input arguments.
    uint32_t input_count = 8;
    uint64_t input[input_count];
    input[0] = (uint64_t) strdup(virtualDeviceName);  // device name
    input[1] = strlen((char *)input[0]);  // name length
    input[2] = (uint64_t) report_descriptor;  // report descriptor
    input[3] = sizeof(report_descriptor);  // report descriptor len
    input[4] = (uint64_t) strdup(virtualDeviceSn);  // serial number
    input[5] = strlen((char *)input[4]);  // serial number len
    input[6] = (uint64_t) 2;  // vendor ID
    input[7] = (uint64_t) 3;  // device ID
    
    kern_return_t ret = IOConnectCallScalarMethod(virtualHidConnection, FOOHID_CREATE, input, input_count, NULL, 0);
    if (ret != KERN_SUCCESS) {
        std::cout << "Unable to create HID device. May be fine if created previously" << std::endl;
    }
}

void DS4Manager::sendVirtualDeviceReport(mouse_report_t mouse) {
    if (virtualHidConnection == IO_OBJECT_NULL) {
        return;
    }
    
    // Arguments to be passed through the HID message.
    uint32_t send_count = 4;
    uint64_t send[send_count];
    send[0] = (uint64_t) strdup(virtualDeviceName);  // device name
    send[1] = std::strlen(virtualDeviceName);  // name length
    send[2] = (uint64_t) &mouse;  // mouse struct
    send[3] = sizeof(struct mouse_report_t);  // mouse struct len
    
    kern_return_t ret = IOConnectCallScalarMethod(virtualHidConnection, FOOHID_SEND, send, send_count, NULL, 0);
    if (ret != KERN_SUCCESS) {
        std::cout << "Unable to send message to HID device" << std::endl;
    }
}

void DS4Manager::destroyVirtualDevice() {
    if (virtualHidConnection == IO_OBJECT_NULL) {
        return;
    }
    
    uint32_t destroy_count = 2;
    uint64_t destroy[destroy_count];
    destroy[0] = (uint64_t) strdup(virtualDeviceName);  // device name
    destroy[1] = strlen((char *)destroy[0]);  // name length
    
    kern_return_t ret = IOConnectCallScalarMethod(virtualHidConnection, FOOHID_DESTROY, destroy, destroy_count, NULL, 0);
    if (ret != KERN_SUCCESS) {
        std::cout << "Unable to remove HID device" << std::endl;
    }
}

static void Handle_DeviceMatchingCallback(void* context,
                                          IOReturn result,
                                          void* sender,
                                          IOHIDDeviceRef iOHIDDeviceRef) {
    std::cout << "Device connected" << std::endl;
    
    std::cout << "Starting run loop" << std::endl;
    
    CFRunLoopRun();
}

static void Handle_DeviceRemovalCallback(void* context,
                                         IOReturn result,
                                         void* sender,
                                         IOHIDDeviceRef iOHIDDeviceRef) {
    std::cout << "Device disconnected" << std::endl;
    
    CFRunLoopStop(CFRunLoopGetCurrent());
    
    std::cout << "Run loop stopped" << std::endl;
}

static void Handle_InputReportCallback(void* context,
                                       IOReturn result,
                                       void* sender,
                                       IOHIDReportType type,
                                       uint32_t reportID,
                                       uint8_t *report,
                                       CFIndex reportLength) {
    if (context == nullptr) {
        return;
    }
    
    const uint8_t leftStickXIdx = 1;
    const uint8_t leftStickYIdx = 2;
    const uint8_t buttonsIdx = 5;
    
    const uint8_t xButtonMask = 0b00100000;
    const uint8_t oButtonMask = 0b01000000;
    const uint8_t mouseButton1Mask = 0b00000001; //0000 0001
    const uint8_t mouseButton3Mask = 0b00000010; //0000 0010
    
    const int8_t fuzzFactor = 2;
    
    bool mouseMoved = false;
    bool buttonPressed = false;
    mouse_report_t mouseReport;
    mouseReport.buttons = 0;
    mouseReport.x = 0;
    mouseReport.y = 0;
    
    {
        int reportButtons = (int)report[buttonsIdx];
        if ((reportButtons & xButtonMask) == xButtonMask) {
            std::cout << "X Button Pressed" << std::endl;
            mouseReport.buttons = mouseReport.buttons | mouseButton1Mask;
            buttonPressed = true;
        }
        if ((reportButtons & oButtonMask) == oButtonMask) {
            std::cout << "O Button Pressed" << std::endl;
            mouseReport.buttons = mouseReport.buttons | mouseButton3Mask;
            buttonPressed = true;
        }
    }
    
    {
        int reportLeftStickX = (int)report[leftStickXIdx];
        if (reportLeftStickX < 125 ||
            reportLeftStickX > 135) {
            std::cout << "left stick x: " << reportLeftStickX << std::endl;
            mouseMoved = true;
        }
        // 0 max left, 255 max right
        mouseReport.x = (int8_t) reportLeftStickX - 128;
        mouseReport.x = mouseReport.x / fuzzFactor;
    }
    
    {
        int reportLeftStickY = (int)report[leftStickYIdx];
        if (reportLeftStickY < 125 ||
            reportLeftStickY > 135) {
            std::cout << "left stick y: " << reportLeftStickY << std::endl;
            mouseMoved = true;
        }
        // 0 max up, 255 max down
        mouseReport.y = (int8_t) reportLeftStickY - 128;
        mouseReport.y = mouseReport.y / fuzzFactor;
    }
    
    if (mouseMoved || buttonPressed) {
        ((DS4Manager*)context)->sendVirtualDeviceReport(mouseReport);
    }
    if (buttonPressed) {
        mouseReport.buttons = 0;
        mouseReport.x = 0;
        mouseReport.y = 0;
        ((DS4Manager*)context)->sendVirtualDeviceReport(mouseReport);
    }
}
