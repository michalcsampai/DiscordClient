#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pcap.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <ifaddrs.h>
#include <csignal>
#include <map>
#include <chrono>

#include "protocolModel.h"

struct PendingMessage
{
    std::vector<uint8_t> data;
    sockaddr_in addr;
    std::chrono::steady_clock::time_point lastSent;
    int retries;
};

class UdpComm : public ProtocolModel
{

public:
    int CreateSocket(int port, string ipAdress) override;
    int GetSocket() override;

    void SendMessage(const uint8_t type, const vector<string> msgVec) override;

    void SendConfirmMessage(const uint8_t type, const vector<string> msgVec, const uint16_t id);

    ssize_t RecieveMessage(std::vector<uint8_t> &buffer)override;

    void CheckPendingMessages(int UDP_Retry, int UDP_confirmationTimeOut);
    void RemovePendingMessage(uint16_t id);

    void CloseSocket() override;

private:
    uint16_t messageID;
    int sockfd;
    sockaddr_in serverAddr{};
    std::map<uint16_t, PendingMessage> pendingMessages;

    vector<uint8_t> buildMessageBinary(const uint16_t messageId, const uint16_t messageType, vector<string> messageContents);
};