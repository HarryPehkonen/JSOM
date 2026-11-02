jsom(3)

# NAME

jsom - JavaScript Object Model (C++17 JSON library)

# SYNOPSIS

The installed CMake package:

	find_package(JSOM CONFIG REQUIRED)
	target_link_libraries(app PRIVATE JSOM::jsom)

pkg-config, for build systems that do not use CMake:

	c++ app.cpp $(pkg-config --cflags --libs jsom)

A short program: parse a document, read a value with a JSON Pointer, print it.

	#include <jsom/jsom.hpp>
	#include <iostream>

	int main() {
		auto doc = jsom::parse_document(R"({"users": [{"name": "Alice"}]})");
		const auto& name = doc.at("/users/0/name");
		std::cout << name.as<std::string>() << '\\n';
	}

# DESCRIPTION

JSOM is a C++17 library for working with JSON in memory. The public header is
_<jsom/jsom.hpp>_, which includes the rest. The design rests on a few
decisions:

*The RFC 8259 grammar is enforced while scanning.*
	The number grammar of section 6 is checked in the same pass that reads the text,
	so being strict is free; the lexical rules (escape sequences, control
	characters and whitespace) are enforced unconditionally. Strictness is
	the default, and the non-JSON number forms of section 9 are an opt-in extension.

*Numbers are kept as text.*
	The original representation is stored in a _LazyNumber_ and converted
	only when a value is asked for, so _1.500_ and _1e10_ round-trip byte for
	byte.

*Navigation is RFC 6901 with no cache.*
	A lookup reads the document directly, so it cannot go stale, and a const
	document can be navigated from several threads at once. Mutable
	navigation is a separate overload.

*Formatting is driven by presets.*
	_compact_, _pretty_, _config_, _api_ and _debug_ cover the common shapes,
	and every field of the options structure can be set directly.

*One version, one home.*
	The project version is written once, in the project(JSOM VERSION ...)
	line of CMakeLists.txt; CMake generates _<jsom/version.hpp>_ from it, so
	the library, the CLI and the release tag all read the same value.

# PARSING

_jsom::parse_document_(_json_) and _parse_document_(_json_, _options_) are the
entry points. There is exactly one parser behind them, _FastParser_, a
recursive-descent parser that builds the document directly, so "is this valid
JSON?" is answered the same way everywhere.

_jsom::JsonParseOptions_ (_json_parse_options.hpp_) controls a parse:

*convert_unicode_escapes* (default false)
	Convert \\uXXXX escapes to UTF-8. Off, the escape is kept as literal
	text, which preserves a document byte for byte across a round trip.

*allow_comments* (default false)
	Accept // line and /\* \*/ block comments.

*max_depth* (default 256)
	The deepest nesting accepted. Input that nests deeper is rejected with a
	depth error rather than exhausting the C++ stack.

*allow_loose_numbers* (default false)
	Accept number forms that are not valid JSON (_01_, _1._, _1eE2_). Off,
	the number grammar of section 6 is enforced.

The presets _ParsePresets::Default_, _ParsePresets::Unicode_,
_ParsePresets::Comments_ and _ParsePresets::Loose_ select the common
combinations.

The lexical rules of the grammar are enforced whatever the options say: a raw
control character inside a string is rejected; \\u must be followed by four hex
digits; an escape outside " \\ / b f n r t u is rejected; and whitespace is
exactly space, tab, line feed and carriage return.

# DOCUMENT MODEL

_jsom::JsonDocument_ holds one JSON value in a _std::variant_ over null, bool,
_LazyNumber_, _std::string_, an object (_std::map<std::string, JsonDocument>_)
and an array (_std::vector<JsonDocument>_). _jsom::JsonType_ names the six
kinds.

A document is built by implicit construction from a primitive
(_JsonDocument name = "Alice";_), from an initializer list of key/value pairs,
or from _make_array_() and _make_object_(); _from_map_() and _from_vector_()
convert the standard containers. It is read with _as_<_T_>() (which throws
_jsom::TypeException_ on a type mismatch) or _try_as_<_T_>() (which returns a
_std::optional_), and inspected with _is_object_(), _size_(), _empty_(),
_contains_(), _keys_() and _items_(). Arrays iterate with the range-for syntax;
objects iterate with _items_() and structured bindings.

Comparison is deep and total, ordered null < boolean < number < string < object
< array, so _==_, _!=_, _<_ and the rest work on whole documents.

# JSON POINTER

Navigation follows RFC 6901. The document methods are:

*at*(_path_)
	The value at _path_, or *jsom::JsonPointerNotFoundException* when the
	path is absent.

*find*(_path_)
	A pointer to the value, or _nullptr_. _at_() and _find_() each have a
	const and a non-const overload; the non-const one returns a mutable
	reference or pointer.

*exists*(_path_)
	_true_ when the path resolves.

*set_at*(_path_, _value_), *remove_at*(_path_), *extract_at*(_path_)
	Set, remove, or remove-and-return the value at _path_. _remove_at_()
	returns _false_ when the path is absent; _set_at_() creates intermediate
	containers as needed.

*at_multiple*(_paths_), *exists_multiple*(_paths_)
	Resolve many paths in one call, returning the results in the order given.

*list_paths*(_max_depth_), *find_paths*(_pattern_), *count_paths*()
	Enumerate every path, the paths matching a pattern, or the number of
	paths. _list_paths_() takes an optional depth limit.

The escapes of RFC 6901 are handled: _~1_ in a segment means / and _~0_ means
~, so the key "a/b" is addressed as "/a~1b". An array index is validated
strictly, with no leading zeros.

Failures throw from the _jsom::JsonPointerException_ hierarchy:
_InvalidJsonPointerException_ for a malformed pointer,
_JsonPointerNotFoundException_ for an absent path, and
_JsonPointerTypeException_ for a mismatch. Each carries _get_pointer_().

# FORMATTING

*to_json*() serializes compactly. *to_json*(_pretty_) indents when _pretty_ is
true. *to_json*(_options_) takes a _jsom::JsonFormatOptions_ for full control.
The presets _FormatPresets::Compact_, _FormatPresets::Pretty_,
_FormatPresets::Config_, _FormatPresets::Api_ and _FormatPresets::Debug_ are
ready-made option sets.

_jsom::JsonFormatOptions_ (_json_format_options.hpp_) has these fields, with
their defaults: _indent_size_ 2, _sort_keys_ off, _max_inline_array_size_ 10,
_max_inline_object_size_ 3, _max_line_width_ 120 (0 means no limit),
_align_values_ off, _colon_spacing_ 1, _bracket_spacing_ off, _trailing_comma_
off, _escape_unicode_ off and _intelligent_wrapping_ off.

# ERRORS

*jsom::ParseError*
	Thrown for every parse and format failure. It derives from
	_std::runtime_error_, so a catch for that keeps working, and carries a
	_code_() of type _jsom::ParseErrorCode_: _NestingDepthExceeded_,
	_InvalidNumber_, _InvalidEscape_, _InvalidUnicodeEscape_,
	_RawControlCharacter_, _UnterminatedString_, _UnterminatedComment_,
	_InvalidLiteral_, _ExpectedToken_, _EmptyInput_ and _TrailingContent_.

*jsom::TypeException*
	Thrown when a value is read as the wrong type.

*jsom::JsonPointerException* and its subclasses
	Thrown by navigation, as described above.

A stack overflow cannot be caught, so the depth limit is what keeps parsing a
total function on hostile input; every traversal (serialization, comparison,
path listing) honours the same bound.

# THREAD SAFETY

Reading a document is safe from any number of threads at once: _at_(), _find_(),
_exists_(), _at_multiple_(), _list_paths_() and serialization on a const document
touch nothing that changes. Mutating a document while another thread reads or
writes it is not supported. A reference or pointer returned by these methods
points into the document and follows ordinary container rules, so any mutation
that reallocates invalidates it. The promise is checked by the project's
_thread_safety_probe_, built with ThreadSanitizer.

# VERSION

The version has one home: the project(JSOM VERSION ...) line of CMakeLists.txt.
CMake configures _<jsom/version.hpp>_ from it, defining _JSOM_VERSION_ and the
_JSOM_VERSION_MAJOR_, _JSOM_VERSION_MINOR_ and _JSOM_VERSION_PATCH_ components.
The CLI's *version* command prints the same string.

# FILES

_${prefix}/include/jsom/_
	The public headers, including the generated _version.hpp_.

_${prefix}/<libdir>/libjsom.so.3_
	The shared library, with soname 3. It is installed by the runtime
	package: _libjsom3_ on Debian-based systems and _jsom-libs_ on
	Fedora-based ones. _<libdir>_ is the multiarch library directory,
	_/usr/lib/<triplet>/_ on Debian and _/usr/lib64/_ on Fedora.

_${prefix}/<libdir>/libjsom.so_
	The linker symlink that -ljsom resolves, in the development package.

_${prefix}/<libdir>/cmake/JSOM/_
	The CMake package config, providing the target _JSOM::jsom_. Development
	package.

_${prefix}/<libdir>/pkgconfig/jsom.pc_
	The pkg-config metadata for -ljsom. Development package.

_${prefix}/share/man/man3/jsom.3_
	This page. Development package.

Debian-based systems split the packages as _jsom_ (this command-line
processor), _libjsom3_ (the shared library) and _libjsom-dev_ (development).
Fedora-based systems use the same split under the names _jsom_, _jsom-libs_ and
_jsom-devel_. The development package ships the headers, the generated
_version.hpp_, the CMake package config (target _JSOM::jsom_), the pkg-config
file and this page.

The packages ship the *shared* library, _libjsom.so.3_. A static archive
(_libjsom.a_) is what an in-tree source build produces by default; it is not
part of the packages.

# EXAMPLES

Parse a document and read a value:

	#include <jsom/jsom.hpp>

	auto doc = jsom::parse_document(R"({"name": "Alice", "age": 30})");
	std::cout << doc.at("/name").as<std::string>() << '\\n';   // Alice

Build a document and format it compactly:

	jsom::JsonDocument user{{"name", "Alice"}, {"age", 30}};
	std::cout << user.to_json(jsom::FormatPresets::Compact) << '\\n';

The original number text survives a round trip:

	auto doc = jsom::parse_document(R"({"n": 1.500})");
	doc.to_json();                       // {"n":1.500}

Parse a commented config, with the loose number forms allowed:

	jsom::JsonParseOptions options;
	options.allow_comments = true;
	options.allow_loose_numbers = true;
	auto cfg = jsom::parse_document(text, options);

Hold a pointer once for a hot loop, instead of navigating each time:

	const auto\* leaf = doc.find("/very/deep/path/to/a/leaf");
	for (int i = 0; i < 10000; ++i)
		consume(leaf->as<std::string>());

Report a parse failure by kind:

	try {
		auto doc = jsom::parse_document(text);
	} catch (const jsom::ParseError& e) {
		switch (e.code()) { /\* ... \*/ }
	}

# SEE ALSO

*jsom*(1) - the command-line JSON processor built on this library

https://github.com/HarryPehkonen/JSOM

# AUTHORS

Harri Pehkonen <harry.pehkonen@gmail.com>
