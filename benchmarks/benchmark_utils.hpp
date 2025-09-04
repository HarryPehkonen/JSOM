#pragma once

#include <string>
#include <sstream>

namespace benchmark_utils {

inline std::string get_small_json() {
    return R"({
        "id": 1234567890,
        "text": "This is a typical social media post with some #hashtags and @mentions",
        "created_at": "2024-01-15T10:30:00Z",
        "user": {
            "id": 9876543210,
            "username": "testuser",
            "display_name": "Test User",
            "verified": true,
            "followers_count": 15420,
            "profile": {
                "bio": "Software developer and JSON enthusiast",
                "location": "San Francisco, CA",
                "website": "https://example.com"
            }
        },
        "metrics": {
            "retweets": 42,
            "likes": 156,
            "replies": 23,
            "engagement_rate": 0.127
        },
        "entities": {
            "hashtags": ["json", "performance", "parsing"],
            "urls": ["https://example.com/article"],
            "mentions": ["testuser2", "developer"]
        },
        "metadata": {
            "source": "web",
            "lang": "en",
            "sensitive": false
        }
    })";
}

inline std::string get_medium_json() {
    std::ostringstream oss;
    oss << R"({
        "status": "success",
        "pagination": {
            "page": 1,
            "per_page": 100,
            "total": 5000,
            "total_pages": 50
        },
        "data": [)";
    
    for (int i = 0; i < 100; ++i) {
        if (i > 0) oss << ",";
        oss << R"(
            {
                "id": )" << (i + 1000) << R"(,
                "sku": "PROD-)" << (i + 1000) << R"(",
                "name": "Product )" << i << R"(",
                "category": "electronics",
                "price": {
                    "amount": )" << (99.99 + i * 0.50) << R"(,
                    "currency": "USD",
                    "tax_rate": 0.08
                },
                "inventory": {
                    "quantity": )" << (100 - i) << R"(,
                    "reserved": )" << (i % 10) << R"(,
                    "available": )" << (100 - i - (i % 10)) << R"(
                },
                "dimensions": {
                    "width": )" << (10.5 + i * 0.1) << R"(,
                    "height": )" << (5.2 + i * 0.05) << R"(,
                    "depth": )" << (2.8 + i * 0.02) << R"(,
                    "weight": )" << (1.5 + i * 0.01) << R"(
                },
                "ratings": {
                    "average": )" << (3.5 + (i % 20) * 0.1) << R"(,
                    "count": )" << (50 + i * 3) << R"(
                },
                "active": )" << (i % 2 == 0 ? "true" : "false") << R"(,
                "created_timestamp": )" << (1640995200 + i * 3600) << R"(
            })";
    }
    
    oss << R"(
        ],
        "meta": {
            "request_id": "req_123456789",
            "processing_time_ms": 45.67,
            "cache_hit": true
        }
    })";
    
    return oss.str();
}

inline std::string get_large_json() {
    std::ostringstream oss;
    oss << R"({
        "export_info": {
            "timestamp": 1641024000,
            "record_count": 5000,
            "version": "2.1"
        },
        "users": [)";
    
    for (int i = 0; i < 5000; ++i) {
        if (i > 0) oss << ",";
        oss << R"(
            {
                "id": )" << (1000000 + i) << R"(,
                "username": "user_)" << i << R"(",
                "email": "user)" << i << R"(@example.com",
                "profile": {
                    "first_name": "User",
                    "last_name": ")" << i << R"(",
                    "age": )" << (18 + i % 50) << R"(,
                    "registration_date": ")" << (2020 + i % 4) << R"(-01-01",
                    "last_login_timestamp": )" << (1640995200 + i * 1800) << R"(,
                    "account_balance": )" << (100.0 + i * 1.5) << R"(,
                    "credit_score": )" << (600 + i % 200) << R"(,
                    "location": {
                        "country": "US",
                        "state": "CA",
                        "city": "San Francisco",
                        "coordinates": {
                            "latitude": )" << (37.7749 + (i % 100) * 0.001) << R"(,
                            "longitude": )" << (-122.4194 + (i % 100) * 0.001) << R"(,
                            "altitude": )" << (50 + i % 100) << R"(
                        }
                    }
                },
                "activity": {
                    "posts_count": )" << (i % 500) << R"(,
                    "followers_count": )" << (i % 10000) << R"(,
                    "following_count": )" << ((i + 50) % 1000) << R"(,
                    "likes_given": )" << (i * 5) << R"(,
                    "likes_received": )" << (i * 3) << R"(
                },
                "preferences": {
                    "notifications": )" << (i % 2 == 0 ? "true" : "false") << R"(,
                    "privacy_level": )" << (i % 3) << R"(,
                    "theme": ")" << (i % 2 == 0 ? "dark" : "light") << R"("
                },
                "subscription": {
                    "plan": ")" << (i % 3 == 0 ? "premium" : "basic") << R"(",
                    "price": )" << (i % 3 == 0 ? 9.99 : 0.0) << R"(,
                    "renewal_date": "2024-)" << ((i % 12) + 1) << R"(-01"
                }
            })";
    }
    
    oss << R"(
        ]
    })";
    
    return oss.str();
}

inline std::string get_deep_nested_json() {
    std::ostringstream oss;
    oss << "{";
    
    for (int level = 0; level < 20; ++level) {
        oss << R"("level)" << level << R"(": {
            "depth": )" << level << R"(,
            "name": "Level )" << level << R"(",
            "next": {)";
    }
    
    // Final level with array
    oss << R"("final": {
            "depth": 20,
            "values": [1, 2, 3, 4, 5],
            "coordinates": [
                {"x": 10.5, "y": 20.7, "z": 30.9},
                {"x": 11.5, "y": 21.7, "z": 31.9},
                {"x": 12.5, "y": 22.7, "z": 32.9}
            ]
        })";
    
    // Close all the nested objects
    for (int level = 0; level < 21; ++level) {
        oss << "}";
    }
    
    return oss.str();
}

inline std::string get_number_heavy_json() {
    std::ostringstream oss;
    oss << R"({
        "metadata": {
            "dataset": "metrics_export",
            "generated_at": 1641024000,
            "record_count": 1000,
            "format_version": 1.2
        },
        "metrics": [)";
    
    for (int i = 0; i < 1000; ++i) {
        if (i > 0) oss << ",";
        oss << R"(
            {
                "timestamp": )" << (1640995200 + i * 60) << R"(,
                "cpu_usage": )" << (0.15 + (i % 85) * 0.01) << R"(,
                "memory_usage": )" << (0.45 + (i % 50) * 0.01) << R"(,
                "disk_io_read": )" << (1000 + i * 10) << R"(,
                "disk_io_write": )" << (500 + i * 5) << R"(,
                "network_in": )" << (1024 + i * 13) << R"(,
                "network_out": )" << (2048 + i * 7) << R"(,
                "requests_per_second": )" << (100 + i % 200) << R"(,
                "response_time_ms": )" << (50.5 + (i % 100) * 0.5) << R"(,
                "error_rate": )" << (0.001 + (i % 50) * 0.0001) << R"(,
                "temperature": )" << (65.2 + (i % 20) * 0.1) << R"(,
                "power_consumption": )" << (150.0 + (i % 30) * 1.5) << R"(,
                "scientific_value": )" << (1.23e10 + i * 1e6) << R"(,
                "negative_metric": )" << (-456.789 - i * 0.1) << R"(
            })";
    }
    
    oss << R"(
        ],
        "summary": {
            "total_samples": 1000,
            "avg_cpu": 0.567,
            "avg_memory": 0.723,
            "max_response_time": 99.5,
            "min_response_time": 50.5,
            "p95_response_time": 89.2,
            "p99_response_time": 95.8
        }
    })";
    
    return oss.str();
}

} // namespace benchmark_utils