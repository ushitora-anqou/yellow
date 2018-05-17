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
#include "main.hpp"

namespace yellow {

#define YELLOW_ASSERT(cond) assert(cond);
#define YELLOW_RANGE(con) std::begin(con), std::end(con)

AsyncNetwork::AsyncNetwork() : curlm_(curl_multi_init()) {}
AsyncNetwork::~AsyncNetwork()
{
    while (true) {
        int msgq = 0;
        struct CURLMsg* msg = curl_multi_info_read(curlm_, &msgq);
        if (msg == NULL) break;
        CURL* curl = msg->easy_handle;
        curl_multi_remove_handle(curlm_, curl);
        curl_easy_cleanup(curl);
    }
    curl_multi_cleanup(curlm_);
}

void AsyncNetwork::https_get(const std::string& url, int timeout,
                             const std::vector<std::string>& headers,
                             Callback cb)
{
    auto handle_ptr = std::shared_ptr<Handle>(
        new Handle{.callback = cb, .curl = curl_easy_init(), .hlist = nullptr});
    auto& handle = *handle_ptr;

    curl_easy_setopt(handle.curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle.curl, CURLOPT_HTTPGET, 1);
    curl_easy_setopt(handle.curl, CURLOPT_SSL_VERIFYPEER, 0);
    if (timeout >= 0) curl_easy_setopt(handle.curl, CURLOPT_TIMEOUT, timeout);
    curl_easy_setopt(handle.curl, CURLOPT_WRITEFUNCTION, curl_writefunc);
    curl_easy_setopt(handle.curl, CURLOPT_WRITEDATA, handle_ptr.get());
    curl_easy_setopt(handle.curl, CURLOPT_ERRORBUFFER, handle.error_buffer);

    for (auto&& header : headers)
        handle.hlist = curl_slist_append(handle.hlist, header.c_str());
    curl_easy_setopt(handle.curl, CURLOPT_HTTPHEADER, handle.hlist);

    curl_multi_add_handle(curlm_, handle.curl);
    handles_.push_back(handle_ptr);
}

void AsyncNetwork::update()
{
    int running_handles;
    curl_multi_perform(curlm_, &running_handles);
    if (running_handles < handles_.size()) {
        while (true) {
            int msgq = 0;
            struct CURLMsg* msg = curl_multi_info_read(curlm_, &msgq);
            if (msg == NULL) break;
            if (msg->msg != CURLMSG_DONE) continue;
            CURL* curl = msg->easy_handle;
            curl_multi_remove_handle(curlm_, curl);
            curl_easy_cleanup(curl);

            auto it = std::find_if(
                YELLOW_RANGE(handles_),
                [curl](auto&& handle) { return handle->curl == curl; });
            YELLOW_ASSERT(it != handles_.end());
            curl_slist_free_all((*it)->hlist);
            handles_.erase(it);
        }
    }
}

// もう少しで封印されるuser.jsonを引っ張ってくるプロセスを開始するための関数。
// とりあえず8月16日までは動くらしい。ジャック☆ドーシーの気が変わらなければ。
void TwitterWorld::stream_user_json(AsyncNetwork& net)
{
    const static std::string url =
        "https://userstream.twitter.com/1.1/user.json";
    net.https_get(
        url, 0, {oauth_.create_get_header(url)},
        [& obs = observers_[0], prev = std::string()](char* s,
                                                      size_t size) mutable {
            // \r\nで入力を区切り、funcに渡す。
            // 入力は分断されて飛んでくるかも知れない。そのため、\r\nで終端していない文字列が
            // 飛んできた場合は、次回のためにprevに保存しておく。
            // あきらかに非効率な実装だが、とりあえずこれで。
            // FIXME:
            // 文字列のコピーが起こらず、もう少しreadableなコンテナを作る。
            auto input = prev + std::string(s, size);
            std::string::size_type i = 0, p = 0;
            while (i < input.size() &&
                   (i = input.find_first_of("\n", i)) != std::string::npos) {
                i++;
                auto length = i - p - 2;
                if (length > 0) {
                    auto json = helper::str2json(input.substr(p, length));
                    for (auto&& ob : obs) ob(json);
                }
                p = i;
            }
            prev = input.substr(p, input.size() - p);
        });
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
    yellow::AsyncNetwork net;
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
        // OAuth認証を行う。Callback機構を使用してネットワーク越しに通信を行う。
        // oauth変数のスコープに注意。これがデストラクトされると、
        // Callback内で存在しないoauth変数を扱うことになる。
        oauth.get_auth_info(net, [&](const std::string& key,
                                     const std::string& secret,
                                     const std::string& url) {
            std::cout << url << std::endl;
            std::string pin;
            std::cin >> pin;
            oauth.construct_from_pin(key, secret, pin, net, [&] {
                // 得られたaccess tokenを保存しておく。
                // 次回の起動時にはこれが使用される。
                std::ofstream ofs(cache_filepath);
                YELLOW_ASSERT(ofs);
                ofs << "{\"access_token_key\":\""
                    << oauth.get_access_token_key()
                    << "\", \"access_token_secret\":\""
                    << oauth.get_access_token_secret() << "\"}" << std::endl;
            });
        });
    }

    std::shared_ptr<yellow::TwitterWorld> twitter;
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        net.update();

        if (!twitter && oauth.has_constructed()) {
            twitter = std::make_shared<yellow::TwitterWorld>(oauth);
            twitter->stream_user_json(net);
            twitter->add_observer(0, [](picojson::value json) {
                json.serialize(std::ostream_iterator<char>(std::cout), true);
            });
        }
    }
    return 0;
}
