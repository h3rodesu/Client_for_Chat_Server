#include "Receive.h"
std::string Receive::receive(SOCKET smth) {
	memset(rxBuffer, 0, sizeof(rxBuffer));//чтобы не было нового текста поверх старого
	recBytes = recv(smth, rxBuffer, sizeof(rxBuffer) - 1, 0);
	if (recBytes > 0) {
		rxBuffer[recBytes] = '\0';
		//std::cout << rxBuffer << std::endl;//Добавил т.к. не было вывода
		return rxBuffer;//Да,возвр.значение стринг а вернет чар
	}
	else {
		std::cout << "Error of receive" << WSAGetLastError() << std::endl;
		return "";//Вместо чар вернет пустоту при ошибке
	}

}