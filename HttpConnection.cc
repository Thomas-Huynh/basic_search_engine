/*
 * Copyright ©2025 Chris Thachuk & Naomi Alterman.  All rights reserved.
 * Permission is hereby granted to students registered for University of
 * Washington CSE 333 for use solely during Autumn Quarter 2025 for
 * purposes of the course.  No other use, copying, distribution, or
 * modification is permitted without prior written consent. Copyrights
 * for third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */

#include <stdint.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <map>
#include <string>
#include <vector>
#include <iostream>

#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpConnection.h"

using std::map;
using std::string;
using std::vector;

namespace hw4 {

static const char *kHeaderEnd = "\r\n\r\n";
static const int kHeaderEndLen = 4;

bool HttpConnection::GetNextRequest(HttpRequest *const request) {
  // Use WrappedRead from HttpUtils.cc to read bytes from the files into
  // private buffer_ variable. Keep reading until:
  // 1. The connection drops
  // 2. You see a "\r\n\r\n" indicating the end of the request header.
  //
  // Hint: Try and read in a large amount of bytes each time you call
  // WrappedRead.
  //
  // After reading complete request header, use ParseRequest() to parse into
  // an HttpRequest and save to the output parameter request.
  //
  // Important note: Clients may send back-to-back requests on the same socket.
  // This means WrappedRead may also end up reading more than one request.
  // Make sure to save anything you read after "\r\n\r\n" in buffer_ for the
  // next time the caller invokes GetNextRequest()!

  // STEP 1:

  // read until we find \r\n\r\n sequence
  size_t pos = buffer_.find(kHeaderEnd);
  unsigned char buf[1024];
  while (pos == string::npos) {
    int num_read = WrappedRead(fd_, buf, 1023);
    buf[num_read] = '\0';
    buffer_.append(string(reinterpret_cast<char *>(buf)));
    pos = buffer_.find(kHeaderEnd);
  }
  // split out, parse request, then save back into the buffer
  string extra = buffer_.substr(pos + kHeaderEndLen, string::npos);
  buffer_.erase(pos + kHeaderEndLen);
  *request = ParseRequest(buffer_);
  buffer_ = extra;
  return true;  // you may need to change this return value
}

bool HttpConnection::WriteResponse(const HttpResponse &response) const {
  // We use a reinterpret_cast<> to cast between unrelated pointer types, and
  // a static_cast<> to perform a conversion from an unsigned type to its
  // corresponding signed one.
  string str = response.GenerateResponseString();
  int res = WrappedWrite(fd_,
                         reinterpret_cast<const unsigned char*>(str.c_str()),
                         str.length());

  if (res != static_cast<int>(str.length()))
    return false;
  return true;
}

HttpRequest HttpConnection::ParseRequest(const string &request) const {
  HttpRequest req("/");  // by default, get "/".

  // Plan for STEP 2:
  // 1. Split the request into different lines (split on "\r\n").
  // 2. Extract the URI from the first line and store it in req.URI.
  // 3. For the rest of the lines in the request, track the header name and
  //    value and store them in req.headers_ (e.g. HttpRequest::AddHeader).
  //
  // Hint: Take a look at HttpRequest.h for details about the HTTP header
  // format that you need to parse.
  //
  // You'll probably want to look up boost functions for:
  // - Splitting a string into lines on a "\r\n" delimiter
  // - Trimming whitespace from the end of a string
  // - Converting a string to lowercase.
  //
  // Note: If a header is malformed, skip that line.

  // STEP 2:
  std::vector<string> req_lines;
  boost::split(req_lines, request, boost::is_any_of("\r\n"));

  std::istringstream iss(req_lines[0]);
  std::string uri;
  iss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
  iss >> uri;
  boost::trim(uri);
  req.set_uri(uri);

  std::vector<string> line_parser;
  for (size_t i = 1; i < req_lines.size(); i++) {
    // add header and value of each line
    line_parser.clear();
    boost::split(line_parser, req_lines[i], boost::is_any_of(":"));
    if (line_parser.size() != 2) {
      continue;
    }
    boost::trim(line_parser[0]);
    boost::trim(line_parser[1]);

    req.AddHeader(line_parser[0], line_parser[1]);

  }

  return req;
}

}  // namespace hw4
