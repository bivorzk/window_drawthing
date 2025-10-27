// Compile: g++ index.cpp -o index.exe -lcomctl32 -luser32

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <iostream>

// Helper storage for EnumWindows
struct EnumData { HWND foundDefView = NULL; };

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

int main() {
    // Initialize common controls
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);

    HWND lv = GetDesktopListView();
    if (!lv) {
        MessageBoxW(NULL, L"Could not find desktop listview.", L"Error", MB_ICONERROR);
        return 1;
    }

    // Get the process ID of the ListView
    DWORD processId = 0;
    GetWindowThreadProcessId(lv, &processId);
    
    // Open the process for memory access
    HANDLE hProcess = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, processId);
    if (!hProcess) {
        std::wcerr << L"Failed to open process. Error: " << GetLastError() << std::endl;
        return 1;
    }

    // Check ListView style
    LONG_PTR style = GetWindowLongPtrW(lv, GWL_STYLE);
    if (!(style & LVS_ICON)) {
        std::wcout << L"ListView is not in icon mode (style: " << style << L"). Trying anyway..." << std::endl;
    }

    // Get item count
    int count = ListView_GetItemCount(lv);
    if (count == -1) {
        std::wcerr << L"ListView_GetItemCount failed. Error: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return 1;
    }
    std::wcout << L"Found " << count << L" items in desktop ListView." << std::endl;

    // Allocate memory in the target process for POINT structure
    POINT* pPoint = (POINT*)VirtualAllocEx(hProcess, NULL, sizeof(POINT), MEM_COMMIT, PAGE_READWRITE);
    if (!pPoint) {
        std::wcerr << L"VirtualAllocEx failed for POINT. Error: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return 1;
    }

    // Allocate memory in the target process for LVITEM structure
    LVITEMW* pLvItem = (LVITEMW*)VirtualAllocEx(hProcess, NULL, sizeof(LVITEMW), MEM_COMMIT, PAGE_READWRITE);
    if (!pLvItem) {
        std::wcerr << L"VirtualAllocEx failed for LVITEM. Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pPoint, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }

    // Allocate memory in the target process for text buffer
    wchar_t* pText = (wchar_t*)VirtualAllocEx(hProcess, NULL, MAX_PATH * sizeof(wchar_t), MEM_COMMIT, PAGE_READWRITE);
    if (!pText) {
        std::wcerr << L"VirtualAllocEx failed for text buffer. Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pPoint, 0, MEM_RELEASE);
        VirtualFreeEx(hProcess, pLvItem, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return 1;
    }

    // Get item positions and names
    for (int i = 0; i < count; ++i) { // Show all items
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
                        std::wcout << L"Item " << i << L": \"" << localText << L"\" at (" << localPoint.x << L", " << localPoint.y << L")" << std::endl;
                    } else {
                        std::wcout << L"Item " << i << L": coordinates (" << localPoint.x << L", " << localPoint.y << L")" << std::endl;
                    }
                } else {
                    std::wcout << L"Item " << i << L": coordinates (" << localPoint.x << L", " << localPoint.y << L")" << std::endl;
                }
            } else {
                std::wcout << L"Item " << i << L": coordinates (" << localPoint.x << L", " << localPoint.y << L")" << std::endl;
            }
        } else {
            DWORD error = GetLastError();
            std::wcerr << L"Failed to get coordinates for item " << i << L". Error: " << error;
            
            // Provide more specific error information
            switch(error) {
                case ERROR_INVALID_WINDOW_HANDLE:
                    std::wcerr << L" (Invalid window handle)";
                    break;
                case ERROR_INVALID_PARAMETER:
                    std::wcerr << L" (Invalid parameter)";
                    break;
                case ERROR_ACCESS_DENIED:
                    std::wcerr << L" (Access denied)";
                    break;
                default:
                    std::wcerr << L" (Unknown error)";
            }
            std::wcerr << std::endl;
        }
    }

    // Clean up
    VirtualFreeEx(hProcess, pPoint, 0, MEM_RELEASE);
    VirtualFreeEx(hProcess, pLvItem, 0, MEM_RELEASE);
    VirtualFreeEx(hProcess, pText, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return 0;
}
