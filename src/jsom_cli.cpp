#include "jsom/jsom.hpp"
#include "jsom/json_pointer.hpp"
#include "jsom/constants.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <iomanip>

using namespace jsom;

// CLI Version
const std::string VERSION = "1.0.0";

// Utility functions
auto read_stdin() -> std::string {
    std::stringstream buffer;
    buffer << std::cin.rdbuf();
    return buffer.str();
}

auto read_file(const std::string& filename) -> std::string {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void write_file(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot write to file: " + filename);
    }
    file << content;
}

// Split string by delimiter
auto split(const std::string& str, char delimiter) -> std::vector<std::string> {
    std::vector<std::string> tokens;
    // NOLINTNEXTLINE(readability-identifier-length)
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

// Dump preset settings in a readable format
void dump_preset_settings(const jsom::JsonFormatOptions& options, const std::string& preset_name) {
    std::cout << "Preset '" << preset_name << "' configuration:\n";
    std::cout << std::string(60, '=') << "\n\n";
    
    // Format each setting with CLI switch as label
    std::cout << "Basic Settings:\n";
    std::cout << "  --indent:                " << (options.indent_size.has_value() ? 
                                                     std::to_string(*options.indent_size) + " spaces" : 
                                                     "compact mode") << "\n";
    
    std::cout << "\nInlining Controls:\n";
    std::cout << "  --inline-arrays:         " << options.max_inline_array_size << "\n";
    std::cout << "  --inline-objects:        " << options.max_inline_object_size << "\n";
    
    std::cout << "\nLayout Controls:\n";
    std::cout << "  --max-width:             " << (options.max_line_width == 0 ? "unlimited" : std::to_string(options.max_line_width)) << "\n";
    std::cout << "  --align-values:          " << (options.align_values ? "enabled" : "disabled") << "\n";
    std::cout << "  --intelligent-wrap:      " << (options.intelligent_wrapping ? "enabled" : "disabled") << "\n";
    
    std::cout << "\nSpacing Controls:\n";
    std::cout << "  --colon-spacing:         " << options.colon_spacing << " spaces\n";
    std::cout << "  --bracket-spacing:       " << (options.bracket_spacing ? "enabled" : "disabled") << "\n";
    
    std::cout << "\nAdvanced Options:\n";
    std::cout << "  --sort-keys:             " << (options.sort_keys ? "enabled" : "disabled") << "\n";
    std::cout << "  --escape-unicode:        " << (options.escape_unicode ? "enabled" : "disabled") << "\n";
    std::cout << "  --trailing-comma:        " << (options.trailing_comma ? "enabled" : "disabled") << "\n";
    
    // Build equivalent command line
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "Equivalent command:\n";
    std::cout << "jsom format";
    
    // Only show non-default options
    JsonFormatOptions defaults;
    if (options.indent_size.has_value()) { std::cout << " --indent=" << *options.indent_size;
}
    if (options.max_inline_array_size != defaults.max_inline_array_size) { std::cout << " --inline-arrays=" << options.max_inline_array_size;
}
    if (options.max_inline_object_size != defaults.max_inline_object_size) { std::cout << " --inline-objects=" << options.max_inline_object_size;
}
    if (options.max_line_width != defaults.max_line_width) { std::cout << " --max-width=" << options.max_line_width;
}
    if (options.align_values) { std::cout << " --align-values";
}
    if (options.colon_spacing != defaults.colon_spacing) { std::cout << " --colon-spacing=" << options.colon_spacing;
}
    if (options.bracket_spacing) { std::cout << " --bracket-spacing";
}
    if (options.sort_keys) { std::cout << " --sort-keys";
}
    if (options.escape_unicode) { std::cout << " --escape-unicode";
}
    if (options.trailing_comma) { std::cout << " --trailing-comma";
}
    if (options.intelligent_wrapping) { std::cout << " --intelligent-wrap";
}
    
    std::cout << " [FILE]\n\n";
}

// Show usage information
void show_usage() {
    std::cout << R"(JSOM - High-performance JSON processor with RFC 6901 JSON Pointer support

USAGE:
    jsom <COMMAND> [OPTIONS] [FILES...]

COMMANDS:
    format      Format JSON with intelligent pretty printing (try --help for presets)
    validate    Validate JSON syntax and report errors
    pointer     JSON Pointer operations per RFC 6901 (try --help for subcommands)
    benchmark   Performance testing and optimization
    help        Show this help message
    version     Show version information

Use 'jsom <COMMAND> --help' for more information on a specific command.
)";
}

// NOLINTBEGIN(readability-function-size)
void show_pointer_usage() {
    std::cout << R"(JSON Pointer operations (RFC 6901)

USAGE:
    jsom pointer <SUBCOMMAND> [OPTIONS] [FILE]

SUBCOMMANDS:
    get <path>              Get value at JSON Pointer path
    exists <path>           Check if path exists
    list [OPTIONS]          List all available paths
    find <pattern>          Find paths matching pattern
    set <path> <value>      Set value at path
    remove <path>           Remove value at path
    extract <path>          Extract subtree at path
    bulk-get <paths...>     Get multiple paths efficiently
    benchmark <paths...>    Benchmark path access performance

OPTIONS:
    --max-depth=<n>         Maximum depth for path enumeration
    --include-values        Include values in path listings
    --cache-warm            Pre-warm path cache for performance
    --cache-stats           Show cache performance statistics
    --format=<fmt>          Output format (json|text|compact)

EXAMPLES:
    jsom pointer get "/users/0/name" data.json
    jsom pointer exists "/config/database/host" config.json
    jsom pointer list --max-depth=3 --include-values data.json
    jsom pointer find "/users/*/email" data.json
    jsom pointer bulk-get "/users/0/name,/users/0/age" data.json
)";
}
// NOLINTEND(readability-function-size)

// Format command with advanced formatting options
auto format_command(const std::vector<std::string>& args) -> int {
    const std::string PRESET_SWITCH = "--preset=";
    const std::string INDENT_SWITCH = "--indent=";
    const std::string INLINE_ARRAYS_SWITCH = "--inline-arrays=";
    const std::string INLINE_OBJECTS_SWITCH = "--inline-objects=";
    const std::string MAX_WIDTH_SWITCH = "--max-width=";
    const std::string COLON_SPACING_SWITCH = "--colon-spacing=";
    jsom::JsonFormatOptions options = jsom::FormatPresets::Pretty;  // Default to pretty
    std::string input_file;
    bool dump_settings = false;
    std::string preset_name = "pretty";
    
    // Parse arguments
    for (size_t i = 2; i < args.size(); ++i) {
        const auto& arg = args[i];
        
        if (arg == "--help") {
            std::cout << R"(Format JSON files with intelligent pretty printing

USAGE:
    jsom format [OPTIONS] [FILE]

PRESET OPTIONS:
    --preset=compact    Minimal bandwidth, storage efficiency
    --preset=pretty     General-purpose readable formatting (default)
    --preset=config     Configuration files, settings
    --preset=api        API responses, data interchange  
    --preset=debug      Debugging, development

CUSTOM OPTIONS:
    --compact           Same as --preset=compact
    --indent=<n>        Set indentation size (default: 2)
    --inline-arrays=<n> Max array size for inlining (default: 10)
    --inline-objects=<n> Max object size for inlining (default: 3)
    --max-width=<n>     Maximum line width (default: 120, 0 = no limit)
    --align-values      Align object values at same column
    --colon-spacing=<n> Spaces around colons: 0, 1, or 2 (default: 1)
    --bracket-spacing   Add spacing inside brackets/braces
    --sort-keys         Sort object keys alphabetically
    --escape-unicode    Escape non-ASCII characters as \uXXXX
    --trailing-comma    Add trailing commas (non-standard)
    --intelligent-wrap  Enable intelligent array wrapping (multiple elements per line)
    --no-intelligent-wrap  Disable intelligent array wrapping

INSPECTION:
    --dump              Show all settings for the selected preset

EXAMPLES:
    jsom format --preset=compact data.json
    jsom format --preset=pretty data.json
    jsom format --preset=config data.json
    jsom format --indent=4 --inline-arrays=5 data.json
    jsom format --max-width=80 --align-values data.json
    echo '{"test": [1,2,3]}' | jsom format --preset=debug
    jsom format --preset=pretty --dump          # Show pretty preset settings
    jsom format --preset=api --dump             # Show API preset settings
    jsom format --dump                          # Show current/default settings
)";
            return 0;
        } if (arg.substr(0, PRESET_SWITCH.length()) == PRESET_SWITCH) {
            std::string preset = arg.substr(PRESET_SWITCH.length());
            preset_name = preset;  // Track preset name for dumping
            if (preset == "compact") {
                options = jsom::FormatPresets::Compact;
            } else if (preset == "pretty") {
                options = jsom::FormatPresets::Pretty;
            } else if (preset == "config") {
                options = jsom::FormatPresets::Config;
            } else if (preset == "api") {
                options = jsom::FormatPresets::Api;
            } else if (preset == "debug") {
                options = jsom::FormatPresets::Debug;
            } else {
                std::cerr << "Unknown preset: " << preset << "\n";
                std::cerr << "Valid presets: compact, pretty, config, api, debug\n";
                return 1;
            }
        } else if (arg == "--compact") {
            options = jsom::FormatPresets::Compact;
            preset_name = "compact";
        } else if (arg.substr(0, INDENT_SWITCH.length()) == INDENT_SWITCH) {
            options.indent_size = std::stoi(arg.substr(INDENT_SWITCH.length()));
        } else if (arg.substr(0, INLINE_ARRAYS_SWITCH.length()) == INLINE_ARRAYS_SWITCH) {
            options.max_inline_array_size = std::stoi(arg.substr(INLINE_ARRAYS_SWITCH.length()));
        } else if (arg.substr(0, INLINE_OBJECTS_SWITCH.length()) == INLINE_OBJECTS_SWITCH) {
            options.max_inline_object_size = std::stoi(arg.substr(INLINE_OBJECTS_SWITCH.length()));
        } else if (arg.substr(0, MAX_WIDTH_SWITCH.length()) == MAX_WIDTH_SWITCH) {
            options.max_line_width = std::stoi(arg.substr(MAX_WIDTH_SWITCH.length()));
        } else if (arg == "--align-values") {
            options.align_values = true;
        } else if (arg.substr(0, COLON_SPACING_SWITCH.length()) == COLON_SPACING_SWITCH) {
            int spacing = std::stoi(arg.substr(COLON_SPACING_SWITCH.length()));
            if (spacing >= 0 && spacing <= 2) {
                options.colon_spacing = spacing;
            } else {
                std::cerr << "Invalid colon spacing: " << spacing << " (must be 0, 1, or 2)\n";
                return 1;
            }
        } else if (arg == "--bracket-spacing") {
            options.bracket_spacing = true;
        } else if (arg == "--sort-keys") {
            options.sort_keys = true;
        } else if (arg == "--escape-unicode") {
            options.escape_unicode = true;
        } else if (arg == "--trailing-comma") {
            options.trailing_comma = true;
        } else if (arg == "--intelligent-wrap") {
            options.intelligent_wrapping = true;
        } else if (arg == "--no-intelligent-wrap") {
            options.intelligent_wrapping = false;
        } else if (arg == "--dump") {
            dump_settings = true;
        } else if (arg[0] != '-') {
            input_file = arg;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }
    
    // If dump was requested, show settings and exit
    if (dump_settings) {
        dump_preset_settings(options, preset_name);
        return 0;
    }
    
    try {
        // Read JSON
        std::string json = input_file.empty() ? read_stdin() : read_file(input_file);
        
        // Parse and format
        auto doc = parse_document(json);
        std::cout << doc.to_json(options) << '\n';
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}

// Validate command
auto validate_command(const std::vector<std::string>& args) -> int {
    if (args.size() < 3) {
        std::cerr << "Usage: jsom validate <file1> [file2] ...\n";
        return 1;
    }
    
    bool all_valid = true;
    
    for (size_t i = cli_constants::FIRST_OPTION_INDEX; i < args.size(); ++i) {
        const auto& filename = args[i];
        if (filename == "--help") {
            std::cout << "Validate JSON files\n\n";
            std::cout << "USAGE: jsom validate <file1> [file2] ...\n";
            return 0;
        }
        
        try {
            std::string json = read_file(filename);
            parse_document(json);
            std::cout << filename << ": Valid JSON" << '\n';
        } catch (const std::exception& e) {
            std::cerr << filename << ": Invalid JSON - " << e.what() << '\n';
            all_valid = false;
        }
    }
    
    return all_valid ? 0 : 1;
}

// Pointer get subcommand
auto pointer_get(const std::string& path, const std::string& input_file) -> int {
    try {
        std::string json = input_file.empty() ? read_stdin() : read_file(input_file);
        auto doc = parse_document(json);
        
        const auto& value = doc.at(path);
        std::cout << value.to_json() << '\n';
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}

// Pointer exists subcommand
auto pointer_exists(const std::string& path, const std::string& input_file) -> int {
    try {
        std::string json = input_file.empty() ? read_stdin() : read_file(input_file);
        auto doc = parse_document(json);
        
        bool exists = doc.exists(path);
        std::cout << (exists ? "true" : "false") << '\n';
        
        return exists ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 2;
    }
}

// Pointer list subcommand
auto pointer_list(const std::vector<std::string>& args, const std::string& input_file) -> int {
    int max_depth = -1;
    bool include_values = false;
    
    // Parse options
    const std::string MAX_DEPTH_SWITCH = "--max-depth=";
    for (const auto& arg : args) {
        if (arg.substr(0, MAX_DEPTH_SWITCH.length()) == MAX_DEPTH_SWITCH) {
            max_depth = std::stoi(arg.substr(MAX_DEPTH_SWITCH.length()));
        } else if (arg == "--include-values") {
            include_values = true;
        }
    }
    
    try {
        std::string json = input_file.empty() ? read_stdin() : read_file(input_file);
        auto doc = parse_document(json);
        
        auto paths = doc.list_paths(max_depth);
        
        for (const auto& path : paths) {
            if (include_values && !path.empty()) {
                try {
                    const auto& value = doc.at(path);
                    std::cout << path << ": " << value.to_json() << '\n';
                } catch (...) {
                    std::cout << path << '\n';
                }
            } else {
                std::cout << path << '\n';
            }
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}

// Pointer find subcommand
auto pointer_find(const std::string& pattern, const std::string& input_file) -> int {
    try {
        std::string json = input_file.empty() ? read_stdin() : read_file(input_file);
        auto doc = parse_document(json);
        
        auto paths = doc.find_paths(pattern);
        
        for (const auto& path : paths) {
            std::cout << path << '\n';
        }
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}

// Pointer set subcommand
auto pointer_set(const std::string& path, const std::string& value_str, const std::string& input_file) -> int {
    try {
        std::string json = input_file.empty() ? read_stdin() : read_file(input_file);
        auto doc = parse_document(json);
        
        // Try to parse value as JSON first
        JsonDocument value;
        try {
            value = parse_document(value_str);
        } catch (...) {
            // If not valid JSON, treat as string
            value = JsonDocument(value_str);
        }
        
        doc.set_at(path, value);
        std::cout << doc.to_json(true) << '\n';
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}

// Pointer remove subcommand
auto pointer_remove(const std::string& path, const std::string& input_file) -> int {
    try {
        std::string json = input_file.empty() ? read_stdin() : read_file(input_file);
        auto doc = parse_document(json);
        
        bool removed = doc.remove_at(path);
        if (!removed) {
            std::cerr << "Path not found: " << path << '\n';
            return 1;
        }
        
        std::cout << doc.to_json(true) << '\n';
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}

// Pointer extract subcommand
auto pointer_extract(const std::string& path, const std::string& input_file) -> int {
    try {
        std::string json = input_file.empty() ? read_stdin() : read_file(input_file);
        auto doc = parse_document(json);
        
        auto extracted = doc.extract_at(path);
        std::cout << extracted.to_json(true) << '\n';
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}

// Pointer bulk-get subcommand
auto pointer_bulk_get(const std::string& paths_str, const std::string& input_file) -> int {
    try {
        std::string json = input_file.empty() ? read_stdin() : read_file(input_file);
        auto doc = parse_document(json);
        
        auto paths = split(paths_str, ',');
        auto results = doc.at_multiple(paths);
        
        std::cout << "{" << '\n';
        for (size_t i = 0; i < paths.size(); ++i) {
            std::cout << "  \"" << paths[i] << "\": ";
            if (results[i] != nullptr) {
                std::cout << results[i]->to_json();
            } else {
                std::cout << "null";
            }
            if (i < paths.size() - 1) {
                std::cout << ",";
            }
            std::cout << '\n';
        }
        std::cout << "}" << '\n';
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}

// Pointer benchmark subcommand
auto pointer_benchmark(const std::string& paths_str, const std::string& input_file, bool warm_cache) -> int {
    try {
        std::string json = input_file.empty() ? read_stdin() : read_file(input_file);
        auto doc = parse_document(json);
        
        auto paths = split(paths_str, ',');
        
        // Warm cache if requested
        if (warm_cache) {
            doc.warm_path_cache(paths);
        }
        
        // Benchmark each path
        std::cout << "Path Access Benchmarks:" << '\n';
        std::cout << std::string(cli_constants::SEPARATOR_LINE_WIDTH, '-') << '\n';
        
        for (const auto& path : paths) {
            // Time benchmark iterations
            auto start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < cli_constants::BENCHMARK_ITERATIONS; ++i) {
                volatile auto* result = doc.find(path);
                (void)result; // Prevent optimization
            }
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            double avg_ns = duration.count() / cli_constants::BENCHMARK_DIVISOR;
            
            std::cout << std::left << std::setw(cli_constants::BENCHMARK_PATH_COLUMN_WIDTH) << path 
                     << std::right << std::setw(cli_constants::BENCHMARK_TIME_COLUMN_WIDTH) << std::fixed << std::setprecision(cli_constants::BENCHMARK_PRECISION) 
                     << avg_ns << " ns/access" << '\n';
        }
        
        // Show cache stats
        auto stats = doc.get_path_cache_stats();
        std::cout << std::string(cli_constants::SEPARATOR_LINE_WIDTH, '-') << '\n';
        std::cout << "Cache Statistics:" << '\n';
        std::cout << "  Exact cache size: " << stats.exact_cache_size << '\n';
        std::cout << "  Prefix cache size: " << stats.prefix_cache_size << '\n';
        std::cout << "  Total entries: " << stats.total_entries << '\n';
        std::cout << "  Memory usage: " << stats.memory_usage_estimate << " bytes" << '\n';
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}

// Pointer command dispatcher
auto pointer_command(const std::vector<std::string>& args) -> int {
    // args[0] = "jsom", args[1] = "pointer", args[2] = subcommand
    if (args.size() < 3 || (args.size() >= 3 && args[2] == "--help")) {
        show_pointer_usage();
        return 0;
    }
    
    const std::string& subcommand = args[2];
    
    // Extract options and file
    std::vector<std::string> options;
    std::string input_file;
    bool warm_cache = false;
    
    // For most subcommands, we need to identify which arg is the file
    // The pattern is: jsom pointer <subcommand> <path/pattern> [options] [file]
    // Special cases: 
    // - "list" has no required path argument
    // - "set" has two required arguments before the file
    // - "bulk-get" and "benchmark" take comma-separated paths
    
    size_t expected_args = 3; // Default: subcommand + path
    if (subcommand == "list") {
        expected_args = 2; // No path required
    } else if (subcommand == "set") {
        expected_args = 4; // path + value
    }
    
    // Find the file argument (last non-option argument)
    for (size_t i = 3; i < args.size(); ++i) {
        const auto& arg = args[i];
        if (arg == "--cache-warm") {
            warm_cache = true;
        } else if (arg[0] == '-') {
            options.push_back(arg);
        }
    }
    
    // The file is the last argument if it's beyond the expected arguments and not an option
    if (args.size() > expected_args && args.back()[0] != '-') {
        input_file = args.back();
    }
    
    // Dispatch to subcommand
    if (subcommand == "get") {
        if (args.size() < 4) {
            std::cerr << "Usage: jsom pointer get <path> [file]\n";
            return 1;
        }
        return pointer_get(args[3], input_file);
        
    } if (subcommand == "exists") {
        if (args.size() < 4) {
            std::cerr << "Usage: jsom pointer exists <path> [file]\n";
            return 1;
        }
        return pointer_exists(args[3], input_file);
        
    } if (subcommand == "list") {
        return pointer_list(options, input_file);
        
    } else if (subcommand == "find") {
        if (args.size() < 4) {
            std::cerr << "Usage: jsom pointer find <pattern> [file]\n";
            return 1;
        }
        return pointer_find(args[3], input_file);
        
    } else if (subcommand == "set") {
        if (args.size() < 5) {
            std::cerr << "Usage: jsom pointer set <path> <value> [file]\n";
            return 1;
        }
        return pointer_set(args[3], args[4], input_file);
        
    } else if (subcommand == "remove") {
        if (args.size() < 4) {
            std::cerr << "Usage: jsom pointer remove <path> [file]\n";
            return 1;
        }
        return pointer_remove(args[3], input_file);
        
    } else if (subcommand == "extract") {
        if (args.size() < 4) {
            std::cerr << "Usage: jsom pointer extract <path> [file]\n";
            return 1;
        }
        return pointer_extract(args[3], input_file);
        
    } else if (subcommand == "bulk-get") {
        if (args.size() < 4) {
            std::cerr << "Usage: jsom pointer bulk-get <paths> [file]\n";
            return 1;
        }
        return pointer_bulk_get(args[3], input_file);
        
    } else if (subcommand == "benchmark") {
        if (args.size() < 4) {
            std::cerr << "Usage: jsom pointer benchmark <paths> [file]\n";
            return 1;
        }
        return pointer_benchmark(args[3], input_file, warm_cache);
        
    } else {
        std::cerr << "Unknown pointer subcommand: " << subcommand << std::endl;
        show_pointer_usage();
        return 1;
    }
}

// Benchmark command
auto benchmark_command(const std::vector<std::string>& args) -> int {
    std::string input_file;
    
    for (size_t i = cli_constants::FIRST_OPTION_INDEX; i < args.size(); ++i) {
        const auto& arg = args[i];
        if (arg == "--help") {
            std::cout << "Benchmark JSON operations\n\n";
            std::cout << "USAGE: jsom benchmark [FILE]\n";
            return 0;
        } if (arg[0] != '-') {
            input_file = arg;
        }
    }
    
    try {
        std::string json = input_file.empty() ? read_stdin() : read_file(input_file);
        
        // Parse benchmark
        auto parse_start = std::chrono::high_resolution_clock::now();
        auto doc = parse_document(json);
        auto parse_end = std::chrono::high_resolution_clock::now();
        
        // Serialize benchmark
        auto serialize_start = std::chrono::high_resolution_clock::now();
        std::string output = doc.to_json();
        auto serialize_end = std::chrono::high_resolution_clock::now();
        
        auto parse_time = std::chrono::duration_cast<std::chrono::milliseconds>(parse_end - parse_start);
        auto serialize_time = std::chrono::duration_cast<std::chrono::milliseconds>(serialize_end - serialize_start);
        
        std::cout << "Benchmark Results:" << '\n';
        std::cout << "  Input size: " << json.size() << " bytes" << '\n';
        std::cout << "  Parse time: " << parse_time.count() << " ms" << '\n';
        std::cout << "  Serialize time: " << serialize_time.count() << " ms" << '\n';
        std::cout << "  Total time: " << (parse_time.count() + serialize_time.count()) << " ms" << '\n';
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}

auto main(int argc, char* argv[]) -> int {
    if (argc < 2) {
        show_usage();
        return 1;
    }
    
    std::vector<std::string> args(argv, argv + argc);
    const std::string& command = args[1];
    
    if (command == "help" || command == "--help" || command == "-h") {
        show_usage();
        return 0;
    } if (command == "version" || command == "--version" || command == "-v") {
        std::cout << "JSOM version " << VERSION << '\n';
        return 0;
    } if (command == "format") {
        return format_command(args);
    } else if (command == "validate") {
        return validate_command(args);
    } else if (command == "pointer") {
        return pointer_command(args);
    } else if (command == "benchmark") {
        return benchmark_command(args);
    } else {
        std::cerr << "Unknown command: " << command << std::endl;
        show_usage();
        return 1;
    }
}
