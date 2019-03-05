namespace YAML {  // allows to omit YAML:: from \ref from the 
/** \mainpage yaml-path

# Introduction

<a href="https://yaml.org/">YAML</a> is a structured document format.\n
<a href="https://github.com/jbeder/yaml-cpp">yaml-cpp</a> is a C++ library for reading and writing yaml files. Its central class, \c Node, represents a document node\n
<b>yaml-path</b> adds path selectors to yaml-cpp:

<code>\ref YAML::Select "Select"(Node node, PathArg path)</code> selects one or more nodes from \c node as specified by the string \c path,
and returns that selection as a node.

Simple examples: a path \c "[0]" selects the first element from a sequence, a path \c "k1" selects the value for the key \c "k1" from a map.
Such selectors can be chained, e.g. if that element is a sequence, \c "k1[0]" would select its first element.

# Core API

The most important methods are

   - \ref Select "Select"(node, path) selecting a node. If no node can be matched, an empty node is returned
   - \ref Require "Require"(node, path) Like \c select, but failure to match a node throws an exception
   - \ref PathResolve for incremental matching
   - \ref PathValidate for validating a path

   - \ref SelectByKey, \ref SelectByIndex, \ref SelectBySeqMapFilter


# Selectors


The following selectors exists:

## Key Selector

<code>Select(node, "key")</code>\n

If \c node is a map, returns the value of the item with the key \c "key".\n
If \c node is a sequence, it takes all maps from the sequence that contain a key \c "key", and returns a sequence of their values.\n

Example:
\code
{ k : letter, 2 : number }  ==&rarr;   "letter"
[ { k : 1, l : 2}, { k : letter, 2 : number}, { l : letter } ]   ==&rarr;  [ 1, letter ]
\endcode

## Index

<code>Select(node, "[2]")</code>

If \c node is a sequence, selects the n'th node (where \c n is the number in brackets).\n
If \c node is a scalar or a map, and \c n is 0, selects that scalar or map

## Seq-Map Filter

<code>Select(node, "{key=value}")</code> or <code>Select(node, "{key=}")</code>

Selects maps from a sequence if they contain the specified key-value pair, or the specified key with an arbitrary value.

If \c node is a map, selects the node if it contains a key \c "key" with the value \c "value" \n
If \c node is a sequence, selects all maps from the sequence that contain a key \c "key" with value \c "value"

\c value can be omitted, in which case the same rules apply, except that a map is selected if it contains a key \c "key",
independent of its value.\n
To check for an empty key, you can use quotes, e.g. <code>Select(node, "{key=''}"</code> (see quoting)

## Selector Chaining

<code>Select(node, "keyA.keyB")</code>

Selects \c "keyA" from \c node, then selects "keyB" from the result.

Key selectors need to be separated by a period. A period is not necessary if the first selector ends with,
or the second selector starts with a bracket. i.e. \c "k[1]" and \c "k.[1]" are equivalent.

## Quoting

All <i>string tokens</i> can be quoted. String tokens are:

   - key selector
   - key of a seq-map filter
   - value of a seq-map filter

The token can be wrapped either in single or double quotes. A token in single quotes may contain double quotes, and vice versa.

## argument binding

<code>Select(node, "%.[%]", { "key", 12 });</code> is equivalent to <code>Select(node, "key.[12]")</code>

\c Select supports a list of \ref PathBoundArg as an optional parameter.
Each element in the list can be initialized by either a \c PathArg or an unsigned integer.
If a \c "%" is found where a a <i>bindable token</i> is expected, the next value from the argument list is taken instead.

Bindable tokens are all string tokens (see "Quoting") and the value of an index selector.

## Character Set and Case Sensitivity

In accordance with the YAML standard, comparisons are case sensitive. This includes the value comparison of a seq-map filter.\n
All non-ASCII characters are treated as if they were letters, i.e. are considered part of a string token.

## Error Handling

yaml-path uses \ref EPathError codes to distinguish different error conditions.
\c PathException provides additional diagnostics, suitable for diagnosing malformed paths and problems finding / matching nodes.

There are two categories of errors: \ref PathException::IsPathError "path errors" indicate a malformed path,
\ref PathException::IsNodeError "node errors" indicate a failure
to find a matching node for a selector.


*/

}
