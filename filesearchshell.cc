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

#include <cstdlib>    // for EXIT_SUCCESS, EXIT_FAILURE
#include <iostream>   // for std::cout, std::cerr, etc.
#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>

#include "./QueryProcessor.h"

using std::cerr;
using std::cout;
using std::cin;
using std::endl;
using std::string;
using std::istringstream;
using std::vector;
using std::list;
using hw3::QueryProcessor;

// Error usage message for the client to see
// Arguments:
// - prog_name: Name of the program
static void Usage(char *prog_name);

// Your job is to implement the entire filesearchshell.cc
// functionality. We're essentially giving you a blank screen to work
// with; you need to figure out an appropriate design, to decompose
// the problem into multiple functions or classes if that will help,
// to pick good interfaces to those functions/classes, and to make
// sure that you don't leak any memory.
//
// Here are the requirements for a working solution:
//
// The user must be able to run the program using a command like:
//
//   ./filesearchshell ./foo.idx ./bar/baz.idx /tmp/blah.idx [etc]
//
// i.e., to pass a set of filenames of indices as command line
// arguments. Then, your program needs to implement a loop where
// each loop iteration it:
//
//  (a) prints to the console a prompt telling the user to input the
//      next query.
//
//  (b) reads a white-space separated list of query words from
//      std::cin, converts them to lowercase, and constructs
//      a vector of c++ strings out of them.
//
//  (c) uses QueryProcessor.cc/.h's QueryProcessor class to
//      process the query against the indices and get back a set of
//      query results.  Note that you should instantiate a single
//      QueryProcessor  object for the lifetime of the program, rather
//      than  instantiating a new one for every query.
//
//  (d) print the query results to std::cout in the format shown in
//      the transcript on the hw3 web page.
//
// Also, you're required to quit out of the loop when std::cin
// experiences EOF, which a user passes by pressing "control-D"
// on the console.  As well, users should be able to type in an
// arbitrarily long query -- you shouldn't assume anything about
// a maximum line length.  Finally, when you break out of the
// loop and quit the program, you need to make sure you deallocate
// all dynamically allocated memory.  We will be running valgrind
// on your filesearchshell implementation to verify there are no
// leaks or errors.
//
// You might find the following technique useful, but you aren't
// required to use it if you have a different way of getting the
// job done.  To split a std::string into a vector of words, you
// can use a std::stringstream to get the job done and the ">>"
// operator. See, for example, "gnomed"'s post on stackoverflow for
// their example on how to do this:
//
//   http://stackoverflow.com/questions/236129/c-how-to-split-a-string
//
// (Search for "gnomed" on that page. They use an istringstream, but
// a stringstream gets the job done too.)
//
// Good luck, and write beautiful code!

// Parses a string into space-separated words
// Args:
//   s: string taken directly from input to process
// Output:
//   A vector filled with words that serve as the current query
static vector<string> GetQueryLine(const string& s);

// Converts a string to all lowercase letters
// Args:
//   s: reference to string, will be mutated by the method
// Output:
//   The lowercase version of the inputted string
static void toLower(const string& s, string* dst);

int main(int argc, char **argv) {
  if (argc < 2) {
    Usage(argv[0]);
  }

  // STEP 1:
  // Implement filesearchshell!
  // Probably want to write some helper methods ...

  // Create the list of indices from arguments and creates the query processor
  list<string> idx_list;
  for (int i = 1; i < argc; i++) {
    idx_list.push_back(string(argv[i]));
  }
  QueryProcessor qp = QueryProcessor(idx_list);


  // Begin the query loop
  cout << "Enter query:" << endl;
  string s;
  while (std::getline(cin, s)) {
    // ctrl+D not pressed yet, prepare query
    // then process query and format result
    vector<string> query = GetQueryLine(s);
    vector<QueryProcessor::QueryResult> qr = qp.ProcessQuery(query);
    for (size_t i = 0; i < qr.size(); i++) {
      cout << "  " << qr[i].document_name << " (" << qr[i].rank << ")" << endl;
    }
    cout << "Enter query:" << endl;
  }

  return EXIT_SUCCESS;
}

static vector<string> GetQueryLine(const string& s) {
  vector<string> res;
  string word;
  string lc_query;
  toLower(s, &lc_query);
  istringstream iss(lc_query);

  // Keep streaming word and putting on the vector
  while (iss >> word) {
    res.push_back(word);
  }

  return res;
}

static void toLower(const string& s, string* dst) {
  // prepares string in heap and lowercases each character in s
  char* buf = new char[s.size() + 1];
  for (size_t i = 0; i < s.size(); i++) {
    buf[i] = std::tolower(s[i]);
  }
  buf[s.size()] = '\0';
  *dst = string(buf);
  delete[] buf;
}


static void Usage(char *prog_name) {
  cerr << "Usage: " << prog_name << " [index files+]" << endl;
  exit(EXIT_FAILURE);
}
