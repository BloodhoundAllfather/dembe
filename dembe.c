// ******************************************
//       Coded by BloodhoundAllfather
// ------------------------------------------
//       dembe: TCP data tunnel
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
#define IP_MAX_LEN 			20
#define LOOP_SLEEP_DURATION 5000 // 5 milliseconds
#define MAX_QUEUE_NUM		10 // maximum number of connections to be in listen queue
#define LISTEN_MODE 		0
#define CONNECT_MODE 		1
#define CONN_DISCONNECTED 	0
#define CONN_IN_PROGRESS 	1
#define CONN_CONNECTED 		2
#define CONN_UNINITIALIZED	3

void* 	socketThread(void *);
int8_t 	validateNumber(char*);
int8_t 	validateIP(char*);
void 	signalHandler(int);
void 	printUsage();

struct Connection
{
	char ip[IP_MAX_LEN];
	uint16_t port;
	int socket;
	int8_t connectionStatus;
};

pthread_t thread1, thread2;
int8_t continueRunning = 1;
struct Connection connections[2];
pthread_mutex_t threadMutexes[2] = { PTHREAD_MUTEX_INITIALIZER };
//--------------------------------------------------------------------------------------------------------------------------------
int main(int argc, char const* argv[])
{
	if( argc < 5)
	{
		printUsage();
		return -1;
	}
	
	struct sigaction sigIntHandler;
	int thread1Status, thread2Status;
	const int8_t thread1Id = 0, thread2Id = 1;
	int8_t parameterCounter = 1;

	// handling SIGINT: user's process interruption via Ctrl + C 
	sigIntHandler.sa_handler = signalHandler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);

	// check parameters
	if(strcmp(argv[parameterCounter], "-l") != 0 && strcmp(argv[parameterCounter], "-c") != 0)
	{
		printUsage();
		return -1;
	}
	
	// initialize thread1 data
	if(strcmp(argv[parameterCounter], "-c") == 0)
	{
		if( strlen(argv[++parameterCounter]) < IP_MAX_LEN && validateIP(argv[parameterCounter]) )
				strcpy(connections[thread1Id].ip, argv[parameterCounter]);
		else
		{
			printf("Invalid IP address: %s\nExiting...", argv[parameterCounter]);
			return -1;
		}
	}
	else
		connections[thread1Id].ip[0] = 0;
	
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
	if(strcmp(argv[++parameterCounter], "-c") == 0)
	{
		if( strlen(argv[++parameterCounter]) < IP_MAX_LEN && validateIP(argv[parameterCounter]) )
				strcpy(connections[thread2Id].ip, argv[parameterCounter]);
		else
		{
			printf("Invalid IP address: %s\nExiting...", argv[parameterCounter]);
			return -1;
		}
	}
	else
		connections[thread2Id].ip[0] = 0;
	
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

	if(++parameterCounter < argc)
	{
		printUsage();
		return -1;
	}
	
	connections[thread1Id].connectionStatus = CONN_UNINITIALIZED;
	connections[thread2Id].connectionStatus = CONN_UNINITIALIZED;
	
	// create threads and wait for them
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
// the thread function: it takes only id from parameter to know its own identity
// then using global data and mutexes, work with other thread to trasnfer TCP data both way
void* socketThread(void *param)
{
	int8_t threadId = (*(int8_t*)param) + 1;
    struct sockaddr_in serv_addr, sendAddr, peerAddr;
	int theSocket = 0, readBytesCount, peerSocket = 0, receiveSocket = 0, flags, errorNo;
	socklen_t peerAddrLen;
	int connectRetVal;
	char buffer[ 2048 ];
	int8_t thisThread  = threadId - 1;
	int8_t otherThread = thisThread == 0 ? 1: 0;
	int8_t listenMode = connections[thisThread].ip[0] == 0 ? LISTEN_MODE : CONNECT_MODE;

	// initialize sockaddr_in instances
	memset(&serv_addr, '0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons( connections[thisThread].port );
	serv_addr.sin_addr.s_addr = listenMode == LISTEN_MODE ? htonl(INADDR_ANY) : inet_addr( connections[thisThread].ip );

	memset(&sendAddr, '0', sizeof(sendAddr));
	sendAddr.sin_family = AF_INET;
	sendAddr.sin_port = htons( connections[otherThread].port );
	sendAddr.sin_addr.s_addr = inet_addr( connections[otherThread].ip );

	// setup listener socket if -l parameter is passed: listen and bind to the given port
	if(listenMode == LISTEN_MODE)
	{
		// initialize socket
		if ( (theSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
		{
			printf("Socket creation error\nExiting thread %d\n", threadId);
			continueRunning = 0;
			return NULL;
		}

		// set SO_REUSEADDR flag to the listening socket
		flags = 1;
		if(setsockopt(theSocket, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags)) == -1)
		{
			printf("setsockopt SO_REUSEADDR error for port %d\nExiting thread %d\n", connections[thisThread].port, threadId);
			continueRunning = 0;
			close(theSocket);
			return NULL;
		}

		if( bind(theSocket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1 )
		{
			printf("Bind error at port %d\nExiting thread %d\n", connections[thisThread].port, threadId);
			continueRunning = 0;
			close(theSocket);
			return NULL;
		}
		
		if( listen(theSocket, MAX_QUEUE_NUM) == -1)
		{
			printf("Listen error at port %d\nExiting thread %d\n", connections[thisThread].port, threadId);
			continueRunning = 0;
			close(theSocket);
			return NULL;
		}

		// make the socket non-blocking to be responsive during wait
		if( (flags = fcntl(theSocket, F_GETFL) == -1) || fcntl(theSocket, F_SETFL, flags | O_NONBLOCK) == -1)
		{
			printf("Socket non-block initialize error\nExiting thread %d\n", threadId);
			continueRunning = 0;
			close(theSocket);
			return NULL;
		}
	}

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// the main receive-send loop that transfers the data
	while( continueRunning )
	{
		if(connections[thisThread].connectionStatus == CONN_UNINITIALIZED)
		{
			if(listenMode == CONNECT_MODE)
			{
				// initialize socket
				if ( (theSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0 )
				{
					printf("Socket creation error\nExiting thread %d\n", threadId);
					continueRunning = 0;
					return NULL;
				}

				// make it non-blocking
				if( (flags = fcntl(theSocket, F_GETFL) == -1) || fcntl(theSocket, F_SETFL, flags | O_NONBLOCK) == -1)
				{
					printf("Socket non-block initialize error\nExiting thread %d\n", threadId);
					continueRunning = 0;
					return NULL;
				}
			}			
			connections[thisThread].connectionStatus = CONN_DISCONNECTED;
		}
		
		// while loop for making connection or accepting a connection
		while(connections[thisThread].connectionStatus != CONN_CONNECTED && continueRunning)
		{
			if(listenMode == LISTEN_MODE)
			{
				// waiting for a peer to connect
				if( (peerSocket = accept(theSocket, (struct sockaddr*)NULL, NULL)) == -1 )
				{
					errorNo = errno;
					if(errorNo != EWOULDBLOCK)
					{
						printf("Connection terminated after accept - thread %d\nExiting...\n", threadId);
						connections[thisThread].connectionStatus = CONN_DISCONNECTED;
					}
					else
						usleep(LOOP_SLEEP_DURATION);
				}
				else
				{
					peerAddrLen = sizeof(struct sockaddr);
					if( getpeername(peerSocket, (struct sockaddr*)&peerAddr, &peerAddrLen) == 0 )
						printf("Accepted connection from %s:%d at thread %d\n", inet_ntoa(peerAddr.sin_addr), ntohs(peerAddr.sin_port), threadId);
					
					// now the peer socket has to be non-blocking too
					if( (flags = fcntl(peerSocket, F_GETFL) == -1) || fcntl(peerSocket, F_SETFL, flags | O_NONBLOCK) == -1)
					{
						printf("Peer socket non-block initialize error\nExiting thread %d\n", threadId);
						continueRunning = 0;
						close(theSocket);
						close(peerSocket);
						return NULL;
					}

					receiveSocket = peerSocket;
					connections[thisThread].socket = peerSocket;
					connections[thisThread].connectionStatus = CONN_CONNECTED;					
				}
			}
			else // listenMode == CONNECT_MODE
			{
				// if -c paramter is passed, connect to the given IP:Port
				if( connect(theSocket, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0 )
				{
					errorNo = errno;
					// a connect is already in progress
					if(errorNo == EALREADY || errorNo == EINPROGRESS)
					{
						connections[thisThread].connectionStatus = CONN_IN_PROGRESS;
						usleep(LOOP_SLEEP_DURATION);
					}
					// already connected
					else if(errorNo == EISCONN)
					{
						receiveSocket = theSocket;
						connections[thisThread].socket = theSocket;
						connections[thisThread].connectionStatus = CONN_CONNECTED;
						printf("Connection established at thread %d\n", threadId);
					}
					else
					{
						printf("Connection failed at thread %d errorNo: %d\nExiting...\n", threadId, errorNo);
						usleep(LOOP_SLEEP_DURATION);
					}
				}
			}
		}
		
		// each socket should be accessed with mutex because when the thread reads data from 'receiveSocket',
		// the other thread's socket should send the data out
		pthread_mutex_lock(&threadMutexes[thisThread]);
		readBytesCount = recv(receiveSocket, buffer, sizeof(buffer), flags);
		errorNo = errno;
		pthread_mutex_unlock(&threadMutexes[thisThread]);
		
		if(readBytesCount == -1)
		{
			if(errorNo != EWOULDBLOCK)
			{
				printf("Connection terminated unexpectedly at thread %d - errno %d\nExiting...\n", threadId, errorNo);
				pthread_mutex_lock(&threadMutexes[thisThread]);
				connections[thisThread].connectionStatus = CONN_UNINITIALIZED;
				close(receiveSocket);
				pthread_mutex_unlock(&threadMutexes[thisThread]);
			}
			else
				// since I made the socket non-blocking, it returns -1 but with EWOULDBLOCK errno
				// so I sleep the thread very shortly to call receive again
				usleep(LOOP_SLEEP_DURATION);
		}
		else if(readBytesCount == 0)
		{
			printf("Connection closed at thread %d\nExiting...\n", threadId);
			pthread_mutex_lock(&threadMutexes[thisThread]);
			connections[thisThread].connectionStatus = CONN_UNINITIALIZED;
			close(receiveSocket);
			pthread_mutex_unlock(&threadMutexes[thisThread]);
		}
		else if(readBytesCount > 0)
		{
			// access the other thread's socket to send out read data
			if( connections[otherThread].connectionStatus == CONN_CONNECTED )
			{
				pthread_mutex_lock(&threadMutexes[otherThread]);
				sendto(connections[otherThread].socket, buffer, readBytesCount, 0, (struct sockaddr*)&sendAddr, sizeof(sendAddr));
				pthread_mutex_unlock(&threadMutexes[otherThread]);
			}
		}
	}

	// clean-up
	close(theSocket);
	if(peerSocket != 0)
		close(peerSocket);
    
	return NULL;
}
//--------------------------------------------------------------------------------------------------------------------------------
// returns 1 if given string is number in string form
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
// returns 1 if given string is IPv4 adddress
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
// handler of signal for SIGINT (user interruption)
void signalHandler(int sig)
{
	printf("Closing...\n");
	continueRunning = 0;
}
//--------------------------------------------------------------------------------------------------------------------------------
// print parameters and usage info
void printUsage()
{
	printf("usage: dembe <mode> [<TCP_IP1>] <PORT1> <mode> [<TCP_IP2>] <PORT2>\n\n");
	printf("mode\n");
	printf("   -l\t Listen mode:  It will listen for a port. This mode doesn't require TCP_IP\n");
	printf("   -c\t Connect mode: It will connect to given IP:Port\n\n");
	printf("example: ./dembe -l 8080 -c 10.10.10.40 5000\n");
	printf("         This will listen on port 8080 and will connect to 10.10.10.40:5000\n\n");
}
//--------------------------------------------------------------------------------------------------------------------------------
