/*
 * Copyright ©2024 Hannah C. Tang.  All rights reserved.  Permission is
 * hereby granted to students registered for University of Washington
 * CSE 333 for use solely during Autumn Quarter 2024 for purposes of
 * the course.  No other use, copying, distribution, or modification
 * is permitted without prior written consent. Copyrights for
 * third-party components of this work must be honored.  Instructors
 * interested in reusing these course materials should contact the
 * author.
 */
#include "./DocTable.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libhw1/CSE333.h"
#include "libhw1/HashTable.h"

#define HASHTABLE_INITIAL_NUM_BUCKETS 2

// This structure represents a DocTable; it contains two hash tables, one
// mapping from document id to document name, and one mapping from
// document name to document id.
struct doctable_st {
  HashTable* id_to_name;  // mapping document id to document name
  HashTable* name_to_id;  // mapping document name to document id
  DocID_t    max_id;      // max doc ID allocated so far
};

DocTable* DocTable_Allocate(void) {
  DocTable* dt = (DocTable*) malloc(sizeof(DocTable));
  Verify333(dt != NULL);

  dt->id_to_name = HashTable_Allocate(HASHTABLE_INITIAL_NUM_BUCKETS);
  dt->name_to_id = HashTable_Allocate(HASHTABLE_INITIAL_NUM_BUCKETS);
  dt->max_id = 1;  // we reserve max_id = 0 for the invalid docID

  return dt;
}

void DocTable_Free(DocTable* table) {
  Verify333(table != NULL);

  // STEP 1.
  HashTable_Free(table->id_to_name, &free);
  HashTable_Free(table->name_to_id, &free);
  // Free the strings inside of these table by pointing to the traditional
  // free method as our free here

  free(table);
}

int DocTable_NumDocs(DocTable* table) {
  Verify333(table != NULL);
  return HashTable_NumElements(table->id_to_name);
}

DocID_t DocTable_Add(DocTable* table, char* doc_name) {
  char *doc_copy;
  DocID_t *doc_id;
  DocID_t res;
  HTKeyValue_t kv, old_kv;

  Verify333(table != NULL);

  // STEP 2.
  // Check to see if the document already exists.  Then make a copy of the
  // doc_name and allocate space for the new ID.

  // Res is DocId built for us at the top
  res = DocTable_GetDocID(table, doc_name);
  if (res != INVALID_DOCID) {
    return res;
    // Since we want to return the exist docid if it already exists
  }

  // We will NOT do Malloc checks here or anywhere else in the file
  doc_id = (DocID_t*)malloc(sizeof(DocID_t) );
  doc_copy = (char*) malloc(strlen(doc_name) +1);
  strncpy(doc_copy, doc_name, strlen(doc_name)+1);
  // making copies of doc_name, +1 for null termiantor



  // Code right above is mine, two lines here were written for us
  *doc_id = table->max_id;
  table->max_id++;


  // STEP 3.
  // Set up the key/value for the id->name mapping, and do the insert.
  kv.key = (HTKey_t)*doc_id;
  kv.value = (HTValue_t)doc_copy;
  // Name gets sent in as a pointer and docid is a pointer waiting for its
  // value. We make copies and malloc because we want to maintain its
  // existence past the runtime of this function
  HashTable_Insert(table->id_to_name, kv, &old_kv);
  // old_kv is return param declared above


  // STEP 4.
  // Set up the key/value for the name->id, and/ do the insert.
  // Be careful about how you calculate the key for this mapping.
  // You want to be sure that how you do this is consistent with
  // the provided code.

  // In code written in fileparser, its written like this:
  //  hash_key = FNVHash64((unsigned char*) word, strlen(word));
  // Hopefullty this is close enough
  kv.key = FNVHash64((unsigned char*) doc_copy, strlen(doc_copy));

  // We can use the same kv variable here as its just a container
  kv.value = (HTValue_t) doc_id;
  // Notice that when we put doc_id into value, we dont dereference it!
  // Values in these tables are pointers, caused a bug once for me :((
  // For the id->name tablem, we sent in doc_copy as the value, which was by
  // default a pointer

  HashTable_Insert(table->name_to_id, kv, &old_kv);
  return *doc_id;
}

DocID_t DocTable_GetDocID(DocTable* table, char* doc_name) {
  HTKey_t key;
  HTKeyValue_t kv;
  DocID_t res;

  Verify333(table != NULL);
  Verify333(doc_name != NULL);

  // STEP 5.
  // Try to find the passed-in doc in name_to_id table.
  key = FNVHash64((unsigned char*) doc_name, strlen(doc_name));
  if (HashTable_Find(table->name_to_id, key, &kv)) {
    // Find returns a boolean! // Also, kv is output param
    res = *((DocID_t*)kv.value);
    // So res wants DocID_t instead of HTValue_t, but we cant just cast like
    // (DocId_t) so we cast with the pointer, and then dereference
    // The ed post titled "Confused on GetNameTo_IdTable" really helped me here
    return res;
  }


  return INVALID_DOCID;  // you may need to change this return value
}

char* DocTable_GetDocName(DocTable* table, DocID_t doc_id) {
  HTKeyValue_t kv;

  Verify333(table != NULL);
  Verify333(doc_id != INVALID_DOCID);

  // STEP 6.
  // Lookup the doc_id in the id_to_name table,
  // and either return the string (i.e., the (char *)
  // saved in the value field for that key) or
  // NULL if the key isn't in the table.
  if (HashTable_Find(table->id_to_name, (HTKey_t) doc_id, &kv)) {
    // id->name table so send in id as second param (key)
    return kv.value;
  }


  return NULL;  // you may need to change this return value
}

HashTable* DT_GetIDToNameTable(DocTable* table) {
  Verify333(table != NULL);
  return table->id_to_name;
}

HashTable* DT_GetNameToIDTable(DocTable* table) {
  Verify333(table != NULL);
  return table->name_to_id;
}
