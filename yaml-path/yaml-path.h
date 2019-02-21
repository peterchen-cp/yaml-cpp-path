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

#include <string_view>
#include <variant>
#include <optional>

namespace YAML
{
   class Node;
   class PathException;

   using path_arg = std::string_view;
   enum class EPathError;

   using path_bind_arg = std::variant<size_t, path_arg>;
   using path_bind_args = std::initializer_list<path_bind_arg>;

   Node Select(Node node, path_arg path, path_bind_args args = {});
   Node Require(Node node, path_arg path, path_bind_args args = {});
   EPathError PathValidate(path_arg p, std::string * valid = 0, size_t * errorOffs = 0);
   EPathError PathResolve(Node & node, path_arg & path, path_bind_args args = {}, PathException * px = 0);

   enum class EPathError
   {
      None = 0,
      Internal,

      // parsing errors
      InvalidToken,
      InvalidIndex,
      UnexpectedEnd,

      // node navigation errors
      FirstNodeError_ = 100,     ///< all error codes after this indicate the selector was valid, but a matching node could not be found
      InvalidNodeType,
      NodeNotFound
      /* to add a new error code, also add: a formatter to PathException::What */
   };

   namespace YamlPathDetail { class PathScanner; }

   class PathException : public std::exception
   {
   public:
      PathException() = default;

      EPathError Error() const { return m_error; }       ///< error code for this exception
      bool IsNodeError() const { return IsNodeError(m_error); }
      bool IsPathError() const { return IsPathError(m_error); }

      std::string FullPath() const { return m_fullPath; }
      std::string ResolvedPath() const { return m_fullPath.substr(0, m_offsSelectorScan);  }
      std::size_t ErrorOffset() const { return m_offsTokenScan; }
      std::optional<size_t> BoundArg() const { return m_fromBoundArg;  }      // if token was taken from a bound arg, this is its index
      std::string ErrorItem() const;

      char const * what() const override { return What().c_str(); }
      std::string const & What(bool detailed = true) const;

      static std::string GetErrorMessage(EPathError error);
      static bool IsNodeError(EPathError error) { return error >= EPathError::FirstNodeError_; }
      static bool IsPathError(EPathError error) { return error < EPathError::FirstNodeError_ && error != EPathError::None; }

   private:
      friend class YamlPathDetail::PathScanner; // if scanner has a non-null diags member, it will feed it scan state information

      EPathError m_error = EPathError::None;
      std::string m_fullPath;
      std::size_t m_offsTokenScan = 0;
      std::size_t m_offsSelectorScan = 0;
      std::optional<size_t> m_fromBoundArg;

      uint64_t    m_validTypes = 0;    // BitsOf(YAML::NodeType) for node errors, BitsOf(EToken) for token errors
      unsigned    m_errorType = 0;     // ESelector, or EToken

      // will be generated on demand
      mutable std::string m_short;
      mutable std::string m_detailed;
      mutable std::string m_errorItem;
   };

   namespace YamlPathDetail
   {
      template <EPathError error> 
      class PathExceptionT : public PathException
      {
         public: 
            PathExceptionT(size_t offset, std::string value) : PathException(error, offset, value) {}
      };
   }

   using PathInternalException = YamlPathDetail::PathExceptionT<EPathError::Internal>;
   using PathInvalidTokenException = YamlPathDetail::PathExceptionT<EPathError::InvalidToken>;
   using PathInvalidIndexException = YamlPathDetail::PathExceptionT<EPathError::InvalidIndex>;
   using PathUnexpectedEndException = YamlPathDetail::PathExceptionT<EPathError::UnexpectedEnd>;

   using PathInvalidNodeTypeException = YamlPathDetail::PathExceptionT<EPathError::InvalidNodeType>;
   using PathNodeNotFoundException = YamlPathDetail::PathExceptionT<EPathError::NodeNotFound>;
}
