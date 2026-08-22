#include<iostream>
#include<string>
#include<thread>
#include<mutex>
#include<chrono>
#include<WinSock2.h>
#include<WS2tcpip.h>
#pragma comment(lib,"WS2_32.lib")
#include "Receive.h"
//Сделать класс для recv отдельный
int main() {
	setlocale(LC_ALL, "Russian");
	std::mutex mtx;
	Receive read;
	int choice;
	bool lostcon = false;
	bool userquit = false;
	//Подготовка-инициализация
	while (lostcon == false) {
		WSADATA wsa;//Адрес
		WSAStartup(MAKEWORD(2, 2), &wsa);//Инициализация
		//Создание сокета
		SOCKET UserSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (UserSocket == INVALID_SOCKET) {//не уверен что нужна проверка т.к. она по идее выполняется на сервере
			std::cerr << WSAGetLastError() << std::endl;
			closesocket(UserSocket);
			return 1;
		}
		sockaddr_in clientADDR;//Хранит адрес сервера,здесь настраивается адрес СЕРВЕРА
		clientADDR.sin_family = AF_INET;
		clientADDR.sin_port = htons(8080);
		if (inet_pton(AF_INET, "127.0.0.1", &clientADDR.sin_addr) <= 0) {
			std::cerr << "ERROR!NON VALID IP!" << std::endl;
		}
		else {
			std::cout << "inet_pton completed the task" << std::endl;
		}
		if (connect(UserSocket, (sockaddr*)&clientADDR, sizeof(clientADDR)) < 0) {
			std::cerr << "WARNING!CONNECTION ERROR!" << WSAGetLastError() << std::endl;
			closesocket(UserSocket);//Перед новой попыткой покючения обязательно чищу сокет
			//lostcon = true;
		//	break;//Выход из цикла 
			continue;//Обратно в начало цикла для реконнекта
		}
		else {
			std::cout << "Successful connection!" << std::endl;
			std::string welcome = read.receive(UserSocket);//Принимается строка welcome
			if (welcome.empty()) {
				std::cout << "ERROR" << WSAGetLastError() << std::endl;
			}
			else {
				while (true) {
					std::cout << welcome << std::endl;
					std::cout << "1-signin 2-registration" << std::endl;
					std::cin >> choice;
					if (choice == 2) {
						std::string nickname;
						std::cin >> std::ws;
						std::getline(std::cin, nickname);
						std::string newnick = "REGISTRATION|" + nickname + "\n";
						send(UserSocket, newnick.c_str(), (int)newnick.size(), 0);
						std::string waitauth = read.receive(UserSocket);
						if (waitauth.find("Register_OK|") != std::string::npos) {//Если слово найдено то все ок
							std::cout << "Welcome to chat!" << std::endl;
							break;
							//send(UserSocket, newnick.c_str(), (int)newnick.size(), 0);
						}
					}
					else if (choice == 1) {
						std::string nickname;
						std::cin >> std::ws;
						std::getline(std::cin, nickname);
						std::string newnick = "SIGNIN|" + nickname + "\n";
						send(UserSocket, newnick.c_str(), (int)newnick.size(), 0);
						std::string waitauth = read.receive(UserSocket);
						if (waitauth.find("Auth_OK|") != std::string::npos) {
							std::cout << "Welcome to chat!" << std::endl;
							break;
						}
					}
				}

			}
		}
		std::thread write([UserSocket, &userquit]() {
			while (true) {
				std::string message;
				std::getline(std::cin, message);
				if (message.rfind("/nick", 0) == 0) {
					std::string newNick = message.substr(6);//беру все что после /nick
					if (!newNick.empty()) {
						std::string com = "CHANGE_NICK|" + newNick + "";
						send(UserSocket, com.c_str(), (int)com.size(), 0);

					}
				}
				else if (message.rfind("/quit", 0) == 0) {
					std::string quit = "QUIT|";
					send(UserSocket, quit.c_str(), (int)quit.size(), 0);
					//lostcon здесь так же=фолс
					userquit = true;
					break;
				}
				std::string fullmsg = message + "\n";
				int res = send(UserSocket, fullmsg.c_str(), (int)fullmsg.size(), 0);
				if (res == SOCKET_ERROR) {

					break;
				}
			}
			});
		write.detach();//фикс abort()
		std::thread learn([UserSocket, &read, &lostcon]() {
			while (true) {
				std::string incmsg = read.receive(UserSocket);
				if (!incmsg.empty()) {
					std::cout << incmsg << std::endl;
				}
				else {
					std::cout << "Lost connection with server...disconnect" << std::endl;
					lostcon = true;//Смена флага
					break;

				}
			}
			});
		learn.join();//нужно отвязать поток,объект внури него уничтожается а ждать смерти этог потока не нужно
		//}
		if (userquit == true) {
			closesocket(UserSocket);
			break;
		}
		if (lostcon == true) {
			std::cout << "Trying reconnect..." << std::endl;
			closesocket(UserSocket);
			lostcon = false;
			std::this_thread::sleep_for(std::chrono::seconds(3));//Попытка реконнекта раз в 3 секунды
		}
	}
	WSACleanup();
	return 0;
};