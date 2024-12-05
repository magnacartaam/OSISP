#include <windows.h>
#include <iostream>
#include <string>
#include <stdexcept>
 
#define CHAT_PIPE_NAME "\\\\.\\pipe\\ChatPipe"
#define MAX_MESSAGE_LENGTH 256
#define MAX_USERNAME_LENGTH 32
 
struct ChatMessage {
    char username[MAX_USERNAME_LENGTH];
    char content[MAX_MESSAGE_LENGTH];
};
 
class ChatClient {
private:
    HANDLE pipHandle;
    HANDLE messageThread;
    ChatMessage currentMessage;
 
    static DWORD WINAPI receiveMessages(LPVOID param) {
        ChatClient* client = static_cast<ChatClient*>(param);
        HANDLE pipe = client->pipHandle;
 
        while (true) {
            DWORD bytesAvailable;
            if (PeekNamedPipe(pipe, NULL, 0, NULL, &bytesAvailable, NULL) && bytesAvailable > 0) {
                DWORD bytesRead;
                ChatMessage receivedMsg;
                BOOL result = ReadFile(pipe, &receivedMsg, sizeof(receivedMsg), &bytesRead, NULL);
 
                if (result && bytesRead > 0) {
                    std::cout << "\r[" << receivedMsg.username << "] " << receivedMsg.content << "\n> ";
                }
                else {
                    std::cout << "Disconnected from server.\n";
                    break;
                }
            }
            Sleep(100);
        }
 
        return 0;
    }
 
    void connectToPipe() {
        while (true) {
            pipHandle = CreateFileA(
                CHAT_PIPE_NAME,
                GENERIC_READ | GENERIC_WRITE,
                0,
                NULL,
                OPEN_EXISTING,
                0,
                NULL
            );
 
            if (pipHandle != INVALID_HANDLE_VALUE) break;
 
            std::cout << "Waiting for server...\n";
            Sleep(1000);
        }
        std::cout << "Connected to the server.\n";
    }
 
public:
    ChatClient() : pipHandle(INVALID_HANDLE_VALUE), messageThread(NULL) {
        connectToPipe();
    }
 
    void start() {
        std::cout << "Enter your name: ";
        std::cin.getline(currentMessage.username, MAX_USERNAME_LENGTH);
 
        messageThread = CreateThread(NULL, 0, receiveMessages, this, 0, NULL);
        if (!messageThread) {
            throw std::runtime_error("Failed to create message thread");
        }
 
        std::string userInput;
        while (true) {
            std::cout << "> ";
            std::getline(std::cin, userInput);
 
            if (userInput == "exit") break;
 
            strncpy_s(currentMessage.content, userInput.c_str(), MAX_MESSAGE_LENGTH);
 
            DWORD bytesWritten;
            WriteFile(pipHandle, &currentMessage, sizeof(currentMessage), &bytesWritten, NULL);
        }
    }
 
    ~ChatClient() {
        if (messageThread) {
            WaitForSingleObject(messageThread, INFINITE);
            CloseHandle(messageThread);
        }
        if (pipHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(pipHandle);
        }
    }
};
 
int main() {
    try {
        ChatClient client;
        client.start();
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
 
    return 0;
}