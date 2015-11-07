#pragma once

#define SAFE_FREE(ptr)  \
	if(ptr != NULL) {   \
		free(ptr);		\
		ptr = NULL;		\
	}

enum CFTP_ERRORS
{
	ERR_HOSTPORT_INVALID_FORMAT = 1,
	ERR_NET_HOST_NOT_FOUND = 2,
	ERR_NET_SOCK_CREATE = 3,
	ERR_NET_CONNECT = 4,
	ERR_FTP_HELLO_NOT_RECVD = 5,
	ERR_FTP_LOGIN = 6,
	ERR_NET_SOCK_TIMEOUT = 7,
	ERR_IO_FILE_OPEN = 8,
	ERR_FTP_UNEXPECTED = 9,
	ERR_FTP_FILE_NO_EXISTS = 10
};

const char *error_desc(int error);

#define HANDLE_ERROR(v) \
	printf("Error: %s\n", error_desc(v)); \
	goto exit

#define CHECK_ERROR(v) \
	if(v != 0) { \
		HANDLE_ERROR(v); \
	}
