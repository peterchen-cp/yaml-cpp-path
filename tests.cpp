#include "yaml-path/yaml-path.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>
#include <yaml-cpp/yaml.h>
#include <yaml-path/yaml-path.h>
#include <yaml-path/yaml-path-internals.h>

using namespace YAML;

namespace YAML
{
   doctest::String toString(EPathError value) { return (std::stringstream() << (int)value).str().c_str(); }
}

TEST_CASE("Internal: SplitAt")
{
   using namespace YAML::YamlPathDetail;

   auto Check = [](path_arg p, size_t offset, path_arg expectedResult, path_arg expectedP)
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
   void CheckSplit(path_arg p, path_arg expectedResult, path_arg expectedP)
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

TEST_CASE("Internal: TokenScanner")
{
   using namespace YamlPathDetail;
   {
      path_arg scanMe = "a.beta.'a b[c]'.\"a.b\".[].~.abc";
      TokenScanner scan(scanMe);
      scan.ThrowOnError = false;
      CHECK(scan);
      CHECK(scan.NextToken().id == EToken::UnquotedIdentifier); 
      CHECK(scan.Token().value == "a");

      CHECK(scan);
      CHECK(scan.NextToken().id == EToken::Period);
      CHECK(scan.NextToken().id == EToken::UnquotedIdentifier);
      CHECK(scan.Token().value == "beta");

      CHECK(scan.NextToken().id == EToken::Period);
      CHECK(scan.NextToken().id == EToken::QuotedIdentifier);
      CHECK(scan.Token().value== "a b[c]");

      CHECK(scan.NextToken().id == EToken::Period);
      CHECK(scan.NextToken().id == EToken::QuotedIdentifier);
      CHECK(scan.Token().value == "a.b");

      CHECK(scan.NextToken().id == EToken::Period);
      CHECK(scan.NextToken().id == EToken::OpenBracket);
      CHECK(scan.NextToken().id == EToken::CloseBracket);

      CHECK(scan.NextToken().id == EToken::Period);
      CHECK(scan.NextToken().id == EToken::Invalid);

      CHECK(!scan);  // at end when invalid
      CHECK(scan.NextToken().id == EToken::Invalid); // remains at "invalid"
   }
}

TEST_CASE("PathValidate")
{
   CHECK(PathValidate("") == EPathError::None);
   CHECK(PathValidate("a") == EPathError::None);
   CHECK(PathValidate("a.b") == EPathError::None);
   CHECK(PathValidate("a.[2]") == EPathError::None);
   CHECK(PathValidate("a[2]") == EPathError::None);
   CHECK(PathValidate("[2]") == EPathError::None);

   CHECK(PathValidate("~") == EPathError::InvalidToken);
   CHECK(PathValidate("[2[") == EPathError::InvalidToken);
   CHECK(PathValidate("[2222222222222222222222]") == EPathError::IndexExpected);  // index overflows 64 but uint
   CHECK(PathValidate(".a.b") == EPathError::InvalidToken);
   CHECK(PathValidate("].a.b") == EPathError::InvalidToken);
   CHECK(PathValidate("a.") == EPathError::InvalidToken);
}

YAML::Node CheckPathResolve(YAML::Node node, YAML::path_arg path, std::string expectedRemainder)
{
   PathResolve(node, path);
   CHECK(path == expectedRemainder);
   return node;
}

TEST_CASE("PathResolve - Sequence")
{
   YAML::Node root = YAML::Load("[1, 2, 3]");

   {
      auto n = CheckPathResolve(root, "", "");
      CHECK(n == root); // should be the same node
   }

   {
      auto n = CheckPathResolve(root, "[0]", "");
      CHECK(n.as<std::string>() == "1");
   }

   {
      auto n = CheckPathResolve(root, "[1]", "");
      CHECK(n.as<std::string>() == "2");
   }

   {
      auto n = CheckPathResolve(root, "[2]", "");
      CHECK(n.as<std::string>() == "3");
   }

   {
      //auto n = CheckPathResolve(root, "[4]", "[4]");  // TODO: start offset should stick to base
      //CHECK(!n);
   }
}

TEST_CASE("PathResolve - Map")
{
   char const * sroot = 
R"(1 : Hello
2 : World
3 :
  - SeqA
  - SeqB
  - { Letter : X, Digit : 4 })";

   using S = std::string;

   auto root = YAML::Load(sroot);

   {
      auto n = CheckPathResolve(root, "1", "");
      CHECK(n.as<S>() == "Hello");
   }

   {
      auto n = CheckPathResolve(root, "2", "");
      CHECK(n.as<S>() == "World");
   }

   {
      auto n = CheckPathResolve(root, "3", "");
      CHECK(n.IsSequence());
   }

   {
      auto n = CheckPathResolve(root, "3[0]", "");
      CHECK(n.as<S>() == "SeqA");
   }

   {
      auto n = CheckPathResolve(root, "3[2].Letter", "");
      CHECK(n.as<S>() == "X");
   }

   {
      auto n = CheckPathResolve(root, "3[2].Digit", "");
      CHECK(n.as<S>() == "4");
   }
}
