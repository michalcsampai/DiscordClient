CC = g++
CXXFLAGS = -Wall -Wextra -std=c++20
LDFLAGS = -lpcap
TARGET = chat-client
SRC = main.cpp udpComm.cpp tcpComm.cpp protocolModel.h

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)
