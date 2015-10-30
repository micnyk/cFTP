#include <math.h>
#include "errors.h"

static const char *errors[] = {
	"Unknown error",
	"Invalid format of hostname argument, should be <hostname:port>",
	"Hostname not found",
	"Cannot create TCP socket",
	"Cannot connect to host",
	"Cannot receive server hello message",
	"FTP protocol error: cannot login",
	"FTP protocol error: quit",
};
int errors_len = 8;

const char *error_desc(int error) {
	if (error < errors_len)
		return errors[error];

	return errors[0];
}