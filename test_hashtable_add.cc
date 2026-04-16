#include <set>
#include <string>

#include "gtest/gtest.h"

extern "C" {
    #include "./HashTable.h"
    #include "./HashTable_priv.h"
    #include "./LinkedList.h"
    #include "./LinkedList_priv.h"
  }

#include "./test_suite.h"

using std::set;
using std::string;

namespace hw1 {
    class Test_HashTable : public ::testing::Test {
        
    };  // class Test_HashTable


    static void HTNoOpFree(HTValue_t freeme) { }
    
    TEST_F(Test_HashTable, Hashing) {
        HW1Environment::OpenTestCase();
        unsigned char buffer[2] = {0x11, 0xff};
        int len = 2;
        
        uint64_t expected_ans = 0x0865c907b51737f9ULL;

        ASSERT_EQ(expected_ans, FNVHash64(buffer, len));
        HW1Environment::AddPoints(0);
    }

    TEST_F(Test_HashTable, IteratorGoesOver) {
        static const int kTableSize = 2;
        static const int kNumIterations = kTableSize * 2.5;
        
        HW1Environment::OpenTestCase();
        unsigned int val[kNumIterations];
        HashTable *table = HashTable_Allocate(kTableSize);
        for (unsigned int i = 0; i < kNumIterations; i++) {
            val[i] = 2 * i;
            HTKeyValue_t keyvalue = {i, (HTValue_t)(val + i)};
            HTKeyValue_t oldval;
            HashTable_Insert(table, keyvalue, &oldval);
        }
        HTIterator *it = HTIterator_Allocate(table);

        for(int i = 0; i < kNumIterations; i++) {
            HTIterator_Next(it);
        }
        ASSERT_FALSE(HTIterator_Next(it));
        
        HTIterator_Free(it);
        HashTable_Free(table, &HTNoOpFree);
        HW1Environment::AddPoints(0);
    }


}