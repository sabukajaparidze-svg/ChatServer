#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

vector<SOCKET> clients;
mutex clientsMutex;

void broadcastMessage(const string& message, SOCKET senderSocket) {
    lock_guard<mutex> lock(clientsMutex);

    for (SOCKET client : clients) {
        if (client != senderSocket) {
            send(
                client,
                message.c_str(),
                static_cast<int>(message.size()),
                0
            );
        }
    }
}

void handleClient(SOCKET clientSocket) {
    cout << "A client connected!\n";

    {
        lock_guard<mutex> lock(clientsMutex);
        clients.push_back(clientSocket);
    }

    char buffer[1024];

    while (true) {
        int bytesReceived = recv(
            clientSocket,
            buffer,
            sizeof(buffer) - 1,
            0
        );

        if (bytesReceived <= 0) {
            cout << "A client disconnected.\n";
            break;
        }

        buffer[bytesReceived] = '\0';

        string message = buffer;

        cout << "Client says: "
             << message << "\n";

        string broadcast =
            "Client: " + message;

        broadcastMessage(broadcast, clientSocket);
    }

    {
        lock_guard<mutex> lock(clientsMutex);

        clients.erase(
            remove(
                clients.begin(),
                clients.end(),
                clientSocket
            ),
            clients.end()
        );
    }

    closesocket(clientSocket);
}

int main() {
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed.\n";
        return 1;
    }

    SOCKET serverSocket = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

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

    if (listen(serverSocket, 10) == SOCKET_ERROR) {
        cout << "Listen failed.\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "=================================\n";
    cout << "       Chat Server Started       \n";
    cout << "=================================\n";
    cout << "Waiting for clients...\n";

    while (true) {
        SOCKET clientSocket = accept(
            serverSocket,
            nullptr,
            nullptr
        );

        if (clientSocket == INVALID_SOCKET) {
            cout << "Accept failed.\n";
            continue;
        }

        thread clientThread(
            handleClient,
            clientSocket
        );

        clientThread.detach();
    }

    closesocket(serverSocket);
    WSACleanup();

    return 0;
}