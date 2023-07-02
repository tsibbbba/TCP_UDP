#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
// Директива линковщику: использовать библиотеку сокетов
#pragma comment(lib, "ws2_32.lib")
#else // LINUX
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <arpa/inet.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
//#include <unistd.h>
#define WEBHOST "google.com"
#define MAX_FILE_NAME 128
#define MAX_IP_LENGTH 21
// #define EXIT_FAILURE -1
#define MAX_LENGTH_OF_NUMBER 12+1
#define EXIT_SUCCES 0
int cnt_of_send = 0;
int init()
{
#ifdef _WIN32
	// Для Windows следует вызвать WSAStartup перед началом использования сокетов
	WSADATA wsa_data;
	return (0 == WSAStartup(MAKEWORD(2, 2), &wsa_data));
#else
	return 1; // Для других ОС действий не требуется
#endif
}
void deinit()
{
#ifdef _WIN32
	// Для Windows следует вызвать WSACleanup в конце работы
	WSACleanup();
#else
	// Для других ОС действий не требуется
#endif
}
int sock_err(const char* function, int s)
{
	int err;
#ifdef _WIN32
	err = WSAGetLastError();
#else
	err = errno;
#endif
	fprintf(stderr, "%s: socket error: %d\n", function, err);
	return -1;
}
void s_close(int s)
{
#ifdef _WIN32
	closesocket(s);
#else
	close(s);
#endif
}
// Функция определяет IP-адрес узла по его имени.
// Адрес возвращается в сетевом порядке байтов.
unsigned int get_host_ipn(const char* name)
{
	struct addrinfo* addr = 0;
	unsigned int ip4addr = 0;
	// Функция возвращает все адреса указанного хоста
	// в виде динамического однонаправленного списка
	if (0 == getaddrinfo(name, 0, 0, &addr))
	{
		struct addrinfo* cur = addr;
		while (cur)
		{
			// Интересует только IPv4 адрес, если их несколько - то первый
			if (cur->ai_family == AF_INET)
			{
				ip4addr = ((struct sockaddr_in*)cur->ai_addr)->sin_addr.s_addr;
				break;
			}
			cur = cur->ai_next;
		}
		freeaddrinfo(addr);
	}
	return ip4addr;
}

typedef struct {
	char number1[MAX_LENGTH_OF_NUMBER];
	char number2[MAX_LENGTH_OF_NUMBER];
	char hh[3];
	char mm[3];
	char ss[3];
	char* message;
} _MSG;

// Отправляет http-запрос на удаленный сервер
int send_request(int s, char* request)
{
	//const char* request = "GET / HTTP/1.0\r\nServer: " WEBHOST "\r\n\r\n";
	int size = strlen(request);
	int sent = 0;
#ifdef _WIN32
	int flags = 0;
#else
	int flags = MSG_NOSIGNAL;
#endif
	while (sent < size)
	{
		// Отправка очередного блока данных
		int res = send(s, request + sent, size - sent, flags);
		if (res < 0)
			return sock_err("send", s);
		sent += res;
		printf(" %d bytes sent.\n", sent);
	}
	return 0;
}
int recv_response(int s)
{
	char buffer[256];
	int res;
	char ok[3];
	memset(ok, 0x00, 3);
	int offset = 0;
	int cnt_ok = 0;
	// Принятие очередного блока данных.
	// Если соединение будет разорвано удаленным узлом recv вернет 0

	res = recv(s, buffer, sizeof(buffer), 0);
	while (res > 1)
	{
		memcpy(ok, buffer + offset, 2);
		if (strcmp(ok, "ok") == 0)
		{
			cnt_ok++;
		}
		offset += 2;
		res -= 2;
		memset(ok, 0x00, 3);
	}
	printf(" %d bytes received\n", offset);
	if (res < 0) {
		printf("ddd\n");
		return sock_err("recv", s);
	}

	return cnt_ok;
}

void print_cmd_params(int argc, char* argv[]) {
	printf("argc = %d\n", argc);
	for (int i = 0; i < argc; i++) {
		printf("%s\n", argv[i]);
	}
}

void send_put(int s, char* put) {
	if (send(s, put, 3, 0) == -1) {
		printf("send_put error\n");
		exit(-1);
	}
}

int filesize(FILE* f) {
	fseek(f, 0, SEEK_END);
	int size = ftell(f);
	fseek(f, 0, SEEK_SET);
	return size;
}

// 0            13           26       36
// +70001234567 +79991234567 12:44:51 message1
void filling_socketdata(_MSG* _socket, char* msg, int len) {
	int start = 0;
	while (msg[start++] != '+');
	start--; // для того чтобы начинать считывать начиная с номера
	memset(_socket->number1, 0x00, MAX_LENGTH_OF_NUMBER);
	memcpy(_socket->number1, msg + start, MAX_LENGTH_OF_NUMBER - 1);
	//printf("%s ",_socket->number1 );

	memset(_socket->number2, 0x00, MAX_LENGTH_OF_NUMBER);
	memcpy(_socket->number2, msg + 13 + start, MAX_LENGTH_OF_NUMBER - 1);
	//printf("%s ",_socket->number2 );

	memset(_socket->hh, 0x00, 3);
	memcpy(_socket->hh, msg + 26 + start, 2);

	memset(_socket->mm, 0x00, 3);
	memcpy(_socket->mm, msg + 29 + start, 2);
	//printf("%s ",_socket->mm );

	memset(_socket->ss, 0x00, 3);
	memcpy(_socket->ss, msg + 32 + start, 2);
	//printf("%s ",_socket->ss );

	memset(_socket->message, 0x00, len);
	memcpy(_socket->message, msg + 35 + start, len - 1);
	//Зануление последнего элемента \n
	if(_socket->message[strlen(_socket->message) - 1] == '\n')
	_socket->message[strlen(_socket->message) - 1] = '\0';

}

int send_s_data(int s, _MSG* _socket, int str_num) {
	char hh = (char)atoi(_socket->hh);
	char mm = (char)atoi(_socket->mm);
	char ss = (char)atoi(_socket->ss);
	int num = htonl(str_num);
	int res;
	char* to_send = (char*)malloc(4 + strlen(_socket->number1) + strlen(_socket->number2) +
		3 + strlen(_socket->message));
	memset(to_send, 0x00, 4 + strlen(_socket->number1) + strlen(_socket->number2) +
		3 + strlen(_socket->message));
	memcpy(to_send, (char*)&num, 4);
	memcpy(to_send + 4, _socket->number1, 12);
	memcpy(to_send + 16, _socket->number2, 12);
	memcpy(to_send + 28, &hh, 1);
	memcpy(to_send + 29, &mm, 1);
	memcpy(to_send + 30, &ss, 1);
	memcpy(to_send + 31, _socket->message, strlen(_socket->message));
	res = send(s, to_send, 4 + strlen(_socket->number1) + strlen(_socket->number2) +
		3 + strlen(_socket->message), 0);
	return EXIT_SUCCES;
}

int wait_ok(int s, int cnt_ok) {
	return cnt_ok = recv_response(s);
}

void pars_and_send(int s, char* fname, char* ip) {
	FILE* f = fopen(fname, "r");
	if (f == NULL) {
		printf("file open error\n");
		exit(EXIT_FAILURE);
	}
	_MSG* socket_data = (_MSG*)malloc(sizeof(_MSG));

	int size_of_file = filesize(f);
	socket_data->message = (char*)malloc((size_of_file));
	char* str_arr = (char*)malloc(sizeof(char) * size_of_file);
	memset(str_arr, 0x00, size_of_file);
	// printf("%d\n",sizeof(str_arr) );
	int i = 1;
	int number_of_string = -1;
	int cnt_ok;
	printf("---------------------------------------\n");
	while (!feof(f)) {
		fgets(str_arr, size_of_file + 1, f);
		if (strlen(str_arr) < 28) continue;
		number_of_string++;
		printf("%d\n", number_of_string);
		filling_socketdata(socket_data, str_arr, size_of_file);
		if (send_s_data(s, socket_data, number_of_string) == EXIT_FAILURE) {
			printf("socket send error %d\n", i++);
			exit(EXIT_FAILURE);
		}
		memset(str_arr, 0x00, size_of_file);
		memset(socket_data, 0x00, sizeof(_MSG));
		socket_data->message = (char*)malloc((size_of_file));
		cnt_ok = wait_ok(s, number_of_string);//-put
		if (cnt_ok != 7)
		{
			printf("Answer not found\n");
		}
	}

	//
	printf("----------------------------------------\n");
}

char* get_ip(char* ip_only, char* ip_addr) {
	strcpy(ip_only, ip_addr);
	ip_only[strlen(ip_addr) - 6] = '\0';
	return ip_only;
}

int get_port(char* ip_addr) {
	int len = strlen(ip_addr) - 1;
	char port[10]; memset(port, 0x00, 10);
	char* get_port_pos = strstr(ip_addr, ":");
	int pos = get_port_pos - ip_addr;
	memcpy(port, ip_addr + pos + 1, strlen(ip_addr) - pos);
	return atoi(port);
}
void swap(char* x, char* y) {
	char t = *x; *x = *y; *y = t;
}

// Функция для обращения `buffer[i…j]`
char* reverse(char* buffer, int i, int j)
{
	while (i < j) {
		swap(&buffer[i++], &buffer[j--]);
	}

	return buffer;
}

// Итеративная функция для реализации функции `itoa()` в C
char* __itoa(int value, char* buffer, int base)
{
	// неправильный ввод
	if (base < 2 || base > 32) {
		return buffer;
	}

	// считаем абсолютное значение числа
	int n = abs(value);

	int i = 0;
	while (n)
	{
		int r = n % base;

		if (r >= 10) {
			buffer[i++] = 65 + (r - 10);
		}
		else {
			buffer[i++] = 48 + r;
		}

		n = n / base;
	}

	// если число равно 0
	if (i == 0) {
		buffer[i++] = '0';
	}

	// Если основание равно 10 и значение отрицательное, результирующая строка
	// предшествует знак минус (-)
	// При любой другой базе значение всегда считается беззнаковым
	if (value < 0 && base == 10) {
		buffer[i++] = '-';
	}

	buffer[i] = '\0'; // нулевая завершающая строка

	// переворачиваем строку и возвращаем ее
	return reverse(buffer, 0, i - 1);
}
void parsing(char* name_of_file, char* ip_addr, char* ip_only, int* port, char** argv) {
	strcpy(ip_addr, argv[1]);      //111.111.111.111:9000
	strcpy(name_of_file, argv[2]); //sample.txt
	strcpy(ip_only, ip_addr);
	*port = get_port(ip_addr);
	char _port[10];
	ip_only[strlen(ip_addr) - strlen(__itoa(*port, _port, 10)) - 1] = '\0';
}

int main(int argc, char* argv[])
{
	//PARSING ARGUMENTS-----------------------------
  //argv[0] - name of programm - tcpclient
  //argv[1] - ip address:port
  //argv[2] - name of file
	//print_cmd_params(argc, argv);
	char name_of_file[MAX_FILE_NAME]; memset(name_of_file, 0x00, MAX_FILE_NAME);
	char ip_addr[MAX_IP_LENGTH];      memset(ip_addr, 0x00, MAX_IP_LENGTH);
	char ip_only[MAX_IP_LENGTH];      memset(ip_only, 0x00, MAX_IP_LENGTH);
	int port;
	parsing(name_of_file, ip_addr, ip_only, &port, argv);

	int s;
	struct sockaddr_in addr;
	FILE* f;
	// Инициалиазация сетевой библиотеки
	init();
	// Создание TCP-сокета
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return sock_err("socket", s);
	// Заполнение структуры с адресом удаленного узла
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);     //htons(80); //htonl(port)
	//addr.sin_port = htons(6000);
	addr.sin_addr.s_addr = inet_addr(ip_only);//ip_only

	int attempt = 10;
	while (attempt) {
		if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) != 0)
		{
			attempt--;
			if (attempt == 0) {
				printf("connect error\n");
				s_close(s);
				return sock_err("connect", s);
			}
			Sleep(100);
		}
		else break;
	}
	printf("Connected is success\n");

	send_put(s, (char*)"put");
	char put[3]; memset(put, 0x00, 3);
	//int rcv = recv(s, put, 3, 0);
	pars_and_send(s, name_of_file, ip_addr);
	// Закрытие соединения
	s_close(s);
	deinit();
	return 0;
}
