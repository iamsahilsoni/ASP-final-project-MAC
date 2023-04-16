// Sahil Soni(110093229), Aashi Thakkar(110093562)
// University of Windsor
// Advanced System Programming - Server Client Project
// April 15th 2023

// Client-Side code
// Usage :-
// 1. gcc -o client client.c
// 2. ./client <server-ip> <mirror-ip>

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

/**
 * Function to extract the contents of a tar.gz archive.
 * The function first uncompresses the archive using gzip, then extracts the files using tar.
 *
 * @return 0 if the extraction was successful, 1 otherwise
 */
int unzip_tar()
{
	int ret;
	// uncompress the tar.gz archive
	ret = system("gzip -d received.tar.gz");

	// extract the files from the uncompressed tar archive
	ret = system("tar xf received.tar");

	if (ret != 0)
	{
		// print an error message if the extraction failed
		fprintf(stderr, "Failed to extract files from received.tar.gz\n");
		return 1;
	}
	// return 0 to indicate successful extraction
	return 0;
}

/**
 * Connects to a server or mirror at the given IP address and port number.
 *
 * @param ip The IP address of the server or mirror.
 * @param port The port number to connect to.
 *
 * @return 1 if the connection was successful and the server/mirror accepted the connection,
 *         0 otherwise.
 */
int connectToServerOrMirror(char *ip, int port)
{
	// Print debug information about the connection attempt.
	printf("Trying to connect to %s with port no. %d\n", ip, port);

	// Create a socket for the connection.
	struct sockaddr_in serv_addr;
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Socket creation error\n");
		return 0;
	}

	// Set up the socket address structure.
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);

	// Convert the IP address string to a network address structure.
	if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0)
	{
		printf("Invalid address or Address not supported\n");
		return 0;
	}

	// Connect to the server/mirror.
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		return 0;
	}

	// Read a flag from the server/mirror indicating whether the connection was accepted.
	int isAccepted;
	read(sock, &isAccepted, sizeof(isAccepted));

	// Return the flag.
	return isAccepted;
}

/*
 * Verify the validity of command-line arguments passed to the program.
 * Returns:
 *   0 if the arguments are valid
 *   -1 if the arguments are invalid
 *   1 if the -u flag is present and needs to be handled separately
 */
int verify_arguments(char arguments[][MAX_COMMAND_LENGTH], int num_arguments)
{
	// Extract the command from the arguments
	char *command = arguments[0];

	// Check for the "quit" command
	if (strcmp(command, "quit") == 0)
	{
		if (num_arguments != 1)
		{
			printf("Invalid arguments: quit command does not take any arguments\n");
			return -1;
		}
		return 0;
	}

	// Check for commands that require at least two arguments
	if (num_arguments < 2)
	{
		printf("Invalid arguments: must specify a command and a filename/extension list\n");
		return -1;
	}

	// Check for the "findfile" command
	if (strcmp(command, "findfile") == 0)
	{
		if (num_arguments != 2)
		{
			printf("Invalid arguments: findfile command must have a single filename argument\n");
			return -1;
		}

		// Check for the -u flag
		char *filename = arguments[num_arguments - 1];
		if (strcmp(filename, "-u") == 0)
		{
			printf("Invalid arguments: findfile command must have a single filename argument\n");
			return -1;
		}
		return 0;
	}

	// Check for the "sgetfiles" and "dgetfiles" commands
	else if (strcmp(command, "sgetfiles") == 0 || strcmp(command, "dgetfiles") == 0)
	{
		if (num_arguments < 3 || num_arguments > 4)
		{
			printf("Invalid arguments: sgetfiles/dgetfiles command must have size/date range arguments and optionally the -u flag\n");
			return -1;
		}

		// Check for the size range arguments for the "sgetfiles" command
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

		// Check for the date range arguments for the "dgetfiles" command
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
	// Declare necessary variables for client program
	char buffer[1024] = {0};  // buffer for incoming data
	char response_text[1024]; // buffer for response text
	char command[1024];		  // buffer for command entered by user
	char server_ip[16];		  // IP address of server
	char mirror_ip[16];		  // IP address of mirror

	// Check if user has entered server IP address and mirror IP address as command-line arguments
	if (argc < 3)
	{
		printf("Usage: %s <server_ip> <mirror_ip>\n", argv[0]);
		return 1;
	}
	strcpy(server_ip, argv[1]);
	strcpy(mirror_ip, argv[2]);

	// Try to connect to the server. If connection fails, try connecting to the mirror.
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

	// Read welcome message from server/mirror
	read(sock, buffer, 1024);
	printf("Message from server: '%s' \n", buffer);

	// Loop until user quits
	while (1)
	{
		printf("\nEnter a command:\n");

		// Clear buffer and get user input
		memset(buffer, 0, sizeof(buffer));
		fgets(buffer, 1024, stdin);

		// Copy user input to command buffer
		strcpy(command, buffer);

		// Extract arguments from command
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

		// Check if command is for unzipping file
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

		// Concatenate arguments into single string
		char *result = malloc(MAX_ARGUMENTS * 100);
		memset(result, 0, sizeof(result));
		result[0] = '\0';

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
		// read the response type from the socket
		read(sock, &response_type, sizeof(response_type));

		if (response_type == RESPONSE_QUIT)
		{
			// the server has indicated that the client should disconnect
			printf("\nDisconnected from the server.\n");
			exit(0);
		}
		else if (response_type == RESPONSE_TEXT)
		{
			// the server has sent a text response
			memset(response_text, 0, sizeof(response_text));  // clear the response text buffer
			read(sock, response_text, sizeof(response_text)); // read the response text from the socket
			printf("\nReceived text response:\n%s\n", response_text);
		}
		else
		{
			// the server has sent a tar file
			FILE *fp = fopen("received.tar.gz", "wb"); // open a file to write the received tar file to
			if (fp == NULL)
			{
				// error opening the file
				printf("Error: Could not open file for writing.\n");
			}
			else
			{
				// read the size of the tar file
				long filesize;
				read(sock, &filesize, sizeof(filesize));

				fflush(stdout);

				char buffer[1024];
				int bytes_read;
				// read the tar file data from the socket in chunks and write it to the file
				while ((filesize > 0) && ((bytes_read = read(sock, buffer, sizeof(buffer))) > 0))
				{
					if (fwrite(buffer, 1, bytes_read, fp) != bytes_read)
					{
						// error writing to the file
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
					// error receiving the file data
					printf("Error: Failed to receive file.\n");
				}
				else if (unZip)
				{
					// if the `unZip` flag is set, attempt to unzip the received tar file
					if (unzip_tar() != 0)
					{
						printf("Error: Failed to unzip a tar file.\n");
					}
					else
					{
						printf("Received Tar file unzipped successfully!\n");
					}
				}
				fclose(fp); // close the file
			}
		}
	}
	close(sock);
	return 0;
}
