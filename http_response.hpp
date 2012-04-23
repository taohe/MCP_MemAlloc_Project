#ifndef MCP_HTTP_RESPONSE_HEADER
#define MCP_HTTP_RESPONSE_HEADER

#include <string>

namespace http {

using std::string;

struct Response {
  string statusLine;
  string headerRemainder;
  string body;

  void clear() { statusLine.clear(); headerRemainder.clear();  body.clear(); }
};

} // namespace http

#endif // MCP_HTTP_RESPONSE_HEADER
