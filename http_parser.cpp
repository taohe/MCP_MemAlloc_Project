#include <sstream>

#include "http_parser.hpp"
#include "http_request.hpp"
#include "http_response.hpp"

namespace http {

using std::istringstream;

int Parser::parseRequest(Buffer::Iterator* in, Request* request) {
  request->clear();

  // GET / HTTP/1.1
  // Host: localhost:15001
  // User-Agent: Mozilla/5.0 (X11; U; Linux x86_64; en-US; rv:1.9.0.13) Gecko/2009080315 Ubuntu/9.04 (jaunty) Firefox/3.0.13
  // Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
  // Accept-Language: en-us,en;q=0.5
  // Accept-Encoding: gzip,deflate
  // Accept-Charset: ISO-8859-1,utf-8;q=0.7,*;q=0.7
  // Keep-Alive: 300
  // Connection: keep-alive
  // <empty line>
  //
  // GET /4k.html HTTP/1.1
  // User-Agent: httperf/0.9.0
  // Host: localhost
  // <empty line>

  string dummy;

  try {
    parseString(in, &request->method);
    skipChar(in, ' ');
    skipChar(in, '/');
    parseString(in, &request->address);
    skipChar(in, ' ');
    parseString(in, &request->version);
    skipNewLine(in);

    while (true) {
      dummy.clear();
      parseLine(in, &dummy);
      if (dummy.size() == 0) break;
    }

    return 0;
  }

  catch (IncompleteInput) {
    return 1;
  }

  catch (ErrorInRequest) {
    return -1;
  }
}

int Parser::parseResponse(Buffer::Iterator* in, Response* response) {
  response->clear();

  try {
    parseLine(in, &response->statusLine);

    size_t contentSize = 0;
    for (;;) {
      string line;
      parseLine(in, &line);

      if (line == "") {
        break;
      }

      const char lengthString[] = "Content-Length:";
      size_t pos = line.find(lengthString);
      if (pos != string::npos) {
        istringstream is(string(line, sizeof(lengthString)));
        is >> contentSize;
      }

      response->headerRemainder.append(line);
    }

    if (in->bytesTotal() - in->bytesRead() < contentSize) {
      throw IncompleteInput();
    }

    while (contentSize--) {
      response->body.push_back(in->getChar());
      in->next();
    }

    return 0;
  }

  catch (IncompleteInput) {
    return 1;
  }

  catch (ErrorInRequest) {
    return -1;
  }
}

void Parser::parseString(Buffer::Iterator* in, string* res) {
  if (in->eob()) throw IncompleteInput();
  while (!in->eob()) {
    const char c = in->getChar();
    if (c == ' ') break;
    if (c == '\r') break;
    res->push_back(c);
    in->next();
  }
  if (in->eob()) throw IncompleteInput();
}

void Parser::parseLine(Buffer::Iterator* in, string* res) {
  if (in->eob()) throw IncompleteInput();
  while (!in->eob()) {
    const char c = in->getChar();
    if (c == '\r') {
      in->next();
      if (in->getChar() == '\n') {
        in->next();
        return;
      } else {
        throw ErrorInRequest();
      }
    }
    res->push_back(c);
    in->next();
  }
  if (in->eob()) throw IncompleteInput();
}

void Parser::skipChar(Buffer::Iterator* in, char skip) {
  if (in->eob()) throw IncompleteInput();
  const char c = in->getChar();
  if (c != skip) throw ErrorInRequest();
  in->next();
}

void Parser::skipNewLine(Buffer::Iterator* in) {
  if (in->eob()) throw IncompleteInput();
  const char first = in->getChar();
  in->next();
  if (in->eob()) throw IncompleteInput();
  const char second = in->getChar();

  if ((first != '\r') || (second != '\n')) throw ErrorInRequest();
  in->next();
}

bool isEmptyLine(const string& line) {
  return (line.size() == 2) && (line == "\r\n");
}

} // namespace http
