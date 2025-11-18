#include "DataStructure.h"
#include "DataSource.h"
#include "Headers.h"
#include "Items.h"
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#pragma warning(disable : 4290)

// Implementation for Struct2 (HEADER_C + buckets for second-word initial)

// We implement the structure for ITEM2 only, as NITEM = 2
static const int NITEM = 2;
static const int ALPHA = 26; // A..Z buckets for second word initial

DataStructure::DataStructure() : pStruct2(nullptr) {}

// Helpers
namespace {
// map 'A'..'Z' to [0..25]; returns -1 if not a letter
int alphaIndex(char c) {
  unsigned char uc = static_cast<unsigned char>(c);
  char upperChar = static_cast<char>(std::toupper(uc));
  if (upperChar >= 'A' && upperChar <= 'Z')
    return upperChar - 'A';
  return -1;
}

// Extract first letters of first and second word from ID; throws on invalid
void parseID(const char* id, char& first, char& second) {
  if (!id)
    throw std::runtime_error("ID is null");
  // Expect exactly one space separating the words
  const char* sp = std::strchr(id, ' ');
  if (!sp || sp == id || *(sp + 1) == '\0')
    throw std::runtime_error("ID must contain two words separated by space");
  first = id[0];
  second = *(sp + 1);
}

// Create a fresh HEADER_C node for given first-letter
HEADER_C* makeHeader(char firstLetter) {
  HEADER_C* h = new HEADER_C;
  h->cBegin = firstLetter;
  h->pNext = nullptr;
  // allocate ALPHA bucket pointers and zero them
  h->ppItems = new void*[ALPHA];
  for (int i = 0; i < ALPHA; ++i)
    h->ppItems[i] = nullptr;
  return h;
}

// Find header by firstLetter; keep track of prior for insertion
HEADER_C* findHeader(HEADER_C* head, char firstLetter, HEADER_C** pPrior) {
  HEADER_C* prev = nullptr;
  HEADER_C* cur = head;
  while (cur && cur->cBegin < firstLetter) {
    prev = cur;
    cur = cur->pNext;
  }
  if (pPrior)
    *pPrior = prev;
  if (cur && cur->cBegin == firstLetter)
    return cur;
  return nullptr;
}

// Insert header in sorted order after prev (may be null for list-head)
void insertHeaderSorted(HEADER_C*& head, HEADER_C* prev, HEADER_C* node) {
  if (!prev) {
    node->pNext = head;
    head = node;
  } else {
    node->pNext = prev->pNext;
    prev->pNext = node;
  }
}

// Deep copy a single ITEM2 (without next)
pointer_to_item cloneItem2(const pointer_to_item src) {
  pointer_to_item p = new ITEM2;
  size_t len = std::strlen(src->pID);
  p->pID = new char[len + 1];
  strcpy_s(p->pID, len + 1, src->pID);
  // std::memcpy();
  p->Code = src->Code;
  if (src->pTime) {
    p->pTime = new TIME;
    *p->pTime = *src->pTime;
  } else {
    p->pTime = nullptr;
  }
  p->pNext = nullptr;
  return p;
}

// Destroy item (ID string and time included)
void destroyItem2(pointer_to_item it) {
  if (!it)
    return;
  if (it->pID)
    delete[] it->pID;
  if (it->pTime)
    delete it->pTime;
  delete it;
}
} // namespace

// 3. Constructor that creates data structure of n items using DataSource
DataStructure::DataStructure(int n) : pStruct2(nullptr) {
  if (n < 0)
    throw std::runtime_error("n cannot be negative");
  int inserted = 0;
  const int batchSize = 32;
  while (inserted < n) {
    int toGenerate = std::min(batchSize, n - inserted);
    pointer_to_item items[batchSize] = {nullptr};
    int generated = 0;
    // Generate a batch of items
    for (int i = 0; i < toGenerate; ++i) {
      items[i] = static_cast<pointer_to_item>(::GetItem(NITEM));
      if (!items[i])
        throw std::runtime_error("Failed to generate item");
      items[i]->pNext = nullptr;
      ++generated;
    }
    // Insert the batch
    for (int i = 0; i < generated && inserted < n; ++i) {
      try {
        *this += items[i];
        ++inserted;
      } catch (const std::exception&) {
      }
    }
  }
}

// 4. Constructor: read from previously written binary file
/*
Custom binary file format:
- FileHeader (12 bytes):
    uint32_t magic   // Magic number: 'S2DS' (0x53324453)
    uint32_t version // Format version: 1
    uint32_t count   // Number of items in file
- For each item (variable size):
    uint32_t len     // Length of ID string (not including null terminator)
    char[len]        // ID string (not null-terminated)
    uint32_t code    // Code value
    char hasTime     // 1 if TIME struct follows, 0 if not
    TIME (optional)  // Present only if hasTime == 1; sizeof(TIME) bytes
*/
DataStructure::DataStructure(std::string Filename) : pStruct2(nullptr) {
  std::ifstream in(Filename, std::ios::binary);
  if (!in)
    throw std::runtime_error("File not found or cannot be opened");

  struct FileHeader {
    uint32_t magic;   // 'S2DS'
    uint32_t version; // 1
    uint32_t count;   // number of items
  } hdr{};

  in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
  if (!in || hdr.magic != 0x53324453u /*'S2DS'*/ || hdr.version != 1)
    throw std::runtime_error("Invalid file format");

  for (uint32_t i = 0; i < hdr.count; ++i) {
    uint32_t len = 0;
    in.read(reinterpret_cast<char*>(&len), sizeof(len));
    if (!in || len == 0)
      throw std::runtime_error("Corrupt file: invalid ID length");
    std::string id(len, '\0');
    // Safe to use &id[0] here because len > 0 (checked above) and string is
    // sized to len
    in.read(&id[0], len);
    uint32_t code = 0;
    in.read(reinterpret_cast<char*>(&code), sizeof(code));
    char hasTime = 0;
    in.read(reinterpret_cast<char*>(&hasTime), sizeof(hasTime));
    TIME t{};
    if (hasTime)
      in.read(reinterpret_cast<char*>(&t), sizeof(t));
    if (!in)
      throw std::runtime_error("Corrupt file while reading item");

    // build item
    pointer_to_item it = new ITEM2;
    it->pID = new char[len + 1];
    std::memcpy(it->pID, id.c_str(), len);
    it->pID[len] = '\0';
    it->Code = code;
    it->pTime = hasTime ? new TIME(t) : nullptr;
    it->pNext = nullptr;

    // insert using operator+= to keep structure invariant
    *this += it;
  }
}

// 5. Destructor: delete items, buckets, and headers
DataStructure::~DataStructure() {
  HEADER_C* h = pStruct2;
  while (h) {
    // For each bucket 0..ALPHA-1 delete linked list of ITEM2
    if (h->ppItems) {
      for (int i = 0; i < ALPHA; ++i) {
        pointer_to_item it = static_cast<pointer_to_item>(h->ppItems[i]);
        while (it) {
          pointer_to_item next = it->pNext;
          destroyItem2(it);
          it = next;
        }
        h->ppItems[i] = nullptr;
      }
      delete[] h->ppItems;
    }
    HEADER_C* nextH = h->pNext;
    delete h;
    h = nextH;
  }
  pStruct2 = nullptr;
}

// 6. Copy constructor (deep copy)
DataStructure::DataStructure(const DataStructure& Original)
    : pStruct2(nullptr) {
  HEADER_C* src = Original.pStruct2;
  HEADER_C* lastDst = nullptr;
  while (src) {
    HEADER_C* dst = makeHeader(src->cBegin);
    // copy all buckets
    for (int i = 0; i < ALPHA; ++i) {
      pointer_to_item head = nullptr;
      pointer_to_item tail = nullptr;
      for (pointer_to_item s = static_cast<pointer_to_item>(src->ppItems[i]); s;
           s = s->pNext) {
        pointer_to_item cpy = cloneItem2(s);
        if (!head) {
          head = tail = cpy;
        } else {
          tail->pNext = cpy;
          tail = cpy;
        }
      }
      dst->ppItems[i] = head;
    }
    if (!pStruct2)
      pStruct2 = dst;
    else
      lastDst->pNext = dst;
    lastDst = dst;
    src = src->pNext;
  }
}

// 7. Number of items in data structure
int DataStructure::GetItemsNumber() {
  int count = 0;
  for (HEADER_C* h = pStruct2; h; h = h->pNext) {
    for (int i = 0; i < ALPHA; ++i) {
      for (pointer_to_item it = static_cast<pointer_to_item>(h->ppItems[i]); it;
           it = it->pNext)
        ++count;
    }
  }
  return count;
}

// 8. Returns pointer to item with the specified ID
pointer_to_item DataStructure::GetItem(char* pID) {
  if (!pID)
    return nullptr;
  char f = 0, s = 0;
  try {
    parseID(pID, f, s);
  } catch (...) {
    return nullptr;
  }
  HEADER_C* prior = nullptr;
  HEADER_C* h = findHeader(pStruct2, f, &prior);
  if (!h)
    return nullptr;
  int idx = alphaIndex(s);
  if (idx < 0)
    return nullptr;
  pointer_to_item it = static_cast<pointer_to_item>(h->ppItems[idx]);
  while (it) {
    if (std::strcmp(it->pID, pID) == 0)
      return it;
    it = it->pNext;
  }
  return nullptr;
}

// 9. Add a new item into the data structure
void DataStructure::operator+=(pointer_to_item p) {
  if (!p)
    throw std::runtime_error("Null item pointer");
  pointer_to_item src = static_cast<pointer_to_item>(p);
  char f = 0, s = 0;
  parseID(src->pID, f, s);
  if (GetItem(src->pID))
    throw std::runtime_error("Item with this ID already exists");

  HEADER_C* prior = nullptr;
  HEADER_C* h = findHeader(pStruct2, f, &prior);
  if (!h) {
    h = makeHeader(f);
    insertHeaderSorted(pStruct2, prior, h);
  }
  int idx = alphaIndex(s);
  if (idx < 0)
    throw std::runtime_error("Invalid ID: second word initial not a letter");
  // clone so we own the memory (safe to delete later)
  pointer_to_item it = cloneItem2(src);
  it->pNext = static_cast<pointer_to_item>(h->ppItems[idx]);
  h->ppItems[idx] = it;
}

// 10. Remove and destroy item with given ID
void DataStructure::operator-=(char* pID) {
  char f = 0, s = 0;
  parseID(pID, f, s);
  HEADER_C* prior = nullptr;
  HEADER_C* h = findHeader(pStruct2, f, &prior);
  if (!h)
    throw std::runtime_error("No item with the specified ID exists");
  int idx = alphaIndex(s);
  if (idx < 0)
    throw std::runtime_error("No item with the specified ID exists");
  pointer_to_item cur = static_cast<pointer_to_item>(h->ppItems[idx]);
  pointer_to_item prev = nullptr;
  while (cur) {
    if (std::strcmp(cur->pID, pID) == 0) {
      if (prev)
        prev->pNext = cur->pNext;
      else
        h->ppItems[idx] = cur->pNext;
      destroyItem2(cur);
      // If header becomes empty, remove it from list
      bool empty = true;
      for (int i = 0; i < ALPHA; ++i) {
        if (h->ppItems[i]) {
          empty = false;
          break;
        }
      }
      if (empty) {
        if (!prior)
          pStruct2 = h->pNext;
        else
          prior->pNext = h->pNext;
        delete[] h->ppItems;
        delete h;
      }
      return;
    }
    prev = cur;
    cur = cur->pNext;
  }
  throw std::runtime_error("No item with the specified ID exists");
}

// 11. Assignment operator (deep copy)
DataStructure& DataStructure::operator=(const DataStructure& Right) {
  if (this == &Right)
    return *this;
  this->~DataStructure();
  new (this) DataStructure(Right);
  return *this;
}

// 12. Equality: same number of items and each ID present with same code/time
bool DataStructure::operator==(DataStructure& Other) {
  if (GetItemsNumber() != Other.GetItemsNumber())
    return false;
  for (HEADER_C* h = pStruct2; h; h = h->pNext) {
    for (int i = 0; i < ALPHA; ++i) {
      for (pointer_to_item a = static_cast<pointer_to_item>(h->ppItems[i]); a;
           a = a->pNext) {
        pointer_to_item b = static_cast<pointer_to_item>(Other.GetItem(a->pID));
        if (!b)
          return false;
        if (a->Code != b->Code)
          return false;
        if (!!a->pTime != !!b->pTime)
          return false;
        if (a->pTime && b->pTime) {
          if (a->pTime->Hour != b->pTime->Hour ||
              a->pTime->Min != b->pTime->Min || a->pTime->Sec != b->pTime->Sec)
            return false;
        }
      }
    }
  }
  return true;
}

// 13. Write structure to binary file (custom format)
void DataStructure::Write(std::string Filename) {
  int total = GetItemsNumber();
  if (total == 0)
    throw std::runtime_error("Data structure is empty");
  std::ofstream out(Filename, std::ios::binary);
  if (!out)
    throw std::runtime_error("Problems with file handling");

  struct FileHeader {
    uint32_t magic;   // 'S2DS'
    uint32_t version; // 1
    uint32_t count;   // items
  } hdr{0x53324453u, 1u, static_cast<uint32_t>(total)};
  out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

  for (HEADER_C* h = pStruct2; h; h = h->pNext) {
    for (int i = 0; i < ALPHA; ++i) {
      for (pointer_to_item it = static_cast<pointer_to_item>(h->ppItems[i]); it;
           it = it->pNext) {
        uint32_t len = static_cast<uint32_t>(std::strlen(it->pID));
        out.write(reinterpret_cast<const char*>(&len), sizeof(len));
        out.write(it->pID, len);
        out.write(reinterpret_cast<const char*>(&it->Code), sizeof(it->Code));
        char hasTime = it->pTime ? 1 : 0;
        out.write(reinterpret_cast<const char*>(&hasTime), sizeof(hasTime));
        if (hasTime)
          out.write(reinterpret_cast<const char*>(it->pTime), sizeof(TIME));
      }
    }
  }
}

// 14. Print all items: "<ID> <Code>" per line
std::ostream& operator<<(std::ostream& ostr, const DataStructure& str) {
  for (HEADER_C* h = str.pStruct2; h; h = h->pNext) {
    for (int i = 0; i < ALPHA; ++i) {
      for (pointer_to_item it = static_cast<pointer_to_item>(h->ppItems[i]); it;
           it = it->pNext) {
        ostr << it->pID << " " << it->Code << std::endl;
      }
    }
  }
  return ostr;
}