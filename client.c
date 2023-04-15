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
#define RESPONSE_QUIT 3
#define MAX_COMMAND_LENGTH 256

int sock = 0;

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

int validate_command(char *command_name,int num_arguments) {

    // check the command name and number of arguments
    if (strcmp(command_name, "findfile") == 0) {
        if (num_arguments != 1) {
            return 0; // invalid number of arguments for findfile command
        }
    } else if (strcmp(command_name, "sgetfiles") == 0) {
        if (num_arguments < 2 || num_arguments > 3) {
            return 0; // invalid number of arguments for sgetfiles command
        }
    } else if (strcmp(command_name, "dgetfiles") == 0) {
        if (num_arguments < 2 || num_arguments > 3) {
            return 0; // invalid number of arguments for dgetfiles command
        }
    } else if (strcmp(command_name, "getfiles") == 0) {
        if (num_arguments < 1 || num_arguments > 7) {
            return 0; // invalid number of arguments for getfiles command
        }
    } else if (strcmp(command_name, "gettargz") == 0) {
        if (num_arguments < 1 || num_arguments > 7) {
            return 0; // invalid number of arguments for gettargz command
        }
    } else if (strcmp(command_name, "quit") == 0) {
        if (num_arguments != 0) {
            return 0; // invalid number of arguments for quit command
        }
    } else {
        return 0; // invalid command name
    }

    return 1; // command is valid
}


int main(int argc, char *argv[])
{
	int valread;

	char buffer[1024] = {0};
	char response_text[1024];
	char command[1024];
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

		strcpy(command, buffer);

		char command_name[MAX_COMMAND_LENGTH];
		char arguments[10][MAX_COMMAND_LENGTH];
		int num_arguments = 0;
		int i = 0;
		int k=0;
		int len = strlen(command)-1;

		while (i < len && command[i] == ' ') {
			i++;
		}

		// extract the command name
		while (i < len && command[i] != ' ' && command[i] != '\n') {
			command_name[k] = command[i];
			i++;
			k++;
		}
		command_name[k] = '\0';

		// skip any extra spaces before the arguments
		while (i < len && command[i] == ' ') {
			i++;
		}

		// extract the arguments
		while (i < len && num_arguments < 10) {
			int j = 0;
			while (i < len && command[i] != ' ') {
				arguments[num_arguments][j] = command[i];
				i++;
				j++;
			}
			arguments[num_arguments][j] = '\0';
			num_arguments++;

			// skip any extra spaces between the arguments
			while (i < len && command[i] == ' ') {
				i++;
			}
		}

		if (!validate_command(command_name,num_arguments))
		{
			printf("Invalid Command Syntax !\n");
			continue;
		}
		char* result = malloc(MAX_ARGUMENTS * 100);
		memset(result, 0, sizeof(result));
		// initialize the result string to an empty string
		result = command_name;
		strcat(result, " ");
		
		// concatenate each argument to the result string
		for (int i = 0; i < num_arguments ; i++) {
			strcat(result, arguments[i]);
			if (i!=num_arguments-1) {
				strcat(result, " ");
			}
		}

		// printf("the filtered command is '%s' with size %lu\n",result,strlen(result));

		// send command to server
		send(sock, result, strlen(result), 0);

		int response_type;
		read(sock, &response_type, sizeof(response_type));

		if(response_type == RESPONSE_QUIT){
			printf("\nDisconnected from the server.\n" );
			exit(0);
		}
		else if (response_type == RESPONSE_TEXT)
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
