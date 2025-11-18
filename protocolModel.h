#pragma once 

#include <iostream>
#include <vector>
#include <string>
#include <cstring>

using namespace std;

class ProtocolModel
{

public:
    virtual int CreateSocket(int port, string ipAdress) = 0;

    virtual int GetSocket() = 0;

    virtual void SendMessage(const uint8_t type,const vector<string> msgVec)= 0;

    virtual void CloseSocket() = 0;

    virtual ssize_t RecieveMessage(std::vector<uint8_t> &buffer) = 0;
};