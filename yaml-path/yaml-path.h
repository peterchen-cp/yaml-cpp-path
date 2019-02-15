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

namespace YAML
{
   class Node;
   using path_arg = std::string_view;
   enum class EPathError;

   EPathError PathValidate(path_arg p, path_arg * valid = 0, size_t * scanOffs = 0);
   Node PathAt(Node node, path_arg path);
   EPathError PathResolve(Node & node, path_arg & path);

   enum class EPathError
   {
      None,
      Internal,

      // parsing errors
      InvalidToken,
      InvalidIndex,
      UnexpectedEnd,

      // node navigation errors
      FirstNodeError_ = 100,     ///< all error codes after this indicate the selector was valid, but a matching node could not be found
      InvalidNodeType,
      NodeNotFound
      /* to add a new error code, also add:
          - a new exception typedef below (using Path...Exception = YamlPathDetail::PathExceptionT<EPathError::code>;)
          - a formatter to PathException::What
          - a throw expression to PathException::ThrowDerived
      */
   };


   class PathException : public std::exception
   {
   public:

      PathException(EPathError error, size_t offs = 0, std::string value = std::string()) : m_error(error), m_offset(offs), m_value(value) {}

      EPathError Error() const { return m_error; }
      size_t Offset() const { return m_offset; }
      auto Value() const { return m_value; }

      char const * what() const override { return What().c_str(); }
      std::string const & What() const;

      void ThrowDerived();

   private:
      EPathError m_error = EPathError::None;
      std::string m_value;
      size_t m_offset = 0;
      mutable std::string m_what;
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
