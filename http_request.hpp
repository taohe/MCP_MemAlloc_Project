#ifndef MCP_HTTP_REQUEST_HEADER
#define MCP_HTTP_REQUEST_HEADER

#include <string>

namespace base {
class Buffer;
}

namespace http {

using std::string;
using base::Buffer;

struct Request {
  string method;
  string address;
  string version;

  void clear();
  void cloneFrom(const Request& other);
  void toBuffer(Buffer* out) const;
};

} // namespace http

#endif // MCP_HTTP_REQUEST_HEADER
