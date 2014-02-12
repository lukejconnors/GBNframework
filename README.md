GBNframework
============

Authors: 
Kevin Colin
Luke Connors

Description:
This is a program designed to send a file from a client to a server using the  
GBNclient executable and GBNserver executable. The file is sent in UDP packets. Go-Back-N
sliding window protocol is used by the client and server to ensure that the entire file
is sent and received and written to the output file in the correct order.

Compile:
	$ make
	$ make clean
		
Execute:
	$ ./GBNserver <server_port> <error_rate> <random_seed> <output_file> <receive_log>
	$ ./GBNclient <server_ip_address> <server_port> <error_rate> <random_seed> <send_file> <send_log>

Example
	$ ./GBNserver 8080 0.5 25 output_file.txt server_log.txt
	$ ./GBNclient 127.0.0.1 8080 0.3 12 input_file.txt client_log.txt

Working functionality:
	Reliably transfer a data file between a client and server despite possible packet 
	losses (packet error rate will vary during our testing to evaluate the correctness 
	of your protocol implementation).
	Sliding windows at both the client and server, with cumulative acknowledgements.
	Timeouts with retransmission.
	Discarding of out­of­range packets.
	Buffering out­of­order packets.

Technical details:
Server log file is entitled server_log.txt. Client log file is entitled client_log.txt. 
Input file for client is entitled input.txt. The error rate must be < 1 and > 0. Both the 
Server and the Client can be run locally over 127.0.0.1.