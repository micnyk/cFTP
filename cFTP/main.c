#include "debug.h"

#include <stdio.h>
#include <string.h>

#include "errors.h"
#include "ftp.h"

#define BUFFER_SIZE 2048
#define INPUT_BUFFER_SIZE 512

void print_help(void);
int _parse_host_port(char *str, char **hostname, int *port);

int main(int argc, char **argv)
{
	struct ftp_connection	*connection = NULL;
	char					*hello_msg = NULL;
	int						exit = 0;

	char					*hostname = NULL;
	int						port = 0;
	char					*username = NULL;
	char					*password = NULL;

	char					input_buffer[INPUT_BUFFER_SIZE];
	char					file_path[INPUT_BUFFER_SIZE];

	// Check if there is proper number of arguments
	if(argc != 2 && argc != 4) {
		print_help();
		system("pause");
		return 0;
	}

	// If there are 4 arguments- use username and password
	if(argc == 4) {
		username = argv[2];
		password = argv[3];
	}

	ftp_init();

	CHECK_ERROR(_parse_host_port(argv[1], &hostname, &port));

	printf("Connecting to host: '%s' at port: '%d'\n", hostname, port);
	CHECK_ERROR(ftp_connect(&connection, hostname, port));
	SAFE_FREE(hostname);

	CHECK_ERROR(ftp_hello(connection, &hello_msg));
	printf("Connected, server hello message:\n%s\n", hello_msg);
	SAFE_FREE(hello_msg);

	printf("Logging in as '%s'...\n", (username == NULL) ? "anonymous" : username);
	CHECK_ERROR(ftp_login(connection, username, password));
	printf("Logged in\n\n");


	while(1) {
		// Read command line input
		printf("> ");
		fgets(input_buffer, INPUT_BUFFER_SIZE, stdin);

		// HELP command
		if(_strnicmp(input_buffer, "help", 4) == 0) {
			print_help();
		}

		// QUIT command
		else if(_strnicmp(input_buffer, "quit", 4) == 0) {
			ftp_send_cmd(connection, "QUIT", stdout, stdout, 0);
			break;
		}

		//  RETR command - download file
		else if(_strnicmp(input_buffer, "retr", 4) == 0) {
			input_buffer[strlen(input_buffer) - 1] = '\0';

			printf("Enter local filename:\n> ");

			fgets(file_path, BUFFER_SIZE, stdin);
			file_path[strlen(file_path) - 1] = '\0';

			CHECK_ERROR(ftp_retr(connection, input_buffer + 5, file_path));
		}

		//  STOR command - upload file
		else if (_strnicmp(input_buffer, "stor", 4) == 0) {
			input_buffer[strlen(input_buffer) - 1] = '\0';
			printf("Enter local filename:\n> ");

			fgets(file_path, BUFFER_SIZE, stdin);
			file_path[strlen(file_path) - 1] = '\0';

			CHECK_ERROR(ftp_stor(connection, file_path, input_buffer + 5));
		}

		// Not implemented commands
		else {
			ftp_send_cmd(connection, input_buffer, stdout, stdout, 1);
		}
	}

exit:
	ftp_disconnect(connection);
	ftp_cleanup();
	system("pause"); 
#ifdef _DEBUG
	_CrtDumpMemoryLeaks();
#endif
	return 0;
}

int _parse_host_port(char *str, char **hostname, int *port) {
	int		hostname_len = 0;
	char	*colon_position;

	colon_position = strstr(str, ":");

	if(colon_position == NULL) {
		return ERR_HOSTPORT_INVALID_FORMAT;
	}

	hostname_len = (int)(colon_position - str);

	*hostname = (char*)malloc(hostname_len + 1);
	memcpy(*hostname, str, hostname_len);
	(*hostname)[hostname_len] = '\0';

	*port = atoi(colon_position + 1);

	return 0;
}

void print_help(void) {
	printf("Usage:\n"
		"cftp <hostname:port> [ <username> ] [ <password> ]\n\n"
		"pwd\t\t\t\tPrint current directory path\n"
		"cwd <path>\t\t\tChange current directory\n"
		"list\t\t\t\tList files in current directory\n"
		"stor <remote-filename>\t\tUpload file\n"
		"retr <remote-filename>\t\tDownload file\n");
}