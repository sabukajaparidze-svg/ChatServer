#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

int main() {
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed.\n";
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket == INVALID_SOCKET) {
        cout << "Socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(55000);

    inet_pton(
        AF_INET,
        "127.0.0.1",
        &serverAddress.sin_addr
    );

    if (connect(
        clientSocket,
        (sockaddr*)&serverAddress,
        sizeof(serverAddress)
    ) == SOCKET_ERROR) {
        cout << "Connection failed.\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    cout << "Connected to server!\n";

    string message;

    cout << "Enter a message: ";
    getline(cin >> ws, message);

    send(
        clientSocket,
        message.c_str(),
        static_cast<int>(message.size()),
        0
    );

    cout << "Message sent!\n";

    closesocket(clientSocket);
    WSACleanup();

    return 0;
}