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
#include <zlib.h>
#include <libtar.h> //sudo apt-get install libtar-dev
#include <fcntl.h>

#define SERVER_PORT 8080
#define MIRROR_PORT 8081
#define MAX_ARGUMENTS 10
#define RESPONSE_TEXT 1
#define RESPONSE_FILE 2
#define RESPONSE_QUIT 3
#define MAX_COMMAND_LENGTH 256

int sock = 0;

int unzip_tar()
{
	int ret;
	ret = system("gzip -d received.tar.gz");
	ret = system("tar xf received.tar");

	if (ret != 0)
	{
		fprintf(stderr, "Failed to open tar received.tar.gz\n");
		return 1;
	}
	return 0;
}

int connectToServerOrMirror(char *ip, int port)
{
	printf("Trying to connect to %s with port no. %d\n", ip, port);
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

int verify_arguments(char arguments[][MAX_COMMAND_LENGTH], int num_arguments)
{
	char *command = arguments[0];
	if (strcmp(command, "quit") == 0)
	{
		if (num_arguments != 1)
		{
			printf("Invalid arguments: quit command does not take any arguments\n");
			return -1;
		}
		return 0;
	}
	if (num_arguments < 2)
	{
		printf("Invalid arguments: must specify a command and a filename/extension list\n");
		return -1;
	}
	if (strcmp(command, "findfile") == 0)
	{
		if (num_arguments != 2)
		{
			printf("Invalid arguments: findfile command must have a single filename argument\n");
			return -1;
		}
		char *filename = arguments[num_arguments - 1];
		if (strcmp(filename, "-u") == 0)
		{
			printf("Invalid arguments: findfile command must have a single filename argument\n");
			return -1;
		}
		return 0;
	}
	else if (strcmp(command, "sgetfiles") == 0 || strcmp(command, "dgetfiles") == 0)
	{
		if (num_arguments < 3 || num_arguments > 4)
		{
			printf("Invalid arguments: sgetfiles/dgetfiles command must have size/date range arguments and optionally the -u flag\n");
			return -1;
		}

		if (strcmp(command, "sgetfiles") == 0)
		{
			int range1 = atoi(arguments[1]);
			int range2 = atoi(arguments[2]);
			if (range1 < 0 || range2 < 0 || range1 > range2)
			{
				printf("Invalid arguments: sgetfiles command must have both size1 & size2 >= 0 and size1 <= size2\n");
				return -1;
			}
		}
		else
		{
			// Extract the dates from the arguments
			char *date1 = arguments[1];
			char *date2 = arguments[2];

			// Convert the dates to integers
			int year1, month1, day1;
			int year2, month2, day2;

			if (sscanf(date1, "%d-%d-%d", &year1, &month1, &day1) != 3)
			{
				fprintf(stderr, "Error: Invalid date format: %s\n", date1);
				return -1;
			}

			if (sscanf(date2, "%d-%d-%d", &year2, &month2, &day2) != 3)
			{
				fprintf(stderr, "Error: Invalid date format: %s\n", date2);
				return -1;
			}

			// Compare the dates
			if (year1 < year2 || (year1 == year2 && (month1 < month2 || (month1 == month2 && day1 <= day2))))
			{
				// do nothing, valid dates
			}
			else
			{
				printf("%s is greater than %s\n", date1, date2);
				return -1;
			}
		}
		char *flag = arguments[num_arguments - 1];
		if (strcmp(flag, "-u") == 0)
		{
			if (num_arguments - 2 == 0)
			{
				printf("Invalid arguments: sgetfiles/dgetfiles command must have size/date range arguments and optionally the -u flag\n");
				return -1;
			}
			return 1;
		}
		else if (num_arguments == 4 && strcmp(flag, "-u") != 0)
		{
			printf("Invalid arguments: sgetfiles/dgetfiles command must have -u flag as last argument if specified\n");
			return -1;
		}
		return 0;
	}
	else if (strcmp(command, "getfiles") == 0 || strcmp(command, "gettargz") == 0)
	{
		if (num_arguments < 2)
		{
			printf("Invalid arguments: getfiles/gettargz command must have at least one filename/extension argument\n");
			return -1;
		}
		if (num_arguments > 8)
		{
			printf("Invalid arguments: getfiles/gettargz command can have at most 6 filename/extension arguments and optionally the -u flag\n");
			return -1;
		}
		char *flag = arguments[num_arguments - 1];
		if (strcmp(flag, "-u") == 0)
		{
			if (num_arguments - 2 == 0)
			{
				printf("Invalid arguments: getfiles/gettargz command can have at most 6 filename/extension arguments and optionally the -u flag\n");
				return -1;
			}
			return 1;
		}
		else if (num_arguments == 8 && strcmp(flag, "-u") != 0)
		{
			printf("Invalid arguments: getfiles command must have -u flag as last argument if specified\n");
			return -1;
		}
		return 0;
	}
	else
	{
		printf("Invalid command: %s\n", command);
		return -1;
	}
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

		char arguments[10][MAX_COMMAND_LENGTH];
		int num_arguments = 0;
		int i = 0;
		int k = 0;
		int len = strlen(command) - 1;

		while (i < len && command[i] == ' ')
		{
			i++;
		}

		// extract the arguments
		while (i < len && num_arguments < 10)
		{
			int j = 0;
			while (i < len && command[i] != ' ')
			{
				arguments[num_arguments][j] = command[i];
				i++;
				j++;
			}
			arguments[num_arguments][j] = '\0';
			num_arguments++;

			// skip any extra spaces between the arguments
			while (i < len && command[i] == ' ')
			{
				i++;
			}
		}

		int unZip = 0;
		int checkArguments = verify_arguments(arguments, num_arguments);
		if (checkArguments == -1)
		{
			continue;
		}
		else if (checkArguments == 1)
		{
			unZip = 1;
		}
		else
		{
			unZip = 0;
		}
		char *result = malloc(MAX_ARGUMENTS * 100);
		memset(result, 0, sizeof(result));
		result[0] = '\0';

		// concatenate each argument to the result string
		for (int i = 0; i < num_arguments - unZip; i++)
		{
			strcat(result, arguments[i]);
			if (i != num_arguments - 1 - unZip)
			{
				strcat(result, " ");
			}
		}

		// send command to server
		send(sock, result, strlen(result), 0);

		int response_type;
		read(sock, &response_type, sizeof(response_type));

		if (response_type == RESPONSE_QUIT)
		{
			printf("\nDisconnected from the server.\n");
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
				printf("Tar File received successfully.\n");
				if (bytes_read < 0)
				{
					printf("Error: Failed to receive file.\n");
				}
				else if (unZip)
				{
					if (unzip_tar() != 0)
					{
						printf("Error: Failed to unzip a tar file.\n");
					}
					else
					{
						printf("Received Tar file unzipped successfully!\n");
					}
				}
				fclose(fp);
			}
		}
	}
	close(sock);
	return 0;
}
