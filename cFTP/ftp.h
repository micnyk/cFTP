#pragma once

#include <WinSock2.h>

#define PORT_CHAR_COUNT 6

void append_string(char **str, char *app);

struct ftp_connection
{
	SOCKET socket;
	SOCKET passive_socket;
	struct addrinfo *addr, hints;
};

void ftp_init(void);
void ftp_cleanup(void);
void ftp_connection_free(struct ftp_connection **_connection);
int ftp_connect(struct ftp_connection **connection, char *hostname, int port);
int ftp_hello(struct ftp_connection *connection, char **hello);
int ftp_login(struct ftp_connection *connection, char *username, char *password);
int ftp_send_cmd(struct ftp_connection *connection, char *cmd, FILE *cmd_output, FILE *data_output);
int ftp_passive(struct ftp_connection *connection, char *pasv_response);
