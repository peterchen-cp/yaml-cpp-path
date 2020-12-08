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
#include <algorithm>
#include <cstring>
#include <strings.h>

/// namspace shared by yaml-cpp and yaml-path
namespace YAML
{

   EPathError SelectByKey(Node & node, PathArg key)
   {
      // possible optimizations: reserve for a node sequence, node[string_view] without copy-to-string
      if (node.IsMap())
      {
         Node result = node[std::string(key)];
         if (!result)
            return EPathError::NodeNotFound;

         node.reset(result);
         return EPathError::OK;
      }
      else if (node.IsSequence())
      {
         Node result;
         for (auto && el : node)
         {
            Node val = el[std::string(key)];
            if (val)
               result.push_back(val);
         }
         if (!result.IsSequence())    // first call to push-back turns this into a sequence
            return EPathError::NodeNotFound;
         node.reset(result);
         return EPathError::OK;
      }
      return EPathError::InvalidNodeType;
   }

   EPathError SelectByIndex(Node & node, size_t index)
   {
      if (node.IsScalar() || node.IsMap())
      {
         if (index != 0)
            return EPathError::NodeNotFound;
         return EPathError::OK;     // for scalar node and map, [0] remains at the same node
      }
      if (node.IsSequence())
      {
         if (index >= node.size())
            return EPathError::NodeNotFound;

         node.reset(node[index]);
         return EPathError::OK;
      }
      return EPathError::InvalidNodeType;
   }


   namespace YamlPathDetail
   {
      /// \internal uses the same mapping as \ref MapValue to create diagnostic for a bit mask (e.g. created by \ref BitsOf)
      template <typename T2, typename TBit, typename TMask>
      std::string MapBitMask(TMask value, std::initializer_list<std::pair<TBit, T2>> values, T2 sep = ", ")
      {
         std::stringstream result;
         bool noSep = true;
         auto Sep = [&]() -> std::stringstream & { if (!Exchange(noSep, false)) result << sep; return result; };
         for (auto && p : values)
         {
            TMask mask = ((TMask)1) << static_cast<TMask>(p.first);
            if (value & mask)
            {
               Sep() << p.second;
               value &= ~mask;
            }
         }
         if (value != 0)
            Sep() << "(" << std::hex << value << "h)";
         return result.str();
      }

      /// \internal name mapping for EToken
      std::initializer_list<std::pair<EToken, char const *>> MapETokenName =
      {
         { EToken::OpenBracket, "open bracket"},
         { EToken::CloseBracket, "closing bracket" },
         { EToken::Equal, "equal" },
         { EToken::None, "end of path" },
         { EToken::Period, "period" },
         { EToken::QuotedIdentifier, "quoted identifier" },
         { EToken::UnquotedIdentifier, "unquoted identifier" },
         { EToken::Index, "Index" },
         { EToken::FetchArg, "bound argument" },
         { EToken::OpenBrace, "open brace" },
         { EToken::CloseBrace, "close brace" },
         { EToken::Exclamation, "exclamation mark" },
         { EToken::Caret, "caret" },
         { EToken::Asterisk, "asterisk" },
         { EToken::Tilde, "tilde" },
         { EToken::Comma, "comma" },
      };

      /// \internal name mapping for yaml-cpp node type
      std::initializer_list<std::pair<NodeType::value, char const *>> MapNodeTypeName =
      {
         { NodeType::Map, "map" },
         { NodeType::Sequence, "sequence" },
         { NodeType::Scalar , "scalar" },
         { NodeType::Null, "(null)" },
         { NodeType::Undefined, "(undefined)" },
      };

      /// \internal name mapping for ESelector
      std::initializer_list<std::pair<ESelector, char const *>> MapESelectorName =
      {
         { ESelector::Index,  "index" },
         { ESelector::Key,    "key" },
         { ESelector::MapFilter, "map filter" },
         { ESelector::None, "(none)" },
         { ESelector::Invalid, "(invalid)" },
      };

      /// \internal name mapping for EPathError
      std::initializer_list<std::pair<EPathError, char const *>> MapEPathErrorName =
      {
         { EPathError::OK ,               "(OK)" },
         { EPathError::Internal,          "(internal, please report)" },
         { EPathError::InvalidIndex,      "invalid index" },
         { EPathError::InvalidNodeType,   "selector cannot not match node type" },
         { EPathError::InvalidToken,      "invalid token" },
         { EPathError::NodeNotFound,      "no node matches selector" },
         { EPathError::UnexpectedEnd,     "unexpected end of path" },
         { EPathError::SelectorNotSupported, "selector not supported by this operation" },
      };

      // ----- Utility functions

      /// C\internal reates an undefined YAML node (<code>(bool)UndefinedNode() == false</code>)
      Node UndefinedNode()
      {
         static auto undefinedNode = Node()["x"];
         return undefinedNode;
      }

      /// \internal result = target; target = newValue
      template <typename T1, typename T2>
      T1 Exchange(T1 & target, T2 newValue)
      {
         T1 result = std::move(target);
         target = std::move(newValue);
         return std::move(result);
      }


      /** \internal splits path at offset, 
          returning everything left of [offset], assigning everything right of it to \c path. 
      */
      PathArg SplitAt(PathArg & path, size_t offset)
      {
         if (offset == 0)
            return PathArg();

         if (offset >= path.size())
            return Exchange(path, PathArg());

         PathArg result = path.substr(0, offset);
         path = path.substr(offset);
         return result;
      }

      /// \internal Helper used by \c NextToken etc to set m_curToken
      TokenData const & PathScanner::SetToken(EToken id, PathArg p)
      {
         // Generate error when ValidTokens are specified:
         m_curToken = { id, std::move(p) };

         if (id != EToken::Invalid && m_diags)
            m_diags->m_offsTokenScan = ScanOffset();

         /* skipping whitespace after token, so that if this was the last token,
            we get to the end of the string and operator bool becomes false */
         SkipWS();
         return m_curToken;
      }

      /// \internal Helper used by \c NextToken etc to set m_curToken with an index value
      YAML::YamlPathDetail::TokenData const & PathScanner::SetToken(EToken id, size_t index)
      {
         m_curToken = { id, {}, index };

         if (id != EToken::Invalid && m_diags)
            m_diags->m_offsTokenScan = ScanOffset();

         SkipWS();
         return m_curToken;
      }

      /// \internal removes whitespace from the path
      void PathScanner::SkipWS()
      {
         // non-ascii chars are NOT considered whitespace
         Split(m_rpath, [](char c) { return isascii(c) && isspace(c); });
      }

      inline PathScanner::PathScanner(PathArg p, PathBoundArgs args, PathException * diags) : m_rpath(p), m_args(args), m_diags(diags), m_fullPath(p)
      {
         if (m_diags)
            *m_diags = PathException();
         SkipWS();
      }

      /** \internal removes the next token from the path and makes it the current one.
      */
      TokenData const & PathScanner::NextToken()
      {
         if (m_rpath.empty())
            return SetToken(EToken::None, PathArg());

         if (m_error != EPathError::OK)
            return m_curToken;

         // single-char special tokens
         char head = m_rpath[0];
         EToken t = MapValue(head, {
            { '.', EToken::Period },
            { '[', EToken::OpenBracket },
            { ']', EToken::CloseBracket },
            { '{', EToken::OpenBrace },
            { '}', EToken::CloseBrace },
            { '=', EToken::Equal },
            { '%', EToken::FetchArg },
            { '!', EToken::Exclamation },
            { '^', EToken::Caret },
            { '*', EToken::Asterisk },
            { '~', EToken::Tilde },
            { ',', EToken::Comma },
            }, EToken::None);

         if (t != EToken::None)
         {
            SplitAt(m_rpath, 1);
            return SetToken(t, {});
         }

         // quoted token
         if (head == '\'' || head == '"')
         {
            size_t end = m_rpath.find(m_rpath[0], 1);
            if (end == std::string::npos)
               return SetToken(EToken::Invalid, PathArg());

            return SetToken(EToken::QuotedIdentifier, SplitAt(m_rpath, end + 1).substr(1, end - 1));
         }

         // unquoted token. non-ascii characters ARE treated as part of the token.
         auto result = Split(m_rpath, [](char c) { return !isascii(c) || !(isspace(c) || ispunct(c)); });
         if (result.empty())
            return SetError(EPathError::InvalidToken), m_curToken;

         return SetToken(EToken::UnquotedIdentifier, result);
      }

      /** \internal Helper to indicate an error during token or selector parsing */
      EPathError PathScanner::SetError(EPathError error, uint64_t validTypes)
      {
         assert(error != EPathError::OK);
         m_error = error;
         if (m_diags)
         {
            m_diags->m_fullPath = m_fullPath;
            m_diags->m_error = error;
            m_diags->m_validTypes = validTypes;

            if (PathException::IsPathError(error))
               m_diags->m_errorType = (decltype(m_diags->m_errorType))m_curToken.id;
            else if (PathException::IsNodeError(error))
               m_diags->m_errorType = (decltype(m_diags->m_errorType))m_selector;

         }
         m_curToken = { EToken::Invalid };
         SetSelector(ESelector::Invalid, ArgNull{});
         return error;
      }

      enum class EAsIndex { OK, NotAnIndex, IndexOverflow };

      /** \internal tries to convert a string token to integer */
      EAsIndex AsIndex(std::string_view v, size_t & index)
      {
         size_t value = 0;
         for (auto c : v)
         {
            if (c < '0' || c >'9')
               return EAsIndex::NotAnIndex;

            size_t prev = value;
            value = value * 10 + (c - '0');
            if (value < prev)  // unsigned integer overflow
               return EAsIndex::IndexOverflow;
         }
         index = value;
         return EAsIndex::OK;
      }

      /** Supports pending token, but neither fetches args nor translates to index. 
          if the token is not the expected one, it's unread and false is returned. Otherwise, it's read and true is returned.
      */
      bool PathScanner::PeekSelectorToken(uint64_t validTokens)
      {
         if (m_tokenPending)
            m_tokenPending = false;    // re-use previous token once after pushing it back
         else
            NextToken();

         if (BitsContain(validTokens, m_curToken.id))
            return true;

         m_tokenPending = true;
         return false;
      }

      /** \internal Used by the selector scanner to retrieve and process the next token
         In addition to \c NextToken, this

           - allows to put back one token by setting \c m_tokenPending to true
           - checks if the token retrieved is one of the tokens supported, and sets an error if not
           - converts the current token to an index if an index token is allowed, and the current string is convertable
      */
      bool PathScanner::NextSelectorToken(uint64_t validTokens, EPathError error)
      {
         if (m_tokenPending)
            m_tokenPending = false;    // re-use previous token once after pushing it back
         else
            NextToken();

         /// \todo bug: tokenPending can not push back FetchArg

         // Fetch argument from argument list if required
         if (m_curToken.id == EToken::FetchArg)
         {
            auto & v = m_args.begin()[m_argIdx];
            if (m_diags)
               m_diags->m_fromBoundArg = m_argIdx;
            ++m_argIdx;

            switch (v.index())
            {
               case 0:  SetToken(EToken::Index, std::get<size_t>(v)); break;
               case 1:  SetToken(EToken::QuotedIdentifier, std::get<std::string_view>(v)); break;
               default: SetError(EPathError::Internal); return false;      // TODO: InvalidArg error code?
            }
         }

         // translate unquoted token to index, if caller accepts an index
         if (m_curToken.id == EToken::UnquotedIdentifier && BitsContain(validTokens, EToken::Index))
         {
            switch (AsIndex(m_curToken.value, m_curToken.index))
            {
               case EAsIndex::OK:               m_curToken.id = EToken::Index; break;
               case EAsIndex::IndexOverflow:    SetError(EPathError::InvalidIndex); return false;  // TODO: IndexOverflow error code?
               case EAsIndex::NotAnIndex:       break;
            }
         }

         if (BitsContain(validTokens, m_curToken.id))
            return true;

         if (m_curToken.id == EToken::None)     // change any error to "unexpected end" if EToken::None is found but not allowed
            error = EPathError::UnexpectedEnd;
         SetError(error, validTokens);
         return false;
      }

      bool PathScanner::ReadKVToken(KVToken & kvtoken, uint64_t endTokens)
      {
         kvtoken = KVToken();

         const auto nameTokens = BitsOf({ EToken::FetchArg, EToken::QuotedIdentifier, EToken::UnquotedIdentifier });
         auto validTokens = BitsOf({ EToken::Exclamation, EToken::Caret, EToken::Asterisk }) | nameTokens;
         while (true)
         {
            if (!NextSelectorToken(validTokens))
               return false;

            switch (m_curToken.id)
            {
               case EToken::Exclamation:
                  validTokens &= ~BitsOf({ EToken::Exclamation });
                  kvtoken.required = true;
                  continue;

               case EToken::Caret:
                  validTokens &= ~BitsOf({ EToken::Caret });
                  kvtoken.noCase = true;
                  continue;

               case EToken::QuotedIdentifier:
               case EToken::UnquotedIdentifier:
               case EToken::FetchArg:
                  validTokens &= ~(nameTokens | BitsOf({ EToken::Caret, EToken::Exclamation }));
                  validTokens |= endTokens;
                  kvtoken.token = m_curToken.value;
                  continue;

               case EToken::Asterisk:
                  validTokens = endTokens;
                  kvtoken.starry = true;
                  continue;

               default:
                  m_tokenPending = true; // stuff it back
                  if (BitsContain(endTokens, m_curToken.id))
                     return true;

                  return false;
            }
         }
      }

      /** retrieves the next selector. */
      ESelector PathScanner::NextSelector()
      {
         // sticky on error
         if (m_error != EPathError::OK)
            return ESelector::Invalid;

         if (m_diags)
            m_diags->m_offsSelectorScan = ScanOffset();

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
                  return SetError(EPathError::UnexpectedEnd, ValidTokensAtStart), ESelector::Invalid;
               return ESelector::None;

            case EToken::QuotedIdentifier:
            case EToken::UnquotedIdentifier:
            {
               SetSelector(ESelector::Key, ArgKey{ m_curToken.value });
               m_periodAllowed = true;
               return m_selector;
            }

            case EToken::OpenBracket:
            {
               if (!NextSelectorToken(BitsOf({ EToken::Index }), EPathError::InvalidIndex))
                  return ESelector::Invalid;

               const size_t index = m_curToken.index;
               if (!NextSelectorToken(BitsOf({ EToken::CloseBracket })))
                  return ESelector::Invalid;

               m_periodAllowed = true;
               return SetSelector(ESelector::Index, ArgIndex{ index });
            }


            case EToken::OpenBrace:
            {
               ArgMapFilter arg; /// \todo optimization: a std::vector replacement with a small buffer optimization of length 1 would be pretty useful here
               while (true)
               {
                  ArgKVPair kvp;
                  if (!ReadKVToken(kvp.key, BitsOf({ EToken::Tilde, EToken::Equal, EToken::Comma, EToken::CloseBrace })))
                     return ESelector::Invalid;

                  if (!NextSelectorToken(BitsOf({ EToken::Tilde, EToken::Equal, EToken::Comma, EToken::CloseBrace})))
                     return ESelector::Invalid;


                  bool atEnd = false;
                  switch (m_curToken.id)
                  {
                     case EToken::Tilde:
                        if (!NextSelectorToken(BitsOf({ EToken::Equal })))
                           return ESelector::Invalid;
                        kvp.op = EKVOp::NotEqual;
                        break;

                     case EToken::Equal:
                        kvp.op = EKVOp::Equal;
                        break;

                     case EToken::Comma:
                        kvp.op = EKVOp::Select;
                        arg.push_back(kvp);
                        continue;

                     case EToken::CloseBrace:
                        kvp.op = EKVOp::Select;
                        arg.push_back(kvp);
                        atEnd = true;
                        break;
                  }
                  if (atEnd)
                     break;

                  if (PeekSelectorToken(BitsOf({ EToken::CloseBrace, EToken::Comma })))
                  {
                     m_tokenPending = true;
                     if (kvp.op == EKVOp::Equal) kvp.op = EKVOp::Exists;
                     else if (kvp.op == EKVOp::NotEqual)
                        return SetError(EPathError::InvalidToken), ESelector::Invalid;    // not equal must have value
                     else 
                        return SetError(EPathError::Internal), ESelector::Invalid;
                  }
                  else
                     if (!ReadKVToken(kvp.value, BitsOf({ EToken::Comma, EToken::CloseBrace })))
                        return ESelector::Invalid;

                  if (!NextSelectorToken(BitsOf({ EToken::Comma, EToken::CloseBrace })))
                     return ESelector::Invalid;

                  arg.push_back(kvp);
                  if (m_curToken.id == EToken::Comma)
                     continue;

                  break;
               }

               // partition: move conditions to front, selectors to the back. Allows arbitrary ordering
               std::partition(arg.begin(), arg.end(), [](ArgKVPair const & kvp) { return kvp.op != EKVOp::Select;  });

               m_periodAllowed = true;
               return SetSelector(ESelector::MapFilter, std::move(arg));
            }
         }
         return ESelector::Invalid;
      }
   } // YamlPathDetail


   using namespace YamlPathDetail;

   /** determines and caches one small part of the diagnostic message on demand */
   std::string PathException::ErrorItem() const
   {
      if (!m_errorItem.length() && m_errorType)
      {
         if (IsNodeError())
            m_errorItem = MapValue((ESelector)m_errorType, MapESelectorName, "");
         else if (IsPathError())
            m_errorItem = MapValue((EToken)m_errorType, MapETokenName, "");
      }
      return m_errorItem;
   }

   /** returns a diagnostic message. 
       if \c detailed is true, the message contains information about the parser state (such as the error position, expected tokens, etc.) 
       on multiple lines. Otherwise, it is a generic single-line message.
   */
   std::string const & PathException::What(bool detailed) const
   {
      if (!m_short.length())
         m_short = GetErrorMessage(m_error);

      if (!detailed || m_error == EPathError::OK)
         return m_short;

      if (m_detailed.length())
         return m_detailed;

      std::stringstream str;
      str << m_short << "\n";

      str << "  error at path offset: " << m_offsTokenScan << "\n";

      if (m_fromBoundArg)
         str << "  token taken from bound arg #" << *m_fromBoundArg << "\n";

      if (IsPathError())
      {
         if (m_validTypes)
            str << "  allowed tokens: " << MapBitMask(m_validTypes, MapETokenName) << "\n";
         if (m_errorType)
            str << "  token found: " << ErrorItem() << "\n";
      }
      else if (IsNodeError())
      {
         if (m_validTypes)
            str << "  supported node type: " << MapBitMask(m_validTypes, MapNodeTypeName) << "\n";
         if (m_errorType)
            str << "  for selector: " << ErrorItem() << "\n";
      }

      if (m_fullPath.length())
         str << "  path to parse: " << m_fullPath << "\n";

      str << "  resolved path: " << ResolvedPath() << "\n";

      return m_detailed = str.str();
   }

   /** returns a generic message for the \c error given */
   std::string PathException::GetErrorMessage(EPathError error)
   {
      return MapValue(error, MapEPathErrorName, "");
   }


   /** validates the syntax of a YAML path. returns an error for invalid path, or EPathError::None, if the path is valid */
   EPathError PathValidate(PathArg p, std::string * valid, size_t * errorOffs)
   {
      PathException x;
      PathScanner scan(p, {}, &x);
      while (scan)
         scan.NextSelector();

      if (valid)
         *valid = x.ResolvedPath();

      if (errorOffs)
         *errorOffs = x.ErrorOffset();
      return scan.Error(); 
   }


   namespace YamlPathDetail
   {
      bool StrIsMatch(KVToken const & tok, Node const & node)
      {
         if (!node.IsScalar())
            return false;

         if (tok.IsAllStar())
            return true;

         std::string snode = node.as<std::string>();

         // length checks that allow to skip comparisons
         // Unicode: the length checks would be applicable only on case sensitive comparison after normalization. 
         if (!tok.starry && snode.length() != tok.token.length())    // non-starry equality requires identical length
            return false;

         if (tok.starry && snode.length() < tok.token.length())      // "cccc*" cannot match "cccc", whatever the c's are
            return false;
         // --

         // Unicode: some assumptions here don't hold. It's strcoll, stricoll, and I'm not sure how to implement no-case starry matches
         size_t cmpLen = std::min(tok.token.length(), snode.length());
         int result = tok.noCase ? strncasecmp(&tok.token[0], snode.c_str(), cmpLen) : strncmp(&tok.token[0], snode.c_str(), cmpLen);

         return result == 0; // under assumption of above length-based shortcuts
      }

      bool KeyIsMatch(ArgKVPair const & arg, Node const & key)
      {
         return StrIsMatch(arg.key, key);
      }

      bool ValueIsMatch(ArgKVPair const & arg, Node const & value)
      {
         if (arg.op == EKVOp::Exists)
            return true;      // any value, including non-scalars and null, is a match

         bool eq = StrIsMatch(arg.value, value);
         if (arg.op == EKVOp::Equal)
            return eq;

         if (arg.op == EKVOp::NotEqual)
            return !eq;

         assert(false); // unknown / unsupported op
         return false;
      }

      EPathError ApplyMapFilterToMap(Node & node, ArgMapFilter const & arg)
      {
         ArgMapFilter::const_iterator argit = arg.begin();

         // --- for each condition (they are in the beginning of the list):
         bool anyMatch = false;
         for (; argit != arg.end() && argit->op != EKVOp::Select; ++argit) // selects are already sorted to the end of the list
         {
            KVToken const & key = argit->key;
            const bool scanKeys = key.starry || key.noCase; // cannot use the index operator, need to check keys one-by-one

            if (scanKeys)
            {
               for (auto keyit = node.begin(); keyit != node.end(); ++keyit)
               {
                  if (!KeyIsMatch(*argit, keyit->first))
                     continue;

                  if (ValueIsMatch(*argit, keyit->second))
                  {
                     anyMatch = true;
                     break; // don't scan further keys if we have a match in this map already
                  }
               }
            }
            else
            {
               Node el = node[std::string(key.token)];
               if (!el && key.required)
                  return EPathError::NodeNotFound;    // required key was not present

               if (el && ValueIsMatch(*argit, el))
                  anyMatch = true;
               // still have to test further conditions, since there may be other required keys
            }

            if (key.required && !anyMatch)
               return EPathError::NodeNotFound;     // required key was not present
         } // scan all conditions

         if (!anyMatch && argit != arg.begin())  // no match, but there were some conditions
            return EPathError::NodeNotFound;

         // --- select specified keys

         if (argit == arg.end())    // no selector follows the conditions - entire node is selected
            return EPathError::OK;

         Node result;
         for (; argit != arg.end(); ++argit)
         {
            assert(argit->op == EKVOp::Select);
            KVToken const & key = argit->key;

            if (key.IsAllStar())      // entire node is selected
               return EPathError::OK;

            const bool scanKeys = key.starry || key.noCase;

            if (scanKeys)
            {
               for (auto keyit = node.begin(); keyit != node.end(); ++keyit)
               {
                  if (KeyIsMatch(*argit, keyit->first))
                     result[keyit->first] = keyit->second;
               }
            }
            else
            {
               auto value = node[std::string(key.token)];
               if (value)
                  result[std::string(key.token)] = value;
            }
         }
         if (!result.IsMap())
            return EPathError::NodeNotFound;

         node.reset(result);
         return EPathError::OK;
      }
   }

   
   /** Match a YAML path as far as possible

      Matches nodes as long as a valid selector can be removed from the head of \c path and nodes can be matched.
      See \ref Select for an introduction and documentation of YAML paths and selector matching.

      \param node 
        [in] the node where to start to match \c path \n
        [out] the last node that could be matched

      \param path
        [in]  the path to match \n
        [out] the remainder of the path that could not be matched

      \param args
        List of bound arguments, see the respective paragraph in \ref Select

      \param  px
        If not \c nullptr: receives detailed diagnostics if an error occurs.

      \returns Error code that occurred during matching. \c EPathError::None if the entire path could be matched. 
      You can uses \ref PathException::IsNodeError and \ref PathException::IsPathError to check what kind of error occurred.
   */
   EPathError PathResolve(Node & node, PathArg & path, PathBoundArgs args, PathException * px)
   {
      PathScanner scan(path, args, px);

      while (scan)
      {
         if (!node)      // should not trigger except on initial node being undefined (and then only if there is a path given)
            return scan.SetError(EPathError::NodeNotFound);

         path = scan.Right(); // path is updated only when both the selector is valid, and it selects a valid node. 
            
         switch (scan.NextSelector())
         {
            case ESelector::Key:
               if (auto err = SelectByKey(node, scan.SelectorData<ArgKey>().key); err != EPathError::OK)
                  return scan.SetError(err);
            continue;

            case ESelector::Index:
            {
               size_t index = scan.SelectorData<ArgIndex>().index;
               if (auto err = SelectByIndex(node, index); err != EPathError::OK)
                  return scan.SetError(err);
               continue;
            }

            case ESelector::MapFilter:
            {
               auto && arg = scan.SelectorData<ArgMapFilter>();
               EPathError err = EPathError::OK;
               if (node.IsMap())
               {
                  if (auto err = ApplyMapFilterToMap(node, arg); err != EPathError::OK)
                     return scan.SetError(err);
                  continue;
               }

               if (node.IsSequence())
               {
                  Node result;
                  for (auto && el : node)
                  {
                     if (!el.IsMap())
                        continue;
                     auto err = ApplyMapFilterToMap(el, arg);
                     if (err != EPathError::OK)
                        continue;
                     result.push_back(el);
                  }
                  if (!result.IsSequence())    // node didn't become a sequence if nothing did match
                     return scan.SetError(EPathError::NodeNotFound);
                  node.reset(result);
                  continue;
               }

               return scan.SetError(EPathError::InvalidNodeType);
            }

            case ESelector::Invalid:
               return scan.Error();

            default:
               assert(false);    // no other selectors supported right now
               return scan.Error();
         }
      }
      path = scan.Right();
      return EPathError::OK;
   }

   /** Selects one or more sub nodes from \c node, according to the specification in \c path

      \par Error Handling

      if \c path is malformed, an \ref PathException exception is thrown.\n

      if no node can be selected (e.g. the index is out of range, or the node type can not be matched), an <i>invalid node</i> is returned.
      Invalid nodes evaluate to false in a boolean context. Note that in yaml-cpp, there is a distinction between null nodes and invalid nodes.

      \c Select may throw exceptions from yaml-cpp if \c node is malformed. It is intended to not throw such exceptions otherwise.

      \sa Require, PathResolve, PathValidate
   */
   Node Select(Node node, PathArg path, PathBoundArgs args)
   {
      PathException x;
      auto err = PathResolve(node, path, args, &x);
      if (err == EPathError::OK)
         return node;

      if (x.IsNodeError())
         return UndefinedNode();

      throw x;
   }

   /** Like \ref Select, except that it throws a \c PathException if no node can be matched */
   Node Require(Node node, PathArg path, PathBoundArgs args)
   {
      PathException x;
      auto err = PathResolve(node, path, args, &x);
      if (err == EPathError::OK)
         return node;

      throw x;
   }



   namespace YamlPathDetail
   {
      Node EnsureNodeApplyKeyToMapOrNothing(const Node & start, std::string key)
      {
         Node n = start[key];
         if (n)
            return n;
         return Node(NodeType::Null);
      }

      void EnsureNodeApplyKey(std::vector<Node> & result, const Node & start, std::string key, bool recurse)
      {
         if (!start || start.IsNull() || start.IsMap())
            result.push_back(EnsureNodeApplyKeyToMapOrNothing(start, key));
         else if (start.IsSequence() && recurse)
         {
            for (auto& el : start)
               if (el.IsNull() || el.IsMap())
                  EnsureNodeApplyKey(result, el, key, false);
         }
      }

      void EnsureNodeApplyKey(std::vector<Node> & result, std::vector<Node> & start, std::string key)
      {
         for (auto& el : start)
            if (el.IsNull() || el.IsMap())
               EnsureNodeApplyKey(result, el, key, true);
      }
   }

   Node Create(PathArg path, PathBoundArgs args)
   {
      Node root(YAML::NodeType::Null);
      Ensure(root, path, args);
      return root;
   }

   Node Ensure(Node & node, PathArg path, PathBoundArgs args)
   {
      PathException x;
      PathScanner scan(path, args, &x);

      std::vector<Node> next;
      next.push_back(node);

      while (scan)
      {
         switch (scan.NextSelector())
         {
            case YamlPathDetail::ESelector::None:
               break;

            case YamlPathDetail::ESelector::Key:
            {
               std::string key = std::string(scan.SelectorData<ArgKey>().key);
               std::vector<Node> result;
               EnsureNodeApplyKey(result, next, key);

               if (!result.size()) // nothing was added
               {
                  scan.SetError(EPathError::Internal);  // TODO: appropriate error msg
                  throw x;
               }
               next.swap(result);
               continue;
            }

            case YamlPathDetail::ESelector::MapFilter:
            {
               bool haveAssignment = false;
               std::vector<Node> result;
               for (auto && kvp : scan.SelectorData<ArgMapFilter>())
               {
                  if (kvp.op == EKVOp::NotEqual ||
                     kvp.key.starry || kvp.key.noCase || kvp.key.required ||
                     kvp.value.starry || kvp.value.noCase || kvp.value.required)
                  {
                     scan.SetError(EPathError::SelectorNotSupported);
                     throw x;
                  }
                  if (kvp.op == EKVOp::Select)
                     EnsureNodeApplyKey(result, next, std::string(kvp.key.token));
                  else // has assignment
                  {
                     std::vector<Node> assignTo;
                     EnsureNodeApplyKey(assignTo, next, std::string(kvp.key.token));
                     haveAssignment = !assignTo.empty();
                     for (size_t idx = 0; idx < assignTo.size(); ++idx)
                        if (kvp.op != EKVOp::Exists && (!assignTo[idx] || assignTo[idx].IsNull()))
                           assignTo[idx] = Node(std::string(kvp.value.token));
                  }
               }
               if (result.empty())
               {
                  if (haveAssignment)
                     return Node();
                  else
                  {
                     scan.SetError(EPathError::InvalidNodeType);
                     throw x;
                  }
               }
               else
                  next.swap(result);
            }
            continue;

            case YamlPathDetail::ESelector::Index:
            {
               std::vector<Node> result;
               for (auto el : next)
               {
                  if (!el || el.IsNull() || el.IsSequence())
                  {
                     size_t seqSize = el.IsSequence() ? el.size() : 0;
                     size_t idx = scan.SelectorData<ArgIndex>().index;
                     if (idx >= seqSize)
                        for (size_t i = 0; i < idx - seqSize + 1; ++i)
                           el.push_back(Node());
                     result.push_back(el[idx]);
                  }
               }
               if (!result.size())
               {
                  scan.SetError(EPathError::Internal);   // TODO: appropriate error
                  throw x;
               }
               next.swap(result);
            }
            continue;

            default:
               scan.SetError(EPathError::SelectorNotSupported);
               throw x;
         }
      }

      if (!next.size())
         return Node(NodeType::Null);

      Node final;
      for (auto && el : next)
         final.push_back(el);
      return final;
   }

} // namespace YAML
