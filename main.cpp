#include <cassert>
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

#include "picojson.h"
#include "twitcurl.h"

namespace yellow {

#define YELLOW_ASSERT(cond) assert(cond);
#define YELLOW_RANGE(con) std::begin(con), std::end(con)

class TwitterClient {
private:
    twitCurl twit_;

private:
    template <class Func>
    picojson::value get_json_response(Func func)
    {
        func();
        std::string msg;
        twit_.getLastWebResponse(msg);
        picojson::value json;
        picojson::parse(json, msg);
        return json;
    }

public:
    TwitterClient()
    {
        static const std::string
            consumer_key = "gvvptGxzifQsssUppNBTxC25m",
            consumer_secret =
                "g5wny2GLzP5V8Naza9kFNdGfhuDMmceTJyMhDwyzUTF0y9KGlT";

        twit_.getOAuth().setConsumerKey(consumer_key);
        twit_.getOAuth().setConsumerSecret(consumer_secret);
    }

    std::string get_token_key()
    {
        std::string key;
        twit_.getOAuth().getOAuthTokenKey(key);
        return key;
    }
    std::string get_token_secret()
    {
        std::string secret;
        twit_.getOAuth().getOAuthTokenSecret(secret);
        return secret;
    }

    void set_token_key(const std::string& key)
    {
        twit_.getOAuth().setOAuthTokenKey(key);
    }
    void set_token_secret(const std::string& secret)
    {
        twit_.getOAuth().setOAuthTokenSecret(secret);
    }

    template <class Callback>
    void set_token_key_and_secret_by_pin(Callback cb)
    {
        std::string url;
        twit_.oAuthRequestToken(url);
        std::string pin = cb(url);
        twit_.getOAuth().setOAuthPin(pin);
        twit_.oAuthAccessToken();
    }

    picojson::value get_account__verify_credentials()
    {
        return get_json_response([&]() { twit_.accountVerifyCredGet(); });
    }

    picojson::value get_statuses__home_timeline()
    {
        return get_json_response([&]() { twit_.timelineHomeGet(); });
    }

    template <class Callback>
    void get_userstream(Callback cb)
    {
        twit_.usertimeline([cb](const std::string& src) {
            std::cout << "DENUG" << src << std::endl;
            // picojson::value ret;
            // picojson::parse(ret, src);
            // return cb(ret);
        });
    }
};
}  // namespace yellow

bool does_file_exist(const std::string& filename)
{
    std::ifstream ifs(filename);
    return ifs.is_open();
}

picojson::value read_json(const std::string& filename)
{
    std::ifstream ifs(filename);
    assert(ifs);
    std::stringstream ss;
    ss << ifs.rdbuf();
    picojson::value ret;
    YELLOW_ASSERT(picojson::parse(ret, ss.str()).empty());
    return ret;
}

int main(int argc, char** argv)
{
    yellow::TwitterClient client;

    static const std::string cache_filename = "cache.json";
    if (does_file_exist(cache_filename)) {
        std::cout << "read cache" << std::endl;
        auto json = read_json(cache_filename).get<picojson::object>();
        client.set_token_key(json["token_key"].get<std::string>());
        client.set_token_secret(json["token_secret"].get<std::string>());
    }
    else {
        std::cout << "pin" << std::endl;
        client.set_token_key_and_secret_by_pin([](const std::string& url) {
            std::string pin;
            std::cout << url << std::endl;
            std::cin >> pin;
            return pin;
        });

        std::ofstream ofs(cache_filename);
        YELLOW_ASSERT(ofs);
        ofs << "{\"token_key\":\"" << client.get_token_key()
            << "\", \"token_secret\":\"" << client.get_token_secret() << "\"}"
            << std::endl;
    }

    std::cout << "account credentials verification" << std::endl
              << client.get_account__verify_credentials() << std::endl;
    std::cout << "account credentials verification" << std::endl
              << client.get_account__verify_credentials() << std::endl;

    client.get_userstream([](const picojson::value& json) {
        // std::cout << "DEBNG" << json << std::endl;
    });

    return 0;
}
