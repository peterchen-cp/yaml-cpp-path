# yaml-path

[YAML](https://yaml.org/) is a structured document format.  
[yaml-cpp](https://github.com/jbeder/yaml-cpp) (by jbeder) is a C++ library to read and write YAML files.  
yaml-path extends yaml-cpp, making it easier to select content from a YAML file.

yaml-path requires C++ 17.


## What's the point?

The central addition of yaml-path is a function

`Node Select(Node n, path_arg path)`

That uses `path` (a string) to select one or more nodes from `n`.
e.g.

```c++
// example data: 
Nore root = Load(R"(
-  name : Joe
   color: red
   names: 
      first : Joe
	  middle : Joeboy
	  last: Joeson
-  name : Sina
   color: blue
   names: 4
-  name : Estragon
   voice : none)";      
)");

Node result = Select(root, "name"); 
// --> returns a list ["Joe", "Sina", "Estragon"]

Node result = Select(root, "[0].names.middle"); 
// --> returns a scalar "Joeboy"
```

Note: `Node` from yaml-cpp represents a document node of a YAML document. `Load` from yaml-cpp loads text as YAML document and returns
a `Node`. All symbols are defined in the `YAML` namespace.

**Status:**  

Used in production. currently implements the initially required/envisioned features, some ideas (`yaml-path\readme.txt`) are pending.
I'm still willing to break syntaxt, esp. the path specification. 


## Getting Started

The folder `yaml-path` contains all the sources that you need to add to your project.
Make sure `#include <yaml-cpp/yaml.h>` finds the yaml-cpp headers, and link to the yaml-cpp library.  

`test.cpp` contains tests, using [doctest](https://github.com/onqtam/doctest).  

There is no make script.  
The "msvc" branch contains a MSVC 2017 project that includes a snapshot of yaml-cpp and doctest. 
It's always ahead of master, and matches the current master (in other words, there's real history of that)

In the github "Releases" section, you find 

 - doxygen documentation, including full specification of selectors
 - a precompiled Windows x86binary that allows to test and play around with yaml paths.


 ## Examples


 For a YAML document of 

	- name: Joe
	  color: red
	  friends: ~
	- name: Sina
	  color: blue
	- name: Estragon
	  color: red
	  friends:
		Wladimir: good
		Godot: unreliable

this is a list of example paths and the nodes they return:

**Path:** `"name"` 

    - Joe
    - Sina
    - Estragon

**Path:** `"[1].name"`

    Sina

**Path:** `"name[2]"`

    Estragon

**Path:** `"color"`

    - red
    - blue
    - red

**Path:** `"{color=red}"`

	- name: Joe
	  color: red
	  friends: ~
	- name: Estragon
	  color: red
	  friends:
		Wladimir: good
		Godot: unreliable

**Path:** `"{color=blue}"`

    - name: Sina
      color: blue


**Path:** `"{friends=}"`

	- name: Joe
	  color: red
	  friends: ~
	- name: Estragon
	  color: red
	  friends:
		Wladimir: good
		Godot: unreliable

**Path:** `"friends.Wladimir"`

    - good

**Path:** `"friends.Wladimir[0]"`

    unreliable

**Path:** `"[1]"`

    - name: Sina
      color: blue


**Path:** `"[1].color"`

      blue






## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details
