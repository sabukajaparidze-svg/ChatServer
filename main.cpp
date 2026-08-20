#include <iostream>
#include <string>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

int main() {
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed.\n";
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket == INVALID_SOCKET) {
        cout << "Socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(55000);

    if (bind(
        serverSocket,
        (sockaddr*)&serverAddress,
        sizeof(serverAddress)
    ) == SOCKET_ERROR) {
        cout << "Bind failed.\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, 5) == SOCKET_ERROR) {
        cout << "Listen failed.\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "=================================\n";
    cout << "       Chat Server Started       \n";
    cout << "=================================\n";
    cout << "Waiting for a client...\n";

    SOCKET clientSocket = accept(
        serverSocket,
        nullptr,
        nullptr
    );

    if (clientSocket == INVALID_SOCKET) {
        cout << "Accept failed.\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "Client connected!\n";

    char buffer[1024];

    while (true) {
        int bytesReceived = recv(
            clientSocket,
            buffer,
            sizeof(buffer) - 1,
            0
        );

        if (bytesReceived == 0) {
            cout << "Client disconnected.\n";
            break;
        }

        if (bytesReceived == SOCKET_ERROR) {
            cout << "Receive failed.\n";
            break;
        }

        buffer[bytesReceived] = '\0';

        cout << "Client says: "
             << buffer << "\n";

        string response =
            "Server received: " + string(buffer);

        int bytesSent = send(
            clientSocket,
            response.c_str(),
            static_cast<int>(response.size()),
            0
        );

        if (bytesSent == SOCKET_ERROR) {
            cout << "Send failed.\n";
            break;
        }
    }

    closesocket(clientSocket);
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}