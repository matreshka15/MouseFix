#pragma once

#include <stdbool.h>

// Error codes
typedef enum
{
	ERR_SUCCESS = 0,
	ERR_INVALID_CONFIG,
	ERR_HOOK_INSTALL_FAILED,
	ERR_HOOK_UNINSTALL_FAILED,
	ERR_CONFIG_LOAD_FAILED,
	ERR_CONFIG_SAVE_FAILED,
	ERR_OUT_OF_MEMORY,
	ERR_PERMISSION_DENIED,
	ERR_INVALID_ARGUMENT,
	ERR_FILE_NOT_FOUND,
	ERR_UNKNOWN
} ErrorCode;

// Error structure
typedef struct
{
	ErrorCode code;
	char message[512];
	const char* file;
	int line;
} Error;

// Error callback function type
typedef void (*ErrorCallback)(const Error* error, void* user_data);

// Error handler structure
typedef struct
{
	Error errors[32];
	int error_count;
	ErrorCallback callback;
	void* user_data;
	bool initialized;
} ErrorHandler;

// Initialize error handler
bool error_handler_init(ErrorHandler* handler);

// Cleanup error handler
void error_handler_cleanup(ErrorHandler* handler);

// Report an error
void error_handler_report(ErrorHandler* handler, ErrorCode code, const char* message, const char* file, int line);

// Get last error
const Error* error_handler_get_last_error(const ErrorHandler* handler);

// Clear all errors
void error_handler_clear(ErrorHandler* handler);

// Set error callback
void error_handler_set_callback(ErrorHandler* handler, ErrorCallback callback, void* user_data);

// Get error message from error code
const char* error_handler_get_message(ErrorCode code);

// Convenience macros
#define REPORT_ERROR(handler, code, message) error_handler_report(handler, code, message, __FILE__, __LINE__)
#define CHECK_ERROR(condition, handler, code, message) \
	do { \
		if (!(condition)) { \
			REPORT_ERROR(handler, code, message); \
		} \
	} while(0)
