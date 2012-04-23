#include <iostream>
#include <sstream>

#include "acceptor.hpp"
#include "http_service.hpp"

using base::AcceptCallback;
using base::IOService;
using base::makeCallableMany;
using http::HTTPService;

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cout << "Usage: " << argv[0] << " <port> <num-threads>" << std::endl;
    return 1;
  }

  // Parse service ports.
  int http_port;
  std::istringstream port_stream(argv[1]);
  port_stream >> http_port;

  // Parse number of threads in IOService.
  int num_workers;
  std::istringstream thread_stream(argv[2]);
  thread_stream >> num_workers;

  // Setup the protocols. The HTTP server accepts requests to stop the
  // IOService machinery and requests for its stats.
  IOService io_service(num_workers);
  HTTPService http_service(http_port, &io_service);

  // Loop until IOService is stopped via /quit request against the
  // HTTP Service.
  io_service.start();

  return 0;
}
