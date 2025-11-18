#include <vector>
#include <string>
#include <cstdint>
#include <chrono>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "protocolModel.h"


using namespace std;

class TcpComm: public ProtocolModel
{
private:
    int sockfd;
    sockaddr_in serverAddr;
    vector<uint8_t> buildMessageBinary(const uint16_t messageType, vector<string> messageContents);

    

public:
    int CreateSocket(int port, string ipAdress)override;
    void SendMessage(const uint8_t type, const vector<string> msgVec)override;
    ssize_t RecieveMessage(vector<uint8_t> &buffer)override;
    void CloseSocket()override;
    int GetSocket()override;

    uint16_t GetMessageType(vector<uint8_t> buffer);
};