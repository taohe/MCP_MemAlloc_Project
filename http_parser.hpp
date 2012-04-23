#ifndef MCP_HTTP_PARSER_HEADER
#define MCP_HTTP_PARSER_HEADER

#include <string>

#include "buffer.hpp"

namespace http {

using std::string;
using base::Buffer;

class Request;
class Response;

class Parser {
public:
  // -1 error, 0 parsed, + insufficient input
  static int parseRequest(Buffer::Iterator* in, Request* request);
  static int parseResponse(Buffer::Iterator* in, Response* response);

private:
  struct IncompleteInput {};
  struct ErrorInRequest {};

  static void parseString(Buffer::Iterator* in, string* res);
  static void parseLine(Buffer::Iterator* in, string* res);
  static void skipChar(Buffer::Iterator* in, char skip);
  static void skipNewLine(Buffer::Iterator* in);
  static bool isEmptyLine(string line);

  Parser() {}
  ~Parser() {}
};

} // namespace http

#endif // MCP_HTTP_PARSER_HEADER
