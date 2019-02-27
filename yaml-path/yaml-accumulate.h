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

#include <yaml-cpp/node/node.h>

namespace YAML
{

   /** accumulates node values

   Accumulates all nodes in a sequence, or values in a map. 
   If node is a scalar, the result is the scalar converted to \c T. 
   For all other node types, the return value is \c initial.

   Nodes to accumulate are converted to \c T using <code>Node.as&larr;T&rarr;()</code>,
   and then are accumulated by <code>x = op(x, node[i])</code>, starting with \c initial.
   */
   template <typename T, typename TOp>
   T Accumulate(Node n, T initial, TOp op)
   {
      switch (n.Type())
      {
      case NodeType::Scalar:
         return op(std::move(initial), std::move(n.as<T>()));

      case NodeType::Sequence:
         for (auto && el : n)
            initial = op(std::move(initial), std::move(el.as<T>()));
         return initial;

      case NodeType::Map:
         for (auto it = n.begin(); it != n.end(); ++it)
            initial = op(std::move(initial), std::move(it->second.as<T>()));
         return initial;

      case NodeType::Null:
      default:
         return std::move(initial);
      }
   }

   /** Accumulates node values

   like \ref Accumulate, but uses <code>op(x (by reference), node[i])</code>
   */
   template <typename T, typename TOp>
   T AccumulateRefOp(Node n, T initial, TOp refop)
   {
      switch (n.Type())
      {
      case NodeType::Null:
         return std::move(initial);

      case NodeType::Scalar:
         refop(initial, std::move(n.as<T>()));
         return std::move(initial);

      case NodeType::Sequence:
         for (auto && el : n)
            refop(initial, std::move(el.as<T>()));
         return std::move(initial);

      case NodeType::Map:
         for (auto it = n.begin(); it != n.end(); ++it)
            refop(initial, std::move(it->second.as<T>()));
         return std::move(initial);

      default:
         return initial;
      }
   }


   /** like \ref Accumulate, using <code>operator+=(T&, T)</code>
   */
   template <typename T>
   T Accumulate(Node n, T initial = T())
   {
      return AccumulateRefOp(n, initial, [](T & a, T b) { a += b; });
   }

} // namespace YAML
