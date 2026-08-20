#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <fstream>
#include <atomic>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

const int MAX_USERNAME_LENGTH = 20;
const int MAX_MESSAGE_LENGTH = 500;

struct Client {
    SOCKET socket;
    string username;
};

vector<Client> clients;
mutex clientsMutex;
mutex historyMutex;

atomic<bool> serverRunning(true);

SOCKET serverSocket = INVALID_SOCKET;

string getTimestamp() {
    auto now = chrono::system_clock::now();
    time_t currentTime = chrono::system_clock::to_time_t(now);

    tm localTime{};
    localtime_s(&localTime, &currentTime);

    stringstream timestamp;

    timestamp << put_time(
        &localTime,
        "[%H:%M:%S] "
    );

    return timestamp.str();
}

void saveToHistory(const string& message) {
    lock_guard<mutex> lock(historyMutex);

    ofstream historyFile(
        "chat_history.txt",
        ios::app
    );

    if (historyFile.is_open()) {
        historyFile << message << "\n";
    }
}

void sendToClient(
    SOCKET clientSocket,
    const string& message
) {
    send(
        clientSocket,
        message.c_str(),
        static_cast<int>(message.size()),
        0
    );
}

bool usernameExists(const string& username) {
    for (const Client& client : clients) {
        if (client.username == username) {
            return true;
        }
    }

    return false;
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

bool sendPrivateMessage(
    const string& sender,
    const string& target,
    const string& message
) {
    lock_guard<mutex> lock(clientsMutex);

    for (const Client& client : clients) {
        if (client.username == target) {
            string privateMessage =
                getTimestamp() +
                "[Private from " + sender + "] " +
                message;

            sendToClient(
                client.socket,
                privateMessage
            );

            return true;
        }
    }

    return false;
}

string getOnlineUsers() {
    lock_guard<mutex> lock(clientsMutex);

    string result = "Online users:\n";

    for (const Client& client : clients) {
        result += "- " + client.username + "\n";
    }

    return result;
}

void disconnectAllClients() {
    lock_guard<mutex> lock(clientsMutex);

    for (const Client& client : clients) {
        sendToClient(
            client.socket,
            "\nServer is shutting down.\n"
        );

        shutdown(
            client.socket,
            SD_BOTH
        );

        closesocket(client.socket);
    }

    clients.clear();
}

void serverConsole() {
    while (serverRunning) {
        string command;

        if (!getline(cin, command)) {
            break;
        }

        if (command == "/status") {
            lock_guard<mutex> lock(clientsMutex);

            cout << "\n========== SERVER STATUS ==========\n";
            cout << "Server: RUNNING\n";
            cout << "Port: 55000\n";
            cout << "Connected clients: "
                 << clients.size() << "\n";

            if (clients.empty()) {
                cout << "No clients connected.\n";
            }
            else {
                cout << "Users:\n";

                for (const Client& client : clients) {
                    cout << "- "
                         << client.username
                         << "\n";
                }
            }

            cout << "==================================\n\n";
        }
        else if (command == "/shutdown") {
            cout << "\nShutting down server...\n";

            serverRunning = false;

            disconnectAllClients();

            if (serverSocket != INVALID_SOCKET) {
                closesocket(serverSocket);
                serverSocket = INVALID_SOCKET;
            }

            break;
        }
        else if (!command.empty()) {
            cout << "Unknown server command.\n";
            cout << "Available commands:\n";
            cout << "/status\n";
            cout << "/shutdown\n";
        }
    }
}

void handleCommand(
    SOCKET clientSocket,
    const string& username,
    const string& message
) {
    if (message == "/help") {
        string help =
            "\nAvailable commands:\n"
            "/help - Show commands\n"
            "/users - Show online users\n"
            "/msg <username> <message> - Private message\n"
            "exit - Leave the chat\n";

        sendToClient(clientSocket, help);
        return;
    }

    if (message == "/users") {
        sendToClient(
            clientSocket,
            getOnlineUsers()
        );

        return;
    }

    if (message.rfind("/msg ", 0) == 0) {
        string command = message.substr(5);

        stringstream stream(command);

        string target;
        stream >> target;

        string privateText;
        getline(stream, privateText);

        if (target.empty() || privateText.empty()) {
            sendToClient(
                clientSocket,
                "Usage: /msg <username> <message>"
            );

            return;
        }

        if (privateText[0] == ' ') {
            privateText.erase(0, 1);
        }

        if (privateText.length() > MAX_MESSAGE_LENGTH) {
            sendToClient(
                clientSocket,
                "Private message is too long."
            );

            return;
        }

        if (!sendPrivateMessage(
            username,
            target,
            privateText
        )) {
            sendToClient(
                clientSocket,
                "User '" + target + "' is not online."
            );

            return;
        }

        string sentMessage =
            getTimestamp() +
            "[Private to " + target + "] " +
            privateText;

        sendToClient(
            clientSocket,
            sentMessage
        );

        saveToHistory(
            getTimestamp() +
            "[Private] [" + username +
            " -> " + target + "] " +
            privateText
        );

        return;
    }

    sendToClient(
        clientSocket,
        "Unknown command. Type /help."
    );
}

void handleClient(SOCKET clientSocket) {
    char buffer[1024];

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

    if (username.empty() ||
        username.length() > MAX_USERNAME_LENGTH) {

        sendToClient(
            clientSocket,
            "Invalid username. Maximum length is 20 characters."
        );

        closesocket(clientSocket);
        return;
    }

    {
        lock_guard<mutex> lock(clientsMutex);

        if (usernameExists(username)) {
            sendToClient(
                clientSocket,
                "Username already taken."
            );

            closesocket(clientSocket);
            return;
        }

        clients.push_back({
            clientSocket,
            username
        });
    }

    cout << username << " joined the chat.\n";

    string joinMessage =
        getTimestamp() +
        "*** " + username + " joined the chat ***";

    broadcastMessage(
        joinMessage,
        clientSocket
    );

    saveToHistory(joinMessage);

    while (serverRunning) {
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

        if (message == "exit") {
            break;
        }

        if (message.length() > MAX_MESSAGE_LENGTH) {
            sendToClient(
                clientSocket,
                "Message is too long. Maximum is 500 characters."
            );

            continue;
        }

        cout << "[" << username << "] "
             << message << "\n";

        if (!message.empty() && message[0] == '/') {
            handleCommand(
                clientSocket,
                username,
                message
            );
        }
        else {
            string formattedMessage =
                getTimestamp() +
                "[" + username + "] " +
                message;

            broadcastMessage(
                formattedMessage,
                clientSocket
            );

            saveToHistory(formattedMessage);
        }
    }

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

    if (serverRunning) {
        cout << username << " left the chat.\n";

        string leaveMessage =
            getTimestamp() +
            "*** " + username + " left the chat ***";

        broadcastMessage(leaveMessage);

        saveToHistory(leaveMessage);
    }

    closesocket(clientSocket);
}

int main() {
    WSADATA wsaData;

    if (WSAStartup(
        MAKEWORD(2, 2),
        &wsaData
    ) != 0) {

        cout << "WSAStartup failed.\n";
        return 1;
    }

    serverSocket = socket(
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
    cout << "Port: 55000\n";
    cout << "Chat history: chat_history.txt\n";
    cout << "Server commands: /status /shutdown\n";
    cout << "Waiting for clients...\n";

    thread consoleThread(serverConsole);

    while (serverRunning) {
        SOCKET clientSocket = accept(
            serverSocket,
            nullptr,
            nullptr
        );

        if (!serverRunning) {
            break;
        }

        if (clientSocket == INVALID_SOCKET) {
            if (serverRunning) {
                cout << "Accept failed.\n";
            }

            continue;
        }

        thread clientThread(
            handleClient,
            clientSocket
        );

        clientThread.detach();
    }

    serverRunning = false;

    if (consoleThread.joinable()) {
        consoleThread.detach();
    }

    WSACleanup();

    cout << "Server stopped.\n";

    return 0;
}