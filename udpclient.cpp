#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
// Директива линковщику: использовать библиотеку сокетов
#pragma comment(lib, "ws2_32.lib")
#else // LINUX
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#define MAX_FILE_NAME 128
#define MAX_IP_LENGTH 64
#define EXIT_FAILURE 1
#ifdef _WIN32
int flags = 0;
#else
int flags = MSG_NOSIGNAL;
#endif

void send_data(FILE*, int, struct sockaddr_in*);
int _send(int ,char*, char*, int, struct sockaddr_in*);
int recv_response(int );

int ACCESS = 0;
char* get_mes; // содержит номера принятых сообщений по  индексу
int cnt_of_messages;// кол во сообщений в файле
int flag = 0;
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



int get_port(char* ip_addr) {
	int len = strlen(ip_addr) - 1;
	char port[10]; memset(port, 0x00, 10);
	char* get_port_pos = strstr(ip_addr, ":");
	int pos = get_port_pos - ip_addr;
	memcpy(port, ip_addr + pos + 1, strlen(ip_addr) - pos);
	return atoi(port);
}

void parsing(char* name_of_file, char* ip_addr, char* ip_only, int* port, char** argv) {
	strcpy(ip_addr, argv[1]);      //111.111.111.111:9000
	strcpy(name_of_file, argv[2]); //sample.txt
	strcpy(ip_only, ip_addr);
	*port = get_port(ip_addr);
	char _port[10];
	ip_only[strlen(ip_addr) - strlen(_itoa(*port,_port, 10)) - 1] = '\0';
}

int check_cmd_params(int argc) {
	if (argc == 3) return 1;
	return 0;
}

int filesize(FILE* f) {
	fseek(f, 0, SEEK_END);
	int size = ftell(f);
	fseek(f, 0, SEEK_SET);
	return size;
}

void get_net_data(int num, char* send) {
	int tmp = htonl(num);
	//char* data = (char*)&tmp;
	memcpy(send, &tmp, 4);
}

int main(int argc, char* argv[]) {
	// udpclient 192.168.50.7:8700 file1.txt
	if (!check_cmd_params(argc)) {
		printf("cmd params not correct\n");
		exit(EXIT_FAILURE);
	}
	char name_of_file[MAX_FILE_NAME]; memset(name_of_file, 0x00, MAX_FILE_NAME);
	char ip_addr[MAX_IP_LENGTH];      memset(ip_addr, 0x00, MAX_IP_LENGTH);
	char ip_only[MAX_IP_LENGTH];      memset(ip_only, 0x00, MAX_IP_LENGTH);
	int port;
	parsing(name_of_file, ip_addr, ip_only, &port, argv);
	int s;
	struct sockaddr_in addr;
	// Инициалиазация сетевой библиотеки
	init();
	// Создание UDP-сокета
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return sock_err("socket", s);
	// Заполнение структуры с адресом удаленного узла
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(ip_only);

	FILE* f = fopen(name_of_file, "r");
	if (f == NULL) {
		printf("file open error\n");
		exit(EXIT_FAILURE);
	}
	while (1)
	{
		// send == 0 => соединение разорвано
		// recv == 0 => соединение разорвано 6009 6012
		send_data(f, s, &addr);
		recv_response(s);
		if (ACCESS)
		{
			printf("data send\n");
			s_close(s);
			deinit();
			return 0;
		}
	}
}

int recv_response(int s) {
	char datagram[1024];
	struct timeval tv = { 0, 100 * 1000 }; // 100 msec
	int res;
	fd_set fds;
	FD_ZERO(&fds); FD_SET(s, &fds);
	// Проверка - если в сокете входящие дейтаграммы 
	// (ожидание в течение tv)
	res = select(s + 1, &fds, 0, 0, &tv);
	if (res > 0) // Данные есть, считывание их
	{
		struct sockaddr_in addr;
		socklen_t addrlen = sizeof(addr);
		int received = recvfrom(s, datagram, sizeof(datagram), 0, (struct sockaddr*)&addr, &addrlen);
		FILE* f = fopen("test.txt", "w+");
		fwrite(datagram, 1, 100, f);
		fclose(f);
		/*for (size_t i = 0; i < 100; i++)
		{
			printf("%c", datagram[i]);
		}*/
		if (received <= 0) // Ошибка считывания полученной дейтаграммы
		{
			printf("1 - %d", WSAGetLastError());
			sock_err("recvfrom", s);
			return 0;
		}////
		else
		{
				int number_of_message = 0;
				memcpy(&number_of_message, datagram, 4);
				number_of_message = ntohl(number_of_message);
				int index = 0;
				memcpy(&index, datagram, 4);
				index = ntohl(index);
				get_mes[index] = '1';
				int cnt_recv = 0;
				for (size_t i = 0; i < cnt_of_messages; i++)
				{
					if (get_mes[i] == '1')
					{
						cnt_recv++;
					}
				}
				if (cnt_of_messages == cnt_recv /* || cnt_recv >= 20 */ )
				{
					ACCESS = 1;
					return 0;
				}
			
		}
		return 0;
	}
	else if (res == 0) // Данных в сокете нет, возврат ошибки1
		return 0;
}

void send_data(FILE* f, int s, struct sockaddr_in* addr) {
	int file_size = filesize(f);
	char* message = (char*)malloc(sizeof(char) * file_size); memset(message, 0x00, file_size);
	char* to_send = (char*)malloc(sizeof(char) * file_size); memset(to_send, 0x00, file_size);
	int number_of_string = -1;
	int minus = 0;
	while (!feof(f))
	{	
		if (flag == 0)
		{
			fgets(message, file_size + 1, f);
			if (strlen(message) < 28) continue;
			number_of_string++;
			_send(s, message, to_send, number_of_string, addr);
			memset(to_send, 0x00, file_size);
			memset(message, 0x00, file_size);
		} else{
			fgets(message, file_size, f);
			if (strlen(message) < 28) continue;
			number_of_string++;
			if (get_mes[number_of_string] == '1')
			{
				minus++;
				continue;
			}
			_send(s, message, to_send, number_of_string - minus, addr);
			memset(to_send, 0x00, file_size);
			memset(message, 0x00, file_size);
		}
	}
	if (flag == 0)
	{
		cnt_of_messages = number_of_string;
		get_mes = (char*)malloc(sizeof(char) * cnt_of_messages);
		memset(get_mes, 48, cnt_of_messages);
		flag = 1;
	}
	
}

char get_one_byte(char* str, int len) {
	char hh[3]; memset(hh, 0x00, 3);
	memcpy(hh, str + len, 2);
	return (char)atoi(hh);
}

int get_msg_len(char* msg) {
	int i = 35;
	while (msg[i] != '\n') {
		if (msg[i] == '\0')
		{
			return i - 34;
		}
		i++;
	}
	return i - 35;
}

int _send(int s, char* msg, char* to_send, int num, struct sockaddr_in* addr) { //msg - все сообщение
	int msg_len = get_msg_len(msg) - 1;
	if(msg[strlen(msg) - 1] == '\n') msg[strlen(msg) - 1] = '\0';
	get_net_data(num, to_send); // 4 байта - номер сообщения 
	//memcpy(to_send, "1234", 4);
	int len = 4; // длина message
	memcpy(to_send + len, msg + len - 4, 12); // 12 - длина номера телефона
	len += 12 + 1; //+1 так как в файле пробел
	memcpy(to_send + len - 1, msg + len - 4, 12); // 12 - длина номера телефона
	len += 12 + 1;
	char h = get_one_byte(msg, len - 4);
	char hh[1]; memset(hh, 0x00, 1);
	hh[0] = h;
	memcpy(to_send + len - 2, &h, 1);//len - 2
	len += 2 + 1; //:
	char m = get_one_byte(msg, len - 4);
	char mm[1]; memset(mm, 0x00, 1);
	mm[0] = m;
	memcpy(to_send + len - 4, &mm, 1); // len - 2
	len += 2 + 1; //:
	char sec = get_one_byte(msg, len - 4);
	char ss[1]; memset(ss, 0x00, 1);
	ss[0] = sec;
	memcpy(to_send + len - 6 , ss, 1);
	len += 2 + 1;
	memcpy(to_send + len - 8, msg + len - 4, strlen(msg) - 35); //12+1+12+1+2+1+2+1+2+1
	int res = sendto(s, to_send, 31 + msg_len /* + 7 */ +2, 0, (struct sockaddr*)addr, sizeof(struct sockaddr_in)); //4+12+12+1+1+1

	for (size_t i = 0; i < 31 + msg_len; i++)
	{
		printf("%c", to_send[i]);
	}
	printf("\n");
	if (!res)
	{
		printf("sending fail = %d", num);
		sock_err("sendto", s);
	}
	return 1;
}