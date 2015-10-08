#include <stdio.h>
#include <errno.h> 
#include <stdlib.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <linux/net.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <arpa/inet.h>

#define SOCKET_IO_REV   "0.1"
#define HW_VER "0.1"	

#define TIMEOUT	100000L   	/* in us */
#define SOCKET_BUFLEN 1500	/* One standard MTU unit size */ 
#define PORT 9930

/* listening socket */
int udpfd;
struct sockaddr_in servaddr, cliaddr;

int main(int argc, char **argv){

	int n, nready; 
	char datagram[SOCKET_BUFLEN];
	fd_set rset;
	socklen_t addrlen;
	struct timeval	timeout;

	/* Splash ============================================================ */

	/* Start UDP Socket server and listening for commands ================ */
	udpfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(udpfd == -1){
                perror("socket() failed");
		exit(-1);
	}

	/* Prepare the address */
	memset(&servaddr, 0, sizeof(servaddr)); 	
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(PORT);

	if(bind(udpfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) == -1){
		perror("bind() failed");
		exit(-1);
	}

	printf("Listening on port %d\n", PORT);	

	for ( ; ; ) {

		/* descritors set prepared */ 
        	FD_ZERO(&rset);
        	FD_SET(udpfd, &rset);

		/* Set the timeout */
		timeout.tv_sec  = 0;
		timeout.tv_usec = TIMEOUT;

		nready = select(udpfd+1, &rset, NULL, NULL, &timeout);
		if (nready < 0) {
			printf("Error or signal\n");
			if (errno == EINTR)
				continue; /* back to for() */
			else {
      				perror("select() failed");
				exit(-1);
			}
		} else if (nready) {
			/* We have data to read */
				
			addrlen=sizeof(cliaddr);
			if((n = recvfrom(udpfd, datagram, SOCKET_BUFLEN, 0, (struct sockaddr *)&cliaddr, &addrlen))<0){
				/* System error */

				perror("recvfrom() failed");
				exit(-1);
			} else if (n>0){
				/* We have got an n byte datagram */
				datagram[n] = '\0';

				printf("%s\n", datagram);

				/* Send it back */
				//sendto(udpfd, datagram, n, 0, &cliaddr, addrlen);		
	
			} else {
				/* The socket closed, this should not happen with UDP */
				printf("The socket closed ?!?!\n");
			}
			
		} else {
			/* Timeout, expected to happens each 100ms or so */
		}
	}

	return(0);
}
