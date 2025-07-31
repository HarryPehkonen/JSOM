#include "jsom.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <map>
#include <cstring>

class JsomCli {
private:
    std::vector<std::string> args_;
    std::string command_;
    std::map<std::string, std::string> options_;
    std::vector<std::string> files_;

    static constexpr const char* VERSION = "1.0.0";
    static constexpr int SUCCESS_EXIT_CODE = 0;
    static constexpr int ERROR_EXIT_CODE = 1;
    static constexpr size_t OPTION_PREFIX_LENGTH = 2; // "--"
    static constexpr const char* HELP_TEXT = R"(
JSOM - High-performance JSON processor

USAGE:
    jsom <COMMAND> [OPTIONS] [FILES...]

COMMANDS:
    format      Format JSON with intelligent pretty printing
    validate    Validate JSON files and report errors
    parse       Parse and display JSON structure
    help        Show this help message
    version     Show version information

FORMAT OPTIONS:
    --preset=<preset>    Use formatting preset (compact|pretty|config|api|debug)
    --indent=<n>         Set indentation size (default: 2)
    --inline-arrays=<n>  Max array size for inlining (default: 10)
    --inline-objects=<n> Max object size for inlining (default: 3)

GLOBAL OPTIONS:
    --help, -h          Show help
    --version, -v       Show version
    --verbose           Enable verbose output

EXAMPLES:
    jsom format --preset=pretty data.json
    jsom validate *.json
    jsom format --indent=4 < input.json > output.json
    echo '{"test": true}' | jsom format --preset=compact

For more information, visit: https://github.com/your-org/jsom
)";

public:
    explicit JsomCli(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            args_.emplace_back(argv[i]);
        }
        parse_arguments();
    }

    auto run() -> int {
        if (command_.empty() || command_ == "help") {
            show_help();
            return SUCCESS_EXIT_CODE;
        }

        if (command_ == "version") {
            show_version();
            return SUCCESS_EXIT_CODE;
        }

        try {
            if (command_ == "format") {
                return run_format();
            }
            if (command_ == "validate") {
                return run_validate();
            }
            if (command_ == "parse") {
                return run_parse();
            }

            std::cerr << "Error: Unknown command '" << command_ << "'\n";
            std::cerr << "Use 'jsom help' for usage information.\n";
            return ERROR_EXIT_CODE;

        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
            return ERROR_EXIT_CODE;
        }
    }

private:
    void parse_arguments() {
        if (args_.empty()) {
            return;
        }

        // First argument is the command
        command_ = args_[0];

        // Parse options and files
        static constexpr size_t FIRST_OPTION_INDEX = 1;
        for (size_t i = FIRST_OPTION_INDEX; i < args_.size(); ++i) {
            const auto& arg = args_[i];
            
            if (arg == "--help" || arg == "-h") {
                command_ = "help";
                return;
            }
            
            if (arg == "--version" || arg == "-v") {
                command_ = "version";
                return;
            }
            
            if (arg.size() >= OPTION_PREFIX_LENGTH && arg.substr(0, OPTION_PREFIX_LENGTH) == "--") {
                auto eq_pos = arg.find('=');
                if (eq_pos != std::string::npos) {
                    std::string key = arg.substr(OPTION_PREFIX_LENGTH, eq_pos - OPTION_PREFIX_LENGTH);
                    std::string value = arg.substr(eq_pos + 1);
                    options_[key] = value;
                } else {
                    options_[arg.substr(OPTION_PREFIX_LENGTH)] = "true";
                }
            } else {
                files_.push_back(arg);
            }
        }
    }

    void show_help() {
        std::cout << HELP_TEXT << '\n';
    }

    void show_version() {
        std::cout << "jsom " << VERSION << '\n';
        std::cout << "High-performance JSON processor built with JSOM library\n";
    }

    auto get_format_options() -> jsom::JsonFormatOptions {
        jsom::JsonFormatOptions opts;

        // Handle preset option
        if (options_.find("preset") != options_.end()) {
            const auto& preset = options_["preset"];
            if (preset == "compact") {
                opts = jsom::FormatPresets::Compact;
            } else if (preset == "pretty") {
                opts = jsom::FormatPresets::Pretty;
            } else if (preset == "config") {
                opts = jsom::FormatPresets::Config;
            } else if (preset == "api") {
                opts = jsom::FormatPresets::Api;
            } else if (preset == "debug") {
                opts = jsom::FormatPresets::Debug;
            } else {
                throw std::invalid_argument("Unknown preset: " + preset);
            }
        } else {
            opts = jsom::FormatPresets::Pretty; // Default to pretty
        }

        // Override with specific options
        if (options_.find("indent") != options_.end()) {
            opts.indent_size = std::stoi(options_["indent"]);
        }
        if (options_.find("inline-arrays") != options_.end()) {
            opts.max_inline_array_size = std::stoi(options_["inline-arrays"]);
        }
        if (options_.find("inline-objects") != options_.end()) {
            opts.max_inline_object_size = std::stoi(options_["inline-objects"]);
        }

        return opts;
    }

    auto read_json_input() -> std::string {
        std::string input;
        
        if (files_.empty()) {
            // Read from stdin
            std::string line;
            while (std::getline(std::cin, line)) {
                input += line + '\n';
            }
        } else {
            // Read from first file
            std::ifstream file(files_[0]);
            if (!file) {
                throw std::runtime_error("Cannot open file: " + files_[0]);
            }
            
            std::string line;
            while (std::getline(file, line)) {
                input += line + '\n';
            }
        }
        
        return input;
    }

    auto run_format() -> int {
        try {
            auto format_opts = get_format_options();
            
            if (files_.empty()) {
                // Process stdin
                auto input = read_json_input();
                auto doc = jsom::parse_document(input);
                std::cout << doc.to_json(format_opts) << '\n';
            } else {
                // Process each file
                for (const auto& filename : files_) {
                    std::ifstream file(filename);
                    if (!file) {
                        std::cerr << "Warning: Cannot open file: " << filename << '\n';
                        continue;
                    }
                    
                    std::string content;
                    std::string line;
                    while (std::getline(file, line)) {
                        content += line + '\n';
                    }
                    
                    auto doc = jsom::parse_document(content);
                    
                    if (files_.size() > 1) {
                        std::cout << "=== " << filename << " ===\n";
                    }
                    std::cout << doc.to_json(format_opts) << '\n';
                }
            }
            return SUCCESS_EXIT_CODE;
        } catch (const std::exception& e) {
            std::cerr << "Format error: " << e.what() << '\n';
            return ERROR_EXIT_CODE;
        }
    }

    auto run_validate() -> int {
        int error_count = 0;
        
        if (files_.empty()) {
            // Validate stdin
            try {
                auto input = read_json_input();
                jsom::parse_document(input);
                std::cout << "Valid JSON (stdin)\n";
            } catch (const std::exception& e) {
                std::cerr << "Invalid JSON (stdin): " << e.what() << '\n';
                error_count++;
            }
        } else {
            // Validate each file
            for (const auto& filename : files_) {
                try {
                    std::ifstream file(filename);
                    if (!file) {
                        std::cerr << "Error: Cannot open file: " << filename << '\n';
                        error_count++;
                        continue;
                    }
                    
                    std::string content;
                    std::string line;
                    while (std::getline(file, line)) {
                        content += line + '\n';
                    }
                    
                    jsom::parse_document(content);
                    
                    if (options_.find("verbose") != options_.end()) {
                        std::cout << "✓ Valid: " << filename << '\n';
                    }
                } catch (const std::exception& e) {
                    std::cerr << "✗ Invalid: " << filename << " - " << e.what() << '\n';
                    error_count++;
                }
            }
            
            if (error_count == 0 && options_.find("verbose") == options_.end()) {
                std::cout << "All files are valid JSON (" << files_.size() << " files)\n";
            }
        }
        
        return error_count > 0 ? ERROR_EXIT_CODE : SUCCESS_EXIT_CODE;
    }

    auto run_parse() -> int {
        try {
            auto input = read_json_input();
            auto doc = jsom::parse_document(input);
            
            std::cout << "JSON Structure Analysis:\n";
            std::cout << "  Type: " << get_type_name(doc) << '\n';
            std::cout << "  Size: " << get_size_info(doc) << '\n';
            std::cout << "  Depth: " << calculate_depth(doc) << '\n';
            std::cout << "\nFormatted JSON:\n";
            std::cout << doc.to_json(jsom::FormatPresets::Pretty) << '\n';
            
            return SUCCESS_EXIT_CODE;
        } catch (const std::exception& e) {
            std::cerr << "Parse error: " << e.what() << '\n';
            return ERROR_EXIT_CODE;
        }
    }

    static auto get_type_name(const jsom::JsonDocument& doc) -> std::string {
        if (doc.is_null()) return "null";
        if (doc.is_bool()) return "boolean";
        if (doc.is_number()) return "number";
        if (doc.is_string()) return "string";
        if (doc.is_array()) return "array";
        if (doc.is_object()) return "object";
        return "unknown";
    }

    static auto get_size_info(const jsom::JsonDocument& doc) -> std::string {
        if (doc.is_array() || doc.is_object()) {
            return std::to_string(doc.size()) + " elements";
        }
        if (doc.is_string()) {
            return std::to_string(doc.as<std::string>().length()) + " characters";
        }
        return "1 value";
    }

    static auto calculate_depth(const jsom::JsonDocument& doc) -> int {
        static constexpr int BASE_DEPTH = 1;
        static constexpr int OBJECT_ESTIMATE_DEPTH = 2;
        
        if (!doc.is_array() && !doc.is_object()) {
            return BASE_DEPTH;
        }
        
        int max_child_depth = 0;
        if (doc.is_array()) {
            for (size_t i = 0; i < doc.size(); ++i) {
                max_child_depth = std::max(max_child_depth, calculate_depth(doc[i]));
            }
        } else if (doc.is_object()) {
            // Note: We'd need to iterate over object properties, but that requires
            // access to the internal storage. For now, we'll estimate.
            return OBJECT_ESTIMATE_DEPTH; // Simple estimation
        }
        
        return BASE_DEPTH + max_child_depth;
    }
};

auto main(int argc, char* argv[]) -> int {
    JsomCli cli(argc, argv);
    return cli.run();
}