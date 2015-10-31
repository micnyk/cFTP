#include "debug.h"

#include <WS2tcpip.h>
#include <stdio.h>

#include "ftp.h"
#include "errors.h"

#define BUFFER_SIZE 2048
struct WSAData		wsa_data;
const char			*ANON_USERNAME = "anonymous";
const char			*ANON_PASSWORD = "cft@c.com";

void append_string(char **str, char *app) {
	char	*tmp;
	size_t	app_len = strlen(app);
	size_t	str_len = 0;
	size_t  tmp_len = 0;

	if (*str == NULL) {
		*str = (char*)malloc(app_len + 1);
		strcpy(*str, app);
	}
	else {
		str_len = strlen(*str);

		tmp = (char*)malloc(str_len + app_len + 1);
		tmp[0] = '\0';

		strcat(tmp, *str);
		strcat(tmp, app);
		tmp_len = strlen(tmp);

		free(*str);
		*str = (char*)malloc(tmp_len + 1);
		strcpy(*str, tmp);
		free(tmp);
	}
}

void ftp_init(void) {
	WSAStartup(MAKEWORD(2, 2), &wsa_data);
}

void ftp_cleanup(void) {
	WSACleanup();
}

void ftp_connection_free(struct ftp_connection **_connection) {
	struct ftp_connection *connection = *_connection;

	if (connection == NULL)
		return;

	if (connection->socket != INVALID_SOCKET)
		closesocket(connection->socket);

	if (connection->addr != NULL)
		freeaddrinfo(connection->addr);

	free(*_connection);
	*_connection = NULL;	
}

int ftp_connect(struct ftp_connection **connection, char *hostname, int port) {
	struct ftp_connection	*conn;
	int						result;
	char					port_str[PORT_CHAR_COUNT];

	_itoa(port, port_str, 10);

	*connection = (struct ftp_connection*)malloc(sizeof(struct ftp_connection));
	conn = *connection;

	ZeroMemory(conn, sizeof(struct ftp_connection));
	conn->passive_socket = SOCKET_ERROR;
	conn->hints.ai_family = AF_INET;
	conn->hints.ai_protocol = IPPROTO_TCP;
	conn->hints.ai_socktype = SOCK_STREAM;

	result = getaddrinfo(hostname, port_str, &conn->hints, &conn->addr);
	if(result != 0) {
		return HOST_NFOUND;
	}

	conn->socket = socket(conn->hints.ai_family, conn->hints.ai_socktype, conn->hints.ai_protocol);
	if (conn->socket == INVALID_SOCKET)
		return SOCKET_CREATE;

	result = connect(conn->socket, conn->addr->ai_addr, (int)conn->addr->ai_addrlen);
	if(result == SOCKET_ERROR) {
		return CONNECT_ERROR;
	}

	return 0;
}

int ftp_hello(struct ftp_connection *connection, char **hello) {
	char	buffer[256];
	int		recvd = -1;
	
	recvd = recv(connection->socket, buffer, 256, 0);
	if (recvd <= 0)
		return HELLO_NRECVD;
	
	*hello = (char*)malloc(recvd + 1);
	memcpy(*hello, buffer, recvd);
	(*hello)[recvd] = '\0';

	return 0;
}

int ftp_login(struct ftp_connection *connection, char *username, char *password) {
	char	*user;
	char	*pass;
	char	buffer[BUFFER_SIZE];
	int		recvd;

	user = (username != NULL) ? username : ANON_USERNAME;
	pass = (password != NULL) ? password : ANON_PASSWORD;

	send(connection->socket, "USER ", 5, 0);
	send(connection->socket, user, (int) strlen(user), 0);
	send(connection->socket, "\r\n", 2, 0);

	recvd = recv(connection->socket, buffer, BUFFER_SIZE - 1, 0);
	buffer[recvd] = '\0';

	if (strstr(buffer, "331") == NULL)
		return FTP_PROTO_LOGIN;

	send(connection->socket, "PASS ", 5, 0);
	send(connection->socket, pass, (int) strlen(pass), 0);
	send(connection->socket, "\r\n", 2, 0);

	recvd = recv(connection->socket, buffer, BUFFER_SIZE - 1, 0);
	buffer[recvd] = '\0';

	if (strstr(buffer, "230") == NULL)
		return FTP_PROTO_LOGIN;

	return 0;
}

void _resolve_pasv_response(char *pasv, char *addr, int *port) {
	char	*addr_start;
	char	*token;

	addr_start = strstr(pasv, "(") + 1;
	token = strtok(addr_start, ",");

	addr[0] = '\0';
	for (int i = 0; i < 4; i++) {
		strcat(addr, token);

		if (i < 3)
			strcat(addr, ".");

		token = strtok(NULL, ",");
	}

	*port += atoi(token) * 256;
	token = strtok(NULL, ",");
	*port += atoi(token);
}

int ftp_passive(struct ftp_connection *connection, char *pasv_response) {
	int					result = 0;
	char				addr[20];
	int					port = 0;
	struct sockaddr_in	sock_addr = { 0 };

	_resolve_pasv_response(pasv_response, addr, &port);
	
	inet_pton(connection->hints.ai_family, addr, &(sock_addr.sin_addr.S_un.S_addr));
	sock_addr.sin_family = connection->hints.ai_family;
	sock_addr.sin_port = htons((short)port);

	if (connection->passive_socket != SOCKET_ERROR)
		closesocket(connection->passive_socket);

	connection->passive_socket = socket(connection->hints.ai_family, connection->hints.ai_socktype, connection->hints.ai_protocol);
	if (connection->passive_socket == INVALID_SOCKET)
		return SOCKET_CREATE;

	result = connect(connection->passive_socket, (struct sockaddr*) &sock_addr, sizeof(sock_addr));
	if (result == SOCKET_ERROR)
		return CONNECT_ERROR;

	return 0;
}

DWORD WINAPI ftp_pasv_recv(void *args) {
	HANDLE					event;
	struct ftp_connection	*connection;
	FILE					*data_output;
	int						recvd = 0;
	char					buffer[BUFFER_SIZE];

	connection = (struct ftp_connection*) ((void**)args)[0];
	data_output = (FILE*)((void**)args)[1];

	event = WSACreateEvent();
	WSAEventSelect(connection->passive_socket, event, FD_READ | FD_CLOSE);

	if (WaitForSingleObject(event, 2500) == WAIT_OBJECT_0) {
		while(1) {
			recvd = recv(connection->passive_socket, buffer, BUFFER_SIZE - 1, 0);
			if (recvd > 0) {
				buffer[recvd] = '\0';
				fprintf(data_output, buffer);
			}

			if (recvd != BUFFER_SIZE - 1)
				break;

			if(recvd == -1) {
				if (send(connection->passive_socket, "\0", 1, 0) == -1)
					break;
			}
		}
	}
	else
		printf("passive timeout\n");

	WSACloseEvent(event);
	return 0;
}

int ftp_send_cmd(struct ftp_connection *connection, char *cmd, FILE *cmd_output, FILE *data_output, char cut_newline) {
	HANDLE	thread;
	void	*thread_args[2];
	char	*output = NULL;
	char	buffer[BUFFER_SIZE];
	int		recvd = 0;
	HANDLE  event = WSACreateEvent();
	int		len;

	len = (int)strlen(cmd);
	len -= (cut_newline) ? 1 : 0;

	send(connection->socket, cmd, len, 0);
	send(connection->socket, "\r\n", 2, 0);

	WSAEventSelect(connection->socket, event, FD_READ | FD_CLOSE);
	if (WaitForSingleObject(event, 2500) == WAIT_OBJECT_0) {
		while(1) {
			recvd = recv(connection->socket, buffer, BUFFER_SIZE - 1, 0);
			if(recvd > 0) {
				buffer[recvd] = '\0';
				append_string(&output, buffer);

				if (buffer[recvd - 2] == '\r' && buffer[recvd - 1] == '\n')
					break;
			}
		}

		fprintf(cmd_output, output);

		if (strncmp(output, "227", 3) == 0) {
			ftp_passive(connection, output);
		}

		else if(strncmp(output, "150", 3) == 0) {
			thread_args[0] = (void*)connection;
			thread_args[1] = (void*)data_output;

			thread = CreateThread(NULL, 0, ftp_pasv_recv, thread_args, 0, NULL);
			if(thread) {
				if(WaitForSingleObject(event, INFINITE) == WAIT_OBJECT_0) {
					SAFE_FREE(output);

					while (1) {
						recvd = recv(connection->socket, buffer, BUFFER_SIZE - 1, 0);
						if (recvd > 0) {
							buffer[recvd] = '\0';
							append_string(&output, buffer);

							if (buffer[recvd - 2] == '\r' && buffer[recvd - 1] == '\n')
								break;
						}
					}
					fprintf(cmd_output, output);
				}
			}
		}
		SAFE_FREE(output);
	}
	else
		printf("timeout\n");

	WSACloseEvent(event);
	return 0;
}

int ftp_retr(struct ftp_connection* connection, char* remote_path, char *local_path) {
	char			buffer[BUFFER_SIZE];
	char			s_file_size[64];
	unsigned long	file_size;
	unsigned long	offset = 0;
	FILE			*tmp_file;
	FILE			*output;
	HANDLE			event, data_event;
	int				recvd;
	char			*cmd_output = NULL;

	output = fopen(local_path, "w");
	if(output == NULL) {
		printf("Cannot open '%s' to write\n", local_path);
		return -1;
	}

	event = WSACreateEvent();
	tmp_file = tmpfile();

	ftp_send_cmd(connection, "type i", stdout, stdout, 0);
	send(connection->socket, "size ", 5, 0);
	ftp_send_cmd(connection, remote_path, tmp_file, tmp_file, 0);

	rewind(tmp_file);
	fgets(s_file_size, 64, tmp_file);
	fclose(tmp_file);
	s_file_size[strlen(s_file_size) - 2] = '\0';
	file_size = atol(s_file_size + 4);

	printf("Downloading file '%s' size: %lu bytes\n", remote_path, file_size);

	ftp_send_cmd(connection, "PASV", stdout, stdout, 0);
	send(connection->socket, "retr ", 5, 0);
	send(connection->socket, remote_path, (int) strlen(remote_path), 0);
	send(connection->socket, "\r\n", 2, 0);

	WSAEventSelect(connection->socket, event, FD_READ | FD_CLOSE);
	if (WaitForSingleObject(event, 2500) == WAIT_OBJECT_0) {
		while (1) {
			recvd = recv(connection->socket, buffer, BUFFER_SIZE - 1, 0);
			if (recvd > 0) {
				buffer[recvd] = '\0';
				append_string(&cmd_output, buffer);

				if (buffer[recvd - 2] == '\r' && buffer[recvd - 1] == '\n')
					break;
			}
		}
		
		if(strncmp(buffer, "150", 3) == 0) {
			data_event = WSACreateEvent();
			WSAEventSelect(connection->passive_socket, data_event, FD_READ | FD_CLOSE);

			if (WaitForSingleObject(data_event, 2500) == WAIT_OBJECT_0) {
				while (offset < file_size) {
					recvd = recv(connection->passive_socket, buffer, BUFFER_SIZE, 0);

					if (recvd > 0) {
						offset += recvd;
						fwrite(buffer, sizeof(char), recvd, output);
					}
				}

				printf("File download complete\n");
			}
			else
				printf("Data timeout\n");

			WSACloseEvent(data_event);
		}
		else {
			printf("Server unexpected response while downloading file\n");
		}
	}
	else
		printf("timeout\n");

	fclose(output);
	WSACloseEvent(event);
	return 0;
}