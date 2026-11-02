jsom(1)

# NAME

jsom - high-performance JSON processor (RFC 6901 JSON Pointer)

# SYNOPSIS

jsom <command> [options] [files...]

# DESCRIPTION

*jsom* formats, validates and navigates JSON from the command line. The command
is named first and its arguments follow. A command that reads a document reads
a named file, or standard input when no file is given. The processor is built
on the JSOM C++17 library (see *jsom*(3)): the RFC 8259 grammar is enforced
while the text is scanned, numbers are kept as written so a document round-trips
unchanged, and navigation uses RFC 6901 JSON Pointers.

# COMMANDS

*format* [options] [file]
	Reformat a JSON document: pretty-print it, compact it, or apply a preset.
	Reads standard input when no file is named. With no options it uses the
	_pretty_ preset. See *OPTIONS* and *PRESETS* below.

*validate* <file> [file...]
	Parse each named file and report whether it is valid JSON. At least one
	file is required; validation does not read standard input. The exit
	status is 0 only when every named file is valid.

*pointer* <subcommand> [arguments] [file]
	RFC 6901 JSON Pointer operations. See *POINTER SUBCOMMANDS* below.

*benchmark* [file]
	Parse the document and re-serialize it, timing each step in milliseconds.
	Reads standard input when no file is named.

*help*, *--help*, *-h*
	Print the usage summary and exit 0.

*version*, *--version*, *-v*
	Print the JSOM version and exit 0. The string comes from
	_<jsom/version.hpp>_, which CMake generates from the project version (see
	*jsom*(3)).

Every command also accepts *--help* as its first argument, which prints that
command's own help and exits 0.

# POINTER SUBCOMMANDS

*get* <_path_> [_file_]
	Print the value at the JSON Pointer _path_ as compact JSON. The exit
	status is 1 if the path is absent or the pointer is malformed.

*exists* <_path_> [_file_]
	Print _true_ or _false_ for whether _path_ resolves. The exit status
	distinguishes the two: 0 when it exists, 1 when it does not, and 2 when
	the document could not be read or parsed.

*list* [_file_]
	List every path in the document, one per line.

*find* <_pattern_> [_file_]
	List the paths matching _pattern_. A \* matches one path segment, so
	"/users/\*/email" finds the email of every user.

*set* <_path_> <_value_> [_file_]
	Set the value at _path_ and print the whole document (indented). _value_
	is parsed as JSON when it is valid JSON, and stored as a string
	otherwise.

*remove* <_path_> [_file_]
	Remove the value at _path_ and print the document. The exit status is 1
	when the path is absent.

*extract* <_path_> [_file_]
	Remove the value at _path_ and print the removed subtree.

*bulk-get* <_paths_> [_file_]
	Resolve several paths in one call. _paths_ is a comma-separated list; the
	results print as a JSON object keyed by path, with _null_ for a path that
	is absent.

*benchmark* <_paths_> [_file_]
	Time repeated lookups of each path in the comma-separated _paths_ list.

# OPTIONS

Every command accepts *--help*, which prints the command's own help and exits.

*--preset*=<_name_>
	Choose a formatting preset: _compact_, _pretty_, _config_, _api_ or
	_debug_. The default is _pretty_. An unknown name is an error.

*--compact*
	The same as *--preset=compact*.

*--indent*=<_n_>
	Spaces per indentation level. Default 2.

*--inline-arrays*=<_n_>
	Arrays of _n_ or fewer elements stay on one line. Default 10 (8 under the
	default _pretty_ preset).

*--inline-objects*=<_n_>
	Objects of _n_ or fewer properties stay on one line. Default 3.

*--max-width*=<_n_>
	Soft maximum line width; 0 means no limit. Default 120 (100 under the
	default _pretty_ preset).

*--align-values*
	Align object values in a common column. Off by default; already on in the
	_pretty_, _config_ and _debug_ presets.

*--colon-spacing*=<_n_>
	Spaces around a colon: 0, 1 or 2. Default 1. Any other value is an error.

*--bracket-spacing*
	Add a space just inside brackets and braces. Off by default; already on
	in the _api_ and _debug_ presets.

*--sort-keys*
	Sort object keys alphabetically. Off by default; already on in the
	_config_ and _debug_ presets.

*--escape-unicode*
	Write non-ASCII characters as \\uXXXX. Off by default; already on in the
	_debug_ preset.

*--trailing-comma*
	Write a trailing comma after the last element of every object and array
	(non-standard JSON). Off by default.

*--intelligent-wrap*, *--no-intelligent-wrap*
	Enable or disable wrapping several array elements onto one line. Off by
	default; already on in the _pretty_ and _api_ presets.

*--comments*
	Allow // line and /\* \*/ block comments in the input. Off by default.
	Accepted by *format* and *validate*.

*--validation*=numbers|loose
	The number grammar of RFC 8259 section 6 is enforced with _numbers_, the
	default. _loose_ accepts the non-JSON number forms _01_, _1._ and _1eE2_
	as documented extensions. Escape sequences, control characters and
	whitespace are validated in both modes. Accepted by *format* and
	*validate*.

*--dump*
	With *format*, print the resolved settings for the selected preset and
	exit without reading a document.

The *pointer* subcommands take no options: any argument beginning with -- after a
subcommand is rejected, with exit status 1. A pointer subcommand reads a named
file, or standard input when no file is given.

Options are applied left to right, so when two set the same field the later one
wins.

# PRESETS

A preset is a starting point that the switches above modify.

_compact_
	No indentation and no line-width limit: one line.

_pretty_
	The default. Two-space indent; arrays of up to 8 elements inline; line
	width 100; aligned values; intelligent wrapping.

_config_
	Two-space indent; keys sorted; arrays of up to 5 inline and objects of up
	to 1 inline; line width 100; aligned values.

_api_
	Two-space indent; arrays of up to 15 inline; objects of up to 4 inline;
	line width 120; spacing inside brackets; intelligent wrapping.

_debug_
	Four-space indent; keys sorted; every element on its own line; line width
	80; aligned values; spacing inside brackets; \\uXXXX escaping.

# EXIT STATUS

*0*
	Success.

*1*
	Failure. This covers an unknown command or option, an unknown preset or
	an out-of-range colon spacing, a missing required argument, a file that
	cannot be opened, a parse error, a navigation error, a *validate* run in
	which any file is invalid, *pointer remove* on a path that is absent, and
	*pointer exists* on a path that is absent.

*2*
	*pointer exists* only, when its input document could not be read or
	parsed.

# EXAMPLES

Format standard input with the default pretty preset:

	echo '{"b":2,"a":1}' | jsom format

Format to one line:

	echo '{"a":[1,2,3]}' | jsom format --preset=compact

Apply a preset, and override individual switches:

	jsom format --preset=config settings.json

	jsom format --indent=4 --max-width=80 data.json

Validate several files; the exit status is the verdict:

	jsom validate a.json b.json

Validate a commented config, permitting the loose number forms:

	jsom validate --comments --validation=loose exports.jsonc

Read a value with a JSON Pointer:

	jsom pointer get "/users/0/name" data.json

Read a value from standard input, with no file:

	echo '{"users":[{"name":"Alice"}]}' | jsom pointer get "/users/0/name"

Test a path; the exit status says whether it exists:

	jsom pointer exists "/config/database/host" config.json

Per RFC 6901, a key containing / is addressed with the ~1 escape and a key
containing ~ with the ~0 escape. For the document {"a/b":1,"m~n":2}:

	jsom pointer get "/a~1b" data.json
	jsom pointer get "/m~0n" data.json

Find paths by pattern, and read several at once:

	jsom pointer find "/users/\*/email" data.json

	jsom pointer bulk-get "/users/0/name,/users/0/age" data.json

Change a document:

	jsom pointer set "/users/0/active" true data.json
	jsom pointer remove "/users/0/temp" data.json
	jsom pointer extract "/config/legacy" data.json

Time parsing and serialization:

	jsom benchmark large.json

# SEE ALSO

*jsom*(3)

https://github.com/HarryPehkonen/JSOM

# AUTHORS

Harri Pehkonen <harry.pehkonen@gmail.com>
