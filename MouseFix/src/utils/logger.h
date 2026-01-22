#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <windows.h>

// Constants
#define LOGGER_MAX_ERRORS 32
#define LOGGER_LOG_PATH_SIZE 512

// Log levels
typedef enum
{
	LOG_LEVEL_DEBUG = 0,
	LOG_LEVEL_INFO,
	LOG_LEVEL_WARNING,
	LOG_LEVEL_ERROR,
	LOG_LEVEL_CRITICAL
} LogLevel;

// Logger structure
typedef struct
{
	LogLevel level;
	FILE *file;
	bool console_output;
	bool file_output;
	char log_path[LOGGER_LOG_PATH_SIZE];
	bool initialized;
	CRITICAL_SECTION cs; // Thread synchronization
} Logger;

// Initialize logger
bool logger_init(Logger *logger, LogLevel level, const char *log_path);

// Cleanup logger
void logger_cleanup(Logger *logger);

// Log a message
void logger_log(Logger *logger, LogLevel level, const char *file, int line, const char *format, ...);

// Flush log buffer
void logger_flush(Logger *logger);

// Set log level
void logger_set_level(const Logger *logger, LogLevel new_level);

// Check if log level is enabled
bool logger_is_enabled(const Logger *logger, LogLevel check_level);

// Convenience macros
#ifdef NDEBUG
// Release mode: disable all logging for performance and simplicity
#define LOG_DEBUG(logger, ...) ((void)0)
#define LOG_INFO(logger, ...) ((void)0)
#define LOG_WARNING(logger, ...) ((void)0)
#define LOG_ERROR(logger, ...) ((void)0)
#define LOG_CRITICAL(logger, ...) ((void)0)
#else
// Debug mode: enable logging
#define LOG_DEBUG(logger, ...) logger_log(logger, LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(logger, ...) logger_log(logger, LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARNING(logger, ...) logger_log(logger, LOG_LEVEL_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(logger, ...) logger_log(logger, LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_CRITICAL(logger, ...) logger_log(logger, LOG_LEVEL_CRITICAL, __FILE__, __LINE__, __VA_ARGS__)
#endif