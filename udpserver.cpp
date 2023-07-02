#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <Ws2ipdef.h> 
// Директива линковщику: использовать библиотеку сокетов
#pragma comment(lib, "ws2_32.lib")
#else // LINUX
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <time.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#define MAX_CLIENTS 100
#define MAX_CL_ID 30
#define MAX_ACCEPTED_MSG 1000
#define MAX_DATA 2048
int STOP = 0;
typedef struct clients {
	char cl_ID[MAX_CL_ID];
	int accept[MAX_ACCEPTED_MSG]; // принятое сообщение для записи в файл
	int already_send; // для проверки повтроного принятия того же сообщения
	int size_id;
}clients;

clients cl[MAX_CLIENTS];
fd_set rfd;
fd_set wfd;
int nfds;
int client_cnt;
char reverse_arr[10000][10000];
int index = 0;
int get_client(clients* , sockaddr_in );
int check_in_base(char* , clients* );
void parsing_string(char* , int );
void send_acc_msg(int, clients , struct sockaddr_in* );

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
void s_close(int s)
{
#ifdef _WIN32
	closesocket(s);
#else
	close(s);
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

int parsing(int* p1, int* p2, char** argv, int argc) {
	if (argc != 3) {
		printf("cmd error\n");
		exit(EXIT_FAILURE);
	}
	*p1 = atoi(argv[1]);
	*p2 = atoi(argv[2]);
	if (*p1 > *p2)
	{
		printf("first port should be less than second\n");
		exit(EXIT_FAILURE);
	}
	return *p2 - *p1 + 1;
}

struct timeval tv = { 1, 0 };


int main(int argc, char* argv[]) {
	struct sockaddr_in addr;
	int port1;
	int port2;
	int p_cnt = parsing(&port1, &port2, argv, argc);
	int* sockets = (int*)malloc(sizeof(int) * p_cnt);
	memset(sockets, 0x00, sizeof(int) * p_cnt);
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		for (int j = 0; j < MAX_ACCEPTED_MSG; j++)
		{
			cl[i].accept[j] = 0;
		}
	}
#ifdef _WIN32
	int flags = 0;
#else
	int flags = MSG_NOSIGNAL;
#endif
	// Инициалиазация сетевой библиотеки
	init();
	for (size_t i = 0; i < p_cnt; i++)
	{
		sockets[i] = socket(AF_INET, SOCK_DGRAM, 0);
		set_non_block_mode(sockets[i]);
		if (sockets[i] < 0)
			return sock_err("socket", sockets[i]);
	}
	nfds = sockets[0];
	FILE* f = fopen("msg.txt", "w+");
	if (f == NULL)
	{
		printf("file open error 'msg.txt'\n");
		exit(EXIT_FAILURE);
	}
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	for (size_t i = 0; i < p_cnt; i++)
	{
		addr.sin_port = htons(port1 + i);
		if (bind(sockets[i], (struct sockaddr*)&addr, sizeof(addr)) < 0)
			return sock_err("bind", sockets[i]);
	}
	//struct timeval tv = { 1, 0 };
	char buf[10000];
	int sizeof_addr = sizeof(addr);
	int num_for_cur_client;
	int cl_num;
	while (STOP != 1) {

			cl_num = -1;
			FD_ZERO(&rfd);
			FD_ZERO(&wfd);
			//(ls, &rfd);
			for (int i = 0; i < p_cnt; i++)
			{
				FD_SET(sockets[i], &rfd);
				FD_SET(sockets[i], &wfd);
				if (nfds < sockets[i])
					nfds = sockets[i];
			}
			if (select(nfds + 1, &rfd, &wfd, 0, &tv) > 0)
			{
				for (int i = 0; i < p_cnt; i++)
				{
					if (FD_ISSET(sockets[i], &rfd))
					{
						// Сокет cs[i] доступен для чтения. Функция recv вернет данные, recvfrom - дейтаграмму
						int rcv = recvfrom(sockets[i], buf, sizeof(buf), 0, (struct sockaddr*)&addr, &sizeof_addr);
						if (rcv > 0)
						{
							 cl_num = get_client(cl, addr);
							 parsing_string(buf, cl_num);
						}
					}	
					if (FD_ISSET(sockets[i], &wfd))
					{
						// Сокет cs[i] доступен для записи. Функция send и sendto будет успешно завершена
						//отправка номеров принятых сообщений
						if(cl_num != -1)send_acc_msg(sockets[i], cl[cl_num], &addr);
						cl_num = -1;
						//printf("ff");
					}
				}
			}
			else
			{
				printf("timeout error\n");
				exit(EXIT_FAILURE);
			}
	}
	deinit();
	fclose(f);
	free(sockets);
	return 0;
}

void send_acc_msg(int sock, clients cl, struct sockaddr_in* addr) {
	int net_send;
	char* char_send;
	char data[MAX_DATA];
	int answer_len = 0;

	for (int i = 0; i < MAX_ACCEPTED_MSG; i++)
	{
		if (cl.accept[i] == 1)
		{
			net_send = htonl(i);
			char_send = (char*)&net_send;
			memcpy(data + answer_len, char_send, 4);
			answer_len += 4;

		}
	}
	int res = sendto(sock, data, answer_len, 0, (struct sockaddr*)addr, sizeof(*addr));
	if (res < 0) {
		//printf("sendto error");
	}
}

int _new(char* buf, int cl_num){
	char num[5]; memset(num, 0x00, 5);
	int number_of_message = 0;
	memcpy(num, buf, 4); 
	number_of_message = *(int*)(num);
	number_of_message = ntohl(number_of_message);
	if (cl[cl_num].accept[number_of_message] == 0)
	{
		cl[cl_num].accept[number_of_message] = 1;
		return 1;
	}
	return 0;
}

int cnt = 0;
void parsing_string(char* buf, int cl_num) {
	int flag = 0;
	if (strlen(buf + 4) < 27)
	{
		flag = 1;
		//return;
	}
	if (!_new(buf, cl_num))
	{
		//if (cnt == 30) exit(0);
		cnt++;
		return;
	}
	FILE* f = fopen("msg.txt", "a+");
	if (f == NULL)
	{
		printf("open file error\n");
		exit(EXIT_FAILURE);
	}
	fwrite(cl[cl_num].cl_ID, 1, cl[cl_num].size_id, f);
	fwrite(" ", 1, 1, f);
	fwrite(buf + 4, 1, 12, f);//номер телефона 1 
	fwrite(" ", 1, 1, f);
	fwrite(buf + 16, 1, 12, f); // номер телефона 2
	fwrite(" ", 1, 1, f);
	char h = buf[28];
	if ((int)h < 10 && (int)h != 0) fprintf(f, "0");
	else if ((int)h == 0) {
		buf[28] = '1';
		fprintf(f, "00");
	}
	if ((int)h != 0)fprintf(f, "%d", (int)h);
	//fprintf(f, "%d", (int)h);
	fwrite(":", sizeof(char), 1, f);
	char m = buf[29];
	if ((int)m < 10 && (int)m != 0) fprintf(f, "0");
	else if ((int)m == 0) {
		buf[29] = '1';
		fprintf(f, "00");
	}
	if ((int)m != 0) fprintf(f, "%d", (int)m);
	//fprintf(f, "%d", (int)m);
	fwrite(":", sizeof(char), 1, f);
	char s = buf[30];
	if ((int)s < 10 && (int)s != 0) fprintf(f, "0");
	else if ((int)s == 0) {
		buf[30] = '1';
		fprintf(f, "00");
	}
	if ((int)s != 0)fprintf(f, "%d", (int)s);
	//fprintf(f, "%d", (int)s);
	fwrite(" ", 1, 1, f);
	
	char* msg = (char*)malloc(sizeof(char) * (1000)); memset(msg, 0x00, 1000); //strlen(buf + 4) + 4 - 31 + 1
	if (msg == NULL)
	{
		printf("memory allocate error\n");
		exit(-1);
	}
	memcpy(msg, buf + 31, strlen(buf + 4) + 4 - 31);
	fwrite(msg, 1, strlen(buf + 4) + 4 - 31, f);
	fwrite("\n", 1, 1, f);
	int count_of_accept = 0;
	for (int i = 0; i < MAX_ACCEPTED_MSG; i++)
	{
		if (cl[cl_num].accept[i] == 1) count_of_accept++;
	}
	if (strcmp(msg,"stop") == 0)
	{
		STOP = 1;
	}

	free(msg);
}

int get_client(clients* cl, sockaddr_in addr) {
	char* port_ip = inet_ntoa(addr.sin_addr);
	int port_mark = strlen(port_ip);
	port_ip[port_mark++] = ':';
	sprintf(port_ip + port_mark, "%u", addr.sin_port);
	int check = check_in_base(port_ip, cl);
	if (check == -1)
	{
		memcpy(cl[client_cnt].cl_ID, port_ip, strlen(port_ip));
		cl[client_cnt].size_id = strlen(port_ip);
		for(int i = 0; i < MAX_ACCEPTED_MSG; i++)
		cl[client_cnt].accept[i] = 0;
		cl[client_cnt].already_send = 0;
		client_cnt++;
		return client_cnt - 1;
	} else {
		return check;
	}
}

int check_in_base(char* port_ip, clients* cl) {
	for (int i = 0; i < client_cnt; i++)
	{
		if (strcmp(cl[i].cl_ID, port_ip) == 0)
		{
			return i;
		}
	}
	return -1;
}