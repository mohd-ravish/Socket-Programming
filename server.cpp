#include <iostream>
#include <chrono>
#include <fstream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <algorithm>
#include <thread>
#include <vector>
#include <map>
#include <mutex>

using namespace std;

const int MAX_CLIENTS = 5;
const string STOP_WORD = "123";
vector<string> badWords = {"evil", "bad", "hate"};
string key;

vector<int> clientSockets;

// Mutex
mutex cout_mutex;
mutex clientMutex;

bool containsBadWord(const string &message)
{
    for (const string &badWord : badWords)
    {
        if (message.find(badWord) != string::npos)
        {
            return true;
        }
    }
    return false;
}

string encrypt(string key, string text) // CTC
{
    string res = "";
    char matrix[100][100];
    string temp = key;
    sort(temp.begin(), temp.end());
    int index = 0;
    int row = ((text.length() / key.length()) + (text.length() % key.length() != 0) + 1);
    int col = key.length();
    for (int i = 0; i < 1; i++)
    {
        for (int j = 0; j < col; j++)
        {
            matrix[i][j] = key[index];
            index++;
        }
    }
    index = 0;
    for (int i = 1; i < row; i++)
    {
        for (int j = 0; j < col; j++)
        {
            matrix[i][j] = text[index];
            index++;
        }
    }
    for (int k = 0; k < col; k++)
    {
        for (int i = 0; i < col; i++)
        {
            if (temp[k] == matrix[0][i])
            {
                for (int j = 1; j < row; j++)
                {
                    if ((matrix[j][i]))
                    {
                        res += matrix[j][i];
                    }
                    else
                        res += " ";
                }
            }
        }
    }
    return res;
}

string decrypt(string key, string res) // CTC
{
    char dematrix[100][100];
    int index = 0;
    string temp = key;
    sort(temp.begin(), temp.end());
    int row = ((res.length() / key.length()) + (res.length() % key.length() != 0) + 1);
    int col = key.length();
    for (int i = 0; i < col; i++)
    {
        dematrix[0][i] = key[i];
    }
    index = 0;
    for (int k = 0; k < col; k++)
    {
        for (int i = 0; i < col; i++)
        {
            if (temp[k] == dematrix[0][i])
            {
                for (int j = 1; j < row; j++)
                {
                    dematrix[j][i] = res[index++];
                }
            }
        }
    }
    string deres;
    for (int i = 1; i < row; ++i)
    {
        for (int j = 0; j < col; ++j)
        {
            if (dematrix[i][j] != '@')
            {
                deres += dematrix[i][j];
            }
        }
    }
    return deres;
}

void sendMessageToClient(string encKey, string senderclientName, int recipientclientName, const string &message, std::map<int, string> &cname, std::map<int, int> &cn)
{
    clientMutex.lock();
    int recipientSocket = cn[recipientclientName];
    string fullMessage = senderclientName + "|" + message;
    string encMess = encrypt(key, fullMessage);
    send(recipientSocket, encMess.c_str(), encMess.size(), 0);
    clientMutex.unlock();
}

void handleClient(int clientNumber, int clientSocket, string clientName, std::map<int, string> &cname, std::map<int, int> &cn)
{
    char buffer[1024];
    int bytesRead;
    while (true)
    {
        bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        buffer[bytesRead] = '\0';
        string encrypted(buffer);
        string receivedMessage = decrypt(key, encrypted);

        if (containsBadWord(receivedMessage)) // BadWords Check
        {
            cout << "\n"
                 << clientName << "sent a message with a bad word, Message ignored!\n";
            string response = "YOU SENT A BAD WORD i.e : " + receivedMessage;

            string encMess = encrypt(key, response);
            send(clientSocket, encMess.c_str(), encMess.size(), 0);
            continue;
        }
        size_t poskey = receivedMessage.find('/');
        size_t pos = receivedMessage.find("|", poskey + 1); // Check if client want to send to other client -- FORWARDING
        if (pos != string::npos)
        {
            string encKey = receivedMessage.substr(0, poskey);
            string cl = receivedMessage.substr(poskey + 1, pos - poskey - 1);
            // cout << receivedMessage << endl;
            // cout << encKey << endl;
            // cout << cl << endl;
            int recipientClientNumber = stoi(cl);
            string messageToForward = receivedMessage.substr(pos + 1);
            sendMessageToClient(encKey, clientName, recipientClientNumber, messageToForward, cname, cn);
            continue;
        }
        else if (bytesRead > 0) // Clinet Connected
        {
            buffer[bytesRead] = '\0';
            {
                lock_guard<mutex> lock(cout_mutex);
                cout << "\nReceived from " << clientName << ": " << receivedMessage << endl;
                cout << flush;

                string clientNameStr = clientName;
                string clientSocketStr = to_string(clientSocket);
                string fileName = clientNameStr + "_" + clientSocketStr + ".txt";
                fstream f(fileName, ios::app);
                if (!f)
                {
                    cout << "Failed to open or create the file: " << fileName << endl;
                }
                else
                {
                    f << receivedMessage + "\n";
                    f.close();
                }
            }
        }
        else if (bytesRead == 0) // Clinet DISCONNECTED
        {
            {
                lock_guard<mutex> lock(cout_mutex);
                cout << clientName << " disconnected" << endl;

                cn.erase(clientNumber);
                cname.erase(clientNumber);

                clientMutex.lock();
                for (int i = 0; i < clientSockets.size(); ++i)
                {
                    string sockResponse = "cnpls";
                    for (auto &entry : cname)
                    {
                        sockResponse += entry.second + ",";
                    }
                    string encMess = encrypt(key, sockResponse);
                    send(clientSockets[i], encMess.c_str(), encMess.size(), 0);
                }
                clientMutex.unlock();
            }
            break;
        }
        else
        {
            {
                lock_guard<mutex> lock(cout_mutex);
                cerr << "Error receiving data from " << clientName << endl;
            }
            break;
        }
        if (receivedMessage == STOP_WORD)
        {
            cout << "Server is stopping..." << endl;
            clientMutex.lock();
            for (int i = 0; i < clientSockets.size(); ++i)
            {
                close(clientSockets[i]);
            }
            clientSockets.clear();
            clientMutex.unlock();
            break;
        }
    }

    close(clientSocket);
}

void sendMessage(std::map<int, string> &cname, std::map<int, int> &cn)
{
    cout << "Enter Y to send a message to client\n\n";
    while (true)
    {
        char select;
        cin >> select;

        if (select == 0)
        {
            cout << "Exiting send loop..." << endl;
            break;
        }
        else if (select == 'Y' || 'y')
        {
            cout << "\nConnected Clients: " << endl;
            for (const auto &entry : cname)
            {
                cout << entry.first << ". " << entry.second << endl;
            }
            cout << "\nEnter client number: ";
            int clientSelect;
            cin >> clientSelect;

            if (clientSelect >= 1)
            {
                int socket = cn[clientSelect];
                string message;
                cin.ignore();
                cout << "Enter the message to send: ";
                getline(cin, message);
                cout << endl;
                string encMess = encrypt(key, message);
                send(socket, encMess.c_str(), encMess.size(), 0);
            }
            else
            {
                cout << "Invalid client number. Please try again." << endl;
            }
        }
        else
        {
            cout << "Invalid option. Please try again." << endl;
        }
    }
}

int main()
{
    // cin.ignore();
    cout << "Enter the key on which server will decrypt and encrypt message: ";
    getline(cin, key);
    vector<thread> clientThreads;
    map<int, int> clientNumbers;
    map<int, string> clientNames;
    int clientCounter = 1;
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket == -1)
    {
        perror("Error creating socket");
        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(8080); // Port number

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) == -1)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        perror("Error binding");
        return 1;
    }

    if (listen(serverSocket, 5) == -1)
    {
        perror("Error listening");
        return 1;
    }

    cout << "\nServer listening on port 8080..." << endl;

    thread sendThread(sendMessage, ref(clientNames), ref(clientNumbers));
    sendThread.detach();

    while (true)
    {
        sockaddr_in clientAddress;
        socklen_t clientAddressSize = sizeof(clientAddress);

        int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, &clientAddressSize);
        if (clientSocket == -1)
        {
            {
                lock_guard<mutex> lock(cout_mutex);
                cerr << "Error accepting connection" << endl;
            }
            continue;
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));

        char buffer1[1024];
        int bytesRead1;
        bytesRead1 = recv(clientSocket, buffer1, sizeof(buffer1), 0);
        buffer1[bytesRead1] = '\0';
        string encrypted(buffer1);
        string clientName = encrypted;

        cout << "Connection established with " << clientName << endl;

        clientNumbers[clientCounter] = clientSocket;
        clientNames[clientCounter] = clientName;

        clientThreads.emplace_back(handleClient, clientCounter, clientSocket, clientName, ref(clientNames), ref(clientNumbers));

        clientMutex.lock();
        clientSockets.push_back(clientSocket);
        clientMutex.unlock();

        std::this_thread::sleep_for(std::chrono::seconds(1));

        clientMutex.lock();
        for (int i = 0; i < clientSockets.size(); ++i)
        {
            string sockResponse = "cnpls";
            for (auto &entry : clientNames)
            {
                sockResponse += entry.second + ",";
            }
            sockResponse.pop_back();
            string encMess = encrypt(key, sockResponse);
            send(clientSockets[i], encMess.c_str(), encMess.size(), 0);
        }
        clientMutex.unlock();

        clientCounter++;
    }

    // Wait...
    for (auto &thread : clientThreads)
    {
        thread.join();
    }

    close(serverSocket);

    return 0;
}