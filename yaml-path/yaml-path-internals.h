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

/* This exposes yaml-path detaisl to make them accessible to tests. 
   The part after "// ----- Implementation"  can be inserted into yaml-path.cpp 
   instead of the include.
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

      class PathException : public std::exception
      {
      public:

         PathException(EPathError error, size_t offs = 0, std::string value = std::string()) : m_error(error), m_offset(offs), m_value(value) {}

         EPathError Error() const { return m_error;  }
         size_t Offset() const { return m_offset;  }
         auto Value() const { return m_value;  }

         char const * what() const override { return What().c_str(); }

         std::string const & What() const;

      private:
         EPathError m_error = EPathError::None;
         std::string m_value;
         size_t m_offset = 0;
         mutable std::string m_what;
      };

      class PathInvalidTokenException : public PathException
      {
      public:
         PathInvalidTokenException(size_t offset, std::string token) : PathException(EPathError::InvalidToken, offset, token) {}
      };

      class PathIndexExpectedException : public PathException
      {
      public:
         PathIndexExpectedException(size_t offset, std::string token) : PathException(EPathError::IndexExpected, offset, token) {}
      };

      enum class EToken
      {
         Invalid = -1,
         None  = 0,
         QuotedIdentifier,
         UnquotedIdentifier,
         OpenBracket,
         CloseBracket,
         Period,

         // ...
      };

      EToken GetSingleCharToken(char c, std::initializer_list< std::pair<char, EToken> > values);

      class TokenScanner
      {
         path_arg    m_all;      // original path (to get the offset)
         path_arg    m_rpath;    // path remainder
         EToken      m_token;
         path_arg    m_value;

         EToken Select(EToken tok, path_arg p);
         void SkipWS();

         std::optional<PathException> m_curException;

      public:
         TokenScanner(path_arg p);

         explicit operator bool() const { return !m_rpath.empty() && m_token != EToken::Invalid && !m_curException; }

         uint64_t ValidTokens = 0;
         bool     ThrowOnError = true;
         auto const & CurrentException() const { return m_curException; }  ///< exception, 
         auto Right() const { return m_rpath;  }                           ///< remainder (unscanned part)

         EToken Next();
         path_arg Value() const { return m_value;  }
         EToken Token() const { return m_token;  }

         size_t NextOffset() const { return m_all.size() - m_rpath.size();  }    // TODO: replace with pre-parse offset
         void SetError(EPathError error);

         void ThrowDerived();

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
