criu2json
=========

criu image to/from json converter

== Intro ==
criu2json was created to make criu images readable and editable by human being.
Note that criu2json is still under the heavy development and all feedback is
highly appreciated.

== Compilation ==
To compile criu2json you will need jansson and protobuf-c to be installed.
Run:
	make
to do everything automatically. It will clone criu git and compile it to
get needed resources.
If you don't want criu git to be downloaded just put criu sources into
directory named criu and run "make".

== Usage ==
Note, that option parser is extremely dumb and will understand options
only in order described below.

criu2json OPTION SRC DEST [verbose]

OPTION:
	to-json        convert criu image named SRC into json file DEST
	to-img         convert json file named SRC into criu img file DEST
	-v --verbose   be verbose

Examples:
	criu2json to-json core-1234.img core-1234.json
	criu2json to-img core-1234.json core-1234.img
