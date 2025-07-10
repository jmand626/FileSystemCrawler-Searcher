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

#include "./MemIndex.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "libhw1/CSE333.h"
#include "libhw1/HashTable.h"
#include "libhw1/LinkedList.h"


///////////////////////////////////////////////////////////////////////////////
// Internal-only helpers
//
// These functions use the MI_ prefix instead of the MemIndex_ prefix,
// to indicate that they should not be considered part of the MemIndex's
// public API.

// Comparator usable by LinkedList_Sort(), which implements an increasing
// order by rank over SearchResult's.  If the caller is interested in a
// decreasing sort order, they should invert the return value.
static int MI_SearchResultComparator(LLPayload_t e1, LLPayload_t e2) {
  SearchResult* sr1 = (SearchResult*) e1;
  SearchResult* sr2 = (SearchResult*) e2;

  if (sr1->rank > sr2->rank) {
    return 1;
  } else if (sr1->rank < sr2->rank) {
    return -1;
  } else {
    return 0;
  }
}

// Deallocator usable by LinkedList_Free(), which frees a list of
// of DocPositionOffset_t.  Since these offsets are stored inline (ie, not
// malloc'ed), there is nothing to do in this function.
static void MI_NoOpFree(LLPayload_t ptr) { }

// Deallocator usable by HashTable_Free(), which frees a LinkedList of
// DocPositionOffset_t (ie, our posting list).  We use these LinkedLists
// in our WordPostings.
static void MI_PostingsFree(HTValue_t ptr) {
  LinkedList* list = (LinkedList*) ptr;
  LinkedList_Free(list, &MI_NoOpFree);
}

// Deallocator used by HashTable_Free(), which frees a WordPostings.  A
// MemIndex consists of a HashTable of WordPostings (ie, this is the top-level
// structure).
static void MI_ValueFree(HTValue_t ptr) {
  WordPostings* wp = (WordPostings*) ptr;

  free(wp->word);
  HashTable_Free(wp->postings, &MI_PostingsFree);

  free(wp);
}

///////////////////////////////////////////////////////////////////////////////
// MemIndex implementation

MemIndex* MemIndex_Allocate(void) {
  // Happily, HashTables dynamically resize themselves, so we can start by
  // allocating a small hashtable.
  HashTable* index = HashTable_Allocate(16);
  Verify333(index != NULL);
  return index;
}

void MemIndex_Free(MemIndex* index) {
  HashTable_Free(index, &MI_ValueFree);
}

int MemIndex_NumWords(MemIndex* index) {
  return HashTable_NumElements(index);
}

void MemIndex_AddPostingList(MemIndex* index, char* word, DocID_t doc_id,
                             LinkedList* postings) {
  HTKey_t key = FNVHash64((unsigned char*) word, strlen(word));
  HTKeyValue_t mi_kv, postings_kv, unused;
  WordPostings* wp;

  // STEP 1.
  // Remove this early return.  We added this in here so that your unittests
  // would pass even if you haven't finished your MemIndex implementation.

  // -> Removed!

  // First, we have to see if the passed-in word already exists in
  // the inverted index.
  if (!HashTable_Find(index, key, &mi_kv)) {
    // STEP 2.
    // No, this is the first time the inverted index has seen this word.  We
    // need to prepare and insert a new WordPostings structure.  After
    // malloc'ing it, we need to:
    //   (1) find existing memory or allocate new memory for the WordPostings'
    //       word field (hint: remember that this function takes ownership
    //       of the passed-in word).
    //   (2) allocate a new hashtable for the WordPostings' docID->postings
    //       mapping.
    //   (3) insert the the new WordPostings into the inverted index (ie, into
    //       the "index" table).
    // Before we can allocate memory for the word and posting fields, we first
    // must do the same for wp itself, which was declared above
    wp = (WordPostings*) malloc(sizeof(WordPostings));

    // (1) -> To "find" existing memory, we see that the hint tells us to
    // take ownership of the parameter passed in also called word so we do
    wp->word = word;

    // (2) -> We can use HashTable_allocate to make a HT, but it needs num
    // buckets as parameters. How do we know how much to do? In this file, on
    // line 79, it allocates a starting amount of 16, so we will do that!
    wp->postings = HashTable_Allocate(16);

    // (3) Remember, this is the next big table from the MI that maps word to
    // doc ids and then will continue to hold posting lists after we put some
    // We are just holding the struct itself right now
    mi_kv.value = (HTValue_t) wp;
    mi_kv.key = key;  // Key was already hashed for us

    HashTable_Insert(index, mi_kv, &unused);
    // unused appeared to have little other uses, so its our return parameter
  } else {
    // Yes, this word already exists in the inverted index.  There's no need
    // to insert it again.

    // Instead of allocating a new WordPostings, we'll use the one that's
    // already in the inverted index.
    wp = (WordPostings*) mi_kv.value;

    // Ensure we don't have hash collisions (two different words that hash to
    // the same key, which is very unlikely).
    Verify333(strcmp(wp->word, word) == 0);

    // Now we can free the word (since the caller gave us ownership of it).
    free(word);
  }

  // At this point, we have a WordPostings struct which represents the posting
  // list for this word.  Add this new document's postings to it.

  // Verify that this document is not already in the posting list.
  Verify333(!HashTable_Find(wp->postings, doc_id, &postings_kv));

  // STEP 3.
  // Insert a new entry into the wp->postings hash table.
  // The entry's key is this docID and the entry's value
  // is the "postings" (ie, word positions list) we were passed
  // as an argument.
  postings_kv.key = doc_id;

  // We were passed postings as an Linked List so we need to cast into the
  // typical HT value
  postings_kv.value = (HTValue_t) postings;
  // Since unused is pretty much just a temp variable, we use it again here
  HashTable_Insert(wp->postings, postings_kv, &unused);
}

LinkedList* MemIndex_Search(MemIndex* index, char* query[], int query_len) {
  LinkedList* ret_list;
  HTKeyValue_t kv;
  WordPostings* wp;
  HTKey_t key;
  int i;

  // Declared for step 4
  HTIterator* result;
  HTKeyValue_t doc_kv;
  SearchResult* searchR;
  LinkedList* positions;

  // If the user provided us with an empty search query, return NULL
  // to indicate failure.
  if (query_len == 0) {
    return NULL;
  }

  // STEP 4.
  // The most interesting part of Part C starts here...!
  //
  // Look up the first query word (ie, query[0]) in the inverted index.  For
  // each document that matches, allocate and initialize a SearchResult
  // structure (the initial computed rank is the number of times the word
  // appears in that document).  Finally, append the SearchResult onto ret_list.
  ret_list = LinkedList_Allocate();

  // Use HashTable find to loop up query[0] in memindex. Since memindex is also
  // a hashtable, this will work, but we need to hash it ourselves
  key = FNVHash64((unsigned char*) query[0], strlen(query[0]));

  if (!HashTable_Find(index, key, &kv)) {
    LinkedList_Free(ret_list, (LLPayloadFreeFnPtr) free);
    // According to the header file:
    // "the appropriate deallocator to pass into LinkedList_Free is stdlib's
    // free()."
    return NULL;
    // Although I would put EXIT_FAILURE here, the header says to return NULL
  }

  // Since kv was return param for the previous find, we use it to fill up wp.
  wp = (WordPostings*) kv.value;
  // Since the explanation for this step is going for each document that
  // matches our search result, it makes sense that we would use an iterator
  result = HTIterator_Allocate(wp->postings);

  // Traditional iterator loop
  while (HTIterator_IsValid(result)) {
    // Two things to do in here -> Build up Searchlist and append to ret for
    // each document
    HTIterator_Get(result, &doc_kv);
    searchR = (SearchResult*) malloc(sizeof(SearchResult));
    searchR->doc_id = doc_kv.key;

    // This is because the value that HTIterator gets the doc id as a key
    // and the linked list of positions
    positions = (LinkedList*) doc_kv.value;

    // "the initial computed rank is the number of times the word appears in
    // that document"
    searchR->rank = LinkedList_NumElements(positions);

    HTIterator_Next(result);
    LinkedList_Append(ret_list, searchR);
  }

  HTIterator_Free(result);


  // Great; we have our search results for the first query
  // word.  If there is only one query word, we're done!
  // Sort the result list and return it to the caller.
  if (query_len == 1) {
    LinkedList_Sort(ret_list, false, &MI_SearchResultComparator);
    return ret_list;
  }

  // OK, there are additional query words.  Handle them one
  // at a time.
  for (i = 1; i < query_len; i++) {
    LLIterator *ll_it;
    int j, num_docs;

    // STEP 5.
    // Look up the next query word (query[i]) in the inverted index.
    // If there are no matches, it means the overall query
    // should return no documents, so free retlist and return NULL.

    // Hash just like we did before, but for each query
    // This code is almost exactly the same as the ones at the begining of
    // step 5
    key = FNVHash64((unsigned char*) query[i], strlen(query[i]));
    if (!HashTable_Find(index, key, &kv)) {
      LinkedList_Free(ret_list, (LLPayloadFreeFnPtr) free);
      return NULL;
    }

    wp = (WordPostings*) kv.value;

    // STEP 6.
    // There are matches.  We're going to iterate through
    // the docIDs in our current search result list, testing each
    // to see whether it is also in the set of matches for
    // the query[i].
    //
    // If it is, we leave it in the search
    // result list and we update its rank by adding in the
    // number of matches for the current word.
    //
    // If it isn't, we delete that docID from the search result list.
    ll_it = LLIterator_Allocate(ret_list);
    Verify333(ll_it != NULL);
    num_docs = LinkedList_NumElements(ret_list);
    for (j = 0; j < num_docs; j++) {
      LLIterator_Get(ll_it, (LLPayload_t*)&searchR);

      // Is this docid also in the query we are looking for?
      if (!HashTable_Find(wp->postings, searchR->doc_id, &doc_kv)) {
        // No it isnt!
        LLIterator_Remove(ll_it, (LLPayloadFreeFnPtr) free);
        // If we remove, we automatically go on to the next node if it exists
      } else {  // Yes it is!
        positions = (LinkedList*) doc_kv.value;

        // we update its rank by adding in the number of matches for the
        // current word. We get this from counting positions as that is the
        // key value we got
        searchR->rank += LinkedList_NumElements(positions);

        LLIterator_Next(ll_it);
      }
    }
    LLIterator_Free(ll_it);

    // We've finished processing this current query word.  If there are no
    // documents left in our result list, free retlist and return NULL.
    if (LinkedList_NumElements(ret_list) == 0) {
      LinkedList_Free(ret_list, (LLPayloadFreeFnPtr)free);
      return NULL;
    }
  }

  // Sort the result list by rank and return it to the caller.
  LinkedList_Sort(ret_list, false, &MI_SearchResultComparator);
  return ret_list;
}
