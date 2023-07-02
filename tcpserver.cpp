#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
// Директива линковщику: использовать библиотеку сокетов 
#pragma comment(lib, "ws2_32.lib") 
#else // LINUX
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#endif
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <memory.h>

#define ALL_CLIENTS 100
int set_non_block_mode(int s)
{
#ifdef _WIN32
	unsigned long mode = 1;
	return ioctlsocket(s, FIONBIO, &mode);
#else
	int fl = fcntl(s, F_GETFL, 0);
	return fcntl(s, F_SETFL, fl | O_NONBLOCK);
#endif
}

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

#define MAX_NUM_LEN 13
#define HMS_TIME 2
#define ALWAYS1000 1000 // используется в _memcpy, для того \
                           чтобы копирование происходило  только по заданной длине
#define EXIT_FAILURE -1
#define EXIT_SUCCES 0
#define INIT_VALUE 4
#define MAX_IP_LEN 20
#define MAX_PORT_LEN 10
typedef struct socketdata
{
	char* ip;
	char port[MAX_PORT_LEN];
	int cs;
	char put[3];
}s_data;

s_data cl_arr[ALL_CLIENTS];
int stop = 1;
int put_count = 0;
void f_print(char* num1, char* num2, char* hh, char* mm, char* ss, char* msg, FILE* f, char* data_str) {

	fwrite(num1, sizeof(char), strlen(num1), f);
	fwrite(" ", sizeof(char), 1, f);
	fwrite(num2, sizeof(char), strlen(num2), f);
	fwrite(" ", sizeof(char), 1, f);
	//fwrite(hh, sizeof(char), strlen(hh), f); 
	char h = hh[0];
	if ((int)h < 10 && (int)h != 0) fprintf(f, "0");
	else if((int)h == 0) fprintf(f, "00");
	if ((int)h != 0)fprintf(f, "%d", (int)h);
	fwrite(":", sizeof(char), 1, f);
	//fwrite(mm, sizeof(char), strlen(mm), f);
	char m = mm[0];
	if ((int)m < 10 && (int)m != 0) fprintf(f, "0");
	else if ((int)m == 0) fprintf(f, "00");
	if((int)m != 0) fprintf(f, "%d", (int)m);
	fwrite(":", sizeof(char), 1, f);
	//fwrite(ss, sizeof(char), strlen(ss), f);
	char s = ss[0];
	if ((int)s < 10 && (int)s != 0) fprintf(f, "0");
	else if ((int)s == 0) fprintf(f, "00");
	if ((int)s != 0)fprintf(f, "%d", (int)s);
	fwrite(" ", sizeof(char), 1, f);
	fwrite(msg, sizeof(char), strlen(msg), f);
	fwrite("\n", sizeof(char), 1, f);
}

int recv_string(int cs, FILE* f, char* data_str)
{

	char buffer[512];
	memset(buffer, 0x00, 512);
	
	
	int curlen = 0;
	int rcv;
	int ret;
	char* msg = (char*)malloc(sizeof(char) * 1000000);
	memset(msg, 0x00, 1000000);
	//Принятие сообщения put от клиента
	

	/* Массивы для записи в них 2 номеров, время и сообщения*/
	do
	{
		char num[4];
		rcv = recv(cs, num, 4, 0);

		char num1[MAX_NUM_LEN]; memset(num1, 0x00, MAX_NUM_LEN);
		rcv = recv(cs, num1, MAX_NUM_LEN - 1, 0);

		char num2[MAX_NUM_LEN]; memset(num2, 0x00, MAX_NUM_LEN);
		rcv = recv(cs, num2, MAX_NUM_LEN - 1, 0);

		char hh[HMS_TIME + 1];      memset(hh, 0x00, HMS_TIME + 1);
		rcv = recv(cs, hh, 1, 0);

		char mm[HMS_TIME + 1];      memset(mm, 0x00, HMS_TIME + 1);
		rcv = recv(cs, mm, 1, 0);

		char ss[HMS_TIME + 1];      memset(ss, 0x00, HMS_TIME + 1);
		rcv = recv(cs, ss, 1, 0);

		memset(msg, 0x00, 1000000);

		rcv = recv(cs, msg, 1000000, 0); 
		if (msg[0] == '\0') return 0;
		ret = send(cs, "ok", strlen("ok"), 0);
		if (ret <= 0)
			return sock_err("send", cs);
		fwrite(data_str, sizeof(char), strlen(data_str), f);
		fwrite(" ", sizeof(char), 1, f);
		f_print(num1, num2, hh, mm, ss, msg, f, data_str);
	} while (strcmp("stop", msg) != 0 && stop);
	memset(data_str, 0x00, 30);
	stop = 0;
}

int send_notice(int cs, int len)
{
	char buffer[1024];
	int sent = 0;
	int ret;
#ifdef _WIN32
	int flags = 0;
#else
	int flags = MSG_NOSIGNAL;
#endif
	sprintf(buffer, "Length of your string: %d chars.", len);
	while (sent < (int)strlen(buffer))
	{
		ret = send(cs, buffer + sent, strlen(buffer) - sent, flags);
		if (ret <= 0)
			return sock_err("send", cs);
		sent += ret;
	}
	return 0;
}

char* pars(char* ip, char* port) {
	strcat(ip, port);
	strcat(ip, " ");
	return ip;
}

int get_put(int cs) {
	char put[3];
	memset(put, 0x00, 3);
	int rcv = recv(cs, put, sizeof(put), 0);
	return rcv;
}

int cs_cnt = 0;
WSAEVENT events[2]; // Первое событие - прослушивающего сокета, второе - клиентских соединений
int s;

void wsa_event(void) {
	int i;
	events[0] = WSACreateEvent();
	events[1] = WSACreateEvent();
	WSAEventSelect(s, events[0], FD_ACCEPT);
	for (i = 0; i < cs_cnt; i++)
		WSAEventSelect(cl_arr[i].cs, events[1], FD_READ | FD_WRITE | FD_CLOSE);
}

void accept_client(char* data_str) {
	struct sockaddr_in addr;
	int addrlen = sizeof(addr);
	int cs = accept(s, (struct sockaddr*)&addr, &addrlen);
	if (cs < 0)
	{
		sock_err("accept", s);
		return;
	}
		/*int res = send(cs, "ok", sizeof(char) * 2, 0);
		if (res == -1)
			printf("return\n");
			return;*/
		set_non_block_mode(cs);
		cl_arr[cs_cnt].ip = inet_ntoa(addr.sin_addr);
		sprintf((char* const)&(cl_arr[cs_cnt].port), "%u", addr.sin_port);
		strcpy(data_str, cl_arr[cs_cnt].ip);
		data_str[strlen(data_str)] = ':';
		strcat(data_str, cl_arr[cs_cnt].port);
		cl_arr[cs_cnt].cs = cs;  //cs
		cs_cnt++;


}

void work(FILE* f, char* data_str) {
	while (stop)
	{
		WSANETWORKEVENTS ne;
		// Ожидание событий в течение секунды
		DWORD dw = WSAWaitForMultipleEvents(2, events, FALSE, 1000, FALSE);
		WSAResetEvent(events[0]);
		WSAResetEvent(events[1]);
		if (0 == WSAEnumNetworkEvents(s, events[0], &ne) &&
			(ne.lNetworkEvents & FD_ACCEPT))
		{
			// Поступило событие на прослушивающий сокет, можно принимать подключение 
			// функцией accept
			// Принятый сокет следует добавить в массив cs и подключить его к событию 
			// events[1]
			printf("accept client\n");
			accept_client(data_str);
			WSAEventSelect(cl_arr[cs_cnt - 1].cs, events[1], FD_READ | FD_WRITE | FD_CLOSE);
		}
		for (int i = 0; i < cs_cnt; i++)
		{
			if (0 == WSAEnumNetworkEvents(cl_arr[i].cs, events[1], &ne))
			{
				if (ne.lNetworkEvents & FD_WRITE)
				{
					// Сокет cs[i] готов для записи, можно отправлять данные
					int a;
				}
				// По сокету cs[i] есть события
				if (ne.lNetworkEvents & FD_READ)
				{
					/// put надо в занести струткуру, сохранять количество считанных туда символов, при текущей проверке любая ошибка сломает всю программу.
					int rcv = recv(cl_arr[i].cs, cl_arr[i].put + put_count, 3- put_count, 0);
					put_count = strlen(cl_arr[i].put);
					if (put_count < 3) {
						i--;
						continue;
					}
					if (rcv < 0) {
						printf("1 %d\n", WSAGetLastError());
						return;
					}
					if (recv_string(cl_arr[i].cs, f, data_str) == -1)
					{
						i--;
						continue;
					}
					// Есть данные для чтения, можно вызывать recv/recvfrom на 	cs[i]
				}		
				if (ne.lNetworkEvents & FD_CLOSE)
				{
					int b;
					s_close(cl_arr[i].cs);
					// Удаленная сторона закрыла соединение, можно закрыть сокет и 
					// удалить его из cs
				}
			}
		}
		if (stop == 0 ) break;
	}
}

/*ГДЕ ТО ЗАЦИКЛИВАЕТСЯ
ПРИ ОТЛАДКЕ РАБОТАЕТ ПРАВИЛЬНО*/

int main(int argc, char* argv[]) // Пример запуска tcpserver 9000\
								    ip адрес 192.168.56.1\
argv[0] = tcpserver -> NOT_USED\
argv[1] = port

{
	// Создание файла и запись в него начальных данных \
	в формате: IP-адрес, двоеточие, порт клиента, пробел
	FILE* f = fopen("msg.txt", "wb");
	if (f == NULL)
	{
		printf("\nFile open error, name of file 'msg.txt'\n");
		exit(EXIT_FAILURE);
	}
	short int port = atoi(argv[1]);
	//char _ip[128] = "192.168.56.1:";
	char* data_str = (char*)malloc(30);
	memset(data_str, 0x00, 30);
	struct sockaddr_in addr;
	// Инициалиазация сетевой библиотеки
	init();
	// Создание TCP-сокета
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return sock_err("socket", s);

	set_non_block_mode(s);
	// Заполнение адреса прослушивания
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port); // Сервер прослушивает порт указанный в командной строке
	addr.sin_addr.s_addr = htonl(INADDR_ANY); // Все адреса
	// Связывание сокета и адреса прослушивания
	if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0)
		return sock_err("bind", s);
	// Начало прослушивания
	if (listen(s, ALL_CLIENTS) < 0)
		return sock_err("listen", s);
	int one_time = 0;
	int cs;
	wsa_event();
	work(f, data_str);
	s_close(s);
	deinit();
	fclose(f);
	return 0;
}