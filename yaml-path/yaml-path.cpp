/*
MIT License

Copyright(c) 2019 Peter Hauptmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "yaml-path.h"
#include "yaml-path-internals.h"
#include <yaml-cpp/yaml.h>
#include <assert.h>

namespace YAML
{

   // ----- PathException
   std::string const & PathException::What() const
   {
      if (!m_what.length())
         return m_what;
      switch (m_error)
      {
      case EPathError::None:   return m_what = "OK";
      case EPathError::InvalidToken:
         return m_what = (std::stringstream() << "Invalid Token at position " << m_offset << ": " << m_value).str();

      case EPathError::IndexExpected:
         return m_what = (std::stringstream() << "Index expected at position " << m_offset << ": " << m_value).str();

      default:
         return m_what = (std::stringstream() << "Undefined exception #" << (int)m_error << " at offset " << m_offset << ": " << m_value).str();
      }
   }

   void PathException::ThrowDerived()
   {
      switch (m_error)
      {
         case EPathError::None: assert(false);  return;
         case EPathError::Internal: throw PathInternalException(m_offset, m_value);
         case EPathError::InvalidToken: throw PathInvalidTokenException(m_offset, m_value);
         case EPathError::IndexExpected: throw PathIndexExpectedException(m_offset, m_value);
         case EPathError::InvalidIndex: throw PathInvalidIndexException(m_offset, m_value);
         case EPathError::InvalidNodeType: throw PathInvalidNodeTypeException(m_offset, m_value);
         case EPathError::NodeNotFound: throw PathNodeNotFoundException(m_offset, m_value);
         default: throw *this;
      }
   }

   namespace YamlPathDetail
   {
      // ----- Utility functions

      /// Creates an undefined YAML node (<code>(bool)UndefinedNode() == false</code>)
      Node UndefinedNode()
      {
         static auto undefinedNode = Node()["x"];
         return undefinedNode;
      }

      /// result = target; target = newValue
      template <typename T>
      T Exchange(T & target, T newValue)
      {
         std::swap(target, newValue);
         return newValue;
      }


      /** splits path at offset, 
          returning everything left of [offset], assigning everything right of it to \c path. 
      */
      path_arg SplitAt(path_arg & path, size_t offset)
      {
         if (offset == 0)
            return path_arg();

         if (offset >= path.size())
            return Exchange(path, path_arg());

         path_arg result = path.substr(0, offset);
         path = path.substr(offset);
         return result;
      }

      /// converts path_arg to a n unsigned integer; if it is not a valid index, nullopt is returned
      std::optional<size_t> AsIndex(path_arg p)
      {
         size_t value = 0;
         size_t prev = value;
         size_t index = 0;
         while (index < p.size())
         {
            char c = p[index];
            ++index;
            if (c < '0' || c >'9')
               return std::nullopt;

            value = value * 10 + (c - '0');
            if (value < prev)  // overflow
               return std::nullopt;
            prev = value;
         }
         return value;
      }

      // ----- TokenScanner
      EToken GetSingleCharToken(char c, std::initializer_list<std::pair<char, EToken>> values)
      {
         for (auto && v : values)
            if (v.first == c)
               return v.second;
         return EToken::None;
      }


      EToken TokenScanner::Select(EToken tok, path_arg p)
      {
         // Generate error when ValidTokens are specified:
         if (ValidTokens != 0 && !BitsContain(ValidTokens, tok))
         {
            SkipWS();
            SetError(EPathError::InvalidToken);

            // if that doesn't throw...
            assert(m_token == EToken::Invalid);
            m_value = std::move(p);
            return m_token;
         }

         m_token = tok;
         m_value = std::move(p);

         /* skipping whitespace after token, so that if this was the last token,
            we get to the end of the string and operator bool becomes false */
         SkipWS();
         return tok;
      }

      void TokenScanner::SkipWS()
      {
         Split(m_rpath, isspace);
      }

      inline TokenScanner::TokenScanner(path_arg p) : m_rpath(p), m_all(p)
      {
         SkipWS();
         ValidTokens = ValidTokensAtStart;
      }

      EToken TokenScanner::Next()
      {
         if (m_rpath.empty())
            return Select(EToken::None, path_arg());

         if (m_curException)
            return m_token;

         // single-char special tokens
         char head = m_rpath[0];
         EToken t = GetSingleCharToken(head, {
            { '.', EToken::Period },
            { '[', EToken::OpenBracket },
            { ']', EToken::CloseBracket },
            });

         if (t != EToken::None)
            return Select(t, SplitAt(m_rpath, 1));

         // quoted token
         if (head == '\'' || head == '"')
         {
            size_t end = m_rpath.find(m_rpath[0], 1);
            if (end == std::string::npos)
               return Select(EToken::Invalid, path_arg());

            return Select(EToken::QuotedIdentifier, SplitAt(m_rpath, end + 1).substr(1, end - 1));
         }

         // unquoted token
         auto result = Split(m_rpath, [](char c) { return !isspace(c) && !ispunct(c); });
         if (result.empty())
         {
            return SetError(EPathError::InvalidToken), m_token;
         }

         return Select(EToken::UnquotedIdentifier, result);
      }


      inline void TokenScanner::SetError(EPathError error)
      {
         assert(error != EPathError::None);
         m_token = EToken::Invalid;
         m_curException = PathException(error, ScanOffset(), std::string(Value()));
         if (ThrowOnError)
            m_curException->ThrowDerived();
      }

   } // YamlPathDetail

   using namespace YamlPathDetail;

   void TokenScanner::Resolve(YAML::Node * node)
   {
      enum class EContext
      {
         Base,
         Index,
      } ctx = EContext::Base;

      while (*this)
      {
         auto token = Next();
         switch (token)
         {
            case EToken::None:
            case EToken::Invalid:
               return;

            case EToken::OpenBracket:
               assert(ctx == EContext::Base); 
               ctx = EContext::Index;
               ValidTokens = BitsOf({ EToken::UnquotedIdentifier });
               continue;

            case EToken::CloseBracket:
               assert(ctx == EContext::Index);
               ctx = EContext::Base;
               ValidTokens = TokenScanner::ValidTokensAtBase;
               continue;

            case EToken::Period:
               ValidTokens = BitsOf({ EToken::OpenBracket, EToken::QuotedIdentifier, EToken::UnquotedIdentifier});
               continue;

            case EToken::UnquotedIdentifier:
               if (ctx == EContext::Index)
               {
                  auto index = AsIndex(Value());
                  if (!index)
                     return SetError(EPathError::IndexExpected);

                  ValidTokens = BitsOf({ EToken::CloseBracket });
                  if (!node)
                     continue;

                  if (node->IsSequence())
                  {
                     if (*index >= node->size())
                        return SetError(EPathError::InvalidIndex);
                     node->reset((*node)[*index]);
                     continue;
                  }

                  if (node->IsScalar())
                  {
                     if (*index != 0)
                        return SetError(EPathError::InvalidIndex);
                     // node remains the same
                     continue;
                  }
                  return SetError(EPathError::InvalidNodeType);
               }
               // fall through

            case EToken::QuotedIdentifier:
               assert(ctx == EContext::Base);
               ValidTokens = BitsOf({ EToken::Period, EToken::OpenBracket, EToken::None });
               if (!node)
                  continue;

               if (!*node)
                  continue;

               if (node->IsMap())
               {
                  Node newNode = (*node)[std::string(Value())];
                  if (!newNode)
                     return SetError(EPathError::NodeNotFound);
                  node->reset(newNode);
               }
               continue;

            default:
               assert(false);  // all cases should have been covered. This is just a simple ad-hoc parser
               return SetError(EPathError::Internal);
         }

      }
      Select(EToken::None, path_arg());
   }

   /** validates the syntax of a YAML path. returns an error for invalid path, or EPathError::None, if the path is valid
   */
   EPathError PathValidate(path_arg p, size_t * scanOffs)
   {
      TokenScanner scan(p);
      scan.ThrowOnError = false;
      scan.ValidTokens = TokenScanner::ValidTokensAtStart;
      scan.Resolve(nullptr);
      if (scanOffs)
         *scanOffs = scan.ScanOffset();
      return scan.CurrentException() ? scan.CurrentException()->Error() : EPathError::None;
   }

   void PathResolve(YAML::Node & node, path_arg & path)
   {
      TokenScanner scan(path);
      scan.ValidTokens = TokenScanner::ValidTokensAtStart;
      scan.ThrowOnError = false;
      scan.Resolve(&node);
      path = scan.Right();
   }

   Node PathAt(YAML::Node node, path_arg path)
   {
      PathResolve(node, path);
      return path.length() ? UndefinedNode() : node;
   }
} // namespace YAML
