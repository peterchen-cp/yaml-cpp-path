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

   namespace YamlPathDetail
   {
      doctest::String toString(EToken value) { return (std::stringstream() << (int)value).str().c_str(); }
      doctest::String toString(ESelector value) { return (std::stringstream() << (int)value).str().c_str(); }
   }
}

TEST_CASE("Internal: SplitAt")
{
   using namespace YAML::YamlPathDetail;

   auto Check = [](PathArg p, size_t offset, PathArg expectedResult, PathArg expectedP)
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
   void CheckSplit(PathArg p, PathArg expectedResult, PathArg expectedP)
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
      PathArg scanMe = "a.beta.'a b[c]'.\"a.b\".[].~.abc";
      PathScanner scan(scanMe);
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
   CHECK(PathValidate("[2222222222222222222222]") == EPathError::InvalidIndex);  // index overflows 64 but uint
   CHECK(PathValidate(".a.b") == EPathError::InvalidToken);
   CHECK(PathValidate("].a.b") == EPathError::InvalidToken);
   // TODO: CHECK(PathValidate("a.") == EPathError::InvalidToken);
}

YAML::Node CheckPathResolve(YAML::Node node, YAML::PathArg path, std::string expectedRemainder)
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
      auto n = CheckPathResolve(root, "[5]", "[5]");  // TODO: out-of-range offset should not move forward
      CHECK(n.size() == 3);
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


TEST_CASE("PathResolve - SeqMap")
{
   char const * sroot =
      R"(
-  name : Joe
   color: red
   names: 3
-  name : Sina
   color: blue
   names: 4
-  name : Estragon
   voice : none)";

   using S = std::string;

   {
      auto node = YAML::Load(sroot);
      PathArg path = "name";
      CHECK(PathResolve(node, path) == EPathError::None);
      CHECK(path == "");
      CHECK(node.IsSequence());
      CHECK(node.size() == 3);
      CHECK(node[0].as<S>("") == "Joe");
      CHECK(node[1].as<S>("") == "Sina");
      CHECK(node[2].as<S>("") == "Estragon");
   }

   {
      auto node = YAML::Load(sroot);
      PathArg path = "voice";
      CHECK(PathResolve(node, path) == EPathError::None);
      CHECK(path == "");
      CHECK(node.IsSequence());
      CHECK(node.size() == 1);
      CHECK(node[0].as<S>("") == "none");
   }

   {
      auto node = YAML::Load(sroot);
      PathArg path = "xyz";
      CHECK(PathResolve(node, path) == EPathError::NodeNotFound);
      CHECK(path == "xyz");
      CHECK(node.IsSequence());
      CHECK(node.size() == 3);
      CHECK(node[0].IsMap());
   }

}


TEST_CASE("PathResolve - SeqMapFilter")
{
   char const * sroot =
      R"(
-  name : Joe
   color: red
   place: here
-  name : Sina
   color: blue
-  name : Estragon
   color: blue
   place: there)";

   using S = std::string;

   
   {  // has 3 nodes with any "name"
      auto node = YAML::Load(sroot);
      PathArg path = "[name=]"; // all having a name
      CHECK(PathResolve(node, path) == EPathError::None);
      CHECK(path == "");
      CHECK(node.IsSequence());
      CHECK(node.size() == 3);

      CHECK(node[0].IsMap());
      CHECK(node[0].size() == 3);
      CHECK(node[0]["name"].as<S>() == "Joe");
      CHECK(node[0]["color"].as<S>() == "red");

      CHECK(node[1].IsMap());
      CHECK(node[1].size() == 2);
      CHECK(node[1]["name"].as<S>() == "Sina");
      CHECK(node[1]["color"].as<S>() == "blue");

      CHECK(node[2].IsMap());
      CHECK(node[2].size() == 3);
      CHECK(node[2]["name"].as<S>() == "Estragon");
      CHECK(node[2]["color"].as<S>() == "blue");
   }

   {  // has no node with empty "name"
      auto node = YAML::Load(sroot);
      PathArg path = "[name='']"; // all having a name
      CHECK(PathResolve(node, path) == EPathError::NodeNotFound);
      CHECK(path == "[name='']");
      CHECK(node.IsSequence());
      CHECK(node.size() == 3);
      CHECK(node[2]["name"].as<S>() == "Estragon"); // let's call that "sufficient check if this is still the root node"
   }

   {  // has two nodes with any "place"
      auto node = YAML::Load(sroot);
      PathArg path = "[place=]";
      CHECK(PathResolve(node, path) == EPathError::None);
      CHECK(path == "");
      CHECK(node.IsSequence());
      CHECK(node.size() == 2);
      CHECK(node[0]["name"].as<S>() == "Joe");
      CHECK(node[1]["name"].as<S>() == "Estragon");
   }

   {  // has three nodes with any color
      auto node = YAML::Load(sroot);
      PathArg path = "[color=]";
      CHECK(PathResolve(node, path) == EPathError::None);
      CHECK(path == "");
      CHECK(node.IsSequence());
      CHECK(node.size() == 3);
   }

   {  // has three nodes with color "blue"
      auto node = YAML::Load(sroot);
      PathArg path = "[color=blue]";
      CHECK(PathResolve(node, path) == EPathError::None);
      CHECK(path == "");
      CHECK(node.IsSequence());
      CHECK(node.size() == 2);
      CHECK(node[0]["name"].as<S>() == "Sina");
      CHECK(node[1]["name"].as<S>() == "Estragon");
   }
}

namespace
{
   void CheckSelectorError(std::string_view s, EPathError expectedError, std::string_view expectedResolved, std::string_view expectedRight)
   {
      using namespace YamlPathDetail;
      PathException x;
      PathScanner scan(s, {}, &x);

      while (1)
      {
         auto st = scan.NextSelector();
         CHECK(st != ESelector::None);
         if (st == ESelector::Invalid)
            break;
      }
      CHECK(scan.Error() == expectedError);
      CHECK(x.ResolvedPath() == expectedResolved);
      CHECK(scan.Right() == expectedRight);

      // TODO: check expected diagnostics?
   }
}


TEST_CASE("ScanSelector")
{
   using namespace YamlPathDetail;
   {
      PathScanner scan("1.test.'xyz'.[0].abc[2][3]");

      CHECK(scan.NextSelector() == ESelector::Key);
      CHECK(scan.SelectorData< ArgKey>().key == "1");

      CHECK(scan.NextSelector() == ESelector::Key);
      CHECK(scan.SelectorData<ArgKey>().key == "test");

      CHECK(scan.NextSelector() == ESelector::Key);
      CHECK(scan.SelectorData<ArgKey>().key == "xyz");

      CHECK(scan.NextSelector() == ESelector::Index);
      CHECK(scan.SelectorData<ArgIndex>().index == 0);

      CHECK(scan.NextSelector() == ESelector::Key);
      CHECK(scan.SelectorData<ArgKey>().key == "abc");

      CHECK(scan.NextSelector() == ESelector::Index);
      CHECK(scan.SelectorData<ArgIndex>().index == 2);

      CHECK(scan.NextSelector() == ESelector::Index);
      CHECK(scan.SelectorData<ArgIndex>().index == 3);
   }

   {
      // these tests go a little bit into implementation details, 
      // particularly "expectedRight" and "expectedErrorValue" are for diagnostic purposes only, and not exactly guaranteed by the API.
      // However, we check here that they make SOME sense, i.e. not be totally off
      CheckSelectorError(".a", EPathError::InvalidToken, "",      "a");
      CheckSelectorError("a.", EPathError::UnexpectedEnd, "a",    "");
      CheckSelectorError("a..b", EPathError::InvalidToken,        "a", "b");
      CheckSelectorError("a[.]", EPathError::InvalidIndex, "a",   "]");
   }
}

TEST_CASE("ScanSelector - bound arguments")
{
   using namespace YamlPathDetail;
   {
      PathScanner scan("node.%.[%].edon", { "param", 42 });

      CHECK(scan.NextSelector() == ESelector::Key);
      CHECK(scan.SelectorData< ArgKey>().key == "node");

      CHECK(scan.NextSelector() == ESelector::Key);
      CHECK(scan.SelectorData< ArgKey>().key == "param");

      CHECK(scan.NextSelector() == ESelector::Index);
      CHECK(scan.SelectorData< ArgIndex>().index == 42);

      CHECK(scan.NextSelector() == ESelector::Key);
      CHECK(scan.SelectorData< ArgKey>().key == "edon");
   }
}
