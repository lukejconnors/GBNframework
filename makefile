all:
	gcc GBNclient.c -o GBNclient
	gcc GBNserver.c -o GBNserver

clean:
	rm -f GBNclient GBNserver output_file.txt