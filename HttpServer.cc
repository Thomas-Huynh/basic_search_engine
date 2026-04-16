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

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <sstream>

#include "./FileReader.h"
#include "./HttpConnection.h"
#include "./HttpRequest.h"
#include "./HttpUtils.h"
#include "./HttpServer.h"
#include "./libhw3/QueryProcessor.h"

using std::cerr;
using std::cout;
using std::endl;
using std::list;
using std::map;
using std::string;
using std::stringstream;
using std::unique_ptr;

namespace hw4 {
///////////////////////////////////////////////////////////////////////////////
// Constants, internal helper functions
///////////////////////////////////////////////////////////////////////////////
// static
const int HttpServer::kNumThreads = 8;

static const char *kThreegleStr =
  "<html><head><title>333gle</title></head>\n"
  "<body>\n"
  "<center style=\"font-size:500%;\">\n"
  "<span style=\"position:relative;bottom:-0.33em;color:orange;\">3</span>"
    "<span style=\"color:red;\">3</span>"
    "<span style=\"color:gold;\">3</span>"
    "<span style=\"color:blue;\">g</span>"
    "<span style=\"color:green;\">l</span>"
    "<span style=\"color:red;\">e</span>\n"
  "</center>\n"
  "<p>\n"
  "<div style=\"height:20px;\"></div>\n"
  "<center>\n"
  "<form action=\"/query\" method=\"get\">\n"
  "<input type=\"text\" size=30 name=\"terms\" />\n"
  "<input type=\"submit\" value=\"Search\" />\n"
  "</form>\n"
  "</center><p>\n";

// This is the function that threads are dispatched into
// in order to process new client connections.
static void HttpServer_ThrFn(ThreadPool::Task *t);

// Given a request, produce a response.
static HttpResponse ProcessRequest(const HttpRequest &req,
                                   const string &base_dir,
                                   const list<string> &indices);

// Process a file request.
static HttpResponse ProcessFileRequest(const string &uri,
                                       const string &base_dir);

// Process a query request.
static HttpResponse ProcessQueryRequest(const string &uri,
                                        const list<string> &indices,
                                        const string &base_dir);

// Returns true if 's' starts with 'prefix'.
static bool StringStartsWith(const string &s, const string &prefix);

///////////////////////////////////////////////////////////////////////////////
// HttpServer
///////////////////////////////////////////////////////////////////////////////
bool HttpServer::Run(void) {
  // Create the server listening socket.
  int listen_fd;
  cout << "  creating and binding the listening socket..." << endl;
  if (!socket_.BindAndListen(AF_INET6, &listen_fd)) {
    cerr << endl << "Couldn't bind to the listening socket." << endl;
    return false;
  }

  // Spin, accepting connections and dispatching them.  Use a
  // threadpool to dispatch connections into their own thread.
  tp_.reset(new ThreadPool(kNumThreads));
  cout << "  accepting connections..." << endl << endl;
  while (!IsShuttingDown()) {
    // If the HST is successfully added to the threadpool, it'll (eventually)
    // get run and clean itself up.  But we need to manually delete it if
    // it doesn't get added.
    HttpServerTask *hst = new HttpServerTask(HttpServer_ThrFn, this);
    hst->base_dir = static_file_dir_path_;
    hst->indices = &indices_;

    if (!socket_.Accept(&hst->client_fd,
                        &hst->c_addr,
                        &hst->c_port,
                        &hst->c_dns,
                        &hst->s_addr,
                        &hst->s_dns)) {
      // The accept failed for some reason, so quit out of the server.  This
      // can happen when the `kill` command is used to shut down the server
      // instead of the more graceful /quitquitquit handler.
      delete hst;
      break;
    }

    // The accept succeeded; dispatch it to the workers.
    if (!tp_->Dispatch(hst)) {
      delete hst;
      break;
    }
  }
  return true;
}

void HttpServer::BeginShutdown() {
  Verify333(pthread_mutex_lock(&lock_) == 0);
  shutting_down_ = true;
  tp_->BeginShutdown();
  Verify333(pthread_mutex_unlock(&lock_) == 0);
}

bool HttpServer::IsShuttingDown() {
  bool retval;
  Verify333(pthread_mutex_lock(&lock_) == 0);
  retval = shutting_down_;
  Verify333(pthread_mutex_unlock(&lock_) == 0);
  return retval;
}

///////////////////////////////////////////////////////////////////////////////
// Internal helper functions
///////////////////////////////////////////////////////////////////////////////
static void HttpServer_ThrFn(ThreadPool::Task *t) {
  // Cast back our HttpServerTask structure with all of our new client's
  // information in it.  Since we the ones that created this object, we are
  // guaranteed that this is an instance of a HttpServerTask and, per Google's
  // Style Guide, can use a static_cast<> instead of a dynamic_cast<>.
  //
  // Note that, per the ThreadPool::Task API, it is the job of this function
  // to clean up the dynamically-allocated task object.
  unique_ptr<HttpServerTask> hst(static_cast<HttpServerTask*>(t));
  cout << "  client " << hst->c_dns << ":" << hst->c_port << " "
       << "(IP address " << hst->c_addr << ")" << " connected." << endl;

  // Read in the next request, process it, and write the response.

  // Use the HttpConnection class to read and process the next HTTP request
  // from our current client, then write out our response.  Recall that
  // multiple HTTP requests can be sent on the same TCP connection; we
  // need to keep the connection alive until the client sends a
  // "Connection: close\r\n" header; it is only after we finish processing
  // their request that we can shut down the connection and exit
  // this function.

  // STEP 1:
  HttpConnection hc = HttpConnection(hst->client_fd);
  HttpRequest rq = HttpRequest(string(""));
  HttpResponse rp;
  while (!hst->server_->IsShuttingDown()) {

    if (!hc.GetNextRequest(&rq)) {
      break;
    }
    // If the client requested the server to shut down, do so.
    if (StringStartsWith(rq.uri(), "/quitquitquit")) {
      hst->server_->BeginShutdown();
      break;
    }

    rp = ProcessRequest(rq, hst->base_dir, *hst->indices);
    if (!hc.WriteResponse(rp)) {
      break;
    }
    if (rq.GetHeaderValue(string("Connection")) == "close") {
      break;
    }



  }
}

static HttpResponse ProcessRequest(const HttpRequest &req,
                                   const string &base_dir,
                                   const list<string> &indices) {
  // Is the user asking for a static file?
  if (StringStartsWith(req.uri(), "/static/")) {
    return ProcessFileRequest(req.uri(), base_dir);
  }

  // The user must be asking for a query.
  return ProcessQueryRequest(req.uri(), indices, base_dir);
}

static HttpResponse ProcessFileRequest(const string &uri,
                                       const string &base_dir) {
  // The response we'll build up.
  HttpResponse ret;

  // Steps to follow:
  // 1. Use the URLParser class to figure out what file name
  //    the user is asking for. Note that we identify a request
  //    as a file request if the URI starts with '/static/'
  //
  // 2. Use the FileReader class to read the file into memory
  //
  // 3. Copy the file content into the ret.body
  //
  // 4. Depending on the file name suffix, set the response
  //    Content-type header as appropriate, e.g.,:
  //      --> for ".html" or ".htm", set to "text/html"
  //      --> for ".jpeg" or ".jpg", set to "image/jpeg"
  //      --> for ".png", set to "image/png"
  //      etc.
  //    You should support the file types mentioned above,
  //    as well as ".txt", ".js", ".css", ".xml", ".gif",
  //    and any other extensions to get bikeapalooza
  //    to match the solution server.
  //
  // be sure to set the response code, protocol, and message
  // in the HttpResponse as well.

  // text/javascript
  // text/css
  // text/html
  // image/jpeg
  // text/xml
  // image/png
  // gif -> status code 301, "Moved permanently", don't send content_type header

  string file_name = "";
  // STEP 2:
  // url parser
  URLParser up;
  // split out /static/, then parse file name
  file_name = uri.substr(sizeof("/static/") - 1);
  if (file_name.find('?') != string::npos) {
    file_name.erase(file_name.find('?'));
  }
  string suffix = file_name.substr(file_name.find('.') + 1);
  up.Parse(file_name);
  FileReader fr = FileReader(base_dir, up.path());
  string contents;
  ret.set_protocol("HTTP/1.1");
  if (fr.ReadFile(&contents)) {
    ret.AppendToBody(contents);
    if (suffix == "ico") {
      ret.set_response_code(200);
      ret.set_message("OK");
    }
    else if (suffix == "gif") {
      ret.set_response_code(301);
      ret.set_message("Moved permanently");
    } else if (suffix == "jpg" || suffix == "jpeg" || suffix == "png") {
      ret.set_response_code(200);
      ret.set_message("OK");
      ret.set_content_type("image/" + (suffix == "jpg" ? "jpeg" : suffix));
    } else {
      // is text
      ret.set_response_code(200);
      ret.set_message("OK");
      string content_type = "text/";
      if (suffix == "jpg") {
        content_type.append("jpeg");
      } else if (suffix == "htm") {
        content_type.append("html");
      } else if (suffix == "js") {
        content_type.append("javascript");
      } else {
        content_type.append(suffix);
      }
      //ret.set_content_type("text/" + (suffix == "js" ? "javascript" : suffix));
      ret.set_content_type(content_type);
    }
  } else {
    // failed to read file, return HTTP 404 error
    ret.set_response_code(404);
    ret.set_message("Not Found");
    ret.AppendToBody("<html><body>Couldn't find file \""
                    + EscapeHtml(file_name)
                    + "\"</body></html>\n");
  }
  // If you couldn't find the file, return an HTTP 404 error.
  return ret;
}

static HttpResponse ProcessQueryRequest(const string &uri,
                                        const list<string> &indices,
                                        const string &base_dir) {
  // The response we're building up.
  HttpResponse ret;

  // Your job here is to figure out how to present the user with
  // the same query interface as our solution_binaries/http333d server.
  // A couple of notes:
  //
  // 1. The 333gle logo and search box/button should be present on the site.
  //
  // 2. If the user had previously typed in a search query, you also need
  //    to display the search results.
  //
  // 3. you'll want to use the URLParser to parse the uri and extract
  //    search terms from a typed-in search query.  convert them
  //    to lower case.
  //
  // 4. Initialize and use hw3::QueryProcessor to process queries with the
  //    search indices.
  //
  // 5. With your results, try figuring out how to hyperlink results to file
  //    contents, like in solution_binaries/http333d. (Hint: Look into HTML
  //    tags!)

  // STEP 3:

  ret.set_protocol("HTTP/1.1");
  ret.set_response_code(200);
  ret.set_message("OK");
  ret.AppendToBody(kThreegleStr);

  URLParser up;
  std::vector<string> query;
  up.Parse(uri);

  std::map<string, string> req = up.args();
  string terms;
  auto it = req.find("terms");
  if (it != req.end()) {
    terms = EscapeHtml(it->second);
    boost::algorithm::to_lower(terms);
    std::istringstream iss(terms);
    string curr;
    while (iss >> curr) {
      query.push_back(curr);
    }
  }

  if (query.size() == 0) {
    ret.AppendToBody(
      "<p>\n"
      "<br>\n"
      " No results found for\n"
      "<b></b>\n"
      "</p>\n"
    );
    return ret;
  }

  hw3::QueryProcessor qp = hw3::QueryProcessor(indices);
  std::vector<hw3::QueryProcessor::QueryResult> res = qp.ProcessQuery(query);

  if (res.size() == 0) {
    ret.AppendToBody(
      "<p>\n"
      "<br>\n"
      " No results found for " + terms + "\n"
      "<b></b>\n"
      "</p>\n"
    );
  } else {
    ret.AppendToBody(
      "<p>\n"
      "<br>\n"
      + std::to_string(res.size()) + " results found for " + terms + "\n"
      "<b></b>\n"
      "</p>\n"
    );
    ret.AppendToBody("<ul>\n");
    for (size_t i = 0; i < res.size(); i++) {
      if (StringStartsWith(res[i].document_name, "http://")) {
        ret.AppendToBody(
          "<li>\n"
          "<a href=\"" + res[i].document_name + "\">"
          + res[i].document_name + "</a>\n"
          " [" + std::to_string(res[i].rank) + "]\n"
          "<br>\n"
          "</li>\n"
        );
      } else {
        ret.AppendToBody(
          "<li>\n"
          "<a href=\"/static/" + res[i].document_name + "\">"
          + res[i].document_name + "</a>\n"
          " [" + std::to_string(res[i].rank) + "]\n"
          "<br>\n"
          "</li>\n"
        );
      }
    }
    ret.AppendToBody("</ul>\n");
  }
  return ret;
}

static bool StringStartsWith(const string &s, const string &prefix) {
  return s.substr(0, prefix.size()) == prefix;
}


}  // namespace hw4
