#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/wait.h>
#define PORT 32000
#define MAX_ARGUMENTS 10
#define RESPONSE_TEXT 1
#define RESPONSE_FILE 2

int validate_input(char *buffer)
{

	// tokenize input
	char *arguments[MAX_ARGUMENTS];
	int num_arguments = 0;

	// Parse the input commands
	char *token = strtok(buffer, " "); // Tokenize command using space as delimiter

	while (token != NULL)
	{
		arguments[num_arguments++] = token; // Store the token in the array
		token = strtok(NULL, " ");			// Get the next token
	}
	arguments[num_arguments] = NULL; // Set the last element of the array to NULL
	char *cmd = arguments[0];
	// printf("command is %s\n",cmd);
	char *quittkn = strtok(cmd, " \t\n\r"); // tokenize the input string on whitespace characters
	if (quittkn != NULL && strcmp(quittkn, "quit") == 0)
	{
		// Handle "quit" command
		return 1;
	}
	if (num_arguments < 2)
	{
		printf("\nInvalid command, try again\n\n");
		return 0;
	}
	// todo validate input here further

	// return 0 if any error

	return 1;
}

int main(int argc, char *argv[])
{
	int sock = 0, valread;
	struct sockaddr_in serv_addr;
	char buffer[1024] = {0};
	char response_text[1024];
	char valbuf[1024];
	char server_ip[16];

	if (argc < 2)
	{
		printf("Usage: %s <server_ip>\n", argv[0]);
		return 1;
	}
	strcpy(server_ip, argv[1]);

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Socket creation error\n");
		return 1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0)
	{
		printf("Invalid address or Address not supported\n");
		return 1;
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("Connection failed\n");
		return 1;
	}

	printf("Connected to the server! \n");
	read(sock, buffer, 1024);
	printf("Message from server: '%s' \n", buffer);

	while (1)
	{
		printf("Enter a command: \n");

		fgets(buffer, 1024, stdin);
		// buffer[strcspn(buffer, "\n")] = 0; // remove newline character

		strcpy(valbuf, buffer);
		if (!validate_input(valbuf))
			continue;

		// send command to server
		send(sock, buffer, strlen(buffer), 0);

		int response_type;
		read(sock, &response_type, sizeof(response_type));

		if (response_type == RESPONSE_TEXT)
		{
			memset(response_text, 0, sizeof(response_text)); // Clear the response text buffer
			read(sock, response_text, sizeof(response_text));
			printf("Received text response:\n%s\n", response_text);
		}
		else
		{
			FILE *fp = fopen("received.tar.gz", "wb");
			if (fp == NULL) {
				printf("Error: Could not open file for writing.\n");
			} else {

				//read file size
				long filesize;
				read(sock, &filesize, sizeof(filesize));

				// write(sock, &filesize, sizeof(filesize));

				printf("Filesize: %ld", filesize);
				fflush(stdout);

				char buffer[1024];
				int bytes_read;
				while ((filesize>0)&&((bytes_read = read(sock, buffer, sizeof(buffer))) > 0)) {
					// fwrite(buffer, 1, bytes_read, fp);
					if (fwrite(buffer, 1, bytes_read, fp) != bytes_read)
					{
						printf("Error: could not write to file\n");
						fclose(fp);
						return 1;
					}
					printf("Bytes Received: %d\n", bytes_read);
					filesize-=bytes_read;
				}
				if (bytes_read < 0) {
					printf("Error: Failed to receive file.\n");
				}
				fclose(fp);
			}
			// FILE *fp = fopen("received.tar", "wb");
			// if (fp == NULL)
			// {
			// 	// handle error
			// }
			// char buffer[1024];
			// int bytes_read;
			// while ((bytes_read = read(sock, buffer, sizeof(buffer))) > 0)
			// {
			// 	fwrite(buffer, 1, bytes_read, fp);
			// 	printf("Bytes Received: %d\n", bytes_read);
			// }
			// fclose(fp);
		}
	}
	close(sock);
	return 0;
}
