#include <cassert>
#include <iostream>
#include <sstream>

#include "picojson.h"
#include "twitcurl.h"

namespace yellow {

#define YELLOW_ASSERT(cond) assert(cond);
#define YELLOW_RANGE(con) std::begin(con), std::end(con)

namespace twitter {
}
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
    YELLOW_ASSERT(argc == 3);
    std::string username = argv[0], password = argv[1];
    twitCurl twit;
    twit.setTwitterUsername(username);
    twit.setTwitterPassword(password);
    twit.getOAuth().setConsumerKey("gvvptGxzifQsssUppNBTxC25m");
    twit.getOAuth().setConsumerSecret(
        "g5wny2GLzP5V8Naza9kFNdGfhuDMmceTJyMhDwyzUTF0y9KGlT");

    static const std::string cache_filename = "cache.json";
    if (does_file_exist(cache_filename)) {
        std::cout << "read cache" << std::endl;
        [&twit]() {
            auto json = read_json(cache_filename).get<picojson::object>();
            twit.getOAuth().setOAuthTokenKey(
                json["token_key"].get<std::string>());
            twit.getOAuth().setOAuthTokenSecret(
                json["token_secret"].get<std::string>());
        }();
    }
    else {
        std::cout << "pin" << std::endl;
        std::string url, pin;
        twit.oAuthRequestToken(url);
        std::cout << url << std::endl;
        std::cin >> pin;
        twit.getOAuth().setOAuthPin(pin);

        std::cout << "token key / token secret" << std::endl;
        twit.oAuthAccessToken();
        std::string token_key, token_secret;
        twit.getOAuth().getOAuthTokenKey(token_key);
        twit.getOAuth().getOAuthTokenSecret(token_secret);

        std::ofstream ofs(cache_filename);
        YELLOW_ASSERT(ofs);
        ofs << "{\"token_key\":\"" << token_key << "\", \"token_secret\":\""
            << token_secret << "\"}" << std::endl;
    }

    std::cout << "account credentials verification" << std::endl;
    [&twit]() {
        YELLOW_ASSERT(twit.accountVerifyCredGet());
        std::string msg;
        twit.getLastWebResponse(msg);
        picojson::value json;
        picojson::parse(json, msg);
        std::cout << json << std::endl;
    }();

    std::cout << "home timeline" << std::endl;
    [&twit]() {
        YELLOW_ASSERT(twit.timelineHomeGet());
        std::string msg;
        twit.getLastWebResponse(msg);
        picojson::value json;
        picojson::parse(json, msg);
        std::cout << json << std::endl;
    }();

    return 0;
}
