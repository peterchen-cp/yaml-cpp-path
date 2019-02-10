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

namespace YAML
{
   namespace YamlPathDetail
   {
      path_arg SplitAt(path_arg & path, size_t offset);

      template <typename TPred>
      path_arg Split(path_arg & path, TPred pred);

      template <typename TBits = uint64_t, typename T>
      constexpr uint64_t BitsOf(std::initializer_list<T> values);

      template<typename TBits, typename TValue>
      bool constexpr BitsContain(TBits bits, TValue v);

      enum class EToken
      {
         Invalid = -1,
         None = 0,
         QuotedIdentifier,
         UnquotedIdentifier,
         OpenBracket,
         CloseBracket,
         Period,
      };

      EToken GetSingleCharToken(char c, std::initializer_list< std::pair<char, EToken> > values);

      struct Token
      {
         EToken   id = EToken::None;
         path_arg value;
      };

      class TokenScanner
      {
      public:

      private:
         path_arg    m_rpath;    // path to be rendered
         Token       m_curToken;
         std::optional<PathException> m_curException;

         path_arg    m_all;      // original path (just to to get the offset)

         Token const & SelectToken(EToken id, path_arg p, uint64_t validTokens);
         void SetError(EPathError error);
         void SkipWS();

      public:
         bool     ThrowOnError = true;

         TokenScanner(path_arg p);

         explicit operator bool() const { return !m_rpath.empty() && m_curToken.id != EToken::Invalid && !m_curException; }

         auto const & CurrentException() const { return m_curException; }  ///< current error
         auto Right() const { return m_rpath; }                           ///< remainder (unscanned part)
         size_t ScanOffset() const { return m_all.length() - m_rpath.length(); }

         Token const & NextToken(uint64_t validTokens = -1);
         Token const & Token() const { return m_curToken; }

         void Resolve(Node * node);

         inline static const uint64_t ValidTokensAtStart = BitsOf({ EToken::None, EToken::OpenBracket, EToken::QuotedIdentifier, EToken::UnquotedIdentifier });
         inline static const uint64_t ValidTokensAtBase = ValidTokensAtStart | BitsOf({ EToken::Period });
      };
   }
}

// ----- Implementation

namespace YAML
{
   namespace YamlPathDetail
   {
      template <typename TPred>
      path_arg Split(path_arg & path, TPred pred)
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

   }
}
