#pragma once
#ifndef YELLOW_MAIN_HPP
#define YELLOW_MAIN_HPP

#include <curl/curl.h>
#include <functional>
#include <memory>
#include "liboauthcpp/liboauthcpp.h"
#include "picojson.h"

namespace yellow {

namespace helper {
picojson::value str2json(const std::string& str)
{
    picojson::value ret;
    picojson::parse(ret, str);
    return ret;
}
}  // namespace helper

class AsyncNetwork {
public:
    using Callback = std::function<void(char*, size_t)>;

private:
    struct Handle {
        Callback callback;
        CURL* curl;
        curl_slist* hlist;
        char error_buffer[CURL_ERROR_SIZE * 2] = {};
    };

    CURLM* curlm_;
    std::vector<std::shared_ptr<Handle>> handles_;

private:
    // make this class uncopyable
    AsyncNetwork(const AsyncNetwork&) = delete;
    AsyncNetwork& operator=(const AsyncNetwork&) = delete;

    // CURLOPT_WRITEFUNCTIONに指定するためのコールバック関数。
    // CURLOPT_WRITEDATAで指定されたstd::functionを取り出して、
    // 受け取った情報を引数に呼ぶ。
    static size_t curl_writefunc(void* ptr, size_t size, size_t nmemb,
                                 void* data)
    {
        auto handle = reinterpret_cast<Handle*>(data);
        size_t realsize = size * nmemb;
        if (realsize == 0) return 0;
        handle->callback(static_cast<char*>(ptr), realsize);
        return realsize;
    }

public:
    AsyncNetwork();
    ~AsyncNetwork();

    void update();

    void https_get(const std::string& url, int timeout,
                   const std::vector<std::string>& headers, Callback cb);
};

// OAuth認証を行うためのクラス。
// 基本的にはliboauthcppのラッパー。
// 以下出てくる ::OAuth::hogehgoe は、liboauthcppのOAuth名前空間を指す。
// 普通に OAuth と書くと yellow::OAuth を指してしまう。
class OAuth {
private:
    std::shared_ptr<::OAuth::Consumer> consumer_;
    std::shared_ptr<::OAuth::Client> client_;
    std::shared_ptr<::OAuth::Token> access_token_;

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

    // すでにある access token から内部パラメータを作成する。
    void construct_from_raw(const std::string& access_token_key,
                            const std::string& access_token_secret)
    {
        access_token_ = std::make_shared<::OAuth::Token>(access_token_key,
                                                         access_token_secret);
        client_ = std::make_shared<::OAuth::Client>(consumer_.get(),
                                                    access_token_.get());
    }

    template <class Callback>
    void get_auth_info(AsyncNetwork& net, Callback cb)
    {
        static const std::string
            request_token_url_base =
                "https://api.twitter.com/oauth/request_token",
            request_token_url_param = "?oauth_callback=oob";

        ::OAuth::Client oauth(consumer_.get());
        std::string request_url =
            request_token_url_base + "?" +
            oauth.getURLQueryString(
                ::OAuth::Http::Get,
                request_token_url_base + request_token_url_param);
        net.https_get(request_url, -1, {}, [cb](char* p, size_t size) {
            const static std::string authorize_url_base =
                "https://api.twitter.com/oauth/authorize";
            auto token = ::OAuth::Token::extract(std::string(p, size));
            cb(token.key(), token.secret(),
               authorize_url_base + "?oauth_token=" + token.key());
        });
    }

    template <class Callback>
    void construct_from_pin(const std::string& key, const std::string& secret,
                            const std::string& pin, AsyncNetwork& net,
                            Callback cb)
    {
        static const std::string access_token_url_base =
            "https://api.twitter.com/oauth/access_token";
        ::OAuth::Token request_token(key, secret, pin);
        ::OAuth::Client oauth =
            ::OAuth::Client(consumer_.get(), &request_token);
        std::string access_url =
            access_token_url_base + "?" +
            oauth.getURLQueryString(::OAuth::Http::Get, access_token_url_base,
                                    "", true);

        net.https_get(access_url, -1, {}, [cb, this](char* p, size_t size) {
            auto token = ::OAuth::Token::extract(
                ::OAuth::ParseKeyValuePairs(std::string(p, size)));
            construct_from_raw(token.key(), token.secret());
            cb();
        });
    }

    std::string create_get_header(const std::string& url)
    {
        return client_->getFormattedHttpHeader(::OAuth::Http::Get, url);
    }
};

class TwitterWorld {
private:
    OAuth oauth_;
    std::vector<std::function<void(picojson::value)>> observers_[1];

public:
    TwitterWorld(OAuth oauth) : oauth_(oauth) {}

    void stream_user_json(AsyncNetwork& net);
    void add_observer(int index, std::function<void(picojson::value)> ob)
    {
        observers_[index].push_back(ob);
    }
};
}  // namespace yellow

#endif
