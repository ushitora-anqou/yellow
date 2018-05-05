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
- `./yellow TwitterID Password`

## ライセンス

このプロジェクトで書かれた部分はMIT Licenseで浄化されています。
それ以外（`vendor`以下）のソースコードは各々のライセンスに依存します。

- `picojson` は[PicoJSON](https://github.com/kazuho/picojson)から引っ張ってきました。2条項BSDライセンスです。
- `twitcurl` は[twitcurl](https://github.com/swatkat/twitcurl)から引っ張ってきました。ライセンス不明です。

