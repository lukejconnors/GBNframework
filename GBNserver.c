/* GBNserver.c */
/* Witten by Kevin Colin and Luke Connors */


#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h> /* close() */
#include <string.h> /* memset() */
#include <stdlib.h>
#include "sendto_.h"
#include <errno.h>

#define MAXBUFSIZE 1024
#define DATAPACKETSIZE 1004
#define HEADERSIZE 64
#define RWS 6

struct frame {
	int i;
	int s;
	char data[DATAPACKETSIZE];
	};
		
void send_ack(int lfread, int sd, struct sockaddr* cliAddr, int addr_size)
{
	//printf("sending ack %d\n", lfread);
	
	int nbytes;
	
	char packet[9], last_frame_received[9];
	
	//  create header and packet
	sprintf(last_frame_received, "%08x", lfread);
	strcpy(packet, last_frame_received);
	
	if((nbytes = sendto_(sd, packet, sizeof(packet), 0, cliAddr, addr_size)) == 0)
	{
		printf("error sending packet %s\n", strerror(errno));
	}
}

void log_write(char*file, char *logmsg, int i, int lfread, int lfrcvd, int laf)
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

	sprintf(loginfo, "%04d %04d %04d %04d %s", i, lfread, lfrcvd, laf, asctime(loctime));

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
	if(argc<6) {
		printf("usage : %s <server_port> <error rate> <random seed> <output_file> <send_log> \n", argv[0]);
		exit(1);
	}

	printf("error rate : %f\n",atof(argv[2]));

	/* Note: you must initialize the network library first before calling sendto_().  The arguments are the <errorrate> and <random seed> */
	init_net_lib(atof(argv[2]), atoi(argv[3]));
	printf("error rate : %f\n",atof(argv[2]));

	/* socket creation */
	int sd;
	if((sd=socket(PF_INET,SOCK_DGRAM,0))<0)
	{
		printf("%s: cannot open socket \n", argv[0]);
		exit(1);
	}

	/* bind server port to "well-known" port whose value is known by the client */
	struct sockaddr_in servAddr;
	bzero(&servAddr,sizeof(servAddr));                    //zero the struct
	servAddr.sin_family = AF_INET;                   //address family
	servAddr.sin_port = htons(atoi(argv[1]));        //htons() sets the port # to network byte order
	servAddr.sin_addr.s_addr = INADDR_ANY;           //supplies the IP address of the local machine

	if(bind(sd, (struct sockaddr *)&servAddr, sizeof(servAddr))<0)
	{
		printf("%s: cannot to bind port number %d \n",argv[0], atoi(argv[1]));
		exit(1); 
	}
	
	/* open file to send to write output */
	FILE *fp;
	if((fp = fopen(argv[4], "wb")) < 0) {
	  	printf("unable to open send file %s", argv[4]);
	  	exit(1);
	}

	
	struct sockaddr_in cliAddr;
	unsigned int cliLen;
	int nbytes, i, s, lfread = -1;
	char index[9], size[9], logmsg[11];
	char ackBuffer[9];
	sprintf(ackBuffer, "%08x", -1);
	struct frame framebuffer[RWS];
	int j;
	for(j = 0; j < RWS; j++)
	{
		framebuffer[j].i = -1;
		framebuffer[j].s = -1;
	} 
	
	int receiving = 1;
	/* Receive message from client */
	while(receiving)
	{
		char recvmsg[MAXBUFSIZE];
		bzero(recvmsg,sizeof(recvmsg));
		cliLen = sizeof(cliAddr);
		if ((nbytes = recvfrom(sd, &recvmsg, sizeof (recvmsg), 0, (struct sockaddr *) &cliAddr, &cliLen)) < 0)
		{
			printf("error reciving data %s\n", strerror(errno));
		}
		else
		{
			//  extract header
			int j;
			for(j = 0; j < 8; j++)
				index[j] = recvmsg[j];
			index[8] = '\0';
			for(j = 8; j< 16; j++)
				size[j-8] = recvmsg[j];
			index[8] = '\0';
			i = (int)strtol(index, NULL, 16);
			s = (int)strtol(size, NULL, 16);
			
			//  extract packet
			char packet[s];
			strncpy(packet, recvmsg + 17, s);
			
			//  print packet from server
			//printf("Client says\n%d\n%d\n%s\n", i, s, packet);
			//printf("\n\nreceived packet %d\n", i);
			
			//  send ACK
			if((lfread + 1) == i)
			{
				lfread = i;
				strcpy(ackBuffer, index);
				int j;
				//for(j = 0; j< 8; j++)
				  //ackBuffer[j] = index[j];
				ackBuffer[8] = '\0';
				
				//printf("writing packet: %d\n", i);
				//  write packets into file at frame
				if(fwrite(packet, 1, s, fp) == 0)
				{
					printf("error writing packet into file%s\n", strerror(errno));
				}
				else{
				  sprintf(logmsg, "Receive   ");
				  log_write(argv[5], logmsg, lfread, lfread, i, lfread+RWS);
				}
				
				if(s < DATAPACKETSIZE)
				{
					for(j = 0; j < 10; j++)
					{
						send_ack(lfread, sd, (struct sockaddr *) &cliAddr, sizeof(cliAddr));
						sprintf(logmsg, "Resend    ");
						log_write(argv[5], logmsg, lfread, lfread, i, lfread+RWS);
					}
					receiving = 0;
					break;
				}
				
				for(j = 1; j < RWS; j++)
				{
					if(framebuffer[j].i == (lfread + 1))
					{
						//  write packets into file at frame
						// printf("writing packet: %d of size %d \n", lfread + 1, framebuffer[j].s);
						if(fwrite(framebuffer[j].data, 1, framebuffer[j].s, fp) == 0)
						{
							printf("error writing packet into file%s\n", strerror(errno));
						}
						else{
						  sprintf(logmsg, "Receive   ");
						  log_write(argv[5], logmsg, lfread, lfread, i, lfread+RWS);
						}
						lfread ++;
						
						if(framebuffer[j].s < DATAPACKETSIZE)
						{
							for(j = 0; j < 10; j++)
							{
								send_ack(lfread, sd, (struct sockaddr *) &cliAddr, sizeof(cliAddr));
								sprintf(logmsg, "Resend    ");
								//log_write(argv[6], logmsg, lfread, lfread, i, lfread+RWS);
							}
							receiving = 0;
							break;
						}
					}
					else 
					{
						break;
					}
				}
				
				send_ack(lfread, sd, (struct sockaddr *) &cliAddr, sizeof(cliAddr));
				sprintf(logmsg, "Send      ");
				log_write(argv[5], logmsg, lfread, lfread, i, lfread+RWS);
			}
			else if(lfread < i && i < lfread+RWS)
			{
				send_ack(lfread, sd, (struct sockaddr *) &cliAddr, sizeof(cliAddr));
				sprintf(logmsg, "Resend    ");
				log_write(argv[5], logmsg, lfread, lfread, i, lfread+RWS);
				
				//printf("buffering packet: %d from lfread: %d\n", i, lfread);
				//  buffer packet
				framebuffer[i - (lfread + 1)].i = i;
				framebuffer[i - (lfread + 1)].s = s;
				strncpy(framebuffer[i - (lfread + 1)].data, packet, s);
			}
			else
			{
				//printf("packet out of window: %d\n", i);
				// Frame is out of window
				send_ack(lfread, sd, (struct sockaddr *) &cliAddr, sizeof(cliAddr));
				sprintf(logmsg, "Resend    ");
				log_write(argv[5], logmsg, lfread, lfread, i, lfread+RWS);
			}
			
			//  write buffered packets to file, move 
			
			//  server goes to sleep
		}
	}
	
	printf("fclose\n");
	fclose(fp);
	
	close(sd);
	exit(0);
}



