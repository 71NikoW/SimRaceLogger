#pragma once
// Minimal self-contained test harness (no downloads needed).
#include <stdio.h>
#include <string.h>

struct TestCase {
  const char* name;
  void (*fn)();
  TestCase* next;
};
TestCase*& testRegistry();
extern int g_failures;

struct TestRegistrar {
  TestRegistrar(TestCase* tc) {
    tc->next = testRegistry();
    testRegistry() = tc;
  }
};

#define TEST(name)                                                   \
  static void name();                                                \
  static TestCase name##_case{#name, name, nullptr};                 \
  static TestRegistrar name##_reg(&name##_case);                     \
  static void name()

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      ++g_failures;                                                          \
      printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);               \
    }                                                                        \
  } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_STR(a, b)                                                      \
  do {                                                                       \
    if (strcmp((a), (b)) != 0) {                                             \
      ++g_failures;                                                          \
      printf("  FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); \
    }                                                                        \
  } while (0)
