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

      case EPathError::InvalidIndex:
         return m_what = (std::stringstream() << "Index expected at position " << m_offset << ": " << m_value).str();

      case EPathError::UnexpectedEnd:
         return m_what = (std::stringstream() << "unexpected end of path at position " << m_offset << ": " << m_value).str();

      case EPathError::InvalidNodeType:
         return m_what = (std::stringstream() << "node type mismatch at path position " << m_offset << ": " << m_value).str();

      case EPathError::NodeNotFound:
         return m_what = (std::stringstream() << "node not found at path position " << m_offset << ": " << m_value).str();

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
         case EPathError::InvalidIndex: throw PathInvalidIndexException(m_offset, m_value);
         case EPathError::UnexpectedEnd: throw PathUnexpectedEndException(m_offset, m_value);

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
      template <typename T1, typename T2>
      T1 Exchange(T1 & target, T2 newValue)
      {
         T1 result = std::move(target);
         target = std::move(newValue);
         return std::move(result);
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

      // ----- TokenScanner
      EToken GetSingleCharToken(char c, std::initializer_list<std::pair<char, EToken>> values)
      {
         for (auto && v : values)
            if (v.first == c)
               return v.second;
         return EToken::None;
      }


      Token const & TokenScanner::SetToken(EToken id, path_arg p)
      {
         // Generate error when ValidTokens are specified:
         m_curToken = { id, std::move(p) };

         /* skipping whitespace after token, so that if this was the last token,
            we get to the end of the string and operator bool becomes false */
         SkipWS();
         return m_curToken;
      }

      void TokenScanner::SkipWS()
      {
         Split(m_rpath, isspace);
      }

      inline TokenScanner::TokenScanner(path_arg p) : m_rpath(p), m_all(p)
      {
         SkipWS();
      }

      Token const & TokenScanner::NextToken()
      {
         if (m_rpath.empty())
            return SetToken(EToken::None, path_arg());

         if (m_curException)
            return m_curToken;

         // single-char special tokens
         char head = m_rpath[0];
         EToken t = GetSingleCharToken(head, {
            { '.', EToken::Period },
            { '[', EToken::OpenBracket },
            { ']', EToken::CloseBracket },
            });

         if (t != EToken::None)
            return SetToken(t, SplitAt(m_rpath, 1));

         // quoted token
         if (head == '\'' || head == '"')
         {
            size_t end = m_rpath.find(m_rpath[0], 1);
            if (end == std::string::npos)
               return SetToken(EToken::Invalid, path_arg());

            return SetToken(EToken::QuotedIdentifier, SplitAt(m_rpath, end + 1).substr(1, end - 1));
         }

         // unquoted token
         auto result = Split(m_rpath, [](char c) { return !isspace(c) && !ispunct(c); });
         if (result.empty())
            return SetError(EPathError::InvalidToken), m_curToken;

         return SetToken(EToken::UnquotedIdentifier, result);
      }

      EPathError TokenScanner::SetError(EPathError error)
      {
         assert(error != EPathError::None);
         m_curException = PathException(error, ScanOffset(), std::string(m_curToken.value));
         m_curToken = { EToken::Invalid };
         SetSelector(ESelector::Invalid, ArgNull{});
         return error;
      }

      std::optional<size_t> TokenScanner::AsIndex()
      {
         if (m_curToken.id != EToken::UnquotedIdentifier)
            return std::nullopt;

         size_t value = 0;
         size_t index = 0;
         for(auto c : m_curToken.value)
         {
            ++index;
            if (c < '0' || c >'9')
               return std::nullopt;

            size_t prev = value;
            value = value * 10 + (c - '0');
            if (value < prev)  // unsigned integer overflow
               return SetError(EPathError::InvalidIndex), std::nullopt;
         }
         return value;
      }

      bool TokenScanner::NextSelectorToken(uint64_t validTokens, EPathError error)
      {
         if (!m_tokenPending)
            NextToken();
         m_tokenPending = false;

         if (BitsContain(validTokens, m_curToken.id))
            return true;

         SetError(error);
         return false;
      }

      ESelector TokenScanner::NextSelector()
      {
         // sticky on error
         if (m_curException)
            return ESelector::Invalid;


         // skip period if allowed at this point
         if (m_periodAllowed)
         {
            if (!NextSelectorToken(ValidTokensAtStart | BitsOf({ EToken::Period })))
               return ESelector::Invalid;
            m_periodAllowed = false;
            
            if (m_curToken.id == EToken::Period)
               m_selectorRequired = true;    // path cannot end with period after selector
            else
               m_tokenPending = true;        // if not a token, stuff this token back
         }

         // Next token
         if (!NextSelectorToken(ValidTokensAtStart))
            return ESelector::Invalid;

         // analyze token
         switch (m_curToken.id)
         {
            case EToken::None:
               if (m_selectorRequired)
                  return SetError(EPathError::UnexpectedEnd), ESelector::Invalid;
               return ESelector::None;

            case EToken::QuotedIdentifier:
            case EToken::UnquotedIdentifier:
            {
               SetSelector(ESelector::Key, ArgKey{ m_curToken.value });
               m_leftOffset = ScanOffset();
               m_periodAllowed = true;
               return m_selector;
            }

            case EToken::OpenBracket:
            {
               if (!NextSelectorToken(BitsOf({ EToken::UnquotedIdentifier }), EPathError::InvalidIndex))
                  return ESelector::Invalid;

               auto index = AsIndex();
               if (m_curException)
                  return ESelector::Invalid;
               if (index)
               {
                  if (*index == (size_t) -1)
                     return SetError(EPathError::InvalidIndex), ESelector::Invalid;

                  if (!NextSelectorToken(BitsOf({ EToken::CloseBracket })))
                     return ESelector::Invalid;

                  m_periodAllowed = true;
                  m_leftOffset = ScanOffset();
                  return SetSelector(ESelector::Index, ArgIndex{ *index });
               }
               return SetError(EPathError::InvalidIndex), ESelector::Invalid;
            }
         }
         return ESelector::Invalid;
      }



   } // YamlPathDetail


   /** validates the syntax of a YAML path. returns an error for invalid path, or EPathError::None, if the path is valid
   */
   EPathError PathValidate(path_arg p, path_arg * valid, size_t * scanOffs)
   {
      YamlPathDetail::TokenScanner scan(p);
      while (scan)
         scan.NextSelector();

      if (valid)
         *valid= scan.Left();

      if (scanOffs)
         *scanOffs = scan.ScanOffset();
      return scan.CurrentException() ? scan.CurrentException()->Error() : EPathError::None;
   }

   namespace
   {
      bool IsMatch(Node node)
      {
         return !(!node ||
            node.IsNull() ||
            (node.IsSequence() && node.size() == 0) ||
            (node.IsMap() && node.size() == 0));
      }

      EPathError SeqMapByKey(Node & node, path_arg key)
      {
         /**
         \todo Optimization: YAML::Node could uses a reserve
         \todo Optimization: convert<string_view> without copy to string
         */
         YAML::Node newNode;
         if (node.IsSequence())
         {
            for (auto && el : node)
               if (el.IsMap())
               {
                  Node value = el[std::string(key)];
                  if (value)
                     newNode.push_back(value);
               }
         }
         else if (node.IsMap())
            newNode = node[std::string(key)];
         else
            return EPathError::InvalidNodeType;

         if (!IsMatch(newNode))
            return EPathError::NodeNotFound;

         node.reset(newNode);
         return EPathError::None;
      }
   }

   EPathError PathResolve(Node & node, path_arg & path)
   {
      using namespace YamlPathDetail;
      TokenScanner scan(path);

      while (scan)
      {
         if (!node)
            return EPathError::NodeNotFound;

         path = scan.Right(); // path is updated only when both the selector is valid, and it selects a valid node. 
            
         switch (scan.NextSelector())
         {
            case ESelector::Key:
            if (auto err = SeqMapByKey(node, scan.SelectorData<ArgKey>().key); err != EPathError::None)
               return err;
            continue;

            case ESelector::Index:
            {
               size_t index = scan.SelectorData<ArgIndex>().index;
               if (node.IsScalar())
               {
                  if (index != 0)   // for scalar node, [0] sticks to the node
                     return EPathError::NodeNotFound;
                  continue;
               }
               if (node.IsSequence())
               {
                  if (index >= node.size())
                     return EPathError::NodeNotFound;

                  node.reset(node[index]);
                  continue;
               }
               return EPathError::InvalidNodeType;     // cannot go in
            }

            default:
               assert(false);    // no other selectors supported right now
         }
      }
      path = scan.Right();
      return EPathError::None;
   }

   Node PathAt(YAML::Node node, path_arg path)
   {
      PathResolve(node, path);
      return path.length() ? YamlPathDetail::UndefinedNode() : node;
   }
} // namespace YAML


/**

\todo PathValidate, PathResolve etc. swallow some diagnostics. 
      "CurrentException" needs to be replaced / refactored. 
      PathResolve etc. should (optionally) provide diagnostics (both for parse errors, and node errors)

\todo names:
   - PathError is now a path-or-node error
   - token scanner now scans both tokens and selectors
   - Should "PathAt" be called" Select"?
*/
