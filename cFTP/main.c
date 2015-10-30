#define _CRT_SECURE_NO_WARNINGS
#include "debug.h"

#include <stdio.h>
#include <string.h>

#include "errors.h"
#include "ftp.h"

#define BUFFER_SIZE 2048
#define INPUT_BUFFER_SIZE 512

void parse_cmd(int *exit, struct ftp_connection *connection);
void print_help(void);
int parse_host_port_str(char *str, char **hostname, int *port);

int main(int argc, char **argv)
{
	struct ftp_connection	*connection = NULL;
	char					*hello_msg = NULL;
	char					*cmd_output = NULL;
	int						ret = 0;
	int						exit = 0;

	char					*hostname = NULL;
	int						port = 0;
	char					*username = NULL;
	char					*password = NULL;

	char					input_buffer[INPUT_BUFFER_SIZE];
	DWORD					input_read = 0;

	if(argc != 2 && argc != 4) {
		print_help();
		system("pause");
		return 0;
	}

	if(argc == 4) {
		username = argv[2];
		password = argv[3];
	}

	ftp_init();

	CHECK_ERROR(parse_host_port_str(argv[1], &hostname, &port));

	printf("Connecting to host: '%s' at port: '%d'...\n", hostname, port);
	CHECK_ERROR(ftp_connect(&connection, hostname, port));
	CHECK_ERROR(ftp_hello(connection, &hello_msg));
	printf("Connected, server hello message:\n%s", hello_msg);

	printf("Logging in as '%s'...\n", (username == NULL) ? "anonymous" : username);
	CHECK_ERROR(ftp_login(connection, username, password));
	printf("Logged in\n");


	while(1) {
		fgets(input_buffer, INPUT_BUFFER_SIZE, stdin);
		ftp_cmd2(connection, input_buffer, &cmd_output);

		if (cmd_output != NULL) {
			SAFE_FREE(cmd_output);
		}
		
		if (_strnicmp(input_buffer, "quit", 4) == 0)
			break;
	}

exit:
	ftp_connection_free(&connection);
	ftp_cleanup();
	SAFE_FREE(hostname);
	SAFE_FREE(username);
	SAFE_FREE(password);
	SAFE_FREE(hello_msg);
	SAFE_FREE(cmd_output);

	system("pause"); 
#ifdef _DEBUG
	_CrtDumpMemoryLeaks();
#endif
	return 0;
}

int parse_host_port_str(char *str, char **hostname, int *port) {
	int		hostname_len = 0;
	char	*colon_position;

	colon_position = strstr(str, ":");

	if(colon_position == NULL) {
		return HOST_PORT_INVALID_FORMAT;
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
		"cftp <hostname:port> [ <username> ] [ <password> ]\n");
}