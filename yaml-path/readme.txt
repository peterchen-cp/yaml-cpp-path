
Current shortcomings:

	Requires C++ 17. 


Ideas:

	in-path functions. 
		"!ismap", "!isseq", "!isscalar""
			select the node only if it is a map (or sequence, or scalar, respectively)
			if it is of a different type an empty node is selected

		"[$isscalar]" to select only the maps from a sequence  (etc. for other types)
		"{$isscalar}" to select only the pairs from a map where the value is a scalar (etc. for other types)

		--OR--

		"!make(seq)"			wraps scalars and maps in a single-element sequence and null/invalid so that iterating through it works
		"!make(seq, scalar)"	if node is a sequence, selects a sequence of scalars.
								if node is a scalar, select a one-element sequence of this scalar
				etc. for other combinations, as useful

	
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


	EnsureNodes:

		{A,B}			creates two map items, A and B, and returns a sequence of these nodes
		{A=1, B=2}		creates two map items with values A and B, and  the values 1 and 2, respectively. Nothing that can be added anymore
		{A=1, B=, C }	creates three map items A,B,C. A gets the value 1, B gets assigned an empty node, C gets assigned an empty node. 
						returns C's value, which means following selectors apply to only the C node

		{A,B}.{C,D}		creates "{ A : { C : ~, D : ~ }, B : { C : ~, D : ~ } }", and returns a sequence with the (empty) value nodes. 


		How do these selectors work in Resolve / Select?

		If node is a map:

		{A,B,C}			selects only elements with keys A, B, C from the map. They are optional, so if the map only contains keys A and C, they are ignored
						key should allow the following modifiers:
						  - required (map is not selected when the key is missing)			--- prefix !
						  - comparison is case insensitive (default is case sensitive)		--- prefix ^
						  - use any key that starts with the given text						--- suffix *

						This means that {*}	selects the entire map (i.e. all keys and their values). This becomes useful with filters:

		{k1=v1}			the map must contain key k1, and its value must be the scalar v1. 
						keys may 
						Multiple conditions can be separated by comma. If none of them is required, any of them is sufficient to select the maps elements.
						If any condition is required, all required elements must be met (and non-required elements don't matter and are ignored)
							(allowing arbitrary expressions is beyond the skills of this little parser)

		{k1!=v1}		a condition that requires inequality.

		{k1=}			the key must exist, but the value can be any type

						So yes, the weirdest thing to come up with is 
						!^key*!=^val*
							There must exist ("!") a key that starts with ("*") with "key" in any letter case ("^"), i.e. "Key", "kEY" etc, are accepted, too
							It's value start ("*") with "val" in any letter case ("^")

						the token itself can be a bound argument

						(todo: how to specify a bound-lambda-argument filter?)

						A condition does not select the specified key, so {k1=v1,k2} only selects k2 into the resulting map.
						However, if only conditions are given, all elements of the map are selected, so {k1=v1} is equivalent to {k1=v1,*}

		so a map sfilter is specified as 
		map filter = list of (condition | key selector)
		key selector = [!][^][<token>|<token>*|*]   --- and must contain at least token, or star, or both
		condition = (conditon key = condition value | condition key != condition value | condition key =)
		condition key = --- like key selector, without the ! prefix
		condition value = key selector without the ! prefix

		map selector = struct { string_view token; bool required; bool ignoreCase; bool starred; }
		enum MapFilterElementType = { select, condEqual, condNotEqual, condExists }
		map filter element = struct { MapFilterElementType, map selector key, map selector value }
						

		[<int>]			index of a sequence
						if it's a scalar, an index is 0, it's a sequence of a single value

						if it's a map...	???
							Symmetry with the map selector: for all keys that are a sequence, a kye: value to the nth element (would anyone need that?)

		==> Breaking change: [] for index, {} for map elements

		[A,B,C]			takes the values of keys A,B,C and puts them into a sequence (?!)
















		
