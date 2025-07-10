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

// Feature test macro for strtok_r (c.f., Linux Programming Interface p. 63)
#define _XOPEN_SOURCE 600

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
static void Usage(void);
static void ProcessQueries(DocTable* dt, MemIndex* mi);
static int GetNextLine(FILE* f, char** ret_str);

// Helper function we made
static int count_words(const char* query);

//////////////////////////////////////////////////////////////////////////////
// Main
int main(int argc, char** argv) {
  if (argc != 2) {
    Usage();
  }

  // Start by intializing our objects
  DocTable* doct = NULL;
  MemIndex* memi = NULL;

  bool crawl = false;
  crawl = CrawlFileTree(argv[1], &doct, &memi);
  // Crawl through dir and subdirs looking for files to attach to doctable
  // and memindex.
  if (!(crawl)) {
    fprintf(stderr, "Cannot crawl inputed directory");
    return EXIT_FAILURE;
  }

  // Once we have set up all of our data structures, began queries from
  // the user
  ProcessQueries(doct, memi);
  DocTable_Free(doct);

  MemIndex_Free(memi);
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
// Usage was already written by staff!!!

static void ProcessQueries(DocTable* doct, MemIndex* memi) {
  char* query_line;
  int line_success;
  printf("Indexing './test_tree'\n");
  // Same thing as while (true)
  while (1) {
    printf("enter query:\n");
    line_success = GetNextLine(stdin, &query_line);
    // Since getnextline wants a double pointer and queryline is a pointer,
    // we send in its address
    if (line_success < 1) break;
    // We got no bytes here

    // We need the length of the query to create our list of words
    char* query_copy = strdup(query_line);
    // we MUST use A copy since count_words changes its input, and changing
    // that string can mess up later code
    int num_words = count_words(query_copy);
    free(query_copy);

    if (num_words == 0) {
      free(query_line);
      continue;
      // Just want to make sure we got our memory back from here
    }

    // allocate array for words
    char** query_words = (char**) malloc(sizeof(char*) * num_words);
    // Each word is stored in a char pointer
    char* word;
    // Each individual word
    char* saveptr = NULL;  // Saveptr for strtok_r
    for (int i = 0; i < num_words; i++) {
      if (i == 0) {
        // We are at the start of the current string brought in through fgets
        word = strtok_r(query_line, " \t\r\n", &saveptr);
        // For more info about that delimiter, look to the count_words function
        // as that was written before. We put null in the saveptr spot to force
        // this function to search for the next token
      } else {
        word = strtok_r(NULL, " ", &saveptr);
        // Also shown off in count_words, tells compiler to keep going off
        // previous word
      }

      if (word == NULL) break;

      // Allocate and convert to lowercase
      query_words[i] = (char*) malloc(strlen(word) + 1);
      // One more for the null termiantor at the end

      for (int j = 0; j < strlen(word); j++) {
        // Why lower? The tables we use need lowercase letters
        query_words[i][j] = tolower(word[j]);
      }
      // Need to 're-delimit' our words
      query_words[i][strlen(word)] = '\0';
    }

    // Search the index
    LinkedList* listofdocs = MemIndex_Search(memi, query_words, num_words);

    SearchResult* searchR;
    char* name;
    // We use these to examine memory pieces, like we did in memindex itself
    if (listofdocs == NULL) {  // Empty dir
      printf("Please put some files into this directory\n");
    } else {
      printf("Found %d documents\n", LinkedList_NumElements(listofdocs));
      LLIterator* iter = LLIterator_Allocate(listofdocs);
      // Its better we use this than hasNext or Next
      while (LLIterator_IsValid(iter)) {
        searchR = NULL;

        LLIterator_Get(iter, (LLPayload_t*)&searchR);
        name = DocTable_GetDocName(doct, searchR->doc_id);
        // Print out the docname, and rank attached.
        // Equivalently ranked querries are in a random order
        printf("   %s (%d)\n", name, searchR->rank);
        LLIterator_Next(iter);
      }

      LLIterator_Free(iter);
      // Free the LinkedList and its contents
      LinkedList_Free(listofdocs, free);
    }

    for (int w = 0; w < num_words; w++) {
      free(query_words[w]);
      // There exists no other way to free but to individually free each piece
      // of our array
    }
    free(query_words);
    free(query_line);
  }
}

static int GetNextLine(FILE *f, char **ret_str) {
  char* line;
  int sizeOfBuf = 0;
  const int BUFFER_SIZE = 4096;
  char buff[BUFFER_SIZE];
  // fgets's 2nd parameter is the max for chars to read
  if (fgets(buff, BUFFER_SIZE, f) == NULL) {
    // Double pointer since strings are char*'s, and we always want to pass
    // pointers
    *ret_str = NULL;
    // Error with reading -> since we are using fgets and not the posix open
    // we return -1 on null, instead of
    return -1;
  }

  line = (char*) malloc(strlen(buff) + 1);
  // We copy offer from buffer to line since we are returning line
  strncpy(line, buff, strlen(buff)+1);

  sizeOfBuf = strlen(buff);

  // Line takes in the sice of buff + 1, so you might think we need to change
  // sizeOfBuf - 2 since the new line character should be at the end, but...
  if (sizeOfBuf > 0 && line[sizeOfBuf - 1] == '\n') {
    // using sizeOfBuf is a neccesity because buff is where we usually do
    // changes to the input, so its unsafe to use the actual buff itself.
    // ... when we put it into a char*, we also add a null termiantor
    // right to the end before the null terminator
    line[sizeOfBuf - 1] = '\0';
    sizeOfBuf--;
  }

  *ret_str = line;
  return sizeOfBuf;
}

static int count_words(const char* query) {
  int count = 0;
  char* ptr;

  // Strtok_r is essentialy C's version of java's string.split by a delimiter
  // _r means reentrant, and since we are doing concurrency later in this class
  // this makes sense.
  char* token = strtok_r((char*)query, " ", &ptr);
  // the last ptr is rather confusing, but from what I can tell from the man
  // pages it is used internally by this funct to maintain "context"
  // Our escape sequeunec placed in the delim above mean the following
  // " " = the dreaded space
  // \t  = tab;
  // \r = carrige return -> used to split text into lines
  // \n = new line character

  while (token != NULL) {
    count++;
    token = strtok_r(NULL, " ", &ptr);
    // If strtok_r's first value is NULL, it will continue with the same string
    // it already has. A weird and confusing way to do this
  }

  return count;
}

