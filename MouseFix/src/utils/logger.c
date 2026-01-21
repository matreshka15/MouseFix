#define _CRT_SECURE_NO_WARNINGS
#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

static const char* LOG_LEVEL_NAMES[] = {
	"DEBUG",
	"INFO",
	"WARNING",
	"ERROR",
	"CRITICAL"
};

// Initialize logger
bool logger_init(Logger* logger, LogLevel level, const char* log_path)
{
	if (!logger)
		return false;

	memset(logger, 0, sizeof(Logger));
	logger->level = level;
	logger->console_output = false;  // Disable console output for user experience
	logger->file_output = false;
	logger->initialized = true;

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
	(void)log_path;  // Suppress unused parameter warning
#endif

	return true;
}

// Cleanup logger
void logger_cleanup(Logger* logger)
{
	if (!logger)
		return;

	if (logger->file)
	{
		fclose(logger->file);
		logger->file = NULL;
	}

	logger->initialized = false;
}

// Log a message
void logger_log(Logger* logger, LogLevel level, const char* file, int line, const char* format, ...)
{
	if (!logger || !logger->initialized)
		return;

	if (level < logger->level)
		return;

	// Get current time
	time_t now = time(NULL);
	struct tm* tm_info = localtime(&now);
	char time_str[32];
	strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

	// Format the message
	char message[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	// Format the full log line
	char log_line[2048];
	snprintf(log_line, sizeof(log_line),
	         "[%s] [%s] [%s:%d] %s\n",
	         time_str,
	         LOG_LEVEL_NAMES[level],
	         file,
	         line,
	         message);

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
}

// Flush log buffer
void logger_flush(Logger* logger)
{
	if (!logger)
		return;

	if (logger->file)
	{
		fflush(logger->file);
	}

	fflush(stdout);
}

// Set log level
void logger_set_level(Logger* logger, LogLevel level)
{
	if (logger)
	{
		logger->level = level;
	}
}

// Check if log level is enabled
bool logger_is_enabled(Logger* logger, LogLevel level)
{
	if (!logger || !logger->initialized)
		return false;

	return level >= logger->level;
}