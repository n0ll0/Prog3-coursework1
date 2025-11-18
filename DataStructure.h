#pragma once

#include "DataSource.h"
#include "Headers.h"
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>

class DataStructure {
private:
  HEADER_C* pStruct2;

public:
  DataStructure();
  DataStructure(int n);
  DataStructure(std::string Filename);
  ~DataStructure();
  DataStructure(const DataStructure& Original);
  int GetItemsNumber();
  pointer_to_item GetItem(char* pID);
  void operator+=(pointer_to_item p);
  void operator-=(char* pID);
  DataStructure& operator=(const DataStructure& Right);
  bool operator==(DataStructure& Other);
  void Write(std::string Filename);
  friend std::ostream& operator<<(std::ostream& ostr, const DataStructure& str);
};
