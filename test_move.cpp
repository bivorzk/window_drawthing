#include <windows.h>
#include <commctrl.h>
#include <iostream>
#include "desktop_functions.h"

int main() {
    std::wcout << L"Testing desktop icon movement..." << std::endl;
    
    // Get all desktop icons
    std::vector<DesktopIcon> icons = GetDesktopIcons();
    std::wcout << L"Found " << icons.size() << L" desktop icons" << std::endl;
    
    if (icons.empty()) {
        std::wcout << L"No icons found!" << std::endl;
        return 1;
    }
    
    // Print first few icons
    for (int i = 0; i < std::min(5, (int)icons.size()); ++i) {
        std::wcout << L"Icon " << i << L": \"" << icons[i].name << L"\" at (" 
                  << icons[i].position.x << L", " << icons[i].position.y << L")" << std::endl;
    }
    
    // Try to move the first icon by 50 pixels right and down
    if (icons.size() > 0) {
        int newX = icons[0].position.x + 50;
        int newY = icons[0].position.y + 50;
        
        std::wcout << L"Attempting to move first icon from (" << icons[0].position.x 
                  << L", " << icons[0].position.y << L") to (" << newX << L", " << newY << L")" << std::endl;
        
        bool success = MoveDesktopIcon(0, newX, newY);
        std::wcout << L"Move result: " << (success ? L"SUCCESS" : L"FAILED") << std::endl;
        
        // Wait a bit and check the position again
        Sleep(1000);
        std::vector<DesktopIcon> iconsAfter = GetDesktopIcons();
        if (!iconsAfter.empty()) {
            std::wcout << L"Icon position after move: (" << iconsAfter[0].position.x 
                      << L", " << iconsAfter[0].position.y << L")" << std::endl;
        }
    }
    
    return 0;
}