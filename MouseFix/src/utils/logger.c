#define _CRT_SECURE_NO_WARNINGS
#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <windows.h>

// Constants
#define LOG_TIME_BUFFER_SIZE 32
#define LOG_MESSAGE_BUFFER_SIZE 1024
#define LOG_LINE_BUFFER_SIZE 2048

static const char *LOG_LEVEL_NAMES[] = {
	"DEBUG",
	"INFO",
	"WARNING",
	"ERROR",
	"CRITICAL"};

// Initialize logger with specified level and log file path
// Parameters:
//   logger - Pointer to Logger structure to initialize
//   level - Minimum log level to record (messages below this level are ignored)
//   log_path - Path to log file (NULL or empty string to disable file logging)
// Returns:
//   true on success, false on failure
bool logger_init(Logger *logger, LogLevel level, const char *log_path)
{
	if (!logger)
		return false;

	memset(logger, 0, sizeof(Logger));
	logger->level = level;
	logger->console_output = false; // Disable console output for user experience
	logger->file_output = false;
	logger->initialized = true;

	// Initialize critical section for thread safety
	InitializeCriticalSection(&logger->cs);

#ifndef NDEBUG
	// Only create log file in Debug mode
	if (log_path && strlen(log_path) > 0)
	{
		strncpy(logger->log_path, log_path, sizeof(logger->log_path) - 1);
		logger->log_path[sizeof(logger->log_path) - 1] = '\0';

		logger->file = fopen(log_path, "a");
		if (logger->file)
		{
			logger->file_output = true;
		}
	}
#else
	// Release mode: don't create log files
	(void)log_path; // Suppress unused parameter warning
#endif

	return true;
}

// Cleanup and release logger resources
// Parameters:
//   logger - Pointer to Logger structure to cleanup
void logger_cleanup(Logger *logger)
{
	if (!logger)
		return;

	if (logger->file)
	{
		fclose(logger->file);
		logger->file = NULL;
	}

	DeleteCriticalSection(&logger->cs);
	logger->initialized = false;
}

// Log a message with specified level
// Parameters:
//   logger - Pointer to Logger structure
//   level - Log level of this message
//   file - Source file name where log was called
//   line - Line number where log was called
//   format - Printf-style format string
//   ... - Variable arguments for format string
void logger_log(Logger *logger, LogLevel level, const char *file, int line, const char *format, ...)
{
	if (!logger || !logger->initialized)
		return;

	if (level < logger->level)
		return;

	// Get current time
	time_t now = time(NULL);
	struct tm *tm_info = localtime(&now);
	char time_str[LOG_TIME_BUFFER_SIZE];
	strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

	// Format the message
	char message[LOG_MESSAGE_BUFFER_SIZE];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	// Format the full log line
	char log_line[LOG_LINE_BUFFER_SIZE];
	snprintf(log_line, sizeof(log_line),
			 "[%s] [%s] [%s:%d] %s\n",
			 time_str,
			 LOG_LEVEL_NAMES[level],
			 file,
			 line,
			 message);

	// Enter critical section for thread-safe logging
	EnterCriticalSection(&logger->cs);

	// Output to console
	if (logger->console_output)
	{
		printf("%s", log_line);
		fflush(stdout);
	}

	// Output to file
	if (logger->file_output && logger->file)
	{
		fprintf(logger->file, "%s", log_line);
		fflush(logger->file);
	}

	// Leave critical section
	LeaveCriticalSection(&logger->cs);
}

// Flush log buffer to ensure all messages are written
// Parameters:
//   logger - Pointer to Logger structure
void logger_flush(Logger *logger)
{
	if (!logger)
		return;

	EnterCriticalSection(&logger->cs);

	if (logger->file)
	{
		fflush(logger->file);
	}

	fflush(stdout);

	LeaveCriticalSection(&logger->cs);
}

// Set minimum log level
// Parameters:
//   logger - Pointer to Logger structure
//   new_level - New minimum log level to set
void logger_set_level(const Logger *logger, LogLevel new_level)
{
	if (logger)
	{
		// Note: We need to cast away const to modify the level
		// This is safe because we're only modifying the level field
		((Logger *)logger)->level = new_level;
	}
}

// Check if a log level is currently enabled
// Parameters:
//   logger - Pointer to Logger structure
//   check_level - Log level to check
// Returns:
//   true if the level is enabled, false otherwise
bool logger_is_enabled(const Logger *logger, LogLevel check_level)
{
	if (!logger || !logger->initialized)
		return false;

	return check_level >= logger->level;
}