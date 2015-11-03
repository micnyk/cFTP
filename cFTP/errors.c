#include <math.h>
#include "errors.h"

static const char *errors[] = {
	"Unknown error",
	"Invalid format of hostname and port argument, should be <hostname:port>",
	"Hostname not found",
	"Cannot create TCP socket",
	"Cannot connect to host",
	"Server hello message not received",
	"Username or password invalid",
	"Socket timeout",
	"Cannot open local file",
	"Server unexpected response",
};
int errors_len = 10;

const char *error_desc(int error) {
	if (error < errors_len)
		return errors[error];

	return errors[0];
}