#pragma once
#include<string>
#include<iostream>
#include<WinSock2.h>
#include<WS2tcpip.h>
#pragma comment(lib,"WS2_32.lib")
class Receive {
private:
	int recBytes;
	char rxBuffer[1024];
public:
	std::string receive(SOCKET smth);
	~Receive() {}
};