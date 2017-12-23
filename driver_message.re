={driver_message} binderドライバによるメッセージの送受信 - servicemanagerとサービス

//lead{

前節では、binderドライバを扱う基本的なシステムコールとなる、open, mmap, ioctlについて簡単に見ました。
このうち、ioctlについては実際に何を送受信するのか、という所の話はしていません。
本節ではこのioctlでやり取りするメッセージの内容について見ていきます。

メッセージを見ていく時には、そのメッセージのやり取りの相手、という物が登場します。
この相手がサービスです。

そこで本節ではメッセージの内容とサービスの呼び出しについて見ていきます。
その過程で重要で特別なサービスである、servicemanagerについても扱います。
//}

== ドライバに書きこむデータの入れ子構造

ドライバに書きこむデータはbinder_write_read構造体のデータだ、と言いました。
この構造体の中にさらにBC_TRANSACTIONの時にはbinder_transaction_data型のデータが入り、
その中にさらにflat_binder_objectが入ります。
このようにデータは入れ子になっていて、しかも構造は全て似ています。
そのデータの種類があって、さらにバッファのポインタとその長さ、というのがパターンです。

通信にまつわる説明ではありがちな構造ですが、
この手の説明を読むのに慣れてないと、毎回同じような事を言っている別の型が乱立していて、
何の話をしているのか良く分からなくなる、という問題が発生します。

そこで個々のデータ型の説明をする前に、まずは全体の入れ子構造をここでしめしておきたいと思います。
binder_write_read、binder_transaction_data、flat_binder_objectは、以下のような関係にあります。

//image[4_1_1][binder_write_read、binder_transaction_data、flat_binder_objectの包含関係]

以下の節でそれぞれの関係が良く分からなくなってきた時には、この図に戻ってきてみてください。


== ドライバに書き込むデータのフォーマットとコマンドID

ioctlを使って読み書きするデータはbinder_write_read構造体だと言いました。(@<hd>{systemcall|binderドライバのioctlと読み書き})
例えば以下のようなコードで初期化していました。

//list[bwrwriteinit][binder_write_readのwrite側初期化、再掲]{
    struct binder_write_read bwr;
    
    // write関連初期化
    bwr.write_size = len;
    bwr.write_consumed = 0;
    // /* 1 */
    bwr.write_buffer = (uintptr_t) data;
//}

ここで/* 1 */でdataという変数のデータを詰めています。
このデータの中身についての話をしていきます。

このデータの先頭は、コマンドIDとなっています。そして各コマンドIDに応じてそのあとに続くデータが決まります。

良く使うコマンドIDには以下の物があります。

 1. BC_TRANSACTION
 2. BC_REPLY
 3. BC_ACQUIRE
 4. BC_RELEASE

一番大切なのが1のBC_TRANSACTIONです。これはよそのプロセスのメソッド呼び出しを行う時に使用するコマンドです。
2は上記のメソッド呼び出しに対する返信のコマンドです。

また、書き込む時と、書き込んだデータを読み込む時でコマンドIDの接頭辞が変わります。
具体的にはBC_TRANSACTIONを書き込むと、サービス側で読みだす時にはBR_TRANSACTIONというIDになりますし、
BC_REPLYはBR_REPLYに、BC_ACQUIREはBR_ACQUIREになります。

BC_TRANSACTIONしてBC_REPLYが返る様子を図にすると、以下のようになります。
これが一つのメソッド呼び出しに対応します。

//image[4_2_1][BCがBRになり、TRANSACTIONにREPLYが返る]

コマンドの下二つに話を戻すと、BC_ACQUIREとBC_RELEASEはハンドルのリファレンスカウントを上げたり下げたりする時に使います。
ハンドルとは、メソッドを呼び出す相手や呼び手を表す物です。サービスのインスタンスのIDと言えます。
ハンドルさえあれば、binderドライバは対応するサービスのポインタを探す事が出来ます。

Binderの特徴の一つとして、最初からC++やオブジェクト指向を前提としている、という物があります。
そこでリファレンスカウントによるオーナーシップやメソッドという物が最初からある程度下のレイヤにも組み込まれています。
具体的に言うとハンドルというインスタンスを指し示すものがあったり、そのリファレンスカウントが、binderドライバのレベルで存在している、という事です。


それでは以下、BC_TRANSACTIONについて詳しく見ていきましょう。

== BC_TRANSACTIONコマンドとbinder_transaction_data

BINDER_WRITE_READでioctlを呼び出す時に書き込むデータのうち、先頭がBC_TRANSACTIONコマンドIDの場合、
#@# TODO: 「BINDER_WRITE_READで」がどこにかかっているか、少し難しいので直す
送るデータの中身は、コマンドIDの後にbinder_transaction_dataが続く事になっています。長さはsizoef(struct binder_transaction_data)です。

binder_transaction_dataはメンバの多い構造体で全部説明するのは大変ですが主要な物だけど抜き出すと以下になります。

//table[btr_field][binder_transaction_dataのフィールド]{
target.handle	やりとりをする相手を表すハンドル
code	どのメソッドかを表すID
data.ptr.buffer	メソッドの引数のデータ領域をさすポインタ
data_size	メソッドの引数のデータの長さ
//}


//image[4_3_1][binder_transaction_dataとメソッド呼び出し]

メソッドを呼び出す相手、どのメソッドかを表すID、そして引数のデータ（data.ptr.bufferとdata_sizeはセットでデータ）、
という事で、これだけあると相手のメソッドを呼ぶ事が出来るのが分かると思います。

//image[4_3_2][binder_write_readとbinder_transaction_dataの関係]

引数のデータは先頭からベタにバイナリ値が書かれています。
#@# TODO: 上記表との対応を明記
書いた方と読んだ方で対応がとれていれば正しい値が取れる、という形式です。
Parcelというutilityクラスがシリアライズとデシリアライズに使えますが、ただ生のデータを読み書きするだけです。

codeはメソッドを表すIDで、各サービスが勝手に決めます。

さて、あと@<table>{btr_field}「binder_transaction_dataのフィールド」の中で残っているのはtarget.handleだけです。
このtarget.handleさえ分かればメソッドを呼び出す事が出来ます。そこで登場するのがservicemanagerです。

== servicemanagerによるサービスハンドルの取得

#@# TODO: 「サービスハンドルの取得の呼び出し」について、何を呼び出すかあらわにタイトル化出来ないか検討。あと、サブタイトルも欲しい

分散オブジェクトのシステムではオブジェクトを表す何らかの名前から、そのリファレンスを取る所が重要となります。
一般にはそれをネーミングサービスと言いますが、Androidの場合そのネーミングサービスに相当するのがservicemanagerです。

servicemanagerは特別なサービスです。
サービスなので、上記のBC_TRANSACTを送る事によってservicemanagerのメソッドを呼び出す事が出来ます。
ここまでは通常のサービスと変わりません。
servicemanagerが特別なのは、ハンドルが0番である事が最初から決まっている事です。

ですから、全てのクライアントは、最初からservicemanagerのハンドルは知っている事になります。
target.handleに0を代入すれば、それはservicemanagerへのメソッド呼び出しだ、と解釈されます。
こうして、どこからかハンドルを取得しなくても、servicemanagerへだけはBC_TRANSACTを送る事が出来ます。

servicemanagerのメソッドのうち、良く呼び出す物のメソッドIDの一覧を以下に載せます。
#@# TODO: 「よく呼び出す物」何がよく呼び出すか、ご補足可能？
#@# TODO: 「メソッドIDの一覧」は2つだけ？「メソッドIDを以下に挙げます」くらいに？

 1. SVC_MGR_ADD_SERVICE
 2. SVC_MGR_CHECK_SERVICE

1がサービスとして登録するメッセージです。詳細は後述します。

2のSVC_MGR_CHECK_SERVICEで、サービスを名前で検索出来ます。
SVC_MGR_GET_SERVICEというメッセージもあって同じ処理をしていますが、
SVC_MGR_CHECK_SERVICEを使っているようです。

SVC_MGR_CHECK_SERVICEの引数としては、
"android.os.IServiceManager"という文字列と検索したいサービス名の二つです。

例えばSurfaceFlingerサービスのハンドルを検索したい場合のbinder_transaction_dataの作り方は以下のようになります。
(重要な所だけ抜き出しています）
簡単のためParcelというシリアライザを使いますが、特に説明しなくともコードから何をやってるかは想像出来るでしょう（詳細は@<hd>{threadpool|Parcelとシリアライズ}でも扱います）。

//list[btr_build][Parcelを用いたbinder_transacton_dataの作り方]{
// binder_transaction_dataのうち引数の所のデータを作る。
// 作りたいのはベタのデータを書き込んだバイナリ配列だが、Parcelユーティリティクラスを使って作る。
Parcel writeData;

// servicemanagerのインターフェース名。ハードコードされた文字列
writeData.writeString16(String16("android.os.IServiceManager"));

// 探したいサービスの名前
writeData.writeString16(String16("SurfaceFlinger"));


// writeDataが完成したので、次はbinder_transaction_dataを作る。
// BC_TRANSACTIONで送るデータ。
struct binder_transaction_data tr;

// servicemanagerのハンドルは0にハードコード
tr.target.handle = 0;

// 呼び出すメソッドのID。
tr.code = SVC_MGR_CHECK_SERVICE;

// 引数には上で作ったwriteDataを設定
tr.data_size = writeData.dataSize();
tr.data.ptr.buffer = writeData.data();
//}

このbinder_transaction_data構造体のデータをbinderドライバに送りつけて、
結果を取得すると、SurfaceFlingerサービスのハンドルが得られます。

このように、servicemanagerという特別なサービスはハンドルが0と固定の値になっているので、
クライアントのコードは最初からservicemanagerに対してだけはメソッドを呼び出せます。
そしてそのservicemanagerが全サービスの一覧を持っていて、そのservicemanagerにハンドルの検索を頼む訳です。
#@# TODO: 「servicemanagerが全サービスの一覧を持っていて」について、本書内で参照先あったら追加
#@# TODO: このあたりで、図解

== SVC_MGR_CHECK_SERVICEを例に、ioctl呼び出しを復習する

以上で一通りservicemanagerのメソッド呼び出しの解説を終えたのですが、
復習も兼ねてこのbinder_transaction_dataを実際に送信するまでのコードも見てみましょう。
内容としては@<hd>{systemcall|binderドライバのioctlと読み書き}と同じ内容となります。以下、重要なコードだけを抜粋していきます。

まずはbinder_transaction_dataはBC_TRANSACTIONコマンドで送信するのでした。
BC_TRANSACTIONコマンドの送受信にはbinder_write_read構造体を使い、
これをioctlに渡すのでした。

そこでbinder_write_read構造体を用意します。
まずは送信用のデータから。これはコマンドID BC_TRANSACTIONと、
先ほどのbinder_transaction_dataをバイト列に書き込んだ物になります。

//list[bwr_btr][binder_transaction_dataをbinder_write_readにセットする]{
// 送信用のデータのバッファ
byte writebuf[1024];
*((int*)writebuf) = BC_TRANSACTION
memcpy(&writebuf[4], &tr, sizeof(struct binder_transaction_data));
//}

//image[4_5_1][writebufの中身]

binder_transaction_dataにはユーザー領域のデータへのポインタ、data.ptr.bufferが含まれるのですが、
これはドライバ内でallocateしてコピーしてくれます。

あとはこの送信用のデータをbinder_write_readに設定して、
受信用にはデータを受け取るバッファを設定し、ioctlを呼びます。


//list[ioctlrecv][ioctlで結果を受け取る]{
// 結果受け取りのバッファ
byte readbuf[1024];

struct binder_write_read bwr;

// 送信用データ。長さはコマンドIDのサイズ+binder_transaction_dataのサイズ
bwr.write_size = writebuf;
bwr.write_buffer = sizeof(int)+sizeof(struct binder_transaction_data);

// 受信用バッファ
bwr.read_size = 1024;
bwr.read_buffer = readbuf;

// ioctlでservicemanagerのSVC_MGR_CHECK_SERVICEを呼び出す
res = ioctl(fd, BINDER_WRITE_READ, &bwr);

//}

//image[4_5_2][binder_write_readのwritebufとreadbuf]

最初のwriteDataがbinder_transaction_dataに入り、それがwritebufに入ってbuffer_write_read構造体に入る、
という4段階の入れ子になっているのでややこしいですが、一つ一つの処理はかなり単純です。

このコードを実行すると、binderドライバはこのスレッドを一旦止めて、
servicemanagerのスレッドを起こしてこのbinder_tarnsaction_dataを渡して処理させ、
その結果を受け取ってから元の呼び出しのスレッドを起こしてioctlから返ります。

結果はbwr.read_bufferに入ります。

#@# TODO:このあたりに図解追加

では次にこの結果がどういう物か、典型的な処理を見る事で見ていきましょう。


== サービスハンドルの取得の結果 - メッセージ受信

ioctlを用いたメソッド呼び出しの結果は、binder_write_read構造体のread_bufferに書かれます。
書かれたデータの長さはbwr.read_consumedに入ります。

#@# TODO: 前出の8.1.2項初出のコードと、対応をとる

read_bufferには先頭の4バイトに戻りのコマンドIDが、それ以後にそのコマンドの付随データが入ります。
送信の側と同じですね。

BC_TRANSACTIONの結果が正常に返る場合のコマンドは、BR_REPLYと決まっています。
その後に付随するデータはbinder_transaction_dataで、送信時と同じです。

そしてそのbinder_transaction_dataのdata.ptr.bufferの中にはflat_binder_object構造体というのが入っています。
#@# TODO: flat_binder_objectの項への参照追加
この構造体はサービスのハンドルやサービスのプロセスの場合はサービス自身のポインタ、そしてファイルディスクリプタなどを保持できるオブジェクトです。
#@# TODO:「サービスのハンドル」、サービスハンドル、いずれかで統一
今回のケースでは、この構造体の中にハンドルが入っています。

//image[4_6_1][読み出したバッファの構造]

@<hd>{SVC_MGR_CHECK_SERVICEを例に、ioctl呼び出しを復習する}のコードの続きとしては以下のようなコードでこのハンドルが取れます。

//list[extract_readbuf][読みだしたバッファからbinder_transaction_dataを取り出す]{
// /* 1 */ 先頭4バイトはコマンドID
int cmd = *((int*)readbuf);

// BC_TRANSACTIONのリプライは正常時はBR_REPLY
assert(cmd == BR_REPLY);

// /* 2 */ BR_REPLYの後続データもbinder_transaction_data型
struct binder_transaction_data tr_res;
memcpy(&tr_res, &reabuf[4], sizeof(struct binder_transaction_data));

// /* 3 */ binder_transaction_data型にはflat_binder_objectのデータが入っている
struct flat_binder_object *obj;
obj = (struct flat_binder_object*)tr_res.data.ptr.buffer;

// /* 4 */ handleはflat_binder_objectの中のhandleフィールドに入っている
int handle = obj->handle;
//}


少し細かいコードになりますが、このように

 1. コマンドを取る（今回のケースでは使いませんが）
 2. binder_transaction_dataを取り出す
 3. その中からflat_binder_objectを取り出す
 4. その中のhandleフィールドに目的のサービスハンドルが入っている

という手順になります。なお、/* 3 */でflat_binder_objectという物が登場しましたが、これについては次節で詳細に扱います。

こうして目的のサービスのハンドルを取得したら、以後はこのようにして得たハンドルに対して@<hd>{servicemanagerによるサービスハンドルの取得}で説明したのと同様なコードで、
指定したサービスのメソッドが呼び出せます。

ここで解説したコードはサービスのハンドルの取得時の受信した後のコードですが、
メッセージ受信全般でほとんど同じ構造のコードとなります。

ioctlはbinder_write_read構造体のwrite_sizeを0にして呼び出すと、
呼び出し時点でブロックしてメッセージが来るのを待ちます。

サービスの実装側では、この受信としてのioctl呼び出しでブロックしてメッセージが来るのを待ち、
メッセージがやってくるとこのioctlから帰ってくるのでbinder_write_readのread_bufferから、
先ほど解説したサービスハンドルの取得と同じような手順でコマンドIDを読み出し、
コマンドIDに応じた処理を行います。

#@# TODO: このあたりに図解

さて、ioctlを使うコードとしては以上でだいたいの説明が終わりです。
以下では実装側に目をうつし、ioctlの中で何が起こっているのかをもう少し詳しく見ていきましょう。

