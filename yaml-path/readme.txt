
Ideas (!) for Extended Syntax, allowing to select a subset of nodes:

 [*]		- select all nodes of a list
 [5-10]		- select nodes 5..10, of a list including node 10
 [5-14,3]	- as above, but only every 3rd
 [!5-14, 3]	- as above, but the nodes must exist, or scan fails at this point (without '!', specifying more nodes than available doesn't matter)

 {*}		- select all items from a map
 {keyA,keyB}	- from a map, selects only the specified keys listed
 {keyA, !keyB, keyC}	- as above, but keyB must exist

 [keyA=value]	- from a sequence of maps, select only the sequence elements where the map contains a key with the given value, 
			      e.g. from the following list, [type=fish]  will remove the dog:
			      
						- { type : fish, color : silvery }
						- { type : dog, color : brown }
						- { type : fish, color : white }


 [keyA=*]		- as above, but the value does not matter (key must exist)

 [keyA=*, keyB=value]	- as before, where the maps have a keyA, or a keyB with the given value (or both)

 [keyA=* & keyB=value, keyC=*]	- as before, where the maps have a keyC, or an keyA and keyB with the given values

 $()			- in place of an identifier or index or numeric value, that value is popped from the (optional) argument list. () can be omitted if another token follows
 $(2)			- in place of an identifier or index or numeric value, that value is the 2nd argument from the argument list (popped items donÄt count anymore)


 Also: C++ Helper functions:

	Sort(Node, key, pred)		- sorts a sequence of maps by the value of the map element 'key'
	Sum(Node)					- sums double values
	Sum<int>(Node)				- sums int values (any type for which as<type> is available)
	Avg(Node)					- as sum, but average, of course
	Accumulate(Node,...)		- same with start value


e.g. for  node = 

	- { type : fish, color : silvery, weight: 2.2 }
	- { type : dog, color : brown, weight : 24 }
	- { type : fish, color : white, weight : 0.9 }

	Sum(PathSelect(node, "[type=$]", "fish")) == 3.1
	Sum(PathSelect(node, "[type=$]", "dog")) == 24
	Sum(PathSelect(node, "[type=$]", "bird")) == 0

	Note that PathSelect creates the nodes


-----------


Select Path vs. Select Multi

Given a yaml of 
			--- 
			people: 
			  -   name : Joe
				  color: red
				  names: 3
			  -   name : Sina
				  color: blue
				  names: 4
			  -   name : Estragon
				  voice : none
			      color : red
			      
			dogs:
			  - Woof:
				 color: chamois
			  - Bark:
				 color: chocoloco
			peeps: 
			  - Mario: 
				  names: 4
			  - Joe: 
				  names: 3
			  - Mario: 
				  color: red
				  names: 2
			----


	
				
	root:"people" --> [ { name : Joe, ... }, { name : Sina, ... }, { name : Estragon, ... }]

	root:"people.name"  --> [ Joe, Sina, Estragon ]
		a key selector, applied to a sequence, is applied to its elements, returning a sequence of values
		maybe we should also allow "people.[name]" as alternate syntax


	root:"people[name=Joe]" --> [ { name : Joe, ... } ]
		selects all list elements that have the attribute "name=Joe"


	root:"people[color=red]" --> [ { name : Joe, ... }, { name : Estragon, ... } ]

	root:"people[color=red][0]" --> { name : Joe, ... }
		selects the first element of the list


	Idea for Functions:

		$ismap, $isseq, $isscalar
			returns the current nodeif it is a map (or sequence, or scalar, respectively). Returns an empty node and stops the scan otherwise.

		$unique
			if the current node is a sequence with exactly one element, $unique returns its first element. Otherwise, it returns an emoty node and
			stops the scan.




-----------

Things that require a better parser:

[(keyA=A & (keyB=B, (keyC=C, keyC=C2)))]   - i.e. full boolean expressions

Things that require a non-linear search:

{**}   - all items of a given map, and its children, to an arbitrary depth. Could be used as
        {**}.abc   - find all paths that containan abc somewhere






		