#include <windows.h>
#include <iostream>
#include <vector>
 
#define MAX_MESSAGE_LENGTH 256
#define MAX_USERNAME_LENGTH 32
 
struct ChatMessage {
    char username[MAX_USERNAME_LENGTH];
    char content[MAX_MESSAGE_LENGTH];
};
 
#define CHAT_PIPE_NAME "\\\\.\\pipe\\ChatPipe"
 
std::vector<HANDLE> activeChatPipes;
std::vector<HANDLE> clientMessageThreads;
bool chatInitiated = false;
 
void broadcastMessageToClients(const ChatMessage& message) {
    for (HANDLE piphandle : activeChatPipes) {
        DWORD bytesSent;
        WriteFile(piphandle, &message, sizeof(message), &bytesSent, NULL);
    }
}
 
void handleClientMessages(LPVOID pipeHandle) {
    HANDLE clientPipe = (HANDLE)pipeHandle;
    ChatMessage receivedMessage;
    
    while (true) {
        DWORD bytesAvailable = 0;
        if (PeekNamedPipe(clientPipe, NULL, 0, NULL, &bytesAvailable, NULL) && bytesAvailable > 0) {
            DWORD bytesRead;
            BOOL readResult = ReadFile(clientPipe, &receivedMessage, sizeof(receivedMessage), &bytesRead, NULL);
 
            if (readResult && bytesRead > 0) {
                std::cout << "[" << receivedMessage.username << "] " << receivedMessage.content << "\n";
                broadcastMessageToClients(receivedMessage);
            }
            else {
                std::cout << "Client disconnected.\n";
                break;
            }
        }
        Sleep(100);
    }
 
    CloseHandle(clientPipe);
}
 
int main() {
    auto commandMonitorThread = CreateThread(NULL, 0, [](LPVOID) -> DWORD {
        std::string userCommand;
        std::cin >> userCommand;
        if (userCommand == "init") chatInitiated = true;
        return 0;
    }, NULL, 0, NULL);
 
    while (!chatInitiated) {
        HANDLE chatPipeHandle = CreateNamedPipeA(
            CHAT_PIPE_NAME,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            sizeof(ChatMessage),
            sizeof(ChatMessage),
            0,
            NULL
        );
 
        if (chatPipeHandle == INVALID_HANDLE_VALUE) {
            std::cerr << "Failed to create named pipe.\n";
            return 1;
        }
 
        std::cout << "Waiting for client connection...\n";
 
        if (ConnectNamedPipe(chatPipeHandle, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
            std::cout << "Client connected.\n";
            activeChatPipes.push_back(chatPipeHandle);
        }
        else {
            CloseHandle(chatPipeHandle);
        }
    }
 
    CloseHandle(commandMonitorThread);
 
    for (auto pipeHandle : activeChatPipes) {
        auto messageThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)handleClientMessages, pipeHandle, 0, NULL);
        clientMessageThreads.push_back(messageThread);
    }
 
    HANDLE* threadHandles = new HANDLE[clientMessageThreads.size()];
 
    for (size_t i = 0; i < clientMessageThreads.size(); i++) {
        threadHandles[i] = clientMessageThreads[i];
    }
 
    for (size_t i = 0; i < clientMessageThreads.size(); i++) {
        WaitForSingleObject(threadHandles[i], INFINITE);
    }
    
    delete[] threadHandles;
    return 0;
}