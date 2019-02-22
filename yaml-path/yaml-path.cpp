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

/// namspace shared by yaml-cpp and yaml-path
namespace YAML
{
   namespace YamlPathDetail
   {
      /// \internal helper to map enum values to names, used for diagnostics
      template <typename T2, typename TEnum>
      T2 MapValue(TEnum value, std::initializer_list<std::pair<TEnum, T2>> values, T2 dflt = T2())
      {
         for (auto && p : values)
            if (p.first == value)
               return p.second;
         return dflt;
      }

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
         { ESelector::SeqMapFilter, "seq-map filter" },
         { ESelector::None, "(none)" },
         { ESelector::Invalid, "(invalid)" },
      };

      /// \internal name mapping for EPathError
      std::initializer_list<std::pair<EPathError, char const *>> MapEPathErrorName =
      {
         { EPathError::None ,             "(none)" },
         { EPathError::Internal,          "(internal, please report)" },
         { EPathError::InvalidIndex,      "invalid index" },
         { EPathError::InvalidNodeType,   "selector cannot not match node type" },
         { EPathError::InvalidToken,      "invalid token" },
         { EPathError::NodeNotFound,      "no node matches selector" },
         { EPathError::UnexpectedEnd,     "unexpected end of path" },
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

         if (m_error != EPathError::None)
            return m_curToken;

         // single-char special tokens
         char head = m_rpath[0];
         EToken t = MapValue(head, {
            { '.', EToken::Period },
            { '[', EToken::OpenBracket },
            { ']', EToken::CloseBracket },
            { '=', EToken::Equal },
            { '%', EToken::FetchArg },
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
         assert(error != EPathError::None);
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

      /** retrieves the next selector. */
      ESelector PathScanner::NextSelector()
      {
         // sticky on error
         if (m_error != EPathError::None)
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
               if (!NextSelectorToken(BitsOf({ EToken::UnquotedIdentifier, EToken::QuotedIdentifier, EToken::Index }), EPathError::InvalidIndex))
                  return ESelector::Invalid;

               // [1] --> index
               if (m_curToken.id == EToken::Index)
               {
                  const size_t index = m_curToken.index;
                  if (!NextSelectorToken(BitsOf({ EToken::CloseBracket })))
                     return ESelector::Invalid;

                  m_periodAllowed = true;
                  return SetSelector(ESelector::Index, ArgIndex{ index });
               }

               // [key=] or [key=value] for filtering
               auto tokKey = m_curToken.value;

               if (!NextSelectorToken(BitsOf({ EToken::Equal })))
                  return ESelector::Invalid;

               std::optional<PathArg> tokValue = std::nullopt;
               if (!NextSelectorToken(BitsOf({ EToken::QuotedIdentifier, EToken::UnquotedIdentifier, EToken::CloseBracket })))
                  return ESelector::Invalid;
               if (m_curToken.id != EToken::CloseBracket)
               {
                  tokValue = m_curToken.value;
                  if (!NextSelectorToken(BitsOf({ EToken::CloseBracket })))
                     return ESelector::Invalid;
               }

               m_periodAllowed = true;
               return SetSelector(ESelector::SeqMapFilter, ArgSeqMapFilter{tokKey, tokValue});
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

      if (!detailed || m_error == EPathError::None)
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

      /** \internal determines if the resulting node is "matched". (this returning false is a stop condition for the scan) */
      bool IsMatch(Node node)
      {
         return !(!node ||
            node.IsNull() ||
            (node.IsSequence() && node.size() == 0) ||
            (node.IsMap() && node.size() == 0));
      }

      /// \internal applies a key selector
      EPathError SeqMapByKey(Node & node, PathArg key, PathScanner & scan)
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
            return scan.SetError(EPathError::InvalidNodeType, BitsOf({ NodeType::Sequence | NodeType::Map }));

         if (!IsMatch(newNode))
            return scan.SetError(EPathError::NodeNotFound);

         node.reset(newNode);
         return EPathError::None;
      }

      // checks if a YAML map matches the requirements of a seq-map filter
      bool SeqMapFilterMatchElement(Node const & element, ArgSeqMapFilter const & arg)
      {
         if (!element.IsMap())
            return false;

         auto v = element[std::string(arg.key)];
         if (!v)
            return false;

         return !arg.value ||    // if no required value is given, any value accepted as long as key exists
                (v.IsScalar() && v.as<std::string>() == *arg.value);
      }

      // applies a seq-map filter to a seqiuence node
      EPathError SeqMapFilter(Node & node, ArgSeqMapFilter const & arg, PathScanner & scan)
      {
         Node newNode;
         if (node.IsSequence())
         {
            for (auto && el : node)
               if (SeqMapFilterMatchElement(el, arg))
                  newNode.push_back(el);
         }
         else if (node.IsMap())
         {
            if (SeqMapFilterMatchElement(node, arg))
               newNode = node;
         }
         else
            return scan.SetError(EPathError::InvalidNodeType, BitsOf({ NodeType::Sequence | NodeType::Map }));

         if (!IsMatch(newNode))
            return scan.SetError(EPathError::NodeNotFound);

         node.reset(newNode);
         return EPathError::None;
      }
   } // namespace YamlPathDetail


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
            if (auto err = SeqMapByKey(node, scan.SelectorData<ArgKey>().key, scan); err != EPathError::None)
               return err;
            continue;

            case ESelector::Index:
            {
               size_t index = scan.SelectorData<ArgIndex>().index;
               if (node.IsScalar() || node.IsMap())
               {
                  if (index != 0)   // for scalar node, [0] sticks to the node
                     return scan.SetError(EPathError::NodeNotFound);
                  continue;
               }
               if (node.IsSequence())
               {
                  if (index >= node.size())
                     return scan.SetError(EPathError::NodeNotFound);

                  node.reset(node[index]);
                  continue;
               }
               return scan.SetError(EPathError::InvalidNodeType, BitsOf({ NodeType::Scalar, NodeType::Sequence }));
            }

            case ESelector::SeqMapFilter:
            {
               if (auto err = SeqMapFilter(node, scan.SelectorData<ArgSeqMapFilter>(), scan); err != EPathError::None)
                  return err;
               continue;
            }

            case ESelector::Invalid:
               return scan.Error();

            default:
               assert(false);    // no other selectors supported right now
               return scan.Error();
         }
      }
      path = scan.Right();
      return EPathError::None;
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
      if (err == EPathError::None)
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
      if (err == EPathError::None)
         return node;

      throw x;
   }

} // namespace YAML
