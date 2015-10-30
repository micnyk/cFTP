#pragma once

#define SAFE_FREE(ptr)  \
	if(ptr != NULL) {   \
		free(ptr);		\
		ptr = NULL;		\
	}

enum CFTP_ERRORS
{
	HOST_PORT_INVALID_FORMAT = 1,
	HOST_NFOUND = 2,
	SOCKET_CREATE = 3,
	CONNECT_ERROR = 4,
	HELLO_NRECVD = 5,
	FTP_PROTO_LOGIN = 6,
	FTP_PROTO_QUIT = 7,
};

const char *error_desc(int error);

#define HANDLE_ERROR(v) \
	printf("Error: %s\n", error_desc(v)); \
	goto exit

#define CHECK_ERROR(v) \
	if(v != 0) { \
		HANDLE_ERROR(v); \
	}
