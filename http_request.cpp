#include "buffer.hpp"
#include "http_request.hpp"

namespace http {

using base::Buffer;

void Request::clear() {
  method.clear();
  address.clear();
  version.clear();
}

void Request::cloneFrom(const Request& other) {
  method = other.method;
  address = other.address;
  version = other.version;
}

void Request::toBuffer(Buffer* out) const {
  out->write(method.c_str());
  out->write(" ");
  out->write(address.c_str());
  out->write(" ");
  out->write(version.c_str());
  out->write("\r\n\r\n");
}

}  // namespace http
