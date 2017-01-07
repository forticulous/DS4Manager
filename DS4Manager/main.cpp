#include <iostream>

#include "DS4Manager.hpp"

int main(int argc, const char * argv[]) {
    
    DS4Manager manager;
    
    std::cout << "starting" << std::endl;
    
    manager.start();
    
    std::cout << "done" << std::endl;
    
    return 0;
}
