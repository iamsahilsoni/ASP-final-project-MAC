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

#define SERVER_PORT 32000
#define MIRROR_PORT 32001
#define MAX_ARGUMENTS 10
#define RESPONSE_TEXT 1
#define RESPONSE_FILE 2

int sock = 0;

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

int connectToServerOrMirror(char *ip, int port)
{
	printf("Trying to connect to %s with port no. %d\n",ip,port);
	struct sockaddr_in serv_addr;
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Socket creation error\n");
		return 0;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);

	if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0)
	{
		printf("Invalid address or Address not supported\n");
		return 0;
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		return 0;
	}
	int isAccepted;
	read(sock, &isAccepted, sizeof(isAccepted));
	return isAccepted;
}

void remove_linebreak(char **tokens, int num_tokens)
{
    for (int i = 0; i < num_tokens; i++)
    {
        char *token = tokens[i];
        int length = strcspn(token, "\n");
        char *new_token = (char *)malloc(length + 1);
        strncpy(new_token, token, length);
        new_token[length] = '\0';
        tokens[i] = new_token;
    }
}

int main(int argc, char *argv[])
{
	int valread;

	char buffer[1024] = {0};
	char response_text[1024];
	char valbuf[1024];
	char server_ip[16];
	char mirror_ip[16];

	if (argc < 3)
	{
		printf("Usage: %s <server_ip> <mirror_ip>\n", argv[0]);
		return 1;
	}
	strcpy(server_ip, argv[1]);
	strcpy(mirror_ip, argv[2]);

	if (connectToServerOrMirror(server_ip, SERVER_PORT) > 0)
	{
		printf("Connected to the Server! \n");
	}
	else if (connectToServerOrMirror(mirror_ip, MIRROR_PORT) > 0)
	{
		printf("Connected to the Mirror! \n");
	}
	else
	{
		printf("Connection failed\n");
		return 1;
	}

	read(sock, buffer, 1024);
	printf("Message from server: '%s' \n", buffer);

	while (1)
	{
		printf("\nEnter a command:\n");

		memset(buffer, 0, sizeof(buffer));
		fgets(buffer, 1024, stdin);

		strcpy(valbuf, buffer);
		// printf("buff is %s",buffer);
		
		// char *arguments[MAX_ARGUMENTS];
		// memset(arguments, 0, sizeof(arguments));
        // int num_arguments = 0;

        // // Parse the command received from client
        // char *token = strtok(valbuf, " "); // Tokenize command using space as delimiter

        // while (token != NULL)
        // {
        //     arguments[num_arguments++] = token; // Store the token in the array
		// 	printf("token is '%s'\n",token);
        //     token = strtok(NULL, " ");          // Get the next token
        // }
        // arguments[num_arguments] = NULL; // Set the last element of the array to NULL

        // // Remove line breaks from tokens
        // remove_linebreak(arguments, num_arguments);

		// char* result = malloc(MAX_ARGUMENTS * 100);
		// memset(result, 0, sizeof(result));
		// // initialize the result string to an empty string
		// result[0] = '\0';
		
		// printf("%d\n",num_arguments);
		// // concatenate each argument to the result string
		// for (int i = 0; i < num_arguments ; i++) {
		// 	strcat(result, arguments[i]);
		// 	if (i!=num_arguments-1) {
		// 		strcat(result, " ");
		// 	}
		// }

		// printf("the filtered command is '%s' with size %lu\n",result,strlen(result));

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
			printf("\nReceived text response:\n%s\n", response_text);
		}
		else
		{
			FILE *fp = fopen("received.tar.gz", "wb");
			if (fp == NULL)
			{
				printf("Error: Could not open file for writing.\n");
			}
			else
			{
				// read file size
				long filesize;
				read(sock, &filesize, sizeof(filesize));

				fflush(stdout);

				char buffer[1024];
				int bytes_read;
				while ((filesize > 0) && ((bytes_read = read(sock, buffer, sizeof(buffer))) > 0))
				{
					if (fwrite(buffer, 1, bytes_read, fp) != bytes_read)
					{
						printf("Error: could not write to file\n");
						fclose(fp);
						return 1;
					}
					printf("Bytes Received: %d\n", bytes_read);
					filesize -= bytes_read;
				}
				if (bytes_read < 0)
				{
					printf("Error: Failed to receive file.\n");
				}
				fclose(fp);
			}
		}
	}
	close(sock);
	return 0;
}
