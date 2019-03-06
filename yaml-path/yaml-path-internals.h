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

#pragma once

/* This exposes yaml-path details to make them accessible to tests. 
*/


#include "yaml-path.h"
#include <optional>
#include <sstream>
#include <variant>
#include <vector>

namespace YAML
{
   namespace YamlPathDetail
   {
      /// \internal basic parser helper: removes \c offset chars from \c path, and returns the removed chars
      PathArg SplitAt(PathArg & path, size_t offset);

      /// \internal basic parser helper: removes chars from \c path until \c pred is false, and returns the removed chars
      template <typename TPred>
      PathArg Split(PathArg & path, TPred pred);

      /// \internal returns an integer in which the bit positions specified by \c values are set, \sa BitsContain
      template <typename TBits = uint64_t, typename T>
      constexpr uint64_t BitsOf(std::initializer_list<T> values);

      /// \internal checks if the bit at position \v is set in \c bits
      template<typename TBits, typename TValue>
      bool constexpr BitsContain(TBits bits, TValue v);

      /// \internal Tokens supported by the token level parser of \ref PathScanner
      enum class EToken
      {
         Invalid = -1,
         None = 0,
         QuotedIdentifier,
         UnquotedIdentifier,
         OpenBracket,
         CloseBracket,
         Period,
         Equal,
         Index,         // translated from UnquotedIdentifier in NextSelectorToken
         FetchArg,
         OpenBrace,
         CloseBrace,
      };
      /* when adding a new token, also add to:
            - MapETokenName
            - ValidTokensAtStart, if applicable
            - TokenData, if a new data type is required
            - PathScanner::NextToken, to recognize it
            - PathScanner::NextSelector, to process it
      */

      /// \internal data for one token, see \ref PathScanner
      struct TokenData
      {
         EToken   id = EToken::None;
         PathArg value;
         size_t index = 0;
      };

      /// \internal Selectors supported by the selector level parser of \ref PathScanner
      enum class ESelector
      {
         Invalid = -1,
         None = 0,
         Key,
         Index,
         SeqMapFilter,
      };

      // Data for different selector types
      struct ArgNull {};
      struct ArgKey { PathArg key; };
      struct ArgIndex { size_t index; };

      struct ArgKVPair 
      { 
         KVToken key; 
         KVToken value; 
         EKVOp op = EKVOp::Equal;  

         // CTor for current implementation
         ArgKVPair(PathArg key_, std::optional<PathArg> value_);
      };
      using ArgSeqMapFilter = ArgKVPair; 

      /** \internal progressive scanner/parser for a YAML path as specified by YAML::Select
         This class implements two layers of the scan: 
         The <i>token level scanner</i>, retrieves \ref EToken "tokens"  from the path,until nothing is left. 
         \ref NextToken retrieves the next token, \ref Token gives access to the last retrieved one.

         The <i>selector level scanner</i> uses the token scanner to retrieve \ref ESelector "selectors" the same way.
         \ref NextSelector calls \c NextToken until it has retrieved a complete selector to return. 
         \ref Selector returns the type of the last retrieved one, and \ref SelectorData retrieves a union that contains
         the data for the last retrieved selector.

         (The separation wasn't as clear when I started hacking at this, separating them now would be rather easy, but with marginal benefit,
         especially since this is not a public interface.)
      */
      class PathScanner
      {
      public:
         using tSelectorData = std::variant<ArgNull, ArgKey, ArgIndex, ArgSeqMapFilter>;  ///< union of the selector data for all selector types

      private:
         PathArg    m_rpath;        // remainder of path to be scanned
         PathBoundArgs m_args;      // list of arguments that should be used as tokens
         size_t     m_argIdx;       // next argument index to fetch
         TokenData  m_curToken;

         ESelector      m_selector = ESelector::None;
         tSelectorData  m_selectorData;
         bool           m_tokenPending = false;          ///< m_curToken should be processed before reading a new one
         bool           m_periodAllowed = false;         ///< next token may be a period 
         bool           m_selectorRequired = false;      ///< another selector is required for a well-formed path (e.g. after "abc.")

         EPathError     m_error = EPathError::OK;
         PathArg       m_fullPath;
         PathException * m_diags = nullptr;

         TokenData const & SetToken(EToken id, PathArg p);
         TokenData const & SetToken(EToken id, size_t index);

         template<typename TArg>
         ESelector SetSelector(ESelector selector, TArg arg)
         {
            m_selector = selector;
            m_selectorData = std::move(arg);
            return m_selector;
         }

         void SkipWS();
         bool NextSelectorToken(uint64_t validTokens, EPathError error = EPathError::InvalidToken);

      public:
         PathScanner(PathArg p, PathBoundArgs args = {}, PathException * diags = nullptr);

         explicit operator bool() const { return !m_rpath.empty() && m_error == EPathError::OK; }

         auto Error() const            { return m_error; }
         auto const & Right() const    { return m_rpath; }                             ///< remainder (unscanned part)
         size_t ScanOffset() const     { return m_fullPath.length() - m_rpath.length(); }        ///< token scanner position

         // -----token-level scanner
         TokenData const & NextToken();
         TokenData const & Token() const { return m_curToken; }

         // -----selector-level scanner
         ESelector NextSelector();
         ESelector Selector() const { return m_selector; }
         auto const & SelectorDataV()  const { return m_selectorData; }

         template <typename T> 
         T const & SelectorData() const { return std::get<T>(m_selectorData); }

         // for access by utility functions to record an error
         EPathError SetError(EPathError error, uint64_t validTypes = 0);

         inline static const uint64_t ValidTokensAtStart = BitsOf({ EToken::FetchArg, EToken::None, EToken::OpenBracket, EToken::OpenBrace,  EToken::QuotedIdentifier, EToken::UnquotedIdentifier });
      };

      template <typename T2, typename TEnum>
      T2 MapValue(TEnum value, std::initializer_list<std::pair<TEnum, T2>> values, T2 dflt = T2());

      extern std::initializer_list<std::pair<EToken, char const *>> MapETokenName;
      extern std::initializer_list<std::pair<NodeType::value, char const *>> MapNodeTypeName;
      extern std::initializer_list<std::pair<ESelector, char const *>> MapESelectorName;
      extern std::initializer_list<std::pair<EPathError, char const *>> MapEPathErrorName;
   }
}

// ----- Implementation

namespace YAML
{
   namespace YamlPathDetail
   {
      template <typename TPred>
      PathArg Split(PathArg & path, TPred pred)
      {
         size_t offset = 0;
         while (offset < path.size() && pred(path[offset]))
            ++offset;
         return SplitAt(path, offset);
      }

      template <typename TBits, typename T>
      constexpr uint64_t BitsOf(std::initializer_list<T> values)
      {
         __int64 bits = 0;
         for (auto v : values)
            bits |= TBits(1) << TBits(v);
         return bits;
      }

      template<typename TBits, typename TValue>
      bool constexpr BitsContain(TBits bits, TValue v)
      {
         return ((TBits(1) << TBits(v)) & bits) != 0;
      }

      /// \internal helper to map enum values to names, used for diagnostics
      template <typename T2, typename TEnum>
      T2 MapValue(TEnum value, std::initializer_list<std::pair<TEnum, T2>> values, T2 dflt)
      {
         for (auto && p : values)
            if (p.first == value)
               return p.second;
         return dflt;
      }


   }
}
