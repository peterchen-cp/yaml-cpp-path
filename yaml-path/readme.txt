
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

	Positional syntax for bound arguments

		%1, %2 etc. to explicitely request the first, second argument. 
		(Make sure the syntax is unambigous, so that there's no construct where %23 could mean "%, then the noext token is 23")

	Case Insensitive Match
		using allowing case insensitive mathes at least for seq-map filter values, e.g. 
		Select(node, "[key=^value]")			
		(is there a more reasonable symbol for "case insensitive comparison"? A pipe '|' might be, but that should remain reserved as OR)
		
		Could be extended to key selector, or the key of a seq-map filter, but matching those would require a linear scan.
		(which the partial match for these tokens would require, too, anyway)

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






		
