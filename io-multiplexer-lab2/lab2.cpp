#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
 
#define MAX_SOURCES 3
#define BUFFER_SIZE 1024
 
struct IOSource {
   HANDLE handle;
   std::string name;
   OVERLAPPED overlapped;
   char buffer[BUFFER_SIZE];
   DWORD bytesTransferred;
   bool isReady;
};
 
class IOMultiplexer {
private:
   std::vector<IOSource> sources;
   HANDLE* eventHandles;
   int sourceCount;
 
public:
   IOMultiplexer() : sourceCount(0) {
       eventHandles = new HANDLE[MAX_SOURCES];
   }
 
   ~IOMultiplexer() {
       for (auto& source : sources) {
           CloseHandle(source.handle);
           CloseHandle(source.overlapped.hEvent);
       }
       delete[] eventHandles;
   }
 
   bool AddSource(const std::string& name, const std::string& path) {
       if (sourceCount >= MAX_SOURCES) {
           std::cout << "Maximum number of sources reached\n";
           return false;
       }
 
       IOSource source;
       source.name = name;
       source.isReady = false;
 
       source.handle = CreateFileA(
           path.c_str(),
           GENERIC_READ,
           FILE_SHARE_READ,
           NULL,
           OPEN_EXISTING,
           FILE_FLAG_OVERLAPPED,
           NULL
       );
 
       if (source.handle == INVALID_HANDLE_VALUE) {
           std::cout << "Failed to open file: " << path << "\n";
           return false;
       }
 
       auto tmp = CreateEvent(NULL, TRUE, FALSE, NULL);
       if (tmp == NULL) {
           CloseHandle(source.handle);
           return false;
       }
 
       ZeroMemory(&source.overlapped, sizeof(OVERLAPPED));
       source.overlapped.hEvent = tmp;
 
       sources.push_back(source);
       eventHandles[sourceCount] = source.overlapped.hEvent;
       sourceCount++;
 
       return true;
   }
 
   void StartAsyncRead(int sourceIndex) {
       if (sourceIndex >= sourceCount) return;
 
       IOSource& source = sources[sourceIndex];
       BOOL success = ReadFile(
           source.handle,
           source.buffer,
           BUFFER_SIZE - 1,
           &source.bytesTransferred,
           &source.overlapped
       );
 
       if (!success && GetLastError() != ERROR_IO_PENDING) {
           std::cout << "Error starting async read for " << source.name << "\n";
       }
   }
 
   void ProcessSources() {
       for (int i = 0; i < sourceCount; i++) {
           StartAsyncRead(i);
       }
 
       while (true) {
           DWORD result = WaitForMultipleObjects(
               sourceCount,
               eventHandles,
               FALSE,
               5000
           );
 
           if (result == WAIT_TIMEOUT) {
               std::cout << "Timeout occurred\n";
               continue;
           }
 
           if (result == WAIT_FAILED) {
               std::cout << "Wait failed\n";
               break;
           }
 
           int index = result - WAIT_OBJECT_0;
           if (index >= 0 && index < sourceCount) {
               IOSource& source = sources[index];
 
               BOOL success = GetOverlappedResult(
                   source.handle,
                   &source.overlapped,
                   &source.bytesTransferred,
                   FALSE
               );
 
               if (success && source.bytesTransferred > 0) {
                   source.buffer[source.bytesTransferred] = '\0';
                   std::cout << "Data from " << source.name
                       << " (" << source.bytesTransferred << " bytes): "
                       << source.buffer << "\n";
                   Sleep(rand() % 1000);
                   ResetEvent(source.overlapped.hEvent);
                   source.overlapped.Offset += source.bytesTransferred;
                   StartAsyncRead(index);
               }
               else {
                   DWORD error = GetLastError();
                   if (error == ERROR_HANDLE_EOF) {
                       std::cout << "Reached end of file for " << source.name << "\n";
                       source.isReady = true;
​​​​​ResetEvent(source.overlapped.hEvent);
                   }
                   else {
                       std::cout << "Error reading from " << source.name
                           << ". Error code: " << error << "\n";
                   }
               }
           }
 
           bool allDone = true;
           for (const auto& source : sources) {
               if (!source.isReady) {
                   allDone = false;
                   break;
               }
           }
           if (allDone) break;
       }
   }
};
 
void CreateTestFile(const std::string& path, const std::string& content) {
   HANDLE hFile = CreateFileA(
       path.c_str(),
       GENERIC_WRITE,
       0,
       NULL,
       CREATE_ALWAYS,
       FILE_ATTRIBUTE_NORMAL,
       NULL
   );
 
   if (hFile != INVALID_HANDLE_VALUE) {
       DWORD bytesWritten;
       WriteFile(hFile, content.c_str(), content.length(), &bytesWritten, NULL);
       CloseHandle(hFile);
   }
}
 
int main() {
   CreateTestFile("test1.txt", "This is test file 1\nWith multiple lines\nOf content");
   CreateTestFile("test2.txt", "Content from test file 2\nMore data here\nAnd here");
   CreateTestFile("test3.txt", "Test file 3 data\nSome more lines\nFinal line");
 
   IOMultiplexer multiplexer;
 
   multiplexer.AddSource("File1", "test1.txt");
   multiplexer.AddSource("File2", "test2.txt");
   multiplexer.AddSource("File3", "test3.txt");
 
   std::cout << "Starting I/O multiplexing...\n";
   multiplexer.ProcessSources();
   std::cout << "I/O multiplexing completed\n";
 
   DeleteFileA("test1.txt");
   DeleteFileA("test2.txt");
   DeleteFileA("test3.txt");
 
   return 0;
}