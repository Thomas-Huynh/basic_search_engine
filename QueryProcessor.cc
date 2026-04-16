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

#include "./QueryProcessor.h"

#include <iostream>
#include <algorithm>
#include <list>
#include <string>
#include <vector>

extern "C" {
  #include "./libhw1/CSE333.h"
}

using std::list;
using std::sort;
using std::string;
using std::vector;

namespace hw3 {

QueryProcessor::QueryProcessor(const list<string> &index_list,
                                  bool validate) {
  // Stash away a copy of the index list.
  index_list_ = index_list;
  array_len_ = index_list_.size();
  Verify333(array_len_ > 0);

  // Create the arrays of DocTableReader*'s. and IndexTableReader*'s.
  dtr_array_ = new DocTableReader*[array_len_];
  itr_array_ = new IndexTableReader*[array_len_];

  // Populate the arrays with heap-allocated DocTableReader and
  // IndexTableReader object instances.
  list<string>::const_iterator idx_iterator = index_list_.begin();
  for (int i = 0; i < array_len_; i++) {
    FileIndexReader fir(*idx_iterator, validate);
    dtr_array_[i] = fir.NewDocTableReader();
    itr_array_[i] = fir.NewIndexTableReader();
    idx_iterator++;
  }
}

QueryProcessor::~QueryProcessor() {
  // Delete the heap-allocated DocTableReader and IndexTableReader
  // object instances.
  Verify333(dtr_array_ != nullptr);
  Verify333(itr_array_ != nullptr);
  for (int i = 0; i < array_len_; i++) {
    delete dtr_array_[i];
    delete itr_array_[i];
  }

  // Delete the arrays of DocTableReader*'s and IndexTableReader*'s.
  delete[] dtr_array_;
  delete[] itr_array_;
  dtr_array_ = nullptr;
  itr_array_ = nullptr;
}

// This structure is used to store a index-file-specific query result.
typedef struct {
  DocID_t doc_id;  // The document ID within the index file.
  int rank;        // The rank of the result so far.
} IdxQueryResult;

// Search a single index for all valid search results
// Args:
//  itr: A pointer to an IndexTableReader to find the word at an index
//  query: Reference to the query list to continue filtering queries
// Outputs:
//  a list of index-specific query results
static list<IdxQueryResult> SearchIndex(
  IndexTableReader* itr, const vector<string>& query) {
  // Create an initial list of IdxQueryResults based on first word in query
  list<IdxQueryResult> res;
  DocIDTableReader* ditr = itr->LookupWord(query[0]);
  if (ditr == nullptr) {
    return res;
  }
  // Gets the DocID list, prepares an IdxQueryResult, and push into the resulting list
  list<DocIDElementHeader> id_list = ditr->GetDocIDList();
  for (DocIDElementHeader& dih : id_list) {
    IdxQueryResult elem = {dih.doc_id, dih.num_positions};
    res.push_back(elem);
  }

  if (query.size() == 1) {
    delete ditr;
    return res;
  }

  // Now try to go through the rest of the query
  for (size_t i = 1; i < query.size(); i++) {
    ditr = itr->LookupWord(query[i]);
    if (ditr == nullptr) {
      // Definitely no matches, clear and return the empty list
      res.clear();
      return res;
    }

    // We might have matches, now go through the current list of Doc IDs
    // Try to find the current one in the id_list
    id_list = ditr->GetDocIDList();
    for (auto it = res.begin(); it != res.end(); ) {
      bool found = false;
      for (auto& dih : id_list) {
        if (dih.doc_id == it->doc_id) {
          // Found it, now say it is found and update the rank
          found = true;
          it->rank += dih.num_positions;
          break;
        }
      }

      if (!found) {
        // Could not find it, erase the element from the res list
        it = res.erase(it);
      } else {
        ++it;
      }
      delete ditr;
    }
  }
  delete ditr;
  return res;
}

vector<QueryProcessor::QueryResult>
QueryProcessor::ProcessQuery(const vector<string> &query) const {
  Verify333(query.size() > 0);

  // STEP 1.
  // (the only step in this file)
  vector<QueryProcessor::QueryResult> final_result;

  // Iterate over each index, and update main query result accordingly
  for (int i = 0; i < array_len_; i++) {
    list<IdxQueryResult> index_res = SearchIndex(itr_array_[i], query);
    string name;
    for (IdxQueryResult iqr : index_res) {
      // Map the doc id to the file name and prepare a QueryResult
      // Then insert the QueryResult into the main result
      dtr_array_[i]->LookupDocID(iqr.doc_id, &name);
      QueryResult qr;
      qr.document_name = name;
      qr.rank = iqr.rank;
      final_result.push_back(qr);
    }
  }

  // Sort the final results.
  sort(final_result.begin(), final_result.end());
  return final_result;
}

}  // namespace hw3
