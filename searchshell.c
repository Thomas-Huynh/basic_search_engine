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

// Feature test macro for strtok_r (c.f., Linux Programming Interface p. 63)
#define _XOPEN_SOURCE 600
#define MAX_QUERY_SIZE 256

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "libhw1/CSE333.h"
#include "./CrawlFileTree.h"
#include "./DocTable.h"
#include "./MemIndex.h"

//////////////////////////////////////////////////////////////////////////////
// Helper function declarations, constants, etc

// Prints an error message telling the user how to properly
// use the searchshell
// Args: none
// Outputs:
//   An error message showing proper usage of the searchshell
static void Usage(void);

// Processes a query and prints out results
// Args:
//  dt: A DocTable where to find document based on its id
//  mi: An inverted index to find the appropriate words
//  query: an array of strings to look up in the inverted index
//  query_len: the length of the query array
// Outputs:
//   Prints out each file that has the associated set of words
//   and its rank
static void ProcessQueries(DocTable *dt, MemIndex *mi,  char* query[],
                          int query_len);

// Gets the next line in a series of queries
// Args:
//   f: The stream where we get the queries
//   ret_str: An output parameter to store the query separated by \n
// Returns:
//   0 if the getting the next line is successful
//   -1 if there was an error getting the next line
//   (there was no space for buffer or EOF reached)
static int GetNextLine(FILE *f, char **ret_str);


//////////////////////////////////////////////////////////////////////////////
// Main
int main(int argc, char **argv) {
  if (argc != 2) {
    Usage();
  }

  // Implement searchshell!  We're giving you very few hints
  // on how to do it, so you'll need to figure out an appropriate
  // decomposition into functions as well as implementing the
  // functions.  There are several major tasks you need to build:
  //
  //  - Crawl from a directory provided by argv[1] to produce and index
  //  - Prompt the user for a query and read the query from stdin, in a loop
  //  - Split a query into words (check out strtok_r)
  //  - Process a query against the index and print out the results
  //
  // When searchshell detects end-of-file on stdin (cntrl-D from the
  // keyboard), searchshell should free all dynamically allocated
  // memory and any other allocated resources and then exit.
  //
  // Note that you should make sure the fomatting of your
  // searchshell output exactly matches our solution binaries
  // to get full points on this part.

  DocTable* doc;
  MemIndex* index;

  // setup DocTable and MemIndex
  printf("Indexing '%s'\n", argv[1]);
  if (!CrawlFileTree(argv[1], &doc, &index)) {
    fprintf(stderr, "CrawlFileTree failure, nothing is allocated");
    return EXIT_FAILURE;
  }

  char* raw_query;
  char* query[MAX_QUERY_SIZE];
  int query_len;
  char* save_ptr;
  char* token;

  while (true) {
    fprintf(stdout, "enter query:\n");
    if (GetNextLine(stdin, &raw_query) == -1) {
      break;
    }
    token = strtok_r(raw_query, " ", &save_ptr);
    query_len = 0;
    while (token != NULL) {
      query[query_len] = token;
      query_len++;
      token = strtok_r(save_ptr, " ", &save_ptr);
    }


    ProcessQueries(doc, index, query, query_len);

    free(raw_query);
  }

  fprintf(stdout, "shutting down...\n");
  DocTable_Free(doc);
  MemIndex_Free(index);

  return EXIT_SUCCESS;
}


//////////////////////////////////////////////////////////////////////////////
// Helper function definitions

static void Usage(void) {
  fprintf(stderr, "Usage: ./searchshell <docroot>\n");
  fprintf(stderr,
          "where <docroot> is an absolute or relative " \
          "path to a directory to build an index under.\n");
  exit(EXIT_FAILURE);
}

static void ProcessQueries(DocTable *dt, MemIndex *mi, char* query[],
                          int query_len) {
  LinkedList* res = MemIndex_Search(mi, query, query_len);
  if (res == NULL) {
    return;
  }

  // goes through each of the results in the list
  // and accesses doc table to get the file name
  LLPayload_t payload;
  SearchResult* sr;
  while (LinkedList_NumElements(res) > 0) {
    LinkedList_Pop(res, &payload);
    sr = (SearchResult*) payload;
    printf("  %s (%d)\n", DocTable_GetDocName(dt, sr->doc_id), sr->rank);
    free(sr);
  }
  free(res);
}

static int GetNextLine(FILE *f, char **ret_str) {
  char* buf = (char*) malloc(sizeof(char) * MAX_QUERY_SIZE);
  if (buf == NULL) {
    fprintf(stderr, "Could not allocate memory for buffer");
    return -1;
  }
  int curr_ptr = 0;

  // keep reading character by character into buffer until
  // we see newline or end of file
  char c = fgetc(stdin);
  while (c != '\n' && c != EOF) {
    if (isalpha(c)) {
      c = tolower(c);
    }
    buf[curr_ptr] = c;
    curr_ptr++;
    if (curr_ptr >= MAX_QUERY_SIZE) {
      break;
    }
    c = fgetc(stdin);
  }

  if (c == EOF) {
    free(buf);
    return -1;
  }
  buf[curr_ptr] = '\0';
  *ret_str = buf;
  return 0;  // you may need to change this return value
}
