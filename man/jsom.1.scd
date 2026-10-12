jsom(1)

# NAME

jsom - high-performance JSON processor (RFC 6901 JSON Pointer)

# SYNOPSIS

*jsom* <command> [options] [files...]

# DESCRIPTION

*jsom* reads, formats, validates and navigates JSON. RFC 6901 JSON Pointer
support is built in. Files are given as arguments; JSON is read from stdin
when no file is given.

# COMMANDS

*format* [file]
	Format JSON with intelligent pretty printing. Presets:
	_compact_ (minimal bandwidth), _pretty_ (default), _config_,
	_api_, _debug_.

*validate* [file...]
	Validate JSON syntax and report errors.

*pointer* <subcommand> <path> [file]
	JSON Pointer operations per RFC 6901. Subcommands include
	*get* <path>, *exists* <path>.

*benchmark* [file]
	Performance testing and optimization.

*version*
	Print version information.

*help*
	Print usage and exit.

# FORMAT OPTIONS

*--preset=<name>* (compact | pretty | config | api | debug)
	Choose a formatting preset.

*--indent=<n>*
	Set indentation size (default 2).

# EXAMPLES

Format stdin and sort keys:

	echo '{"b":2,"a":1}' | jsom format

Get a value by JSON Pointer:

	jsom pointer get /b/c/0 data.json

# SEE ALSO

https://github.com/HarryPehkonen/JSOM - full documentation

# AUTHORS

Harri Pehkonen <harry.pehkonen@gmail.com>
