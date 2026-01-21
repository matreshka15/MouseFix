#define _CRT_SECURE_NO_WARNINGS
#include "error_handler.h"
#include <string.h>

static const char* ERROR_MESSAGES[] = {
	"Success",
	"Invalid configuration",
	"Failed to install mouse hook",
	"Failed to uninstall mouse hook",
	"Failed to load configuration",
	"Failed to save configuration",
	"Out of memory",
	"Permission denied",
	"Invalid argument",
	"File not found",
	"Unknown error"
};

// Initialize error handler
bool error_handler_init(ErrorHandler* handler)
{
	if (!handler)
		return false;

	memset(handler, 0, sizeof(ErrorHandler));
	handler->initialized = true;

	return true;
}

// Cleanup error handler
void error_handler_cleanup(ErrorHandler* handler)
{
	if (!handler)
		return;

	error_handler_clear(handler);
	handler->initialized = false;
}

// Report an error
void error_handler_report(ErrorHandler* handler, ErrorCode code, const char* message, const char* file, int line)
{
	if (!handler || !handler->initialized)
		return;

	// Add error to the list
	if (handler->error_count < 32)
	{
		Error* error = &handler->errors[handler->error_count];
		error->code = code;

		if (message)
		{
			strncpy(error->message, message, sizeof(error->message) - 1);
			error->message[sizeof(error->message) - 1] = '\0';
		}
		else
		{
			strncpy(error->message, ERROR_MESSAGES[code], sizeof(error->message) - 1);
			error->message[sizeof(error->message) - 1] = '\0';
		}

		error->file = file;
		error->line = line;

		handler->error_count++;
	}

	// Call callback if set
	if (handler->callback)
	{
		handler->callback(&handler->errors[handler->error_count - 1], handler->user_data);
	}
}

// Get last error
const Error* error_handler_get_last_error(const ErrorHandler* handler)
{
	if (!handler || !handler->initialized || handler->error_count == 0)
		return NULL;

	return &handler->errors[handler->error_count - 1];
}

// Clear all errors
void error_handler_clear(ErrorHandler* handler)
{
	if (!handler)
		return;

	handler->error_count = 0;
	memset(handler->errors, 0, sizeof(handler->errors));
}

// Set error callback
void error_handler_set_callback(ErrorHandler* handler, ErrorCallback callback, void* user_data)
{
	if (!handler)
		return;

	handler->callback = callback;
	handler->user_data = user_data;
}

// Get error message from error code
const char* error_handler_get_message(ErrorCode code)
{
	if (code >= 0 && code < ERR_UNKNOWN)
	{
		return ERROR_MESSAGES[code];
	}
	return ERROR_MESSAGES[ERR_UNKNOWN];
}