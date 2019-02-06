#include "yaml-path/yaml-path.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <yaml-cpp/yaml.h>
#include <yaml-path/yaml-path.h>
#include <yaml-path/yaml-path-internals.h>

using namespace YAML;

TEST_CASE("Let's try!")
{
   auto n = Load("alpha : 1");

   CHECK(PathAt(n, "") == n);
   CHECK(!PathAt(n, "xyz"));     // non-existing node
}

TEST_CASE("Internal: SplitAt")
{
   using namespace YAML::YamlPathDetail;

   auto Check = [](yaml_path p, size_t offset, yaml_path expectedResult, yaml_path expectedP)
   {
      CHECK(p.size() == expectedResult.size() + expectedP.size());
      auto result = SplitAt(p, offset);
      CHECK(result == expectedResult);
      CHECK(p == expectedP);
   };

   Check("", 0, "", "");
   Check("", 1, "", "");

   Check("a", 0, "", "a");
   Check("a", 1, "a", "");
   Check("a", 2, "a", "");

   Check("abc", 0, "", "abc");
   Check("abc", 1, "a", "bc");
   Check("abc", 2, "ab", "c");
   Check("abc", 3, "abc", "");
   Check("abc", 4, "abc", "");
}

namespace
{
   void CheckSplit(yaml_path p, yaml_path expectedResult, yaml_path expectedP)
   {
      using namespace YAML::YamlPathDetail;

      CHECK(p.size() == expectedResult.size() + expectedP.size());
      auto result = Split(p, isdigit);
      CHECK(result == expectedResult);
      CHECK(p == expectedP);
   }

}

TEST_CASE("Internal: Split")
{
   CheckSplit("", "", "");
   CheckSplit("12ab", "12", "ab");
   CheckSplit("1234", "1234", "");
}
