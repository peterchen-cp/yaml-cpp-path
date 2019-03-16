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
#include <yaml-cpp/node/node.h>

namespace YAML
{
   class Node;
   class PathException;

   /** \c PathArg is used by yaml-path as parameter and return value representing a slice of a \c std::string.\n

       Implementation detail: \c PathArg is a typedef for \c std::string_view. \n
       The public interface assumes the conversions between \c PathArg and \c std::string work the same as for \c std::string_view.
   */
   using PathArg = std::string_view;
   enum class EPathError;


   using PathBoundArg = std::variant<size_t, PathArg>;         ///< bound argument for a YAML path. See \ref Select
   using PathBoundArgs = std::initializer_list<PathBoundArg>;  ///< list of bound arguments, see \ref Select

   struct KVToken 
   { 
      PathArg token; 
      bool required = false; 
      bool noCase = false; 
      bool starry = false; 
      bool IsAllStar() const { return starry && token.empty();  }
   };
   enum class EKVOp
   {
      Equal,
      NotEqual,
      Exists,
      Select,
   };

   EPathError SelectByKey(Node & node, PathArg key);
   EPathError SelectByIndex(Node & node, size_t index);

   Node Select(Node node, PathArg path, PathBoundArgs args = {}); ///< Select a node
   Node Require(Node node, PathArg path, PathBoundArgs args = {});
   Node Create(PathArg path, PathBoundArgs args = {});
   Node Ensure(Node & node, PathArg path, PathBoundArgs args = {}); ///< ensure one or more nodes exist. 
   EPathError PathValidate(PathArg p, std::string * valid = 0, size_t * errorOffs = 0);
   EPathError PathResolve(Node & node, PathArg & path, PathBoundArgs args = {}, PathException * px = 0);


  
   EPathError SelectByKey(Node & node, PathArg key);

   /** Error code used by yaml-path. For Information on error handling, see \ref PathException */
   enum class EPathError
   {
      OK = 0,
      Internal,

      // parsing errors
      InvalidToken,
      InvalidIndex,
      UnexpectedEnd,
      SelectorNotSupported,

      // node navigation errors
      FirstNodeError_ = 100,     ///< all error codes after this indicate the selector was valid, but a matching node could not be found
      InvalidNodeType,
      NodeNotFound,
      /* to add a new error code, also add: a formatter to PathException::What */
   };

   namespace YamlPathDetail { class PathScanner; }

   /** Exception and diagnostics for yaml-path */
   class PathException : public std::exception
   {
   public:
      PathException() = default;

      EPathError Error() const { return m_error; }                ///< error code for this exception
      bool IsNodeError() const { return IsNodeError(m_error); }   ///< true if \c error indicates failure to find a matching node
      bool IsPathError() const { return IsPathError(m_error); }   ///< true if \c error indicates a malformed path

      std::string FullPath() const { return m_fullPath; }         ///< the full path that was used by the failing command
      std::string ResolvedPath() const { return m_fullPath.substr(0, m_offsSelectorScan);  } ///< the part of the path that was resolved correctly
      std::size_t ErrorOffset() const { return m_offsTokenScan; }                            ///< index into the full path where the error occurred
      std::optional<size_t> BoundArg() const { return m_fromBoundArg;  }                     ///< if token was taken from a bound argument, this is its index

      char const * what() const override { return What().c_str(); }                          ///< overrides \c std::exception::what, returning the detailed error message from \ref What
      std::string const & What(bool detailed = true) const;                

      static std::string GetErrorMessage(EPathError error);
      static bool IsNodeError(EPathError error) { return error >= EPathError::FirstNodeError_; }   ///< true if \c error indicates failure to find a matching node
      static bool IsPathError(EPathError error) { return error < EPathError::FirstNodeError_ && error != EPathError::OK; }  ///< true if \c error indicates a malformed path

   private:
      friend class YamlPathDetail::PathScanner; // if scanner has a non-null diags member, it will feed it scan state information

      EPathError m_error = EPathError::OK;
      std::string m_fullPath;
      std::size_t m_offsTokenScan = 0;
      std::size_t m_offsSelectorScan = 0;
      std::optional<size_t> m_fromBoundArg;

      uint64_t    m_validTypes = 0;    // BitsOf(YAML::NodeType) for node errors, BitsOf(EToken) for token errors
      unsigned    m_errorType = 0;     // ESelector, or EToken

      std::string ErrorItem() const;

      // will be generated on demand
      mutable std::string m_short;
      mutable std::string m_detailed;
      mutable std::string m_errorItem;
   };
}
