// ******************************************
//       Coded by BloodhoundAllfather
// ******************************************
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
//--------------------------------------------------------------------------------------------------------------------------------
#define IP_MAX_LEN 20
#define LOOP_SLEEP_DURATION 1000 // 1 millisecond

void* socketThread(void *);
int8_t validateNumber(char*);
int8_t validateIP(char*);
void signalHandler(int);

struct Connection
{
	char ip[IP_MAX_LEN];
	uint16_t port;
	int socket;
};

pthread_t thread1, thread2;
int8_t continueRunning = 1;
struct Connection connections[2];
pthread_mutex_t threadMutexes[2] = { PTHREAD_MUTEX_INITIALIZER };
//--------------------------------------------------------------------------------------------------------------------------------
int main(int argc, char const* argv[])
{
	if( argc != 5)
	{
		printf("Usage: dembe TCP_IP1 PORT1 TCP_IP2 PORT2");
		return -1;
	}
	
	struct sigaction sigIntHandler;
	int thread1Status, thread2Status;
	const int8_t thread1Id = 0, thread2Id = 1;
	int8_t parameterCounter = 1;

	sigIntHandler.sa_handler = signalHandler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);

	// initialize thread1 data
	if( strlen(argv[parameterCounter]) < IP_MAX_LEN && validateIP(argv[parameterCounter]) )
			strcpy(connections[thread1Id].ip, argv[parameterCounter]);
	else
	{
		printf("Invalid IP address: %s\nExiting...", argv[parameterCounter]);
		return -1;
	}
	
	if(validateNumber(argv[++parameterCounter]))
	{
		connections[thread1Id].port = atoi(argv[parameterCounter]);
		if(connections[thread1Id].port < 0 || connections[thread1Id].port > 65535)
		{
			printf("Invalid port number: %s\nExiting...", argv[parameterCounter]);
			return -1;		
		}
	}
	else
	{
		printf("Invalid port number: %s\nExiting...", argv[parameterCounter]);
		return -1;
	}

	// initialize thread2 data
	if( strlen(argv[++parameterCounter]) < IP_MAX_LEN && validateIP(argv[parameterCounter]) )
			strcpy(connections[thread2Id].ip, argv[parameterCounter]);
	else
	{
		printf("Invalid IP address: %s\nExiting...", argv[parameterCounter]);
		return -1;
	}
	
	if(validateNumber(argv[++parameterCounter]))
	{
		connections[thread2Id].port = atoi(argv[parameterCounter]);
		if(connections[thread2Id].port < 0 || connections[thread2Id].port > 65535)
		{
			printf("Invalid port number: %s\nExiting...", argv[parameterCounter]);
			return -1;		
		}
	}
	else
	{
		printf("Invalid port number: %s\nExiting...", argv[parameterCounter]);
		return -1;
	}
	
	if( (thread1Status = pthread_create( &thread1, NULL, socketThread, (void*) &thread1Id)) != 0 ||
	    (thread2Status = pthread_create( &thread2, NULL, socketThread, (void*) &thread2Id)) != 0 )
	{
		printf("Can't create thread. Error no: %d - %d\nExiting...", thread1Status, thread2Status);
		return -1;
	}

    pthread_join( thread1, NULL);
    pthread_join( thread2, NULL);
	pthread_mutex_destroy(&threadMutexes[thread1Id]);
	pthread_mutex_destroy(&threadMutexes[thread2Id]);

	return 0;
}
//--------------------------------------------------------------------------------------------------------------------------------
void* socketThread(void *param)
{
	int8_t threadId = (*(int8_t*)param) + 1;
    struct sockaddr_in serv_addr, sendAddr;
	int theSocket, readBytesCount, client_fd, flags, errorNo;
	char buffer[ 2048 ];
	int8_t thisThread  = threadId - 1;
	int8_t otherThread = thisThread == 0 ? 1: 0; 

	if ( (theSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
	{
		printf("Socket creation error\nExiting thread %d\n", threadId);
		continueRunning = 0;
		return NULL;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( connections[thisThread].port );
	serv_addr.sin_addr.s_addr = inet_addr( connections[thisThread].ip );

	sendAddr.sin_family = AF_INET;
	sendAddr.sin_port = htons( connections[otherThread].port );
	sendAddr.sin_addr.s_addr = inet_addr( connections[otherThread].ip );

	if( (client_fd = connect(theSocket, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0 )
	{
		printf("Connection failed at thread %d\nExiting...\n", threadId);
		continueRunning = 0;
	}

	if( (flags = fcntl(theSocket, F_GETFL) == -1) || fcntl(theSocket, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		printf("Socket initialize error\nExiting thread %d\n", threadId);
		continueRunning = 0;
		return NULL;
	}

	connections[thisThread].socket = theSocket;

	while( continueRunning )
	{
		pthread_mutex_lock(&threadMutexes[thisThread]);
		readBytesCount = recv(theSocket, buffer, sizeof(buffer), flags);
		errorNo = errno;
		pthread_mutex_unlock(&threadMutexes[thisThread]);
		
		if(readBytesCount == -1)
		{
			if(errorNo != EWOULDBLOCK)
			{
				printf("Connection terminated unexpectedly at thread %d\nExiting...\n", threadId);
				continueRunning = 0;
			}
			else
				usleep(LOOP_SLEEP_DURATION);
		}
		else if(readBytesCount == 0)
		{
			printf("Connection closed at thread %d\nExiting...\n", threadId);
			continueRunning = 0;
		}
		else if(readBytesCount > 0)
		{
			pthread_mutex_lock(&threadMutexes[otherThread]);
			if( sendto(connections[otherThread].socket, buffer, readBytesCount, 0, (struct sockaddr*)&sendAddr, sizeof(sendAddr)) == -1)
			{
				errorNo = errno;
				printf("Failed to send message from thread %d with %d error no\nExiting...\n", threadId, errorNo);
				continueRunning = 0;
			}
			pthread_mutex_unlock(&threadMutexes[otherThread]);
		}
	}

	close(theSocket);
    return NULL;
}
//--------------------------------------------------------------------------------------------------------------------------------
int8_t validateNumber(char *str)
{
	while( *str )
	{
    	if(!isdigit(*str))
    		return 0;

      str++;
   }
   return 1;
}
//--------------------------------------------------------------------------------------------------------------------------------
int8_t validateIP(char *ip)
{
	int num, dots = 0;
	char *start = ip, *end = NULL, buffer[5];

	do
	{
		end = strchr(start, '.');
		if( end == NULL )
		{
			if(dots == 3)
				end = ip + strlen(ip);
			else
				return 0;
		}
		else
			dots++;

		if(end - start > 3)
			return 0;
		memset(buffer, 0, sizeof(buffer));
		memcpy(buffer, start, end - start);
		num = atoi(buffer);
		if (num < 0 && num > 255)
			return 0;

		start = end + 1;		
	} while (start < ip + strlen(ip));
	
	return 1;
}
//--------------------------------------------------------------------------------------------------------------------------------
void signalHandler(int sig)
{
	printf("Caught signal %d\n",sig);
	continueRunning = 0;
}
//--------------------------------------------------------------------------------------------------------------------------------
