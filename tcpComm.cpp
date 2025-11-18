#include "tcpComm.h"
#include <iostream>

vector<uint8_t> TcpComm::buildMessageBinary(const uint16_t messageType, vector<string> messageContents)
{
    std::vector<uint8_t> msg;
    string tcpMessage;

    constexpr uint8_t MSG_REPLY = 0x01;
    constexpr uint8_t MSG_AUTH = 0x02;
    constexpr uint8_t MSG_JOIN = 0x03;
    constexpr uint8_t MSG_MSG = 0x04;
    constexpr uint8_t MSG_PING = 0xFD;
    constexpr uint8_t MSG_ERR = 0xFE;
    constexpr uint8_t MSG_BYE = 0xFF;

    switch (messageType)
    {
    case MSG_AUTH:
        tcpMessage =  "AUTH " + messageContents[0] + " AS " + messageContents[1] + " USING " + messageContents[2] + "\r\n";
        break;
    case MSG_JOIN:
        tcpMessage =  "JOIN " + messageContents[0] + " AS " + messageContents[1] + "\r\n";
        break;
    case MSG_MSG:
        tcpMessage = "MSG FROM "+messageContents[0]+" IS " +messageContents[1]+"\r\n";
        break;
    case MSG_ERR:
        tcpMessage = "ERR FROM "+messageContents[0]+" IS " +messageContents[1]+"\r\n";
        break;
    case MSG_BYE:
        tcpMessage = "BYE FROM "+messageContents[0]+"\r\n";
        break;
    }
    // Header
    
    msg.insert(msg.end(), tcpMessage.begin(), tcpMessage.end());

    return msg;
}

int TcpComm::CreateSocket(int port, string ipAdress)
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(ipAdress.c_str());

    if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
    {
        return 1;
    }

    return 0;
}

void TcpComm::SendMessage(const uint8_t type, const vector<string> msgVec)
{
    auto msg = buildMessageBinary(type, msgVec);
    send(sockfd, msg.data(), msg.size(), 0);
}

ssize_t TcpComm::RecieveMessage(vector<uint8_t> &buffer)
{
    return recv(sockfd, buffer.data(), buffer.size(), 0);
}

void TcpComm::CloseSocket()
{
    if (sockfd >= 0)
    {
        close(sockfd);
        sockfd = -1;
    }
}

int TcpComm::GetSocket()
{
    return sockfd;
}


uint16_t TcpComm::GetMessageType(vector<uint8_t> buffer)
{
    
    constexpr uint8_t MSG_REPLY = 0x01;
    constexpr uint8_t MSG_MSG = 0x04;
    constexpr uint8_t MSG_ERR = 0xFE;
    constexpr uint8_t MSG_BYE = 0xFF;

    string head(buffer.begin(), buffer.begin()+5);

    if(head == "REPLY") return MSG_REPLY;
    head.resize(3);
    if(head == "MSG") return MSG_MSG;
    if(head == "ERR") return MSG_ERR;
    if(head == "BYE") return MSG_BYE;

    return 0x00;
}
