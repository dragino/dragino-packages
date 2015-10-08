
/* Sample UDP client */

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

int main(int argc, char**argv){

	int sockfd,n, enabled;
	struct sockaddr_in servaddr,cliaddr;

	if (argc != 3){
		printf("usage:  udpcli <IP address> <message>\n");
		exit(1);
	}

	sockfd=socket(AF_INET,SOCK_DGRAM,0);

	/* Enable broadcast sending */
	enabled = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &enabled, sizeof(enabled));

	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr=inet_addr(argv[1]);
	servaddr.sin_port=htons(9930);

	sendto(sockfd,argv[2],strlen(argv[2]),0, (struct sockaddr *)&servaddr,sizeof(servaddr));

	return 0;
}
