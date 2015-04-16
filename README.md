# るびま50号 "mruby 用デバッガ「nomitory」の作り方" 補足記事

rubima.c …… mrbgems "mruby-gdb" https://github.com/yamanekko/mruby-gdb を使用した例 (使い方はあとで書く

rubima2.c …… デバッガ拡張用の関数も.c に含めたもの(mrbgemsは不要）※記事内で使用しているのはこちら！

## 注意事項
.gdbinit に 「set print elements 0」を追加してください

/mruby/bin/mrbc を使用して以下のコマンドを実行して rubycode.h を作成してください。

```shell
$ mrbc -g -Bcode -orubycode.h /サンプルを置いたパス/rubima2/rubycode.rb
```
