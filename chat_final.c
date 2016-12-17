#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <termios.h>
#include <pthread.h>
#include <errno.h>

#define IP "127.0.0.1"
#define PORT 3000
#define MAX_CLIENT 1024
#define MAX_DATA 104857

#define MAX_EVENTS 10 
#define MyFile_len  1048576 // 1 MB

static struct termios term_old;
typedef struct clientData{
	char *sbuf;
	char *rbuf;
	int rlen;
	int slen;
	int ret;

} clientData;

void initTermios(void);
void resetTermios(void);

int launch_chat(int numOfthread);
int launch_clients(int numOfclients);
int launch_server(void);
int get_server_status(void);
////
pthread_t ClientThreads[MAX_CLIENT];

int setnonblocking(int connSock);
int do_use_fd(int connSock);
void *client_thread_main(void *);

// file function
clientData openFile(int fn);
clientData sendFile(int sendSock, clientData cdata); // fn = file number
char *storeFile(char *buf, char *rdata, int len);
//

int count_clients = 0;
int numOfclients = 3; // 
int count_eos = 0 ; // count end of send 
int client_disconnect = 0;
int num_fd[MAX_CLIENT];


int mutex1_done = 0 ;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int Eliminate(char *str, char ch, int size);


	int
main(int argc, char *argv[])
{
	int ret = -1;
	if ((argc != 2) && (argc != 3)) {
usage:  fprintf(stderr, "usage: %s a|m|s|c numOfclients\n", argv[0]);
		goto leave;
	}
	if ((strlen(argv[1]) != 1))
		goto usage;
	switch (argv[1][0]) {
		case 'a': if (argc != 3)
					  goto usage;
				  if (sscanf(argv[2], "%d", &numOfclients) != 1)
					  goto usage;
				  // Launch Automatic Clients
				  ret = launch_clients(numOfclients);
				  break;
		case 's': // Launch Server
				  printf("Number of clients = %d\n",numOfclients);
				  ret = launch_server();
				  break;
		case 'm': // Read Server Status
				  ret = get_server_status();
				  break;
		case 'c': // Start_Interactive Chatting Session
				  ret = launch_chat(1);
				  break;
		default:
				  goto usage;
	}
leave:
	return ret;
}

	int
launch_chat(int numOfthread)
{
	int clientSock;
	struct sockaddr_in serverAddr;
	fd_set rfds, wfds, efds;
	int ret = -1;
	char rdata[MAX_DATA*numOfclients];
	int i = 1;
	int client_mode = 0; // 0 -> 수신
	// 1 -> 송수신 가능
	// 2 -> 수신만 가능
	struct timeval tm;
	char buf[2];
	char *pf_buf;
	int rlen = 0;
	int fn; // file number
	clientData cData;

	if ((ret = clientSock = socket(PF_INET, SOCK_STREAM, 0)) == -1) { // 소켓 열기
		perror("socket");
		goto leave;
	}

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(IP);
	serverAddr.sin_port = htons(PORT);

	if ((ret = connect(clientSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)))) {
		perror("connect");
		goto leave1;
	}
	printf("[CLIENT] Connected to %s\n", inet_ntoa(*(struct in_addr *)&serverAddr.sin_addr));
	fflush(stdout);
	// start select version of chatting ...
	i = 1;

	ioctl(0, FIONBIO, (unsigned long *)&i);                               // nonblocking 모드  
	if ((ret = ioctl(clientSock, FIONBIO, (unsigned long *)&i))) {        // clientSocket를 nonblocking 몯로 만들어줌
		perror("ioctlsocket");
		goto leave1;
	}
	tm.tv_sec = 0; tm.tv_usec = 1000;
	// 이벤트가 발생할때까지 기다리는 시간
	while (1) {                                       
		FD_ZERO(&rfds); FD_ZERO(&wfds); FD_ZERO(&efds);
		FD_SET(clientSock, &wfds); // clientsock이 write가 가능한지
		FD_SET(clientSock, &rfds); // clientsock이 read가 가능한지
		FD_SET(clientSock, &efds); // exceptiondl 발생한지

		if ((ret = select(clientSock + 1, &rfds, &wfds, &efds, &tm)) < 0) {  // &tm(1m) 시간동안 발생하는지확인후, return -> cpu 성능 issue
			perror("select"); 
			goto leave1;
		} else if (!ret)	// nothing happened within tm
			continue;


		if (FD_ISSET(clientSock, &efds)) {
			printf("Connection closed\n");
			goto leave1;
		}

		// client 가 read할때
		if (FD_ISSET(clientSock, &rfds)) {
			// server 로 부터 데이터를 읽을 수 있는지
			if (!(ret = recv(clientSock, rdata, MAX_DATA, 0))) {
				printf("Connection closed by remote host\n");
				fflush(stdout);
				goto leave1;
			}
			else if (ret > 0) {
				if (client_mode == 0 ){ // 서버로부터 정보를 얻는 mode
					for (i = 0; i < ret ; i++){
						if(rdata[i] == '$') { // 
							printf("%c",rdata[i]); // $ 출력 
							fflush(stdout);
							strncpy(buf, rdata, i); // buf 에 문자열(숫자)저장
							fn = atoi(buf); // 문자열 -> 10진수 변환	
							cData = openFile(fn); // File open
							client_mode = 1;
							memset(rdata,'\0',i+1); // NULL 로 초기화
							// 다른방법 한번 찾아보기
							rlen = ret - 3;
							sleep(1);
						break;
						}
					}
				}
				else if(client_mode == 1) {
					cData.rbuf = storeFile(cData.rbuf, rdata, ret);
				}
				else if(client_mode == 2){
					for( i = 0 ; i < ret ; i ++){
						if(rdata[i] == '%'){ 
						cData.rbuf = storeFile(cData.rbuf, rdata, i-1);
						goto leave1;
						}
					}
					cData.rbuf = storeFile(cData.rbuf, rdata, ret);
				}
			}
			else 
			{
				printf("ret < 0 \n");
				fflush(stdout);
				goto leave1;
			}
		} // read data 처리 끝
		if (FD_ISSET(clientSock,&wfds) && (client_mode == 1) ) {
			// send the File
			cData = sendFile(clientSock, cData );
			if(cData.ret == 2 ) {
				printf("end of send thread %d\n",numOfthread);
				fflush(stdout);
				client_mode = 2;	
			}
		}
	}
leave1:
	printf("closed clientSock\n");
	close(clientSock);
leave:
	return ret;
}

	int
launch_server(void)
{
	int serverSock, acceptedSock;
	struct sockaddr_in Addr;
	socklen_t AddrSize = sizeof(Addr);
	int ret, count, n, i = 1;
	int j = 0;
	
	// time
	struct timeval before, after;
	int duration;
	//
	struct epoll_event ev, events[MAX_EVENTS]; 
	int  connSock, n_events = 0, epollfd; 
	char buf[3];
	//

	// socket create
	if ((ret = serverSock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		goto leave;
	}

	setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (void *)&i, sizeof(i));
	// socket 의 세부옵션 조정

	Addr.sin_family = AF_INET; // AF_INF
	Addr.sin_addr.s_addr = INADDR_ANY;// IP 주소 (4 bytes)
	Addr.sin_port = htons(PORT); // 포트 번호

	// bind 
	if ((ret = bind(serverSock, (struct sockaddr *)&Addr,sizeof(Addr)))) {
		perror("bind");
		goto error;
	}

	// listen
	if ((ret = listen(serverSock, 1))) { 
		perror("listen");
		goto error;
	}

	// epoll start //
	epollfd = epoll_create(20); // epoll 준비
	if( epollfd == -1){
		perror("epoll_create");
		exit(EXIT_FAILURE);
	}

	ev.events = EPOLLIN;
	ev.data.fd = serverSock ;// ret = listen_sock -> 서버의 
	if ( epoll_ctl(epollfd, EPOLL_CTL_ADD, serverSock, &ev) == -1){
		perror("epoll_ctl: listen_sock");
		exit(EXIT_FAILURE);
	}

	gettimeofday( &before, NULL);

	while(1){
		// client 가 들어올때 까지(evnet 가 발동할때까지)  wait 
		if( ( n_events = epoll_wait(epollfd, events, MAX_EVENTS, 10) ) == -1 ) {
			// epoll_wailt( epfd, epoll event,  , timeout = -1 -> block mode
			// 1 second wailt
			// return 발생한 파일 지정자의 갯수 리턴
			perror("epoll_ctl: listen_sock");
			goto error;
		}
		for ( n = 0; n < n_events; n++ ) {
			if ( events[n].data.fd == serverSock) { // accept() ready
				connSock = accept(serverSock, (struct sockaddr*)&Addr, &AddrSize);
				if (connSock == -1) {
					perror("accept");
					goto error;
				}

				setnonblocking(connSock); // non-blocking mode set
				num_fd[count_clients++] = connSock; // Socket 부여 ??

				printf("[SERVER] Connected to %s\n", inet_ntoa(*(struct in_addr *)&Addr.sin_addr)); // 누구랑 연결됬는지 확인
				fflush(stdout);
				ev.events = EPOLLIN | EPOLLET;
				ev.data.fd = connSock;
				if (epoll_ctl(epollfd, EPOLL_CTL_ADD, connSock, &ev) == -1) {
					perror("epoll_ctl: connSock");
					exit(EXIT_FAILURE);
				}

				// 접속확인후,  모든 client 에게 보내기 
				if ( numOfclients == count_clients){
					for( i = 0 ; i < count_clients; i++){
						sprintf(buf,"%d",num_fd[i]); // num_fd를 문자열로 변환
						strcat(buf,"$");
						if ((ret = send(num_fd[i], buf, 3, 0)) < 0) {
							perror("Send the data failed by server\n");
							goto leave1;
						}
					}
				}
			}
			else {
				if ( numOfclients == count_clients){ 
					if((ret = do_use_fd(events[n].data.fd) ) <= 0){
						if(client_disconnect == 1)
							goto leave1;
						else break; 
					}
				}
			}
		} // end of for(n_events)
	}// end of while 
leave1:
	
	gettimeofday( &after, NULL );
	duration = (after.tv_sec - before.tv_sec) * 1000000 + (after.tv_usec - before.tv_usec);
	printf("Processing time = %d.%06d sec\n", duration / 1000000, duration % 1000000);

	// closed acceptd Socket
	for(i = 0 ; i < numOfclients; i++)
		close(num_fd[i]);
error1:
error:
	close(serverSock);
	printf("Close the Server\n");
leave:
	return ret;
}

	int
launch_clients(int numOfclients)
{
	int i;
	int rc;
	int status;
	int tmp = numOfclients;
	// greate pthread
	for( i = 0 ; i < numOfclients; i++){
		pthread_create(&ClientThreads[i],NULL, &client_thread_main, (void *)(i+1));
		// ClientThreads, att, start routine, arg
	}
	i = 0;
	while(1)
	{ 
		rc = pthread_join(ClientThreads[(i++)%numOfclients], (void **)&status);
		// thread, retval
		// ptread 성공시 return = 0
		// ClientThreads의 return 값을 status에 보냄
		if ( rc == 0 ) 
		{
			printf("completed join with thread %d status = %d\n",i+1,status);
			tmp--;
			if(tmp == 0) {
				printf("pthread finish\n");
				fflush(stdin);
				return 0;
			}
		}
		else 
		{
			printf("ERROR; return code from pthread_join() is %d, thread %d\n", rc, i);
			goto leave;
			return -1;
		}						 

	}
leave:
	for(i = 0 ; i < numOfclients ; i ++ ) close(num_fd[i]);
}
void *client_thread_main(void *arg){

	int ret = 0 ;
	ret = launch_chat((int)arg);
	pthread_exit((void *) 0);
}
	int
get_server_status(void)
{
	return 0;
}
int setnonblocking(int connSock){
	int flag;
	flag = fcntl(connSock, F_GETFL,0);
	fcntl(connSock, F_SETFL, flag| O_NONBLOCK);

	return 0;
}
int do_use_fd(int connSock)
{
	int ret;
	int rlen; // remain size 
	int slen = 0; // send size
	int count;
	int tmp = 0;
	int i,j;
	char data[MyFile_len];
	char *p;

	while(1){
		if (!(ret = count = recv(connSock, data, MAX_DATA, 0))) {
			// ret == 0 -> connSock closed.
			printf("Connect Closed by Client\n");
			fflush(stdout);
			if( --count_clients == 0){
				client_disconnect = 1;
				goto leave;
			}
		}

		if(ret == -1) // nonblocking 에러처리
		{
			switch(errno){
				case EAGAIN: // 백로그 큐에 접속요청 없음
					goto leave;
					continue;
				case EINTR: // 인터럽트
					continue;
				case ENOMEM: // heap 메모리 없음
					puts("no memory for open a fd");
					break;
				case ENFILE: // 파일 디스크립터 한도초과
					puts("max fd overflow");
					break;
				default: // 아무것도 못받았을때
					puts("recv error");
					break;
			}
		}
		
		// 받은 data 처리 
		for(i = 0 ; i < ret ; i++){
			if(data[i] == '@'){
				printf("%c",data[i]);
				fflush(stdout);
				tmp = Eliminate(data, '@', ret);
				count -= tmp; // send 하는 data 갯수 하나 감소
				count_eos++;
				
				if(count_eos == numOfclients){ // 모든 클라이언트가 데이터를 다 보낼때
					printf("\nserver recv all '@', send the '%%'to clients\n");
					fflush(stdout);
					data[i] = '%';
					count++;
				}
				break; // for 문에서 나오기
			}
		}

		// send the num_fd[j]
		for(j = 0 ; j < numOfclients; j++){
			p = data;		
			rlen = count;
			while(rlen) { // nonblocking 모드인 경우 
					if((ret = send(num_fd[j], p, rlen, 0)) < 0 ){
						perror("send");
						goto leave;
					}
				rlen -= ret;
				p = p + ret;
			}
		}
	} // end of while
leave:
	return ret;

}

/* Initialize new terminal i/o settings */
	void
initTermios(void) 
{
	struct termios term_new;

	tcgetattr(0, &term_old); /* grab old terminal i/o settings */
	term_new = term_old; /* make new settings same as old settings */
	term_new.c_lflag &= ~ICANON; /* disable buffered i/o */
	term_new.c_lflag &= ~ECHO;   /* set no echo mode */
	tcsetattr(0, TCSANOW, &term_new); /* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
	void
resetTermios(void) 
{
	tcsetattr(0, TCSANOW, &term_old);
}
clientData openFile(int fn){

	int fr;
	char filename[20];
	char *sbuf, *rbuf; // send buffer / recv buf
	clientData ret={"e","e"};
	int flag = PROT_WRITE | PROT_READ;
	
	// open send  File
	sprintf(filename,"file_%04d",fn-4);

	if( ( fr = open( filename, O_RDWR) ) < 0 ) {
		perror("filename");
		goto leave;
	}

	if( ( sbuf = mmap( 0 , MyFile_len , flag, MAP_SHARED, fr, 0 ) ) == NULL ){
		perror("open file mmap");
		goto leave;
	}


	// open store File
	sprintf(filename,"rev_file_%04d",fn-4);
	if( ( fr = open( filename, O_RDWR|O_CREAT, 0644) ) < 0 ) {
		perror("filename");
		goto leave;
	}

	ftruncate(fn, MyFile_len * numOfclients); // make the Empty file

	// mmap
	if( ( rbuf = mmap( 0 , MyFile_len * numOfclients, flag, MAP_SHARED, fr, 0 ) ) == NULL ){
		perror("open file mmap");
		goto leave;
	}
	ret.sbuf = sbuf;
	ret.rbuf = rbuf;
	ret.rlen = 0;
	ret.slen = 0;
	ret.ret = 0;
	return ret;

leave:
	return ret;
}
clientData sendFile(int sendSock ,clientData data) // fn = file number
{
	
	int ret = 0;
	int sendlen = 0; // 
	char *buf,*sbuf, *pbuf; 	// sbuf 	= start buf
						// pbuf 	= present buf
	clientData tmp = data;

	//////////////////////////
	sbuf = tmp.sbuf;
	pbuf = tmp.sbuf;
	sendlen = 0;

	while(1){
		sendlen++;
		if( (*pbuf++) == '\n'){
			pthread_mutex_lock(&mutex);
			tmp.slen +=sendlen;
			
			while(sendlen){
				// sendSock 이 nonblock 이기 때문에 	
				if ( ( ret = send(sendSock, sbuf, sendlen, 0) ) < 0 ) {
					perror("send the file error");
					goto error;
				}
				sbuf = sbuf + ret;
				sendlen = sendlen - ret;
			}
			tmp.sbuf = pbuf;
			pthread_mutex_unlock(&mutex);
			
			// 파일 다 보냈는지 check

			if( tmp.slen >= MyFile_len ){ // finish read the file
				pthread_mutex_lock(&mutex);
				printf("End of send File\n");
				if( ( ret = send(sendSock, "@", 1, 0) ) < 0 ) {
					perror("send the '@' error");
					goto error;
				}
				tmp.ret = 2;
				pthread_mutex_unlock(&mutex);
			}
	
			break;
		}
	} // end of while(rlen)
error:
	return tmp;
}
char *storeFile(char *buf, char *rdata, int len)
{
	int ret = 0;
	int i = 0 ;
	for (i = 0; i < len ; i ++){
		*buf++ = *rdata++;
	}

	return buf;

}
int Eliminate(char *str, char ch, int size) {

	int ret = 0;
	int i = 0;
	for(i = 0; i < size ; i++)
	{

		if(*str == ch)
		{
			strcpy(str,str+1);
			str--;
			ret++;
		}
		str++;

	}
	return ret;

}
