#include "debug.h"

#include <WS2tcpip.h>
#include <stdio.h>

#include "ftp.h"
#include "errors.h"

#define BUFFER_SIZE 2048
#define DEFAULT_TIMEOUT 2500

struct WSAData		wsa_data;
const char			*ANON_USERNAME = "anonymous";
const char			*ANON_PASSWORD = "cft@c.com";

// Append string to another string
void _append_string(char **output, char *append) {
	char	*tmp;
	size_t	app_len = strlen(append);
	size_t	str_len = 0;
	size_t  tmp_len = 0;

	if (*output == NULL) { // If output string is NULL
		*output = (char*)malloc(app_len + 1); // Allocate memory
		strcpy(*output, append); // Copy the append string to output
	}
	else {
		str_len = strlen(*output); 
		tmp_len = str_len + app_len + 1;

		tmp = (char*)malloc(tmp_len); // Temporary string storing both output and append string
		tmp[0] = '\0';

		// Copy strings to temp string
		strcat(tmp, *output); 
		strcat(tmp, append);

		free(*output);
		*output = (char*)malloc(tmp_len); 
		strcpy(*output, tmp); // Copy temp to output
		free(tmp);
	}
}


// Receive server response
long _recv_response(struct ftp_connection *connection, char **cmd_output) {
	HANDLE		event;
	static char	buffer[BUFFER_SIZE];
	int			recvd;
	long		total = 0;

	// Create socket read event
	event = WSACreateEvent();
	WSAEventSelect(connection->socket, event, FD_READ | FD_CLOSE);

	// Wait for response be ready to read
	if (WaitForSingleObject(event, DEFAULT_TIMEOUT) != WAIT_OBJECT_0) {
		WSACloseEvent(event);
		return -1; // Return -1 if timeout
	}

	// Receive until '\r\n' characters- end of response
	while (1) {
		recvd = recv(connection->socket, buffer, BUFFER_SIZE - 1, 0);

		if (recvd > 0) {
			buffer[recvd] = '\0';
			_append_string(cmd_output, buffer); // Append received fragment to the output string
			total += recvd;

			if (buffer[recvd - 2] == '\r' && buffer[recvd - 1] == '\n') // Check if it is the end of response
				break;
		}
	}

	WSACloseEvent(event);
	return total;
}

// Resolves the server's response to PASV command to find out given IP address and port number
// PASV response has format '227 [...] (a, b, c, d, e, f)' where:
// IP address is a.b.c.d
// Port number is e*256+f
void _resolve_pasv_response(char *pasv, char *addr, int *port) {
	char	*addr_start;
	char	*token;

	addr[0] = '\0';
	// Find position of the address sequence
	addr_start = strstr(pasv, "(") + 1;

	// Split the string using ',' delimiter
	token = strtok(addr_start, ",");
	// First 4 tokens are parts of IP address
	for (int i = 0; i < 4; i++) {
		// Append address with current token
		strcat(addr, token);

		// Add '.' if it is not the last part of the address
		if (i < 3)
			strcat(addr, ".");

		token = strtok(NULL, ",");
	}

	// The next token is the first part of port number sequence
	*port += atoi(token) * 256;
	token = strtok(NULL, ",");
	*port += atoi(token);
}

// Initialize Windows Sockets ver. 2.2
void ftp_init(void) {
	WSAStartup(MAKEWORD(2, 2), &wsa_data);
}

void ftp_cleanup(void) {
	WSACleanup();
}

// Disconnect and release all resources used to establish connection
void ftp_disconnect(struct ftp_connection *connection) {

	if (connection == NULL)
		return;

	if (connection->socket != INVALID_SOCKET)
		closesocket(connection->socket);

	if (connection->addr != NULL)
		freeaddrinfo(connection->addr);

	free(connection);
	connection = NULL;
}

// Connect to server
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

	// Resolve hostname
	result = getaddrinfo(hostname, port_str, &conn->hints, &conn->addr);
	if (result != 0) {
		return ERR_NET_HOST_NOT_FOUND;
	}

	// Create socket
	conn->socket = socket(conn->hints.ai_family, conn->hints.ai_socktype, conn->hints.ai_protocol);
	if (conn->socket == INVALID_SOCKET)
		return ERR_NET_SOCK_CREATE;

	// Try to connect
	result = connect(conn->socket, conn->addr->ai_addr, (int)conn->addr->ai_addrlen);
	if (result == SOCKET_ERROR) {
		return ERR_NET_CONNECT;
	}

	return 0;
}

// Receive server hello message
int ftp_hello(struct ftp_connection *connection, char **hello) {
	// Receive the hello message
	if (_recv_response(connection, hello) <= 0)
		return ERR_FTP_HELLO_NOT_RECVD;  

	// Hello message should start with '220'
	if (strncmp(*hello, "220", 3) != 0)
		return ERR_FTP_HELLO_NOT_RECVD;

	return 0;
}

// Login using given username and password or as anonymous user
int ftp_login(struct ftp_connection *connection, char *username, char *password) {
	char	*user;
	char	*pass;
	char	*response = NULL;

	// Check if username and password are given, if not use anonymous
	user = (username != NULL) ? username : ANON_USERNAME;
	pass = (password != NULL) ? password : ANON_PASSWORD;

	// Send username with USER command
	send(connection->socket, "USER ", 5, 0);
	send(connection->socket, user, (int)strlen(user), 0);
	send(connection->socket, "\r\n", 2, 0);

	// Receive the response
	if(_recv_response(connection, &response) <= 0) {
		SAFE_FREE(response);
		return ERR_FTP_LOGIN;
	}

	// Should start with '331', if not- something went wrong
	if (strncmp(response, "331", 3) != 0) {
		SAFE_FREE(response);
		return ERR_FTP_LOGIN;
	}

	SAFE_FREE(response);

	// Send password with PASS command and receive response
	send(connection->socket, "PASS ", 5, 0);
	send(connection->socket, pass, (int)strlen(pass), 0);
	send(connection->socket, "\r\n", 2, 0);

	if (_recv_response(connection, &response) <= 0) {
		SAFE_FREE(response);
		return ERR_FTP_LOGIN;
	}

	// Response '230' means successfull login
	if (strncmp(response, "230", 3) != 0) {
		SAFE_FREE(response);
		return ERR_FTP_LOGIN;
	}

	SAFE_FREE(response);
	return 0;
}

// Connects to the passive mode socket
int ftp_passive(struct ftp_connection *connection, char *pasv_response) {
	int					result = 0;
	char				addr[20];
	int					port = 0;
	struct sockaddr_in	sock_addr = { 0 };

	// Resolve PASV command response to get IP address and port number
	_resolve_pasv_response(pasv_response, addr, &port);

	// Convert IP address to binary representation
	inet_pton(connection->hints.ai_family, addr, &(sock_addr.sin_addr.S_un.S_addr));
	sock_addr.sin_family = connection->hints.ai_family;
	sock_addr.sin_port = htons((short)port);

	if (connection->passive_socket != SOCKET_ERROR)
		closesocket(connection->passive_socket);

connection->passive_socket = socket(connection->hints.ai_family, connection->hints.ai_socktype, connection->hints.ai_protocol);
if (connection->passive_socket == INVALID_SOCKET)
return ERR_NET_SOCK_CREATE;

// Try to connect
result = connect(connection->passive_socket, (struct sockaddr*) &sock_addr, sizeof(sock_addr));
if (result == SOCKET_ERROR)
return ERR_NET_CONNECT;

return 0;
}

// Receives data in passive mode
// Used when size of data is unknown
DWORD WINAPI _ftp_pasv_recv(void *args) {
	HANDLE					event;
	struct ftp_connection	*connection;
	FILE					*data_output;
	int						recvd = 0;
	char					buffer[BUFFER_SIZE];
	int						attemps = 0;
	static const int		max_attemps = 5;

	// Arguments table should be: [0] - pointer to ftp_connection struct, [1] - pointer to output FILE (stream)
	connection = (struct ftp_connection*) ((void**)args)[0];
	data_output = (FILE*)((void**)args)[1];

	// Event used to wait for data be avaliable to receive
	event = WSACreateEvent();
	WSAEventSelect(connection->passive_socket, event, FD_READ | FD_CLOSE);

	// Wait for data from passive socket
	if (WaitForSingleObject(event, DEFAULT_TIMEOUT) != WAIT_OBJECT_0)
		return ERR_NET_SOCK_TIMEOUT; // Return error if timeout

	while (attemps < max_attemps) {
		recvd = recv(connection->passive_socket, buffer, BUFFER_SIZE, 0);

		// If data was received, write it to the output stream
		if (recvd > 0)
			fwrite(buffer, sizeof(char), recvd, data_output);
		else
			attemps++;
	}

	WSACloseEvent(event);
	return 0;
}

// Sends raw FTP command
int ftp_send_cmd(struct ftp_connection *connection, char *cmd, FILE *cmd_output, FILE *data_output, char cut_newline) {
	HANDLE	thread;
	HANDLE	event;
	HANDLE  handles_array[2];
	DWORD   wait_result;
	void	*thread_args[2];
	int		cmd_len;
	char    *output = NULL;

	// If cutting newline char, substract 1 from command lenght
	cmd_len = (int)strlen(cmd);
	cmd_len -= (cut_newline) ? 1 : 0;

	// Send the command and newline characters
	send(connection->socket, cmd, cmd_len, 0);
	send(connection->socket, "\r\n", 2, 0);

	// Receive response
	if (_recv_response(connection, &output) <= 0) {
		SAFE_FREE(output);
		return ERR_FTP_UNEXPECTED;
	}

	// Print received data to output stream
	fprintf(cmd_output, output);

	// 227- enter passive mode
	if (strncmp(output, "227", 3) == 0) {
		ftp_passive(connection, output);
	}

	// 150- start receiving data in passive mode
	else if (strncmp(output, "150", 3) == 0) {
		event = WSACreateEvent();
		WSAEventSelect(connection->socket, event, FD_READ | FD_CLOSE);

		// Fill thread arguments array
		thread_args[0] = (void*)connection;
		thread_args[1] = (void*)data_output;

		// Create and start passive mode receiving thread
		thread = CreateThread(NULL, 0, _ftp_pasv_recv, thread_args, 0, NULL);
		// If thread is running
		if (thread) {
			SAFE_FREE(output);

			handles_array[0] = thread;
			handles_array[1] = event;

			// Wait for server response or thread finish
			wait_result = WaitForMultipleObjects(2, handles_array, FALSE, INFINITE);

			// If there is a server response, terminate thread
			if (wait_result == WAIT_OBJECT_0 + 1)
				TerminateThread(thread, 0);

			// Receive the response
			if (_recv_response(connection, &output) > 0)
				fprintf(cmd_output, output);
			else
				fprintf(cmd_output, "Transfer completed\n");
		}
		WSACloseEvent(event);
	}
	return 0;
}

// FTP RETR command implementation
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
	int				ret = 0;

	// Try to open local file
	output = fopen(local_path, "w");
	if (output == NULL);
		return ERR_IO_FILE_OPEN;

	event = WSACreateEvent();
	tmp_file = tmpfile();

	// Switch to binary mode
	ftp_send_cmd(connection, "type i", stdout, stdout, 0);
	// Get the size of the file to download, save it to temporary file
	send(connection->socket, "size ", 5, 0);
	ftp_send_cmd(connection, remote_path, tmp_file, tmp_file, 0);

	// Read the file size from temporary file
	rewind(tmp_file);
	fgets(s_file_size, 64, tmp_file);
	fclose(tmp_file);
	// Ignore last the characters ('\r\n')
	s_file_size[strlen(s_file_size) - 2] = '\0';
	// Read the file size (first 4 characters are always '213 '
	file_size = atol(s_file_size + 4);

	// Print the information
	printf("Downloading file '%s' size: %lu bytes\n", remote_path, file_size);

	// Enter passive mode
	ftp_send_cmd(connection, "PASV", stdout, stdout, 0);

	// Send RETR <remote_path> command
	send(connection->socket, "retr ", 5, 0);
	send(connection->socket, remote_path, (int)strlen(remote_path), 0);
	send(connection->socket, "\r\n", 2, 0);

	WSAEventSelect(connection->socket, event, FD_READ | FD_CLOSE);
	if (WaitForSingleObject(event, DEFAULT_TIMEOUT) == WAIT_OBJECT_0) {
		// Receive the response
		while (1) {
			recvd = recv(connection->socket, buffer, BUFFER_SIZE - 1, 0);
			if (recvd > 0) {
				buffer[recvd] = '\0';
				_append_string(&cmd_output, buffer);

				if (buffer[recvd - 2] == '\r' && buffer[recvd - 1] == '\n')
					break;
			}
		}

		// 150- Data is ready to transfer
		if (strncmp(buffer, "150", 3) == 0) {
			data_event = WSACreateEvent();
			WSAEventSelect(connection->passive_socket, data_event, FD_READ | FD_CLOSE);

			if (WaitForSingleObject(data_event, DEFAULT_TIMEOUT) == WAIT_OBJECT_0) {
				// Receive the data
				while (offset < file_size) {
					recvd = recv(connection->passive_socket, buffer, BUFFER_SIZE, 0);

					if (recvd > 0) {
						offset += recvd;
						fwrite(buffer, sizeof(char), recvd, output);
					}
				}

				WSACloseEvent(data_event);
				printf("File download complete\n");
			}
			else
				ret = ERR_NET_SOCK_TIMEOUT;
		}
		else
			ret = ERR_FTP_UNEXPECTED;
	}
	else
		ret = ERR_NET_SOCK_TIMEOUT;

	fclose(output);
	WSACloseEvent(event);

	return ret;
}

int ftp_stor(struct ftp_connection *connection, char *local_path, char *remote_path) {
	HANDLE		event;
	char		buffer[BUFFER_SIZE];
	char		*cmd_output = NULL;
	int			recvd;

	event = WSACreateEvent();
	WSAEventSelect(connection->socket, event, FD_READ | FD_CLOSE);

	ftp_send_cmd(connection, "PASV", stdout, stdout, 0);
	ftp_send_cmd(connection, "TYPE I", stdout, stdout, 0);
		
	send(connection->socket, "STOR ", 5, 0);
	send(connection->socket, remote_path, (int)strlen(remote_path), 0);
	send(connection->socket, "\r\n", 2, 0);

	if (WaitForSingleObject(event, DEFAULT_TIMEOUT) != WAIT_OBJECT_0)
		return ERR_NET_SOCK_TIMEOUT;

	while(1) {
		recvd = recv(connection->socket, buffer, BUFFER_SIZE, 0);

		if(recvd > 0) {
			_append_string(&cmd_output, buffer);
			if (buffer[recvd - 2] == '\r' && buffer[recvd - 1] == '\n')
				break;
		}
	}

	printf(cmd_output);

	SAFE_FREE(cmd_output);
	WSACloseEvent(event);
	return 0;
}