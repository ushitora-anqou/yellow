#pragma once
#ifndef YELLOW_CONFIG_HPP
#define YELLOW_CONFIG_HPP

#include <string>

namespace yellow {
class Config {
public:
    static std::string cache_file_path() { return "cache.json"; }
    static std::string consumer_key() { return "gvvptGxzifQsssUppNBTxC25m"; }
    static std::string consumer_secret()
    {
        return "g5wny2GLzP5V8Naza9kFNdGfhuDMmceTJyMhDwyzUTF0y9KGlT";
    }
};
}  // namespace yellow

#endif
