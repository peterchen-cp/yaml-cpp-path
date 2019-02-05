#include "yaml-path/yaml-path.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <yaml-path/yaml-path.h>

TEST_CASE("Let's try!")
{
   using namespace YAML;

   auto n = Load("alpha : 1");

   CHECK(PathAt(n, "") == n);
   CHECK(!PathAt(n, "xyz"));     // non-existing node
}
