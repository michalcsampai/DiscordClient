#include "udpComm.h"

vector<uint8_t> UdpComm::buildMessageBinary(const uint16_t messageId, const uint16_t messageType, vector<string> messageContents)
{
    std::vector<uint8_t> msg;

    // header
    msg.push_back(messageType);
    msg.push_back((messageId >> 8) & 0xFF);
    msg.push_back(messageId & 0xFF);

    // Message

    for (string mess : messageContents)
    {
        msg.insert(msg.end(), mess.begin(), mess.end());
        msg.push_back('\0');
    }

    return msg;
}

int UdpComm::CreateSocket(int port, string ipAdress)
{
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        fprintf(stdout, "ERROR: Failed to create socket\n");
        return 1;
    }
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(ipAdress.c_str());
    return 0;
}

void UdpComm::CheckPendingMessages(int UDP_Retry, int UDP_confirmationTimeOut)
{
    // check for pending messages
    auto now = std::chrono::steady_clock::now();
    for (auto it = pendingMessages.begin(); it != pendingMessages.end();)
    {
        auto &pending = it->second;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - pending.lastSent).count();

        if (elapsed >= UDP_confirmationTimeOut)
        {
            if (pending.retries >= UDP_Retry)
            {
                it = pendingMessages.erase(it); // Erase and advance iterator
                std::cout << "ERROR: message was not delivered " << std::endl;
            }
            else
            {
                // Resend the message
                sendto(sockfd, pending.data.data(), pending.data.size(), 0,
                       (sockaddr *)&pending.addr, sizeof(pending.addr));
                pending.lastSent = now;
                pending.retries++;
                ++it;
            }
        }
        else
        {
            ++it;
        }
    }
}

void UdpComm::RemovePendingMessage(uint16_t id)
{
    pendingMessages.erase(id);
}


void UdpComm::SendMessage(const uint8_t type, const vector<string> msgVec)
{
    auto msg = buildMessageBinary(messageID, type, msgVec);
    sendto(sockfd, msg.data(), msg.size(), 0,
           (struct sockaddr *)&serverAddr, sizeof(serverAddr));

    PendingMessage pmsg{
        .data = msg,
        .addr = serverAddr,
        .lastSent = std::chrono::steady_clock::now(),
        .retries = 0};

    pendingMessages[messageID] = pmsg;
    messageID++;
}

void UdpComm::SendConfirmMessage(const uint8_t type, const vector<string> msgVec, const uint16_t id)
{
    auto msg = buildMessageBinary(id, type, msgVec);
    sendto(sockfd, msg.data(), msg.size(), 0,
           (struct sockaddr *)&serverAddr, sizeof(serverAddr));
}

ssize_t UdpComm::RecieveMessage(std::vector<uint8_t>& buffer)
{
    sockaddr_in fromAddr{};
    socklen_t fromLen = sizeof(fromAddr);

    ssize_t len = recvfrom(sockfd, buffer.data(), buffer.size(), 0,
                           (struct sockaddr *)&fromAddr, &fromLen);

    serverAddr = fromAddr;
    return len;
}
int UdpComm::GetSocket()
{
    return sockfd;
}

void UdpComm::CloseSocket()
{
    close(sockfd);
}