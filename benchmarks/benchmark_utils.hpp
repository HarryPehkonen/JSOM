#pragma once

#include <string>
#include <memory>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace benchmark_utils {

// Get the path to benchmark data files
inline auto get_data_path() -> std::string {
    return "benchmarks/data/";
}

// Read a JSON file as string
inline auto read_json_file(const std::string& filename) -> std::string {
    std::string filepath = get_data_path() + filename;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        // If file doesn't exist, generate it
        return "";
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

// Generate small JSON (~1KB) - typical tweet/message
inline auto generate_small_json() -> std::string {
    return R"({
  "id": 1234567890,
  "created_at": "2024-01-15T10:30:00Z",
  "user": {
    "id": 12345,
    "username": "benchmark_user",
    "display_name": "Benchmark User",
    "verified": true,
    "followers_count": 15420,
    "following_count": 892
  },
  "text": "This is a sample tweet for benchmarking JSON parsing performance. It contains various data types including numbers, strings, booleans, and nested objects. #benchmark #json #performance",
  "metrics": {
    "retweet_count": 42,
    "like_count": 156,
    "quote_count": 12,
    "reply_count": 8
  },
  "entities": {
    "hashtags": [
      {"text": "benchmark", "start": 147, "end": 157},
      {"text": "json", "start": 158, "end": 163},
      {"text": "performance", "start": 164, "end": 176}
    ],
    "urls": [],
    "mentions": []
  },
  "context_annotations": [
    {
      "domain": {
        "id": "65",
        "name": "Technology",
        "description": "Technology and computing"
      },
      "entity": {
        "id": "781974596752842752",
        "name": "JSON",
        "description": "JavaScript Object Notation"
      }
    }
  ],
  "public_metrics": {
    "retweet_count": 42,
    "like_count": 156,
    "quote_count": 12,
    "reply_count": 8
  },
  "lang": "en",
  "possibly_sensitive": false
})";
}

// Generate medium JSON (~50KB) - typical API response
inline auto generate_medium_json() -> std::string {
    std::ostringstream json;
    json << R"({
  "status": "success",
  "pagination": {
    "page": 1,
    "per_page": 100,
    "total": 1000,
    "total_pages": 10
  },
  "data": [)";
    
    for (int i = 0; i < 100; ++i) {
        if (i > 0) json << ",";
        json << R"(
    {
      "id": )" << (1000 + i) << R"(,
      "name": "Product )" << i << R"(",
      "description": "This is a detailed description of product )" << i << R"( which includes various features and specifications that make it unique in the market.",
      "category": {
        "id": )" << (i % 10 + 1) << R"(,
        "name": "Category )" << (i % 10) << R"(",
        "slug": "category-)" << (i % 10) << R"("
      },
      "price": {
        "amount": )" << (99.99 + i * 10) << R"(,
        "currency": "USD",
        "formatted": "$)" << (99.99 + i * 10) << R"("
      },
      "inventory": {
        "in_stock": )" << (i % 3 == 0 ? "true" : "false") << R"(,
        "quantity": )" << (i * 3 + 10) << R"(,
        "reserved": )" << (i % 5) << R"(
      },
      "attributes": {
        "color": ")" << (i % 2 == 0 ? "blue" : "red") << R"(",
        "size": ")" << (i % 3 == 0 ? "large" : (i % 3 == 1 ? "medium" : "small")) << R"(",
        "weight": )" << (1.5 + i * 0.1) << R"(,
        "dimensions": {
          "length": )" << (10 + i % 5) << R"(,
          "width": )" << (8 + i % 3) << R"(,
          "height": )" << (5 + i % 2) << R"(
        }
      },
      "tags": [)" << (i % 2 == 0 ? "\"popular\", \"featured\"" : "\"new\", \"limited\"") << R"(],
      "created_at": "2024-01-)" << (1 + i % 28) << R"(T)" << (i % 24) << R"(:)" << (i % 60) << R"(:00Z",
      "updated_at": "2024-01-)" << (1 + i % 28) << R"(T)" << ((i + 1) % 24) << R"(:)" << ((i + 30) % 60) << R"(:00Z"
    })";
    }
    
    json << R"(
  ]
})";
    
    return json.str();
}

// Generate large JSON (~2MB) - data export style
inline auto generate_large_json() -> std::string {
    std::ostringstream json;
    json << R"({
  "export_info": {
    "generated_at": "2024-01-15T10:30:00Z",
    "format_version": "1.0",
    "total_records": 5000,
    "source": "benchmark_data_generator"
  },
  "records": [)";
    
    for (int i = 0; i < 5000; ++i) {
        if (i > 0) json << ",";
        json << R"(
    {
      "record_id": )" << i << R"(,
      "timestamp": "2024-01-)" << (1 + i % 28) << R"(T)" << (i % 24) << R"(:)" << (i % 60) << R"(:)" << ((i * 17) % 60) << R"(Z",
      "user_data": {
        "user_id": )" << (10000 + i) << R"(,
        "username": "user_)" << i << R"(",
        "email": "user)" << i << R"(@example.com",
        "profile": {
          "first_name": "User",
          "last_name": ")" << i << R"(",
          "age": )" << (18 + i % 60) << R"(,
          "location": {
            "country": ")" << (i % 5 == 0 ? "US" : (i % 5 == 1 ? "UK" : (i % 5 == 2 ? "CA" : (i % 5 == 3 ? "AU" : "DE")))) << R"(",
            "city": "City )" << (i % 100) << R"(",
            "coordinates": {
              "lat": )" << (40.0 + (i % 180) - 90) << R"(,
              "lng": )" << (-120.0 + (i % 360) - 180) << R"(
            }
          }
        }
      },
      "activity": {
        "login_count": )" << (i % 1000) << R"(,
        "last_login": "2024-01-)" << (1 + i % 28) << R"(T)" << ((i + 12) % 24) << R"(:)" << ((i + 30) % 60) << R"(:00Z",
        "session_duration": )" << (300 + i % 3600) << R"(,
        "pages_visited": [)" << ((i % 10) + 1);
        
        for (int j = 1; j < (i % 5) + 1; ++j) {
            json << ", " << ((i + j) % 50 + 1);
        }
        
        json << R"(],
        "actions_performed": )" << (i % 50) << R"(
      },
      "preferences": {
        "theme": ")" << (i % 2 == 0 ? "dark" : "light") << R"(",
        "language": ")" << (i % 3 == 0 ? "en" : (i % 3 == 1 ? "es" : "fr")) << R"(",
        "notifications": {
          "email": )" << (i % 3 != 0 ? "true" : "false") << R"(,
          "push": )" << (i % 2 == 0 ? "true" : "false") << R"(,
          "sms": )" << (i % 5 == 0 ? "true" : "false") << R"(
        },
        "privacy": {
          "profile_public": )" << (i % 4 != 0 ? "true" : "false") << R"(,
          "activity_tracking": )" << (i % 3 == 0 ? "true" : "false") << R"(
        }
      },
      "metrics": {
        "score": )" << (i % 100) << R"(,
        "level": )" << (1 + i % 20) << R"(,
        "achievements": )" << (i % 15) << R"(,
        "points": )" << (i * 47 % 10000) << R"(
      }
    })";
    }
    
    json << R"(
  ]
})";
    
    return json.str();
}

// Generate deeply nested JSON
inline auto generate_deep_nested_json() -> std::string {
    std::ostringstream json;
    
    // Create a deeply nested structure (20 levels deep)
    json << R"({"level0": {"data": "value0", "next": )";
    for (int i = 1; i < 20; ++i) {
        json << R"({"level)" << i << R"(": {"data": "value)" << i << R"(", "index": )" << i << R"(, "next": )";
    }
    
    json << R"({"final": {"message": "This is the deepest level", "depth": 20, "values": [)";
    for (int i = 0; i < 50; ++i) {
        if (i > 0) json << ", ";
        json << i;
    }
    json << R"(]}})"; 
    
    // Close all the nested objects
    for (int i = 0; i < 20; ++i) {
        json << "}}";
    }
    
    return json.str();
}

// Get small JSON data
inline auto get_small_json() -> std::string {
    static std::string data = generate_small_json();
    return data;
}

// Get medium JSON data
inline auto get_medium_json() -> std::string {
    static std::string data = generate_medium_json();
    return data;
}

// Get large JSON data
inline auto get_large_json() -> std::string {
    static std::string data = generate_large_json();
    return data;
}

// Get deep nested JSON data
inline auto get_deep_nested_json() -> std::string {
    static std::string data = generate_deep_nested_json();
    return data;
}

} // namespace benchmark_utils