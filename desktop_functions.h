#ifndef DESKTOP_FUNCTIONS_H
#define DESKTOP_FUNCTIONS_H

#include <windows.h>
#include <commctrl.h>
#include <iostream>
#include <vector>

// Structure to hold desktop icon information
struct DesktopIcon {
    std::wstring name;
    POINT position;
    int index;
};

// Helper storage for EnumWindows
struct EnumData { 
    HWND foundDefView = NULL; 
};

// EnumWindowsProc - looks for a SHELLDLL_DefView child
BOOL CALLBACK EnumWindowsProc(HWND topWnd, LPARAM lParam) {
    EnumData* ed = reinterpret_cast<EnumData*>(lParam);
    if (!IsWindowVisible(topWnd)) return TRUE;

    HWND defView = FindWindowExW(topWnd, NULL, L"SHELLDLL_DefView", NULL);
    if (defView) {
        ed->foundDefView = defView;
        return FALSE; // Stop enumeration
    }
    return TRUE; // Continue
}

// Returns HWND of the desktop's SysListView32, or NULL on failure
HWND GetDesktopListView() {
    // Step 1: Try finding Progman
    HWND progman = FindWindowW(L"Progman", NULL);
    if (!progman) {
        std::wcerr << L"Failed to find Progman window. Error: " << GetLastError() << std::endl;
        return NULL;
    }

    // Step 2: Try finding SHELLDLL_DefView directly
    HWND defView = FindWindowExW(progman, NULL, L"SHELLDLL_DefView", NULL);
    if (!defView) {
        // Step 3: Send message to Progman to spawn WorkerW
        DWORD_PTR result = 0;
        if (!SendMessageTimeoutW(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, &result)) {
            std::wcerr << L"SendMessageTimeoutW failed. Error: " << GetLastError() << std::endl;
        }

        // Step 4: Wait briefly and retry enumeration
        Sleep(100); // Give time for WorkerW to spawn
        EnumData ed;
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&ed));
        defView = ed.foundDefView;
        if (!defView) {
            std::wcerr << L"Failed to find SHELLDLL_DefView after enumeration." << std::endl;
            return NULL;
        }
    }

    // Step 5: Get SysListView32 child
    HWND sysList = FindWindowExW(defView, NULL, L"SysListView32", NULL);
    if (!sysList) {
        std::wcerr << L"Failed to find SysListView32. Error: " << GetLastError() << std::endl;
        return NULL;
    }

    // Step 6: Validate the ListView handle
    if (!IsWindow(sysList)) {
        std::wcerr << L"SysListView32 handle is not a valid window." << std::endl;
        return NULL;
    }

    // Step 7: Check if we can get the process ID (for security validation)
    DWORD processId = 0;
    GetWindowThreadProcessId(sysList, &processId);
    if (processId == 0) {
        std::wcerr << L"Could not get process ID for SysListView32." << std::endl;
        return NULL;
    }

    std::wcout << L"Successfully found desktop ListView (Process ID: " << processId << L")" << std::endl;
    return sysList;
}

// Get all desktop icons with their positions and names
std::vector<DesktopIcon> GetDesktopIcons() {
    std::vector<DesktopIcon> icons;
    
    // Initialize common controls
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);

    HWND lv = GetDesktopListView();
    if (!lv) {
        std::wcerr << L"Could not find desktop listview." << std::endl;
        return icons;
    }

    // Get the process ID of the ListView
    DWORD processId = 0;
    GetWindowThreadProcessId(lv, &processId);
    
    // Open the process for memory access
    HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, processId);
    if (!hProcess) {
        std::wcerr << L"Failed to open process. Error: " << GetLastError() << std::endl;
        return icons;
    }

    // Get item count
    int count = ListView_GetItemCount(lv);
    if (count == -1) {
        std::wcerr << L"ListView_GetItemCount failed. Error: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return icons;
    }

    // Allocate memory in the target process for POINT structure
    POINT* pPoint = (POINT*)VirtualAllocEx(hProcess, NULL, sizeof(POINT), MEM_COMMIT, PAGE_READWRITE);
    if (!pPoint) {
        std::wcerr << L"VirtualAllocEx failed for POINT. Error: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return icons;
    }

    // Allocate memory in the target process for LVITEM structure
    LVITEMW* pLvItem = (LVITEMW*)VirtualAllocEx(hProcess, NULL, sizeof(LVITEMW), MEM_COMMIT, PAGE_READWRITE);
    if (!pLvItem) {
        std::wcerr << L"VirtualAllocEx failed for LVITEM. Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pPoint, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return icons;
    }

    // Allocate memory in the target process for text buffer
    wchar_t* pText = (wchar_t*)VirtualAllocEx(hProcess, NULL, MAX_PATH * sizeof(wchar_t), MEM_COMMIT, PAGE_READWRITE);
    if (!pText) {
        std::wcerr << L"VirtualAllocEx failed for text buffer. Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pPoint, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, pLvItem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return icons;
    }

    // Get item positions and names
    for (int i = 0; i < count; ++i) {
        POINT localPoint = {0, 0};
        BOOL success = FALSE;
        
        // Try to get item position using cross-process communication
        if (SendMessageW(lv, LVM_GETITEMPOSITION, i, (LPARAM)pPoint)) {
            SIZE_T bytesRead = 0;
            if (ReadProcessMemory(hProcess, pPoint, &localPoint, sizeof(POINT), &bytesRead) && bytesRead == sizeof(POINT)) {
                success = TRUE;
            }
        }
        
        if (success) {
            DesktopIcon icon;
            icon.position = localPoint;
            icon.index = i;
            
            // Try to get item text
            wchar_t localText[MAX_PATH] = L"";
            LVITEMW localLvItem = {0};
            localLvItem.mask = LVIF_TEXT;
            localLvItem.iItem = i;
            localLvItem.pszText = pText;
            localLvItem.cchTextMax = MAX_PATH;
            
            // Write LVITEM to remote process memory
            SIZE_T bytesWritten = 0;
            if (WriteProcessMemory(hProcess, pLvItem, &localLvItem, sizeof(LVITEMW), &bytesWritten) && bytesWritten == sizeof(LVITEMW)) {
                if (SendMessageW(lv, LVM_GETITEMTEXTW, i, (LPARAM)pLvItem)) {
                    SIZE_T bytesRead = 0;
                    if (ReadProcessMemory(hProcess, pText, localText, MAX_PATH * sizeof(wchar_t), &bytesRead)) {
                        localText[MAX_PATH - 1] = L'\0'; // Ensure null termination
                        icon.name = localText;
                    }
                }
            }
            
            // If we couldn't get the name, set it to empty
            if (icon.name.empty()) {
                icon.name = L"Unknown";
            }
            
            icons.push_back(icon);
        }
    }

    // Clean up
    VirtualFreeEx(hProcess, pPoint, 0, MEM_RELEASE);
    VirtualFreeEx(hProcess, pLvItem, 0, MEM_RELEASE);
    VirtualFreeEx(hProcess, pText, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return icons;
}

// Simple function to get just the count of desktop icons
int GetDesktopIconCount() {
    HWND lv = GetDesktopListView();
    if (!lv) return -1;
    
    return ListView_GetItemCount(lv);
}

#endif // DESKTOP_FUNCTIONS_H

// Move a desktop icon by index to a new position (desktop coordinates)
inline bool MoveDesktopIcon(int iconIndex, int newX, int newY) {
    std::wcout << L"Attempting to move icon " << iconIndex << L" to (" << newX << L", " << newY << L")" << std::endl;
    
    HWND lv = GetDesktopListView();
    if (!lv) {
        std::wcout << L"Failed to get desktop ListView handle" << std::endl;
        return false;
    }
    
    // Check if auto-arrange is enabled
    LONG_PTR style = GetWindowLongPtrW(lv, GWL_STYLE);
    if (style & LVS_AUTOARRANGE) {
        std::wcout << L"Warning: Desktop has auto-arrange enabled, move may not work" << std::endl;
    }
    
    // LVM_SETITEMPOSITION expects LPARAM as MAKELPARAM(x, y)
    LPARAM pos = MAKELPARAM(newX, newY);
    std::wcout << L"Sending LVM_SETITEMPOSITION message..." << std::endl;
    LRESULT res = SendMessageW(lv, LVM_SETITEMPOSITION, iconIndex, pos);
    
    std::wcout << L"SendMessage result: " << res << std::endl;
    if (res == 0) {
        DWORD error = GetLastError();
        std::wcout << L"Move failed with error: " << error << std::endl;
    }
    
    // LVM_SETITEMPOSITION returns TRUE on success
    return res != 0;
}
