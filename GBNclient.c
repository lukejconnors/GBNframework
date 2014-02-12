/* GBNclient.c */
/* Witten by Kevin Colin and Luke Connors */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>   /* memset() */
#include <sys/time.h> /* select() */
#include <signal.h>
#include <unistd.h>
#include "sendto_.h"
#include <errno.h>
#include <time.h>

#define MAXBUFSIZE 1024
#define DATAPACKETSIZE 1004
#define HEADERSIZE 18
#define SWS 6

int send_packet(int i, FILE *fp, int sd, struct sockaddr* remoteServAddr, int addr_size)
{
	if(fseek(fp, (i*DATAPACKETSIZE), SEEK_SET) != 0)
	{
		printf("error seeking to position in file");
	}
	
	//size_t data_size;
	int nbytes, s;
	
	char buffer[DATAPACKETSIZE], packet[MAXBUFSIZE], index[9], size[9];
	bzero(buffer,sizeof(buffer));
	if((s = fread(buffer, 1, DATAPACKETSIZE, fp)) > 0)
	{	
		//  create header and packet
		sprintf(index, "%08x", i);
		sprintf(size, "%08x", s);
		sprintf(index, index, 8);
		sprintf(size, size, 8);
		strcpy(packet, index);
		strcat(packet, size);
		strcat(packet, " ");
		strcat(packet, buffer);
		
		//printf("index: %s\n", index);
		
		if((nbytes = sendto_(sd, packet, MAXBUFSIZE, 0, remoteServAddr, addr_size)) == 0)
		{
			printf("error sending packet %s\n", strerror(errno));
		}
		
		//  return 0 if reached last packet at end of file
		if(s < DATAPACKETSIZE) return 0;
		else return 1;
	}
	else
	{
		printf("error reading file into buffer %s\n", strerror(errno));
	}
	
	return 1;
}

void log_write(char*file, char *logmsg, int i, int lfs, int lar)
{
	char full_logmsg[100], loginfo[100];
	
	strcpy(full_logmsg, logmsg);
	
	//  write to log file
	time_t curtime;
	struct tm *loctime;

	curtime = time (NULL);
	loctime = localtime (&curtime);
	
	/* open logfile */
	FILE *lfp;
	if((lfp = fopen(file, "a")) < 0){
	  printf("unable to open log file %s", file);
	  exit(1);
	}
	
	sprintf(loginfo, "%04d %04d %04d %s", i, lfs, lar, asctime(loctime));
	strcat(full_logmsg, loginfo);

	if(fprintf(lfp, "%s", full_logmsg) == 0)
	{
		printf("error writing packet into file%s\n", strerror(errno));
	}
	
	fclose(lfp);
}

int main(int argc, char *argv[]) 
{
	/* check command line args. */
	if(argc<7)
	{
		printf("usage : %s <server_ip> <server_port> <error rate> <random seed> <send_file> <send_log> \n", argv[0]);
		exit(1);
	}

	/* Note: you must initialize the network library first before calling sendto_().  The arguments are the <errorrate> and <random seed> */
	init_net_lib(atof(argv[3]), atoi(argv[4]));
	printf("error rate : %f\n",atof(argv[3]));
	

	/* socket creation */
	int sd;
	if((sd = socket(PF_INET,SOCK_DGRAM,0))<0)
	{
		printf("%s: cannot create socket \n",argv[0]);
		exit(1);
	}

	
	/* get server IP address (input must be IP address, not DNS name) */
	struct sockaddr_in remoteServAddr;
	bzero(&remoteServAddr,sizeof(remoteServAddr));       	//zero the struct
	remoteServAddr.sin_family = AF_INET;                 	//address family
	remoteServAddr.sin_port = htons(atoi(argv[2]));      	//sets port to network byte order
	remoteServAddr.sin_addr.s_addr = inet_addr(argv[1]); 	//sets remote IP address
	
	printf("%s: sending data to '%s:%s' \n", argv[0], argv[1], argv[2]);

	
	/* open file to send to server */
	FILE *fp;
	if((fp = fopen(argv[5], "rb")) < 0){
	  printf("unable to open send file %s", argv[5]);
	  exit(1);
	}

	fseek(fp, 0L, SEEK_END);
	int frameSize =  ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	int numFrames;
	if(frameSize%DATAPACKETSIZE){
	  numFrames = frameSize/DATAPACKETSIZE;
	}
	else{
	  numFrames = (frameSize/DATAPACKETSIZE)+1;
	}

	/* set up timeout */
	fd_set select_fds;                /* fd's used by select */
	struct timeval timeout;           /* Time value for time out */

	FD_ZERO(&select_fds);             /* Clear out fd's */
	FD_SET(sd, &select_fds);          /* Set the bit that corresponds to socket sd */

	timeout.tv_sec = 0;         
	timeout.tv_usec = 50000;
	
	
	int i = 0, nbytes, ack;
    unsigned servLen = sizeof(remoteServAddr);
	int lar = -1, lfs = 0, heighestfs = -1;
	int sending = 1, eof = 0;
	char recvmsg[MAXBUFSIZE], ackmsg[9], logmsg[11];
	
	while (sending)
	{
		//  send all packets in window 
		for(i = lar+1; i < lar+SWS+1; i++)
		{
			if(send_packet(i, fp, sd, (struct sockaddr *) &remoteServAddr, sizeof(remoteServAddr)) == 0)
			{
				//  reached end of file
				eof = 1;
			}
			lfs  = i;
			
			if(lfs > heighestfs) 
			{
				heighestfs = lfs;
				sprintf(logmsg, "Send      ");
				log_write(argv[6], logmsg, i, lfs, lar);
			}
			else
			{
				sprintf(logmsg, "Resend    ");
				log_write(argv[6], logmsg, i, lfs, lar);
			}
			if (i == numFrames)
			{
				break;
			}
		}
		
		int j = 0;

		//  receive acks or timeout for all packets sent
		for(i = lar+1; i <= lfs; i++)
		{
			if (select(sizeof(select_fds), &select_fds, NULL, NULL, &timeout) == 0)
			{
				printf("Select has timed out\n");
				FD_ZERO(&select_fds);             /* Clear out fd's */
				FD_SET(sd, &select_fds);          /* Set the bit that corresponds to socket sd */
			}
			else
			{
				if ((nbytes = recvfrom(sd, &recvmsg, sizeof(recvmsg), 0, (struct sockaddr *) &remoteServAddr, &servLen)) < 0)
				{
					printf("error reciving data %s", strerror(errno));	
				}
				else
				{
					//  print ack from server
					//printf("Server ack %d\n", ack);
					
					memcpy(ackmsg, recvmsg, 8);
					ack = (int)strtol(ackmsg, NULL, 16);
					lar = ack;
					
					sprintf(logmsg, "Receive   ");
					log_write(argv[6], logmsg, ack, lfs, lar);
				}
			}
		}
		
		if(lar == numFrames)
		  sending = 0;
	}
	
	fclose(fp);
	close(sd);
	exit(0);
}
