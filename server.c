#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <errno.h>

#define BUF_SIZE 1024
#define PORT 9527
#define TIMEOUT 30
#define WELCOME "connection accepted.\n"
#define OVERLOAD "system fully loaded, please try again later.\n"
#define MAX_PENDING_CLIENT 5
#define MAX_EXIST_CONNECTION 36

int g_exist_connections = 0;

char *getTimeString()
{
	time_t currentTime;
	time(&currentTime);
	char *str = asctime(localtime(&currentTime));
	str[strlen(str) - 1] = '\0';
	return str;
}

void *client_handler(void *cl)
{
	int client = (*(int*)cl);

	// send wellcome to client
	char msg[] = WELCOME;
	send(client, msg, sizeof(msg), 0);

	while ( true )
	{
		// send what ever this thread received back to client
		char buffer[BUF_SIZE];
		memset(buffer, 0x00, BUF_SIZE);
        int error_number = recv(client, buffer, BUF_SIZE, 0);
		if ( error_number == 0 )
		{
			printf("[%24s] client %d is disconnected\n", getTimeString(), client);
			break;
		}
        else if ( error_number < 0 )
        {
            printf("[%24s] error on recv of server thread for client %d, maybe timeout\n", getTimeString(), client);
            break;
        }
		printf("[%24s] received from client %d, message: %s%c", getTimeString(), client, buffer, buffer[strlen(buffer)-1]=='\n' ? '\0' : '\n');
        
		error_number = send(client, buffer, strlen(buffer), 0);
        if ( error_number <= 0 )
        {
            printf("[%24s] error on send of server thread for client %d\n", getTimeString(), client);
            break;
        }
	}
	
	g_exist_connections--;
	shutdown(client, 2);	// close connection
	close(client);			// close file descriptor
	pthread_exit(NULL);		// terminate thread itself
}

void refuse_client(int client)
{
	char msg[] = OVERLOAD;
	send(client, msg, sizeof(msg), 0);
	
	shutdown(client, 2);	// close connection
	close(client);			// close file descriptor
}

struct sockaddr_in create_server(int server_socket, int port, int timeout_sec)
{
	struct sockaddr_in server_info;
	memset(&server_info, 0x00, sizeof(server_info));
	server_info.sin_family = PF_INET;
	server_info.sin_addr.s_addr = htonl(INADDR_ANY);
	server_info.sin_port = htons(port);

	// setting timeout on receive and receive both
	struct timeval timeout;
	timeout.tv_sec = timeout_sec;
	timeout.tv_usec = 0;

	if ( setsockopt(server_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1 )
	{
		printf("[%24s] server creation fail.\n", getTimeString());
		exit(-1);
	}

	if ( setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1 )
	{
		printf("[%24s] server creation fail.\n", getTimeString());
		exit(-1);
	}

	return server_info;
}

int main()
{
	// create socket
	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if ( server_socket == -1 )
	{
		printf("[%24s] fail to create a socket.\n", getTimeString());
		exit(-1);
	}

	// create server
	struct sockaddr_in server_info = create_server(server_socket, PORT, TIMEOUT);

	// bind the socket with server
	if ( bind(server_socket, (struct sockaddr*)&server_info, sizeof(server_info)) == -1 )
	{
		printf("[%24s] fail to bind the server socket\n", getTimeString());
		exit(-1);
	}

	// server start to listening on port
	if ( listen(server_socket, MAX_PENDING_CLIENT) == -1 )
	{
		printf("[%24s] fail to listening\n", getTimeString());
		exit(-1);
	}
	printf("[%24s] start listening on port %d\n", getTimeString(), PORT);

	struct sockaddr_in client_info;
	int addrlen, exist_connections=0;

	while ( true )
	{
		// ready for client connection
		int client = accept(server_socket, (struct sockaddr*)&client_info, &addrlen);
		if ( client < 0 )
			continue;

		if ( g_exist_connections >= MAX_EXIST_CONNECTION )
		{
			refuse_client(client);
			continue;
		}

		pthread_t thread;
		g_exist_connections++;
		printf("[%24s] prepareing thread for client: %d, accepted connections: %d\n", getTimeString(), client, g_exist_connections);
		pthread_create(&thread, NULL, &client_handler, (void*)&client);
	}

	return 0;
}
