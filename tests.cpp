#include "yaml-path/yaml-accumulate.h"
#include "yaml-path/yaml-path.h"

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <yaml-cpp/yaml.h>
#include <yaml-path/yaml-path.h>
#include <yaml-path/yaml-path-internals.h>
#include <iostream>

using namespace YAML;

namespace YAML
{
   template <typename TEnum>
   doctest::String DT2String(TEnum value, std::initializer_list<std::pair<TEnum, char const *>> map)
   {
      char const * p = YamlPathDetail::MapValue(value, map);
      if (p)
         return p;

      return (std::stringstream() << "(" << (int)value << ")").str().c_str(); 
   }

   doctest::String toString(EPathError value) { return DT2String(value, YamlPathDetail::MapEPathErrorName); }

   namespace YamlPathDetail
   {
      doctest::String toString(EToken value) { return DT2String(value, MapETokenName); }
      doctest::String toString(ESelector value) { return DT2String(value, MapESelectorName); }
   }
}

TEST_CASE("SelectByKey")
{
   {
      Node n = Load("{ A : aa, B : bb }"); 
      CHECK(SelectByKey(n, "A") == EPathError::OK);
      CHECK(n.as<std::string>() == "aa");
   }

   {
      Node n = Load("[ { A : aa, B : bb }, { X : xx, A : aaa } ]");
      CHECK(SelectByKey(n, "A") == EPathError::OK);
      CHECK(n.IsSequence());
      CHECK(n.size() == 2);
      CHECK(n[0].as<std::string>() == "aa");
      CHECK(n[1].as<std::string>() == "aaa");
   }

   {
      Node n = Load("{ A : aa, B : bb }");
      CHECK(SelectByKey(n, "X") == EPathError::NodeNotFound);
   }

   {
      Node n = Load("abcd");
      CHECK(SelectByKey(n, "X") == EPathError::InvalidNodeType);
   }
}

TEST_CASE("SelectByIndex")
{
   {
      Node n = Load("[1, 2, 3, 4]");
      CHECK(SelectByIndex(n, 0) == EPathError::OK);
      CHECK(n.as<std::string>() == "1");
   }

   {
      Node n = Load("[1, 2, 3, 4]");
      CHECK(SelectByIndex(n, 3) == EPathError::OK);
      CHECK(n.as<std::string>() == "4");
   }

   {
      Node n = Load("[1, 2, 3, 4]");
      CHECK(SelectByIndex(n, 7) == EPathError::NodeNotFound);
      CHECK(n.IsSequence());
   }

   {
      Node n = Load("abcd");
      CHECK(SelectByIndex(n, 0) == EPathError::OK);
      CHECK(n.as<std::string>() == "abcd");
   }

   {
      Node n = Load("abcd");
      CHECK(SelectByIndex(n, 1) == EPathError::NodeNotFound);
      CHECK(n.as<std::string>() == "abcd");
   }

}




// ---- parse level 0: SplitAt, Split
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

// ---- parse level 1: TokenScanner
TEST_CASE("Internal: TokenScanner")
{
   using namespace YamlPathDetail;
   {
      PathArg scanMe = "a.beta.'a b[c]'.\"a.b\".[].#.abc";
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

// --- parse level 2: selector scanner
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
      CheckSelectorError(".a", EPathError::InvalidToken, "", "a");
      CheckSelectorError("a.", EPathError::UnexpectedEnd, "a", "");
      CheckSelectorError("a..b", EPathError::InvalidToken, "a", "b");
      CheckSelectorError("a[.]", EPathError::InvalidIndex, "a", "]");
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


TEST_CASE("PathValidate")
{
   CHECK(PathValidate("") == EPathError::OK);
   CHECK(PathValidate("a") == EPathError::OK);
   CHECK(PathValidate("a.b") == EPathError::OK);
   CHECK(PathValidate("a.[2]") == EPathError::OK);
   CHECK(PathValidate("a[2]") == EPathError::OK);
   CHECK(PathValidate("[2]") == EPathError::OK);

   CHECK(PathValidate("~") == EPathError::InvalidToken);
   CHECK(PathValidate("[2[") == EPathError::InvalidToken);
   CHECK(PathValidate("[2222222222222222222222]") == EPathError::InvalidIndex);  // index overflows 64 bit uint
   CHECK(PathValidate(".a.b") == EPathError::InvalidToken);
   CHECK(PathValidate("].a.b") == EPathError::InvalidToken);
   CHECK(PathValidate("a.") == EPathError::UnexpectedEnd);
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
      auto n = CheckPathResolve(root, "[5]", "[5]");
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
      CHECK(PathResolve(node, path) == EPathError::OK);
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
      CHECK(PathResolve(node, path) == EPathError::OK);
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


TEST_CASE("PathResolve - MapFilter (adapted from initial syntax)")
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
      PathArg path = "{name=}"; // all having a name
      CHECK(PathResolve(node, path) == EPathError::OK);
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
      PathArg path = "{name=''}"; 
      CHECK(PathResolve(node, path) == EPathError::NodeNotFound);
      CHECK(path == "{name=''}");
      CHECK(node.IsSequence());
      CHECK(node.size() == 3);
      CHECK(node[2]["name"].as<S>() == "Estragon"); // let's call that "sufficient check if this is still the root node"
   }

   {  // has two nodes with any "place"
      auto node = YAML::Load(sroot);
      PathArg path = "{place=}";
      CHECK(PathResolve(node, path) == EPathError::OK);
      CHECK(path == "");
      CHECK(node.IsSequence());
      CHECK(node.size() == 2);
      CHECK(node[0]["name"].as<S>() == "Joe");
      CHECK(node[1]["name"].as<S>() == "Estragon");
   }

   {  // has three nodes with any color
      auto node = YAML::Load(sroot);
      PathArg path = "{color=}";
      CHECK(PathResolve(node, path) == EPathError::OK);
      CHECK(path == "");
      CHECK(node.IsSequence());
      CHECK(node.size() == 3);
   }

   {  // has three nodes with color "blue"
      auto node = YAML::Load(sroot);
      PathArg path = "{color=blue}";
      CHECK(PathResolve(node, path) == EPathError::OK);
      CHECK(path == "");
      CHECK(node.IsSequence());
      CHECK(node.size() == 2);
      CHECK(node[0]["name"].as<S>() == "Sina");
      CHECK(node[1]["name"].as<S>() == "Estragon");
   }
}


TEST_CASE("Accumulate (simple, tests AccumulateRefOp)")
{
   {
      auto n = Load("[2, 3, 4, 5]");
      int result = Accumulate<int>(n, 1);
      CHECK(result == 15);
   }


   {
      auto n = Load("{ a : 2, b : 3, c : 4, d : 5}");
      int result = Accumulate<int>(n, 1);
      CHECK(result == 15);
   }

   {
      auto n = Load("23");
      int result = Accumulate<int>(n, 17);
      CHECK(result == 40);
   }
}

TEST_CASE("Accumulate with custom op")
{
   auto n = Load("[2, 3, 4, 5]");
   int result = Accumulate<int>(n, 1, [](int a, int b) {return a * b; });
   CHECK(result == 120);
}



bool is(char const * a, char const * b) { return _stricmp(a, b) == 0; }
bool is(char const * a, char const * b, char const * balt) { return is(a,b) || (balt && is(a,balt)); }

int main(int argc, char ** argv)
{
   auto is_ = [&](int argidx, char const * b, char const * balt = nullptr)
   {
      return argc > argidx && is(argv[argidx], b, balt);
   };

   if (is_(1, "--runtests", "-r"))
      return doctest::Context(argc-1, argv+1).run();

   if (argc == 1 || (argc == 2 && is_(1, "--help", "-h")))
   {
      std::cout << R"(Options:
 --runtest, -r   (must be first) Run unit tests. all following arguments are passed to doctest.

 --help, -h      (must be the only argument) show this help

 <YAMLFile> <path> [-v|--verbose] [<command>]

   Run a YAML path command against the specified YAML

   YAMLFile can be: 

                  a path to a YAML file,
       *          for the embedded YAML sample,
       *<yaml>    where <yaml> is a raw YAML string

   <path> is a YAML path

   -v to enable verbose output, appending secondary data and diagnostics

   <command> is the command to run 
     Select or S (default)
     Require or R
     PathResolve or P
     PathValidate or V
)";
      return 0;
   }

   if (argc >= 2 && argc <= 5)
   {
      const bool verbose = is_(3, "--verbose", "-v");

      try
      {
         YAML::Node root;
         char const * sroot = argv[1];

         if (*sroot == '*')
         {
            ++sroot;
            if (*sroot)
               root = YAML::Load(sroot);
            else
               root = YAML::Load(R"(
-  name : Joe
   color: red
   friends : ~
-  name : Sina
   color: blue
-  name : Estragon
   color : red
   friends :
      Wladimir : good
      Godot : unreliable)");
         }
         else
            root = YAML::LoadFile(sroot);

         char const * yamlPath = "";
         if (argc >= 3)
            yamlPath = argv[2];

         int cmdidx = verbose ? 4 : 3;
         if (is_(cmdidx, "S", "Select") || argc <= cmdidx || is_(cmdidx, ""))    // Select
         {
            auto result = YAML::Select(root, yamlPath);
            std::cout << (YAML::Emitter() << result).c_str() << "\n";
         }
         else if (is_(cmdidx, "R", "Require"))
         {
            auto result = YAML::Require(root, yamlPath);
            std::cout << (YAML::Emitter() << result).c_str() << "\n";
         }
         else if (is_(cmdidx, "P", "PathResolve"))
         {
            YAML::PathException x;
            YAML::PathArg path = yamlPath;
            auto result = PathResolve(root, path, {}, verbose ? &x : nullptr);

            std::cout << (YAML::Emitter() << root).c_str() << "\n";

            if (verbose)
            {
               std::cout << "---\n";
               if (result != YAML::EPathError::OK)
                  std::cout << "DIAGS: " << x.what();
               else
                  std::cout << "DIAGS: OK\n";

               std::cout << "---\n"
                  << "remaining path: " << std::string(path);
            }
         }
         else if (is_(cmdidx, "V", "PathValidate"))
         {
            YAML::PathException x;
            std::string valid;
            size_t erroffs = 0;
            auto result = YAML::PathValidate(yamlPath, &valid, &erroffs);

            std::cout << "---\n" << YAML::PathException::GetErrorMessage(result) << "\n"
               << "valid path: " << valid << "\n"
               << "error offset: " << erroffs << "\n";
         }
         else
            throw std::exception("unknown command");
      }
      catch (std::exception const & x)
      {
         if (verbose)
            std::cout << "---\nERROR: " << x.what() << "\n";
      }
   }
   else
      std::cout << "unknown arguments. use -h for help.\n";
}
