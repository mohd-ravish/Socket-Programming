#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <thread>
#include <algorithm>
#include <mutex>
#include <ctime>
#include <sstream>

using namespace std;
// mutex
mutex clientMutex;
mutex cout_mutex;
string key;
string messageFromClient = "";

string totClientActive;
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

void sendMessage(int clientSocket)
{
    while (true)
    {
        vector<string> parts;
        std::stringstream ss(totClientActive);
        std::string token;

        while (std::getline(ss, token, ','))
        {
            parts.push_back(token);
        }
        int i = 1;
        cout << "\nConnected Clients:" << endl;
        for (const std::string &part : parts)
        {
            std::cout << i << ". " << part << endl;
            i++;
        }

        cout << "\n";

        int recipientClientNumber;
        cout << "Enter the client number to send a message (0 to quit): ";
        cin >> recipientClientNumber;

        if (recipientClientNumber == 0)
        {

            cout << "\nClient is quitting...\n";
            break;
        }

        string message1;
        cin.ignore();
        cout << "Enter message to send: ";
        getline(cin, message1);

        string fullMessage = to_string(recipientClientNumber) + "|" + message1;
        string encMess1 = encrypt(key, fullMessage);
        send(clientSocket, encMess1.c_str(), encMess1.size(), 0);
        break;
    }
}

void receiveMessages(int clientSocket)
{
    char buffer[1024];
    while (true)
    {
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            string receivedMessage(decrypt(key, buffer));
            size_t pos = receivedMessage.find("cnpls");
            if (pos != string::npos)
            {
                totClientActive = receivedMessage.substr(pos + 5);
            }
            else
            {
                lock_guard<mutex> lock(cout_mutex);
                string receivedMessage(decrypt(key, buffer));
                receivedMessage += "\n\nEnter message for server: ";
                cout << "Received from server: " << receivedMessage;
                cout << flush;
            }
        }
    }
}

int main()
{
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1)
    {
        perror("Error creating socket");
        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);

    // "127.0.0.1" for same pc
    if (inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr) <= 0)
    {
        perror("Invalid IP address");
        return 1;
    }

    if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1)
    {
        perror("Connection failed");
        return 1;
    }

    string clientName;
    cout << "\nEnter Your name: ";
    getline(cin, clientName);
    string encName = clientName;
    send(clientSocket, encName.c_str(), encName.size(), 0);

    cout << "Enter the key on which Client will decrypt and encrypt message: ";
    getline(cin, key);
    cout << "\nConnected to server on port 8080...\nEnter the Y to send a message to Client\n";

    thread receiveThread(receiveMessages, clientSocket);
    receiveThread.detach();

    while (true)
    {
        if (messageFromClient.empty())
        {
            string message;
            cout << "\nEnter message for server: ";
            getline(cin, message);
            if (message == "y" || message == "Y")
            {
                sendMessage(clientSocket);
            }
            else
            {
                string encMess = encrypt(key, message);
                send(clientSocket, encMess.c_str(), encMess.size(), 0);
                if (message == "quit")
                {
                    cout << "Client is quitting..." << endl;
                    break;
                }
            }
        }
        else
        {
            lock_guard<mutex> lock(cout_mutex);
            string clientkey;
            cout << "Enter Key for decryption: ";
            cin >> clientkey;
            string clientrecieve(decrypt(clientkey, messageFromClient));

            // std::this_thread::sleep_for(std::chrono::seconds(1));
            cout << "Received from client: " << clientrecieve;
            cout << flush;
            messageFromClient = "";
        }
    }

    close(clientSocket);

    return 0;
}