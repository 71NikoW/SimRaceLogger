#include "check.h"

int g_failures = 0;

TestCase*& testRegistry() {
  static TestCase* head = nullptr;
  return head;
}

int main() {
  int count = 0;
  for (TestCase* tc = testRegistry(); tc; tc = tc->next, ++count) {
    const int before = g_failures;
    tc->fn();
    printf("%s %s\n", g_failures == before ? "ok  " : "FAIL", tc->name);
  }
  printf("\n%d tests, %d failed checks\n", count, g_failures);
  return g_failures == 0 ? 0 : 1;
}
