#pragma once

#include <WinSock2.h>

#define PORT_CHAR_COUNT 6

void _append_string(char **str, char *app);

struct ftp_connection
{
	SOCKET socket;
	SOCKET passive_socket;
	struct addrinfo *addr, hints;
};

void ftp_init(void);
void ftp_cleanup(void);
void ftp_disconnect(struct ftp_connection *connection);
int ftp_connect(struct ftp_connection **connection, char *hostname, int port);
int ftp_hello(struct ftp_connection *connection, char **hello);
int ftp_login(struct ftp_connection *connection, char *username, char *password);
int ftp_passive(struct ftp_connection *connection, char *pasv_response);
int ftp_send_cmd(struct ftp_connection *connection, char *cmd, FILE *cmd_output, FILE *data_output, char cut_newline);
int ftp_retr(struct ftp_connection *connection, char *remote_path, char *local_path);
int ftp_stor(struct ftp_connection *connection, char *local_path, char *remote_path);