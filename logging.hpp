#ifndef MCP_BASE_LOGGING_HEADER
#define MCP_BASE_LOGGING_HEADER

// TODO
// * log thread
// * log time and data
// * allow for different log writers (e.g. one for INFO, other for ERROR)
// * implement writer to debug monitor
// * implement writer to log to stdout
// * check if log may skip testing singleton pointer

#include "log_message.hpp"

// A logging system that accepts streaming values.
//
// Usage:
//   LOG(LogMessage::NORMAL) << "This will appear in the log";
//   LOG(LogMessage::WARNING) << "Value of XYZ is now " << xyz;
//
#define LOG(severity) \
   LogMessage(__FILE__, __LINE__, severity).stream()

using base::LogMessage;

#endif // MCP_BASE_LOGGING_HEADER
