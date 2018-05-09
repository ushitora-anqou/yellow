# Yellow

ぼくのかんがえたさいきょうのTwitter Client.

## 特長

- [某大学の某計算機科学実験及び演習](https://www.db.soc.i.kyoto-u.ac.jp/lec/le1/index.php)で使うPCで動くまともなTwitter Clientになる予定。
- C++17で書く予定。

## 名前の由来

[Yellow](http://www.nicovideo.jp/watch/1278053029)

## 使い方

- `cd vendor/twitcurl/libtwitcurl && make install`
- `make`
- `./yellow`

## ライセンス

このプロジェクトで書かれた部分はMIT Licenseで浄化されています。
それ以外（`vendor`以下）のソースコードは各々のライセンスに依存します。

- `picojson` は[PicoJSON](https://github.com/kazuho/picojson)から引っ張ってきました。[2条項BSDライセンス](https://github.com/kazuho/picojson/blob/master/LICENSE) です。
- `liboauthcpp` は[liboauthcpp](https://github.com/sirikata/liboauthcpp)から引っ張ってきました。[MITライセンス](https://github.com/sirikata/liboauthcpp/blob/master/LICENSE) です。

## 構想

あっんなこっといーいな、でっきたっらいーいなー

- 鮟鱇は主に水曜日の10:30〜16:00ぐらいに開発する予定です。
~~- 今はtwitcurlを使っていますが、近い内にliboauthcppに移行します。~~
- GDK付近のものを使ってGUIを作りたい。GDK何も知らんけど。
    - main loopはGUIのほうでもって、Twitterから情報を引っ張ってくるのは別threadで回す。
- Mastodon対応もしたい。
- とりあえずmikutterっぽいなにかを目指す。
    - はりぼてmikutter。
- TwitterはUserStreamの廃止を延期したので、とりあえず使っておく。
    - 廃止されてから書き換えればいいさ。
- MIT License
- 最終的にバイナリ一つで動くようにしたい。
    - 実験室PCに何も入れなくても、とりあえずバイナリをUSBメモリ的なにかで突っ込めば動くようにしたい。
- UIにPane/Tab を実装する。
    - できればユーザーが自由に移動できるようにする。
    - 縦横にユーザーが自由に分断できて、かつ好きなものを流せるTwitter clientは存在しない（鮟鱇調べ）。
    - イメージはVSCode。
- Pluginを書けるようにしたい。
    - PythonかLuaあたりを組み込めば良い気がしている。
- C++17準拠。でもC++何もわからない。
    - （多分無いけど）開発している間にC++のアップデートが来たら移行します。
        - つまりそれは2020年まで開発が続いてるっていうことなんやが……。
- Windows対応は二の次。Mac対応は（実機がないので鮟鱇には）できない。
- とりあえず2018年度前期中に作ることを目標にしたい。
    - 途中で失踪したらごめんね。
- 適当にPull requestを送って頂ければ取り込みます。
- 何か協力したい方はTwitter付近でお声がけ下さい。
    - 今いち鮟鱇も何をしてもらえば良いのか分かっていない節があるけど。
    - 「こんな感じで実装するから取り込んで」とかを歓迎します。
- 何もコードが書けなくても、とりあえず動かそうと思ったら動かなかったーなども歓迎します。
    - その場合はIssueをぶったててください。
    - ソフトウェアをただ動かすだけの人って、自分で感じるよりもかなり貴重だったりするのだ。

