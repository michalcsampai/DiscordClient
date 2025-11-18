#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <csignal>

#include "udpComm.h"
#include "tcpComm.h"
#include "protocolModel.h"

using namespace std;

enum class State
{
    START,
    AUTH,
    OPEN,
    JOIN,
    END
};

volatile sig_atomic_t interrupted = 0;

void handleSigint(int)
{
    interrupted = 1;
}

string resolveIP_Adress(const string &hostname)
{
    // Will return the first found ip
    struct sockaddr_in sa;

    // Check if it's a valid IPv4 address
    if (inet_pton(AF_INET, hostname.c_str(), &(sa.sin_addr)) == 1)
        return hostname;

    string ip;
    struct addrinfo hints{}, *res, *p;

    char ipstr[INET_ADDRSTRLEN];

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    // Resolves the hostname to one or more IP addresses
    if (getaddrinfo(hostname.c_str(), NULL, &hints, &res) != 0)
    {
        cerr << "Failed to resolve hostname: " << hostname << endl;
        return ip;
    }

    // Loop through the list of resolved addresses
    for (p = res; p != NULL; p = p->ai_next)
    {
        if (p->ai_family == AF_INET) // If ipv4 is true, only add IPv4 addresses
        {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            inet_ntop(AF_INET, &ipv4->sin_addr, ipstr, sizeof(ipstr));
            ip = ipstr;
            // break afer first found IvP4 adress
            break;
        }
    }

    freeaddrinfo(res);
    return ip;
}

std::vector<string> splitInputLine(const string &inputLine, const string delimiter)
{
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = inputLine.find(delimiter, pos_start)) != std::string::npos)
    {
        token = inputLine.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }

    res.push_back(inputLine.substr(pos_start));
    return res;
}

std::vector<string> splitMessageLine(const string &inputLine, const char delimiter)
{
    std::vector<std::string> result;
    std::string token;

    for (char ch : inputLine)
    {
        if (ch == delimiter)
        {
            result.push_back(token);
            token.clear();
        }
        else
        {
            token += ch;
        }
    }

    result.push_back(token); // push last segment
    return result;
}

int main(int argc, char *argv[])
{
    int UDP_confirmationTimeOut = 250;
    uint16_t port = 4567;
    uint8_t UDP_Retry = 3;
    string ipAdress;
    string type;

    bool isUdpFlag = false;
    // Handle C-c
    signal(SIGINT, handleSigint);
    // TODO Handle C-d

    constexpr uint8_t MSG_CONFIRM = 0x00;
    constexpr uint8_t MSG_REPLY = 0x01;
    constexpr uint8_t MSG_AUTH = 0x02;
    constexpr uint8_t MSG_JOIN = 0x03;
    constexpr uint8_t MSG_MSG = 0x04;
    constexpr uint8_t MSG_PING = 0xFD;
    constexpr uint8_t MSG_ERR = 0xFE;
    constexpr uint8_t MSG_BYE = 0xFF;

    for (int i = 1; i < argc; ++i)
    {

        string arg = argv[i];

        if (arg == "-t")
        {
            type = argv[i + 1];
            ++i;
        }
        else if (arg == "-s")
        {
            ipAdress = resolveIP_Adress(argv[i + 1]);
            ++i;
        }
        else if (arg == "-p")
        {
            port = stoi(argv[i + 1]);
            ++i;
        }
        else if (arg == "-d")
        {
            UDP_confirmationTimeOut = stoi(argv[i + 1]);
            ++i;
        }
        else if (arg == "-r")
        {
            UDP_Retry = stoi(argv[i + 1]);
            ++i;
        }
        else if (arg == "-h")
        {
            return EXIT_SUCCESS;
        }
    }

    if (type == "")
        return EXIT_FAILURE;
    if (ipAdress == "")
        return EXIT_FAILURE;

    std::string inputLine;
    bool conversationEnded = false;
    string name;
    State currentState = State::START;

    ProtocolModel *protocolModel = nullptr;

    if (type == "udp")
    {
        protocolModel = new UdpComm();
        isUdpFlag = true;
    }
    else if (type == "tcp")
    {
        protocolModel = new TcpComm();
    }

    if (protocolModel->CreateSocket(port, ipAdress) == 1)
    {
        delete protocolModel;
        return EXIT_FAILURE;
    }

    // Set socket and stdin non-blocking
    fcntl(protocolModel->GetSocket(), F_SETFL, O_NONBLOCK);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    while (!conversationEnded)
    {
        // check for end
        if (interrupted)
        {
            protocolModel->SendMessage(MSG_BYE, {name});
            currentState = State::END;
            break;
        }
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(protocolModel->GetSocket(), &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        int maxfd = std::max(protocolModel->GetSocket(), STDIN_FILENO) + 1;
        // setTimeout
        timeval timeout{};
        timeout.tv_sec = UDP_confirmationTimeOut / 1000;
        timeout.tv_usec = 0;

        // Check for Pending Messages
        if (UdpComm *udp = dynamic_cast<UdpComm *>(protocolModel))
        {
            udp->CheckPendingMessages(UDP_Retry, UDP_confirmationTimeOut);
        }

        int activity = select(maxfd, &readfds, nullptr, nullptr, &timeout);

        if (activity < 0)
            continue;

        // Handle user input
        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            std::string userInput;

            if (!std::getline(std::cin, userInput))
            {
                protocolModel->SendMessage(MSG_BYE, {name});

                currentState = State::END;
                conversationEnded = true;
                continue;
            }

            if (!userInput.empty())
            {
                std::vector<string> inputVec = splitInputLine(userInput, " ");
                switch (currentState)
                {
                case State::START:
                {

                    if (inputVec[0] == "/auth")
                    {
                        name = inputVec[2];

                        inputVec.erase(inputVec.begin());
                        protocolModel->SendMessage(MSG_AUTH, inputVec);

                        currentState = State::AUTH;
                    }
                    else if (inputVec[0] == "/help")
                    {
                        std::cout << "Usage:\n /help\n-shows help message\n/auth {name} {displayName} {secret}\n-used in authentication" << '\n';
                    }
                    else if (inputVec[0] == "/rename")
                    {
                        name = inputVec[1];
                    }
                    else
                    {
                        std::cout << "ERROR: Incorect Input: Try /help for showing help message\n";
                    }
                }
                break;
                case State::AUTH:
                {
                    if (inputVec[0] == "/auth")
                    {
                        name = inputVec[2];
                        inputVec.erase(inputVec.begin());
                        protocolModel->SendMessage(MSG_AUTH, inputVec);
                    }
                    else if (inputVec[0] == "/rename")
                    {
                        name = inputVec[1];
                    }
                    else if (inputVec[0] == "/help")
                    {
                        std::cerr << "Usage:\n /help\n-shows help message\n/auth {name} {displayName} {secret}\n-used in autenticication" << '\n';
                    }
                    else
                    {
                        std::cout << "ERROR: Incorect Input: Try /help for showing help message\n";
                    }
                }
                break;
                case State::OPEN:
                {
                    if (inputVec[0] == "/help")
                    {
                        std::cout << "Usage:\n /help\n-shows help message\n/auth {name} {displayName} {secret}\n-used in autenticication" << '\n';
                    }
                    else if (inputVec[0] == "/join")
                    {
                        inputVec.erase(inputVec.begin());
                        inputVec.push_back(name);
                        protocolModel->SendMessage(MSG_JOIN, inputVec);
                        currentState = State::JOIN;
                    }
                    else if (inputVec[0] == "/rename")
                    {
                        name = inputVec[1];
                    }
                    else if (inputVec[0] == "/auth")
                    {
                        std::cout << "ERROR: Incorect Input: Try /help for showing help message\n";
                    }
                    else
                    {
                        protocolModel->SendMessage(MSG_MSG, {name, userInput});
                    }
                }
                break;
                case State::JOIN:
                {
                    if (inputVec[0] == "/rename")
                    {
                        name = inputVec[1];
                    }
                    break;
                }
                case State::END:
                    conversationEnded = true;
                    break;
                }
            }
        }

        // Handle incoming messages
        if (FD_ISSET(protocolModel->GetSocket(), &readfds))
        {
            vector<uint8_t> buffer(1024);

            ssize_t len = protocolModel->RecieveMessage(buffer);

            if (len >= 3)
            {
                uint8_t type;
                uint16_t recvID =  (buffer[1] << 8) | buffer[2];
                if (TcpComm *tcp = dynamic_cast<TcpComm *>(protocolModel))
                {
                    type = tcp->GetMessageType(buffer);
                }
                else
                {
                    type = buffer[0];
                }

                switch (type)
                {
                case MSG_CONFIRM:
                {
                    if (UdpComm *udp = dynamic_cast<UdpComm *>(protocolModel))
                    {
                        udp->RemovePendingMessage(recvID);
                    }
                    break;
                }

                case MSG_REPLY:
                {
                    string replyText;
                    string ok;
                    if(isUdpFlag) replyText = string(buffer.begin() + 6, buffer.begin() + len);
                    else 
                    {
                        vector<string> replyVec;
                        replyVec = splitInputLine(string(buffer.begin() + 6, buffer.begin() + len), " IS ");
                        replyText = replyVec[1];
                        ok = replyVec[0];
                    }
                    
                    if (buffer[3] == 1 || ok == "OK")
                    {
                        fprintf(stdout, "Action Success: %s\n", replyText.c_str());
                        currentState = State::OPEN;
                    }
                    else
                    {
                        fprintf(stdout, "Action Failure: %s\n", replyText.c_str());
                        if (currentState == State::JOIN)
                            currentState = State::OPEN;
                        if (currentState == State::AUTH)
                            currentState = State::AUTH;
                    }
                    if (UdpComm *udp = dynamic_cast<UdpComm *>(protocolModel))
                    {
                        udp->SendConfirmMessage(MSG_CONFIRM, {}, recvID);
                    }

                    break;
                }
                case MSG_MSG:
                {
                    std::string replyText(buffer.begin() + 3, buffer.begin() + len);
                    std::vector<string> reply;
                    if(isUdpFlag) reply = splitMessageLine(replyText, '\0');
                    else 
                    {
                        reply = splitInputLine(string(buffer.begin() + 9, buffer.begin() + len), " IS ");
                    }

                    fprintf(stdout, "%s: %s\n", reply[0].c_str(), reply[1].c_str());

                    if (UdpComm *udp = dynamic_cast<UdpComm *>(protocolModel))
                    {
                        udp->SendConfirmMessage(MSG_CONFIRM, {}, recvID);
                    }
                    break;
                }
                case MSG_ERR:
                {
                    std::string replyText(buffer.begin() + 3, buffer.begin() + len);
                    std::vector<string> reply;;
                    if(isUdpFlag) reply = splitMessageLine(replyText, '\0');
                    else 
                    {
                        reply = splitInputLine(string(buffer.begin() + 9, buffer.begin() + len), " IS ");
                    }

                    fprintf(stdout, "ERROR FROM %s: %s\n", reply[0].c_str(), reply[1].c_str());

                    if (UdpComm *udp = dynamic_cast<UdpComm *>(protocolModel))
                    {
                        udp->SendConfirmMessage(MSG_CONFIRM, {}, recvID);
                    }
                    currentState = State::END;
                    break;
                }
                case MSG_BYE:
                {
                    if (UdpComm *udp = dynamic_cast<UdpComm *>(protocolModel))
                    {
                        udp->SendConfirmMessage(MSG_CONFIRM, {}, recvID);
                    }
                    currentState = State::END;
                    break;
                }
                case MSG_PING:
                {
                    // Send just confirmation message
                    if (UdpComm *udp = dynamic_cast<UdpComm *>(protocolModel))
                    {
                        udp->SendConfirmMessage(MSG_CONFIRM, {}, recvID);
                    }
                    break;
                }
                default:
                {
                    if (UdpComm *udp = dynamic_cast<UdpComm *>(protocolModel))
                    {
                        udp->SendConfirmMessage(MSG_CONFIRM, {}, recvID);
                    }
                    break;
                }
                }

                if (currentState == State::END)
                    break;
            }
        }
    }
    protocolModel->CloseSocket();
    delete protocolModel;
    return 0;

    return EXIT_SUCCESS;
}