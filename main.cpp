#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

struct Client {
    SOCKET socket;
    string username;
};

vector<Client> clients;
mutex clientsMutex;

void sendToClient(SOCKET clientSocket, const string& message) {
    send(
        clientSocket,
        message.c_str(),
        static_cast<int>(message.size()),
        0
    );
}

void broadcastMessage(
    const string& message,
    SOCKET senderSocket = INVALID_SOCKET
) {
    lock_guard<mutex> lock(clientsMutex);

    for (const Client& client : clients) {
        if (client.socket != senderSocket) {
            sendToClient(client.socket, message);
        }
    }
}

void handleClient(SOCKET clientSocket) {
    char buffer[1024];

    // Receive username
    int bytesReceived = recv(
        clientSocket,
        buffer,
        sizeof(buffer) - 1,
        0
    );

    if (bytesReceived <= 0) {
        closesocket(clientSocket);
        return;
    }

    buffer[bytesReceived] = '\0';

    string username = buffer;

    // Add client
    {
        lock_guard<mutex> lock(clientsMutex);

        clients.push_back({
            clientSocket,
            username
        });
    }

    cout << username << " joined the chat.\n";

    string joinMessage =
        "*** " + username + " joined the chat ***";

    broadcastMessage(joinMessage, clientSocket);

    // Receive messages
    while (true) {
        bytesReceived = recv(
            clientSocket,
            buffer,
            sizeof(buffer) - 1,
            0
        );

        if (bytesReceived <= 0) {
            break;
        }

        buffer[bytesReceived] = '\0';

        string message = buffer;

        cout << "[" << username << "] "
             << message << "\n";

        string formattedMessage =
            "[" + username + "] " + message;

        broadcastMessage(
            formattedMessage,
            clientSocket
        );
    }

    // Remove client
    {
        lock_guard<mutex> lock(clientsMutex);

        clients.erase(
            remove_if(
                clients.begin(),
                clients.end(),
                [clientSocket](const Client& client) {
                    return client.socket == clientSocket;
                }
            ),
            clients.end()
        );
    }

    cout << username << " left the chat.\n";

    string leaveMessage =
        "*** " + username + " left the chat ***";

    broadcastMessage(leaveMessage);

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