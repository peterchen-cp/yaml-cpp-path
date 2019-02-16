Selectors:

Key Selector: 

	Select(node, "k")

	if node is a map, returns the value of the item with key "k"
	if node is a sequence, returns the values of of key k from all maps that are elements of the sequence

	e.g.   { k : letter, 2 : number }  ==>   "letter"
	       [ { k : 1, l : 2}, { k : letter, 2 : number}, { l : letter } ]   ==>  [ 1, letter ]


Index: 

	Select(node, "[n]")		
	
	... where n is an unsigned integer not exceeding the range of size_t

	If node is a sequence, returns the nth element of the sequence
	If node is a scalar or a map, and n is 0, returns that element


Seq-Map Filter: 
	
	Select(node, "[k=v]")
	Select(node, "[k=]")

	if node is a map, selects the node if it contains a key k with the value v
	if node is a sequence, selects all maps from the sequence that contain a key k with value v

	if v is not specified ("[k=]"), a mak is selected iff it contains a key k

	To check for an empty key, you can use "[k='']"  (see quoting)


Selector Chaining:  Select(node, "k1.k2")
	
	Selects "k1" from node, then selects k2 from result.
	Key selectors need to be separated by a period. 
	A period is not necessary if the first selector ends with, or the second selector starts with a bracket.
	i.e. "k[1]" and "k.[1]" are equivalent


Quoting

	Selector tokens can be quoted, either by single or double quotes. Single quote selectors may contain double quotes, and vice versa. 


Finding nothing to select

	If a selector does not find anything to select (e.g. a key selector on a scalar, or an index selector that is out of range),
	the scan stops and an empty node is returned (a node that evaluates to false in a boolean context, like yaml-cpp returns for a not-found element)

	PathResolve stops the scan just BEFORE no node can be selected, e.g. 
	Select(YAML::Load("{ k1: v }"), "k1.k2")  ==> "v"

		(k1 selects the scalar "v". k2 would select nothing from "v", so the scan stops, and "v" is returned)

	PathResolve also returns the remainder of the path that was not used to select something. Any leading separator is removed from that path

		(so in above example,  the remaining path would be "k2")

Spaces
	tokens can be surrounded by spaces, which are ignored

--------


Current shortcomings:

	Requires C++ 17. 


Ideas:

	in-path functions. 
		"~ismap", "~isseq", "~isscalar""
			select the node only if it is a map (or sequence, or scalar, respectively)
			if it is of a different type an empty node is selected

		"[~isscalar]" to select only the maps from a sequence  (etc. for other types)
		"{~isscalar}" to select only the pairs from a map where the value is a scalar (etc. for other types)
	
		Aggregate functions such as Sum<double>(node) I would do as "real" C++ functions, (e.g. so we can specify the type)

	Parametrized paths:

		Selector tokens can be replaced by an argument reference, e.g.

			index = ...
			Select(node, "k.[$]", index);

			"k" would act as normal selector, the value for the index selector would be taken from the first argument.
			Also allow positional arguments.

			Mayb $1, $2 etc. for positional arguments, in case one is needed multiple times
			Would only work on token level, e.g. "$1$2" would NOT paste two parameters together to asingle key selector

	Partial Match
			Some tokens might allow wildcards, or at least end in a wild card: * for zero-or more, + for one-or more
			[key=v+] would accept all maps where node["key"] starts with "v"
			
			could be exptended to other tokens (key selector, key of a seq-map filter)

	Multiple selections

			"[k1=v1, k2=v2]" selects maps from a sequence that contain a key k1 with value v1, or a key	k2 with value v2
			"[k1=v1 & k2=v2]" selects maps from a sequence that contain both
			"[k1=v1 & k2=v2, k3=v3]" would be an OR of these conditions

	Sequence Slicing

		[5-7]  selects up to 3 elements with indices 5, 6 and 7 (less if these indices donät exist)
		[!5-7] selects 3 elements (or nothing if not all of them exist)
		[!5-7,2] selects from the range wiht increment of 2, so [5] and [7]

	Map Slicing

		{keyA, keyB, keyC}  Selects from a map, returning a map containing only the kv-pairs wiht the given keys

			Select(Load("{ j : 1, k : 2, l : 4}"), "{k,l, m}")   --> { k : 2, l : 4 }






		