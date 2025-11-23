#include "DataStructure.h"

#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "DataSource.h"
#include "Headers.h"
#include "Items.h"

#pragma warning(disable : 4290)

// Implementation for Struct2 (HEADER_C + buckets for second-word initial)

// We implement the structure for ITEM2 only, as NITEM = 2
static const int NITEM = 2;
static const int ALPHA = 26;  // A..Z buckets for second word initial

// Default constructor: initializes the data structure with no items.
DataStructure::DataStructure() : pStruct2(nullptr) {}

// Helpers
namespace {
// map 'A'..'Z' to [0..25]; returns -1 if not a letter
int alphaIndex(char c) {
  unsigned char unsignedChar = static_cast<unsigned char>(c);
  char upperCaseChar = static_cast<char>(std::toupper(unsignedChar));
  if (upperCaseChar >= 'A' && upperCaseChar <= 'Z') return upperCaseChar - 'A';
  return -1;
}

// Extract first letters of first and second word from ID; throws on invalid
void parseID(const char* id, char& first, char& second) {
  if (!id) throw std::runtime_error("ID is null");

  // Find the position of the space character that separates the two words
  const char* spacePosition = std::strchr(id, ' ');

  // Validate that there is exactly one space: space must exist, not be at the
  // start, and not be at the end
  if (!spacePosition || spacePosition == id || *(spacePosition + 1) == '\0')
    throw std::runtime_error("ID must contain two words separated by space");

  // Extract the first letter of the first word (right after the start)
  first = id[0];

  // Extract the first letter of the second word (right after the space)
  second = *(spacePosition + 1);
}

// Create a fresh HEADER_C node for given first-letter
HEADER_C* makeHeader(char firstLetter) {
  HEADER_C* header = new HEADER_C;
  header->cBegin = firstLetter;
  header->pNext = nullptr;
  // allocate ALPHA bucket pointers and zero them
  header->ppItems = new void*[ALPHA];
  for (int i = 0; i < ALPHA; ++i) header->ppItems[i] = nullptr;
  return header;
}

// Find header by firstLetter; keep track of prior for insertion
HEADER_C* findHeader(HEADER_C* head, char firstLetter,
                     HEADER_C** previousHeader) {
  HEADER_C* previous = nullptr;
  HEADER_C* current = head;
  while (current && current->cBegin < firstLetter) {
    previous = current;
    current = current->pNext;
  }
  if (previousHeader) *previousHeader = previous;
  if (current && current->cBegin == firstLetter) return current;
  return nullptr;
}

// Insert header in sorted order after prev (may be null for list-head)
void insertHeaderSorted(HEADER_C*& head, HEADER_C* previous,
                        HEADER_C* newHeader) {
  if (!previous) {
    newHeader->pNext = head;
    head = newHeader;
  } else {
    newHeader->pNext = previous->pNext;
    previous->pNext = newHeader;
  }
}

// Deep copy a single ITEM2 (without next)
pointer_to_item cloneItem2(const pointer_to_item sourceItem) {
  pointer_to_item clonedItem = new ITEM2;
  size_t idLength = std::strlen(sourceItem->pID);
  clonedItem->pID = new char[idLength + 1];
  strcpy_s(clonedItem->pID, idLength + 1, sourceItem->pID);
  // std::memcpy();
  clonedItem->Code = sourceItem->Code;
  if (sourceItem->pTime) {
    clonedItem->pTime = new TIME;
    *clonedItem->pTime = *sourceItem->pTime;
  } else {
    clonedItem->pTime = nullptr;
  }
  clonedItem->pNext = nullptr;
  return clonedItem;
}

// Destroy item (ID string and time included)
void destroyItem2(pointer_to_item item) {
  if (!item) return;
  if (item->pID) delete[] item->pID;
  if (item->pTime) delete item->pTime;
  delete item;
}
}  // namespace

// 3. Constructor that creates data structure of n items using DataSource
DataStructure::DataStructure(int n) : pStruct2(nullptr) {
  if (n < 0) throw std::runtime_error("n cannot be negative");
  int itemsInserted = 0;
  const int batchSize = 32;
  while (itemsInserted < n) {
    int itemsToGenerate = std::min(batchSize, n - itemsInserted);
    pointer_to_item items[batchSize] = {nullptr};
    int itemsGenerated = 0;
    // Generate a batch of items
    for (int index = 0; index < itemsToGenerate; ++index) {
      items[index] = static_cast<pointer_to_item>(::GetItem(NITEM));
      if (!items[index]) throw std::runtime_error("Failed to generate item");
      items[index]->pNext = nullptr;
      ++itemsGenerated;
    }
    // Insert the batch
    for (int index = 0; index < itemsGenerated && itemsInserted < n; ++index) {
      try {
        *this += items[index];
        ++itemsInserted;
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
  // Open the binary file for reading
  std::ifstream inputFile(Filename, std::ios::binary);
  if (!inputFile)
    throw std::runtime_error("File not found or cannot be opened");

  // Define the file header structure matching the write format
  struct FileHeader {
    uint32_t magic;  // Magic number: 'S2DS' (0x53324453) to identify file type
    uint32_t version;  // Format version: currently 1
    uint32_t count;    // Number of items stored in the file
  } header{};

  // Read the file header from the beginning of the file
  inputFile.read(reinterpret_cast<char*>(&header), sizeof(header));
  // Validate the header: check if read succeeded, magic number matches, and
  // version is supported
  if (!inputFile || header.magic != 0x53324453u /*'S2DS'*/ ||
      header.version != 1)
    throw std::runtime_error("Invalid file format");

  // Read each item based on the count in the header
  for (uint32_t itemIndex = 0; itemIndex < header.count; ++itemIndex) {
    // Read the length of the ID string (uint32_t, not including null
    // terminator)
    uint32_t idLength = 0;
    inputFile.read(reinterpret_cast<char*>(&idLength), sizeof(idLength));
    if (!inputFile || idLength == 0)
      throw std::runtime_error("Corrupt file: invalid ID length");

    // Allocate a string to hold the ID and read it from the file
    std::string idString(idLength, '\0');
    // Safe to use &idString[0] since idLength > 0 and string is sized to
    // idLength
    inputFile.read(&idString[0], idLength);

    // Read the Code value (uint32_t)
    uint32_t itemCode = 0;
    inputFile.read(reinterpret_cast<char*>(&itemCode), sizeof(itemCode));

    // Read the hasTime flag (char: 1 if TIME struct follows, 0 otherwise)
    char hasTimeFlag = 0;
    inputFile.read(reinterpret_cast<char*>(&hasTimeFlag), sizeof(hasTimeFlag));

    // If hasTimeFlag is set, read the TIME structure
    TIME timeStruct{};
    if (hasTimeFlag)
      inputFile.read(reinterpret_cast<char*>(&timeStruct), sizeof(timeStruct));

    // Check for read errors after attempting to read the item data
    if (!inputFile) throw std::runtime_error("Corrupt file while reading item");

    // Construct a new ITEM2 object in memory
    pointer_to_item newItem = new ITEM2;
    // Allocate and copy the ID string, adding null terminator
    newItem->pID = new char[idLength + 1];
    std::memcpy(newItem->pID, idString.c_str(), idLength);
    newItem->pID[idLength] = '\0';
    // Set the Code value
    newItem->Code = itemCode;
    // Allocate and copy TIME if present, otherwise set to nullptr
    newItem->pTime = hasTimeFlag ? new TIME(timeStruct) : nullptr;
    // Ensure pNext is null for the new item
    newItem->pNext = nullptr;

    // Insert the item into the data structure using operator+=
    // This ensures the structure invariants are maintained (e.g., sorted
    // headers, unique IDs)
    *this += newItem;
  }
}

// 5. Destructor: delete items, buckets, and headers
DataStructure::~DataStructure() {
  HEADER_C* currentHeader = pStruct2;
  while (currentHeader) {
    // For each bucket 0..ALPHA-1 delete linked list of ITEM2
    if (currentHeader->ppItems) {
      for (int bucketIndex = 0; bucketIndex < ALPHA; ++bucketIndex) {
        pointer_to_item currentItem =
            static_cast<pointer_to_item>(currentHeader->ppItems[bucketIndex]);
        while (currentItem) {
          pointer_to_item nextItem = currentItem->pNext;
          destroyItem2(currentItem);
          currentItem = nextItem;
        }
        currentHeader->ppItems[bucketIndex] = nullptr;
      }
      delete[] currentHeader->ppItems;
    }
    HEADER_C* nextHeader = currentHeader->pNext;
    delete currentHeader;
    currentHeader = nextHeader;
  }
  pStruct2 = nullptr;
}

// 6. Copy constructor (deep copy)
DataStructure::DataStructure(const DataStructure& Original)
    : pStruct2(nullptr) {
  HEADER_C* sourceHeader = Original.pStruct2;
  HEADER_C* lastDestinationHeader = nullptr;
  while (sourceHeader) {
    HEADER_C* destinationHeader = makeHeader(sourceHeader->cBegin);
    // copy all buckets
    for (int bucketIndex = 0; bucketIndex < ALPHA; ++bucketIndex) {
      pointer_to_item bucketHead = nullptr;
      pointer_to_item bucketTail = nullptr;
      for (pointer_to_item sourceItem =
               static_cast<pointer_to_item>(sourceHeader->ppItems[bucketIndex]);
           sourceItem; sourceItem = sourceItem->pNext) {
        pointer_to_item copiedItem = cloneItem2(sourceItem);
        if (!bucketHead) {
          bucketHead = bucketTail = copiedItem;
        } else {
          bucketTail->pNext = copiedItem;
          bucketTail = copiedItem;
        }
      }
      destinationHeader->ppItems[bucketIndex] = bucketHead;
    }
    if (!pStruct2)
      pStruct2 = destinationHeader;
    else
      lastDestinationHeader->pNext = destinationHeader;
    lastDestinationHeader = destinationHeader;
    sourceHeader = sourceHeader->pNext;
  }
}

// 7. Number of items in data structure
int DataStructure::GetItemsNumber() {
  int itemCount = 0;
  for (HEADER_C* header = pStruct2; header; header = header->pNext) {
    for (int bucketIndex = 0; bucketIndex < ALPHA; ++bucketIndex) {
      for (pointer_to_item item =
               static_cast<pointer_to_item>(header->ppItems[bucketIndex]);
           item; item = item->pNext)
        ++itemCount;
    }
  }
  return itemCount;
}

// 8. Returns pointer to item with the specified ID
pointer_to_item DataStructure::GetItem(char* pID) {
  if (!pID) return nullptr;
  char firstLetter = 0, secondLetter = 0;
  try {
    parseID(pID, firstLetter, secondLetter);
  } catch (...) {
    return nullptr;
  }
  HEADER_C* previousHeader = nullptr;
  HEADER_C* header = findHeader(pStruct2, firstLetter, &previousHeader);
  if (!header) return nullptr;
  int bucketIndex = alphaIndex(secondLetter);
  if (bucketIndex < 0) return nullptr;
  pointer_to_item currentItem =
      static_cast<pointer_to_item>(header->ppItems[bucketIndex]);
  while (currentItem) {
    if (std::strcmp(currentItem->pID, pID) == 0) return currentItem;
    currentItem = currentItem->pNext;
  }
  return nullptr;
}

// 9. Add a new item into the data structure
void DataStructure::operator+=(pointer_to_item p) {
  if (!p) throw std::runtime_error("Null item pointer");
  pointer_to_item sourceItem = static_cast<pointer_to_item>(p);
  char firstLetter = 0, secondLetter = 0;
  parseID(sourceItem->pID, firstLetter, secondLetter);
  if (GetItem(sourceItem->pID))
    throw std::runtime_error("Item with this ID already exists");

  HEADER_C* previousHeader = nullptr;
  HEADER_C* header = findHeader(pStruct2, firstLetter, &previousHeader);
  if (!header) {
    header = makeHeader(firstLetter);
    insertHeaderSorted(pStruct2, previousHeader, header);
  }
  int bucketIndex = alphaIndex(secondLetter);
  if (bucketIndex < 0)
    throw std::runtime_error("Invalid ID: second word initial not a letter");
  // clone so we own the memory (safe to delete later)
  pointer_to_item clonedItem = cloneItem2(sourceItem);
  clonedItem->pNext =
      static_cast<pointer_to_item>(header->ppItems[bucketIndex]);
  header->ppItems[bucketIndex] = clonedItem;
}

// 10. Remove and destroy item with given ID
void DataStructure::operator-=(char* pID) {
  char firstLetter = 0, secondLetter = 0;
  parseID(pID, firstLetter, secondLetter);
  HEADER_C* previousHeader = nullptr;
  HEADER_C* header = findHeader(pStruct2, firstLetter, &previousHeader);
  if (!header) throw std::runtime_error("No item with the specified ID exists");
  int bucketIndex = alphaIndex(secondLetter);
  if (bucketIndex < 0)
    throw std::runtime_error("No item with the specified ID exists");
  pointer_to_item currentItem =
      static_cast<pointer_to_item>(header->ppItems[bucketIndex]);
  pointer_to_item previousItem = nullptr;
  while (currentItem) {
    if (std::strcmp(currentItem->pID, pID) == 0) {
      if (previousItem)
        previousItem->pNext = currentItem->pNext;
      else
        header->ppItems[bucketIndex] = currentItem->pNext;
      destroyItem2(currentItem);
      // If header becomes empty, remove it from list
      bool isHeaderEmpty = true;
      for (int bucketIndex = 0; bucketIndex < ALPHA; ++bucketIndex) {
        if (header->ppItems[bucketIndex]) {
          isHeaderEmpty = false;
          break;
        }
      }
      if (isHeaderEmpty) {
        if (!previousHeader)
          pStruct2 = header->pNext;
        else
          previousHeader->pNext = header->pNext;
        delete[] header->ppItems;
        delete header;
      }
      return;
    }
    previousItem = currentItem;
    currentItem = currentItem->pNext;
  }
  throw std::runtime_error("No item with the specified ID exists");
}

// 11. Assignment operator (deep copy)
DataStructure& DataStructure::operator=(const DataStructure& Right) {
  if (this == &Right) return *this;
  this->~DataStructure();
  new (this) DataStructure(Right);
  return *this;
}

// 12. Equality: same number of items and each ID present with same code/time
bool DataStructure::operator==(DataStructure& Other) {
  if (GetItemsNumber() != Other.GetItemsNumber()) return false;
  for (HEADER_C* header = pStruct2; header; header = header->pNext) {
    for (int bucketIndex = 0; bucketIndex < ALPHA; ++bucketIndex) {
      for (pointer_to_item itemA =
               static_cast<pointer_to_item>(header->ppItems[bucketIndex]);
           itemA; itemA = itemA->pNext) {
        pointer_to_item itemB =
            static_cast<pointer_to_item>(Other.GetItem(itemA->pID));
        if (!itemB) return false;
        if (itemA->Code != itemB->Code) return false;
        if (!!itemA->pTime != !!itemB->pTime) return false;
        if (itemA->pTime && itemB->pTime) {
          if (itemA->pTime->Hour != itemB->pTime->Hour ||
              itemA->pTime->Min != itemB->pTime->Min ||
              itemA->pTime->Sec != itemB->pTime->Sec)
            return false;
        }
      }
    }
  }
  return true;
}

// 13. Write structure to binary file (custom format)
void DataStructure::Write(std::string Filename) {
  int totalItems = GetItemsNumber();
  if (totalItems == 0) throw std::runtime_error("Data structure is empty");
  std::ofstream outputFile(Filename, std::ios::binary);
  if (!outputFile) throw std::runtime_error("Problems with file handling");

  struct FileHeader {
    uint32_t magic;    // 'S2DS'
    uint32_t version;  // 1
    uint32_t count;    // items
  } header{0x53324453u, 1u, static_cast<uint32_t>(totalItems)};
  outputFile.write(reinterpret_cast<const char*>(&header), sizeof(header));

  for (HEADER_C* header = pStruct2; header; header = header->pNext) {
    for (int bucketIndex = 0; bucketIndex < ALPHA; ++bucketIndex) {
      for (pointer_to_item item =
               static_cast<pointer_to_item>(header->ppItems[bucketIndex]);
           item; item = item->pNext) {
        uint32_t idLength = static_cast<uint32_t>(std::strlen(item->pID));
        outputFile.write(reinterpret_cast<const char*>(&idLength),
                         sizeof(idLength));
        outputFile.write(item->pID, idLength);
        outputFile.write(reinterpret_cast<const char*>(&item->Code),
                         sizeof(item->Code));
        char hasTimeFlag = item->pTime ? 1 : 0;
        outputFile.write(reinterpret_cast<const char*>(&hasTimeFlag),
                         sizeof(hasTimeFlag));
        if (hasTimeFlag)
          outputFile.write(reinterpret_cast<const char*>(item->pTime),
                           sizeof(TIME));
      }
    }
  }
}

// 14. Print all items: "<ID> <Code>" per line
std::ostream& operator<<(std::ostream& ostr, const DataStructure& str) {
  for (HEADER_C* header = str.pStruct2; header; header = header->pNext) {
    for (int bucketIndex = 0; bucketIndex < ALPHA; ++bucketIndex) {
      for (pointer_to_item item =
               static_cast<pointer_to_item>(header->ppItems[bucketIndex]);
           item; item = item->pNext) {
        ostr << item->pID << " " << item->Code << std::endl;
      }
    }
  }
  return ostr;
}