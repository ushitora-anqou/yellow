#include <cassert>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <thread>
#include <vector>

#include <curl/curl.h>
#include "liboauthcpp/liboauthcpp.h"
#include "picojson.h"

#include "config.hpp"

namespace yellow {
namespace helper {
picojson::value str2json(const std::string& str)
{
    picojson::value ret;
    picojson::parse(ret, str);
    return ret;
}
}  // namespace helper

#define YELLOW_ASSERT(cond) assert(cond);
#define YELLOW_RANGE(con) std::begin(con), std::end(con)

// libcurlの極薄！1mmラッパー。
class CurlHandler {
private:
    std::shared_ptr<CURL> handle_;

public:
    CurlHandler() : handle_(curl_easy_init(), curl_easy_cleanup) {}

    // curl_easy_setopt(3)のラッパー。
    template <class T>
    void setopt(CURLoption option, T&& param)
    {
        curl_easy_setopt(handle_.get(), option, param);
    }

    // CURLOPT_WRITEFUNCTIONに指定するためのコールバック関数。
    // CURLOPT_WRITEDATAで指定されたstd::functionを取り出して、
    // 受け取った情報を引数に呼ぶ。
    static size_t curl_writefunc(void* ptr, size_t size, size_t nmemb,
                                 void* data)
    {
        auto* callback =
            reinterpret_cast<std::function<void(char*, size_t)>*>(data);
        size_t realsize = size * nmemb;
        if (realsize == 0) return 0;
        (*callback)(static_cast<char*>(ptr), realsize);
        return realsize;
    }

    // curl_easy_perform(3)のラッパー。
    // 勝手にCURLOPT_WRITEFUNCTION, CURLOPT_WRITEDATA, CURLOPT_ERRORBUFFERを
    // ハックして、std::functionのコールバックが適切に呼ばれるようにする。
    // エラーが起こった場合にはエラーを投げる。
    void perform(const std::function<void(char*, size_t)>& callback)
    {
        char error_buffer[CURL_ERROR_SIZE * 2] = {};
        setopt(CURLOPT_WRITEFUNCTION, curl_writefunc);
        setopt(CURLOPT_WRITEDATA, &callback);
        setopt(CURLOPT_ERRORBUFFER, error_buffer);
        auto res = curl_easy_perform(handle_.get());
        if (res != CURLE_OK) throw std::runtime_error(error_buffer);
    }
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

private:
    // HTTPS通信でGETを行うための関数。
    // 要するにurlの先の情報を引っ張ってきて、戻り値で返す。
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

    // すでにある access token から内部パラメータを作成する。
    void construct_from_raw(const std::string& access_token_key,
                            const std::string& access_token_secret)
    {
        access_token_ = std::make_shared<::OAuth::Token>(access_token_key,
                                                         access_token_secret);
        client_ = std::make_shared<::OAuth::Client>(consumer_.get(),
                                                    access_token_.get());
    }

    // PINを使用して access token を作り、内部パラメータとして設定する。
    // 引数のコールバック関数は、PINを得るためのURLを引数として受け取り、
    // ユーザが入力したPINを戻り値として返す。
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

        // request token をまず取得する。
        ::OAuth::Client oauth(consumer_.get());
        auto request_token = ::OAuth::Token::extract(
            get_https(request_token_url_base + "?" +
                      oauth.getURLQueryString(
                          ::OAuth::Http::Get,
                          request_token_url_base + request_token_url_param)));
        request_token.setPin(
            cb(authorize_url_base + "?oauth_token=" + request_token.key()));

        // その次に access token を取得する。
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

// Twitter から情報を引っ張ってくるためのクラス。
class Twitter {
private:
    OAuth oauth_;

private:
    // Streaming API を使用して Twitter に接続するための関数。
    // 受信があると callback に文字列として飛んでくる。
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

    // Userstream を得るための関数。user.json
    void stream_user_json(std::function<void(const picojson::value&)> func)
    {
        std::string prev;
        stream_get(
            "https://userstream.twitter.com/1.1/user.json",
            [&prev, &func](const std::string& src) {
                // \r\nで入力を区切り、funcに渡す。
                // 入力は分断されて飛んでくるかも知れない。そのため、\r\nで終端していない文字列が
                // 飛んできた場合は、次回のためにprevに保存しておく。
                // あきらかに非効率な実装だが、とりあえずこれで。
                // FIXME:
                // 文字列のコピーが起こらず、もう少しreadableなコンテナを作る。
                auto input = prev + src;
                std::string::size_type i = 0, p = 0;
                while (i < input.size() && (i = input.find_first_of("\n", i)) !=
                                               std::string::npos) {
                    i++;
                    auto length = i - p - 2;
                    if (length > 0)
                        func(helper::str2json(input.substr(p, length)));
                    p = i;
                }
                prev = input.substr(p, input.size() - p);
            });
    }
};

struct Event {
    std::string utag, dtag, content;
};
using EventPtr = std::shared_ptr<Event>;

class EventCenter {
private:
    std::shared_timed_mutex mtx_;
    std::vector<EventPtr> events_;

public:
    EventCenter() {}

    void add(EventPtr event);
    std::pair<int, std::vector<EventPtr>> get_since(int since_id);
};

void EventCenter::add(EventPtr event)
{
    std::lock_guard<std::shared_timed_mutex> lock(mtx_);
    events_.push_back(event);
}

std::pair<int, std::vector<EventPtr>> EventCenter::get_since(int since_id)
{
    std::shared_lock<std::shared_timed_mutex> lock(mtx_);

    if (events_.size() <= since_id)
        return std::make_pair(since_id, std::vector<EventPtr>());
    std::vector<EventPtr> ret;
    std::copy(events_.begin() + since_id, events_.end(),
              std::back_inserter(ret));
    return std::make_pair(since_id + ret.size(), ret);
}

}  // namespace yellow

// filename というファイルが存在するかを返す。
bool does_file_exist(const std::string& filename)
{
    std::ifstream ifs(filename);
    return ifs.is_open();
}

// JSON ファイルを読み込む。
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
    yellow::EventCenter ec;

    std::thread curl_thread([&ec] {
        // まずOAuth認証を行う。
        yellow::OAuth oauth;

        const std::string cache_filepath = yellow::Config::cache_file_path();
        if (does_file_exist(cache_filepath)) {
            // cache fileが存在するなら、そこに入っているaccess
            // tokenを使用する。
            std::cout << "read cache" << std::endl;
            auto json = read_json(cache_filepath).get<picojson::object>();
            oauth.construct_from_raw(
                json["access_token_key"].get<std::string>(),
                json["access_token_secret"].get<std::string>());
        }
        else {
            // cache が無ければPINを使用してaccess tokenを取得する。
            std::cout << "pin" << std::endl;
            oauth.construct_from_pin([](const std::string& url) {
                std::string pin;
                std::cout << url << std::endl;
                std::cin >> pin;
                return pin;
            });

            // 取得したaccess tokenは再利用可能なのでcacheとして保存しておく。
            std::ofstream ofs(cache_filepath);
            YELLOW_ASSERT(ofs);
            ofs << "{\"access_token_key\":\"" << oauth.get_access_token_key()
                << "\", \"access_token_secret\":\""
                << oauth.get_access_token_secret() << "\"}" << std::endl;
        }

        // Twitterとの戦いを始める。
        yellow::Twitter twitter(oauth);

        twitter.stream_user_json([&ec](const picojson::value& json) {
            YELLOW_ASSERT(!json.is<picojson::null>());
            ec.add(std::make_shared<yellow::Event>(
                yellow::Event({"twitter", "event", json.serialize(true)})));
        });
    });

    int since_id = 0;
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto res = ec.get_since(since_id);
        if (since_id == res.first) continue;
        since_id = res.first;
        for (auto&& ev : res.second) {
            std::cout << ev->content << std::endl;
        }
    }

    return 0;
}
