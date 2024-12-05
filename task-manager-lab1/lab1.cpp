#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <tlhelp32.h>
 
struct ProcessInfo {
   std::wstring filePath;
   PROCESS_INFORMATION procInfo;
};
 
std::vector<ProcessInfo> processList;
 
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
   DWORD processId;
   GetWindowThreadProcessId(hwnd, &processId);
   std::cout << lParam << " -lParam" << processId << " -processId\n";
   if (lParam == processId) {
       PostMessage(hwnd, WM_CLOSE, 0, 0);
       return FALSE;
   }
   return TRUE;
}
 
std::wstring ChooseExecutableFile() {
   OPENFILENAME ofn;
   WCHAR szFile[260] = { 0 };
 
   ZeroMemory(&ofn, sizeof(ofn));
   ofn.lStructSize = sizeof(ofn);
   ofn.hwndOwner = nullptr;
   ofn.lpstrFile = szFile;
   ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
   ofn.lpstrFilter = L"Executable Files\0*.exe\0";
   ofn.nFilterIndex = 1;
   ofn.lpstrFileTitle = nullptr;
   ofn.nMaxFileTitle = 0;
   ofn.lpstrInitialDir = nullptr;
   ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
 
   if (GetOpenFileName(&ofn)) {
       return szFile;
   }
   return L"";
}
 
bool LaunchProcess(const std::wstring& executablePath) {
   STARTUPINFO si;
   PROCESS_INFORMATION pi;
 
   ZeroMemory(&si, sizeof(si));
   si.cb = sizeof(si);
   ZeroMemory(&pi, sizeof(pi));
 
   if (!CreateProcess(
       executablePath.c_str(),
       nullptr,
       nullptr,
       nullptr,
       FALSE,
       0,
       nullptr
       nullptr,
       &si,
       &pi)) {
       std::cerr << "Failed to launch process: " << GetLastError() << std::endl;
       return false;
   }
 
   processList.push_back({ executablePath, pi });
   return true;
}
 
void DisplayProcessStatus() {
   std::vector<HANDLE> processHandles;
   for (const auto& proc : processList) {
       processHandles.push_back(proc.procInfo.hProcess);
   }
   int time;
   std::cout << "Enter time for WaitForMultipleObjects()/n";
   std::cin >> time;
   std::cout << "WaitForMultipleObjects() waiting for " << time << "ms, then returning processes status/n";
   DWORD waitResult = WaitForMultipleObjects(
       static_cast<DWORD>(processHandles.size()),
       processHandles.data(),
       FALSE,
       1000
   );
 
   for (size_t i = 0; i < processList.size(); ++i) {
       DWORD exitCode;
       DWORD processID = processList[i].procInfo.dwProcessId;
 
       if (GetExitCodeProcess(processList[i].procInfo.hProcess, &exitCode)) {
           if (exitCode == STILL_ACTIVE) {
               std::wcout << L"Process " << i + 1 << L" (" << processList[i].filePath
                   << L") is running. PID: " << processID << std::endl;
           }
           else {
               std::wcout << L"Process " << i + 1 << L" (" << processList[i].filePath
                   << L") has terminated. PID: " << processID << std::endl;
           }
       }
       else {
           std::cerr << "Failed to get the exit code for process " << i + 1 << std::endl;
       }
   }
 
   if (waitResult == WAIT_TIMEOUT) {
       std::wcout << L"No processes have terminated within the timeout period." << std::endl;
   }
   else if (waitResult >= WAIT_OBJECT_0 && waitResult < WAIT_OBJECT_0 + processHandles.size()) {
       std::wcout << L"Process " << (waitResult - WAIT_OBJECT_0 + 1) << L" has terminated." << std::endl;
   }
   else {
       std::cerr << "An error occurred during WaitForMultipleObjects: " << GetLastError() << std::endl;
   }
}
 
bool SendWMClose(DWORD pid) {
   HWND hwnd = nullptr;
 
   if (EnumWindows(&EnumWindowsProc, pid)) {
       std::cout << "Process closed\n";
       return true;
   }
 
   std::cerr << "Failed to find a window for process with PID: " << pid << std::endl;
   return false;
}
 
void CleanupProcesses() {
   for (auto& proc : processList) {
       CloseHandle(proc.procInfo.hProcess);
       CloseHandle(proc.procInfo.hThread);
   }
}
 
int main() {
   while (true) {
       std::wcout << L"\n1. Launch process\n2. Display process status\n3. Send WM_CLOSE to process\n4. Exit\n";
       int choice;
       std::cin >> choice;
       switch (choice) {
       case 1: {
           std::wstring exeFile = ChooseExecutableFile();
           if (!exeFile.empty() && LaunchProcess(exeFile)) {
               std::wcout << L"Process launched: " << exeFile << std::endl;
           }
           else {
               std::cerr << "Failed to launch process." << std::endl;
           }
           break;
       }
       case 2:
           DisplayProcessStatus();
           break;
       case 3: {
           std::wcout << L"Enter process index to send WM_CLOSE: ";
           int index;
           std::cin >> index;
           if (!SendWMClose(index)) {
               std::cerr << "Failed to send WM_CLOSE." << std::endl;
           }
           break;
       }
       case 4:
           CleanupProcesses();
           return 0;
       default:
           std::cerr << "Invalid choice!" << std::endl;
           break;
       }
   }
   return 0;
}