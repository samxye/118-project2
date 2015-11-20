/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 64
#define HDRSIZE sizeof(int)

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char **argv) {
  int sockfd; /* socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char payloadbuf[BUFSIZE];
  char packetbuf[BUFSIZE]; /* response packet buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */
  char* hdrbuf = (char*)malloc(HDRSIZE);
  int acknum = 0; // only for receiving
  int seqnum = 0;
  

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  /* 
   * socket: create the parent socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   */
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  /* 
   * bind: associate the parent socket with a port 
   */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  /* 
   * main loop: wait for a datagram, then echo it
   */
  clientlen = sizeof(clientaddr);
  while (1) {

    /*
     * recvfrom: receive a UDP datagram from a client
     */
    bzero(buf, BUFSIZE);
    bzero(packetbuf, BUFSIZE);
    n = recvfrom(sockfd, buf, BUFSIZE, 0,
		 (struct sockaddr *) &clientaddr, &clientlen);
    if (n < 0)
      error("ERROR in recvfrom");

    /* 
     * gethostbyaddr: determine who sent the datagram
     */
    hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, 
			  sizeof(clientaddr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr");
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    printf("Server received datagram from %s (%s)\n", 
	   hostp->h_name, hostaddrp);
    printf("Server received %d/%d bytes: %s\n", strlen(buf), n, buf);
    
    /*
     * open and read file. send file contents via packets.
     */
    FILE *fp;
    fp = fopen(buf, "r");
    if (fp == NULL) 
	    error("ERROR in fopen");
    // Construct payload: get file info
    while (fgets(payloadbuf, BUFSIZE-HDRSIZE, (FILE*)fp) != NULL) {
      // Construct header
      memcpy(hdrbuf, &seqnum, HDRSIZE);

      // Packet = payload + header
      memcpy(packetbuf, hdrbuf, HDRSIZE);
      memcpy(packetbuf+HDRSIZE, payloadbuf, BUFSIZE-HDRSIZE);
      printf("Packet content: %s\n", packetbuf+HDRSIZE);

      // Send packet
      n = sendto(sockfd, packetbuf, BUFSIZE, 0,
		 (struct sockaddr *) &clientaddr, clientlen);
      if (n < 0) 
	    error("ERROR in sendto");

      // Wait for ACK from client
      n = recvfrom(sockfd, hdrbuf, HDRSIZE, 0,
		   (struct sockaddr *) &clientaddr, &clientlen);
      if (n < 0) 
	    error("ERROR in recvfrom");
      memcpy(&acknum, hdrbuf, HDRSIZE);
      printf("Ack number: %d\n", acknum);

      seqnum++;
    }
    fclose(fp);
    printf("Successfully sent file!\n");
    // TODO: send some special message to client to
    // signal that the file finished sending.
    
  }
}
