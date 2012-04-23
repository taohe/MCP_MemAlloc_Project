#include <string>

#include "buffer.hpp"
#include "http_parser.hpp"
#include "http_request.hpp"
#include "test_unit.hpp"

namespace {

using std::string;

using base::Buffer;
using http::Parser;
using http::Request;

TEST(SingleRequest, CompleteAndValid) {
  string method("GET");
  string address("x.html");
  string version("HTTP/1.1");
  string start_line(method + " /" + address + " " + version + "\r\n");
  Buffer buf;
  buf.write(start_line.c_str());
  buf.write("User-Agent: httperf/0.9.0\r\n");
  buf.write("Host: localhost\r\n");
  buf.write("\r\n");

  Request req;
  Buffer::Iterator it(buf.begin());
  EXPECT_EQ(Parser::parseRequest(&it, &req), 0);  // parse successful
  EXPECT_EQ(req.method, method);
  EXPECT_EQ(req.address, address);
  EXPECT_EQ(req.version, version);
  EXPECT_TRUE(it.eob());
}

TEST(SingleRequest, Invalid) {
  string request1("GET x\r\n\r\n");
  Buffer buf;
  buf.write(request1.c_str());

  Request req;
  Buffer::Iterator it(buf.begin());
  EXPECT_EQ(Parser::parseRequest(&it, &req), -1); // error parsing
}

TEST(SingleRequest, Incomplete) {
  string method("GET");
  string address("x.html");
  string version("HTTP/1.1");
  string start_line(method + " /" + address + " " + version + "\r\n");
  Buffer buf;
  buf.write(start_line.c_str());
  buf.write("User-Agent: httperf/0.9.0\r\n");

  Request req;
  Buffer::Iterator it(buf.begin());
  EXPECT_EQ(Parser::parseRequest(&it, &req), +1);  // incomplete parsing

  req.clear();
  buf.write("Host: localhost\r\n");
  buf.write("\r\n");
  buf.write("ABC"); // start of the next request
  Buffer::Iterator another_it(buf.begin());
  EXPECT_EQ(Parser::parseRequest(&another_it, &req), 0);  // parse successful
  EXPECT_EQ(req.method, method);
  EXPECT_EQ(req.address, address);
  EXPECT_EQ(req.version, version);
  EXPECT_FALSE(another_it.eob());

  buf.consume(another_it.bytesRead());
  string read_string(buf.readPtr(), buf.readSize());
  EXPECT_EQ(read_string, "ABC");
}

} // unnamed namespace

int main(int argc, char *argv[]) {
  return RUN_TESTS(argc, argv);
}
