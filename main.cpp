#include <cassert>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>

#include <curl/curl.h>
#include "liboauthcpp/liboauthcpp.h"
#include "picojson.h"

#include "config.hpp"

namespace yellow {

#define YELLOW_ASSERT(cond) assert(cond);
#define YELLOW_RANGE(con) std::begin(con), std::end(con)

class CurlHandler {
private:
    std::shared_ptr<CURL> handle_;

public:
    CurlHandler() : handle_(curl_easy_init(), curl_easy_cleanup) {}

    template <class T>
    void setopt(CURLoption option, T&& param)
    {
        curl_easy_setopt(handle_.get(), option, param);
    }

    static size_t perform_callback(void* ptr, size_t size, size_t nmemb,
                                   void* data)
    {
        auto* callback =
            reinterpret_cast<std::function<void(char*, size_t)>*>(data);
        size_t realsize = size * nmemb;
        if (realsize == 0) return 0;
        (*callback)(static_cast<char*>(ptr), realsize);
        return realsize;
    }

    void perform(const std::function<void(char*, size_t)>& callback)
    {
        char error_buffer[CURL_ERROR_SIZE * 2] = {};
        setopt(CURLOPT_WRITEFUNCTION, perform_callback);
        setopt(CURLOPT_WRITEDATA, &callback);
        setopt(CURLOPT_ERRORBUFFER, error_buffer);
        auto res = curl_easy_perform(handle_.get());
        if (res != CURLE_OK) throw std::runtime_error(error_buffer);
    }
};

class OAuth {
private:
    std::shared_ptr<::OAuth::Consumer> consumer_;
    std::shared_ptr<::OAuth::Client> client_;
    std::shared_ptr<::OAuth::Token> access_token_;

private:
    std::string get_https(const std::string& url)
    {
        CurlHandler curl;

        curl.setopt(CURLOPT_URL, url.c_str());
        curl.setopt(CURLOPT_HTTPGET, 1);
        curl.setopt(CURLOPT_SSL_VERIFYPEER, 0);

        std::string ret;
        curl.perform(
            [&ret](char* p, size_t size) { ret = std::string(p, size); });
        return ret;
    }

public:
    OAuth()
        : consumer_(std::make_shared<::OAuth::Consumer>(
              Config::consumer_key(), Config::consumer_secret()))
    {
    }

    const std::string& get_access_token_key() const
    {
        return access_token_->key();
    }
    const std::string& get_access_token_secret() const
    {
        return access_token_->secret();
    }

    ::OAuth::Client& get_client() { return *client_; }

    void construct_from_raw(const std::string& access_token_key,
                            const std::string& access_token_secret)
    {
        access_token_ = std::make_shared<::OAuth::Token>(access_token_key,
                                                         access_token_secret);
        client_ = std::make_shared<::OAuth::Client>(consumer_.get(),
                                                    access_token_.get());
    }

    template <class Callback>
    void construct_from_pin(Callback cb)
    {
        static const std::string
            request_token_url_base =
                "https://api.twitter.com/oauth/request_token",
            request_token_url_param = "?oauth_callback=oob",
            authorize_url_base = "https://api.twitter.com/oauth/authorize",
            access_token_url_base =
                "https://api.twitter.com/oauth/access_token";

        ::OAuth::Client oauth(consumer_.get());
        auto request_token = ::OAuth::Token::extract(
            get_https(request_token_url_base + "?" +
                      oauth.getURLQueryString(
                          ::OAuth::Http::Get,
                          request_token_url_base + request_token_url_param)));
        request_token.setPin(
            cb(authorize_url_base + "?oauth_token=" + request_token.key()));

        oauth = ::OAuth::Client(consumer_.get(), &request_token);
        auto token =
            ::OAuth::Token::extract(::OAuth::ParseKeyValuePairs(get_https(
                access_token_url_base + "?" +
                oauth.getURLQueryString(::OAuth::Http::Get,
                                        access_token_url_base, "", true))));
        construct_from_raw(token.key(), token.secret());
    }

    std::string create_get_header(const std::string& url)
    {
        return client_->getFormattedHttpHeader(::OAuth::Http::Get, url);
    }
};

class Twitter {
private:
    OAuth oauth_;

private:
    void stream_get(const std::string& url,
                    std::function<void(const std::string&)> callback)
    {
        CurlHandler curl;
        curl.setopt(CURLOPT_URL, url.c_str());

        auto oauth_header = oauth_.create_get_header(url);
        std::shared_ptr<curl_slist> headers(
            curl_slist_append(nullptr, oauth_header.c_str()),
            curl_slist_free_all);
        curl.setopt(CURLOPT_HTTPHEADER, headers.get());

        curl.setopt(CURLOPT_HTTPGET, 1);
        curl.setopt(CURLOPT_SSL_VERIFYPEER, 0);
        curl.setopt(CURLOPT_TIMEOUT, 0);

        curl.perform([&callback](char* p, size_t size) {
            return callback(std::string(p, size));
        });
    }

public:
    Twitter(OAuth oauth) : oauth_(oauth) {}

    void stream_user_json(std::function<void(const picojson::value&)> func)
    {
        std::stringstream ss;
        stream_get("https://userstream.twitter.com/1.1/user.json",
                   [&ss](const std::string& src) {
                       std::cout << src << std::endl;
                       // ss << src;
                       // for (std::string line; std::getline(ss, line);) {
                       //    if (ss.eof())
                       //        ss << line;
                       //    else
                       //        func(line);
                       //}
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
    yellow::OAuth oauth;

    const std::string cache_filepath = yellow::Config::cache_file_path();
    if (does_file_exist(cache_filepath)) {
        std::cout << "read cache" << std::endl;
        auto json = read_json(cache_filepath).get<picojson::object>();
        oauth.construct_from_raw(
            json["access_token_key"].get<std::string>(),
            json["access_token_secret"].get<std::string>());
    }
    else {
        std::cout << "pin" << std::endl;
        oauth.construct_from_pin([](const std::string& url) {
            std::string pin;
            std::cout << url << std::endl;
            std::cin >> pin;
            return pin;
        });

        std::ofstream ofs(cache_filepath);
        YELLOW_ASSERT(ofs);
        ofs << "{\"access_token_key\":\"" << oauth.get_access_token_key()
            << "\", \"access_token_secret\":\""
            << oauth.get_access_token_secret() << "\"}" << std::endl;
    }

    yellow::Twitter twitter(oauth);

    twitter.stream_user_json(
        [](const picojson::value& json) { std::cout << json << std::endl; });

    return 0;
}
