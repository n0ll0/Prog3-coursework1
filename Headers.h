#pragma once
//
// Declarations of HEADER_A, HEADER_B, HEADER_C, HEADER_D
// Include item declarations so ITEMn types are available to all modules
//
#include "DateTime.h"
#include "Items.h"
typedef struct headerA {
  void* pItems; // Pointer to the linked list of items.
                // Items may be of types ITEM1...ITEM10.
  char cBegin;  // The linked list contains objects in which
                // the first letter of the second word
                // of ID cBegin.
  headerA* pNext;
} HEADER_A;

typedef struct headerB {
  HEADER_A* pHeaderA; // Pointer to the linked list of headers type A.
  char cBegin;        // The first letter of ID of objects related to
                      // header is cBegin.
  headerB* pNext;
} HEADER_B;

typedef struct headerC {
  void** ppItems; // Pointer to the array of pointers to items.
                  // Items may be of types ITEM1...ITEM10.
  char cBegin;    // The first letter of ID of objects related to
                  // header is cBegin.
  headerC* pNext;
} HEADER_C;

typedef struct headerD {
  HEADER_A* pHeaderA; // Pointer to the linked list of headers type A.
  char cBegin;        // The first letter of ID of objects related to
                      // header is cBegin.
  headerD* pNext;
  headerD* pPrior;

} HEADER_D;

typedef struct headerE {
  void** ppItems; // Pointer to the array of pointers to items.
                  // Items may be of types ITEM1...ITEM10.
  char cBegin;    // The first letter of ID of objects related to
                  // header is cBegin.
  struct headerE* pNext;
  struct headerE* pPrior;
} HEADER_E;

// pointer_to_item is a generic pointer type to any ITEMn structure
typedef item2* pointer_to_item;
