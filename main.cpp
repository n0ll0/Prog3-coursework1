#include "DataSource.h"
#include "DataStructure.h"
#include "DateTime.h"
#include "Headers.h"
#include "Items.h"
#include "Test.h"
#include <exception>
#include <iostream>

int main() {
  std::ios_base::sync_with_stdio(false);
  std::cin.tie(nullptr);
  try // uncomment you Struct
  {
    // HEADER_B* p1 = GetStruct1(5, 100);
    HEADER_C* p2 = GetStruct2(5, 100);
    // HEADER_A** pp3 = GetStruct3(5, 100);
    // HEADER_D* p4 = GetStruct4(5, 100);
    // HEADER_E* p5 = GetStruct5(5, 100);
    // std::cout << *p2 << "\n";

    DataStructure* Structure = new DataStructure(200);

    std::cout << *Structure << "\n";

    EvaluationTest<ITEM2>(2, "./Datastructure.bin");

  } catch (std::exception& e) {
    std::cout << e.what() << std::endl;
  }
  return 0;
}
