={threadpool-layer} スレッドプールのレイヤ - BBinderとIPCThreadState
#@# 旧8.4

//lead{
前節までで解説したbinderドライバさえあれば、サービスの実現に必要な事は全て出来ます。
ですが、これはあまりにも低レベルな為、これだけで分散オブジェクトのシステムを実装すると、
各サービスの開発者皆が同じようなコードを書く事になってしまいます。

そこでioctlを直接呼び出して決まった処理を行う部分をスレッドプールで行うレイヤがあります。
このレイヤはデータをシリアライズ-デシリアライズするParcel、最終的に処理を受け取るBBinder、
BBinderに対応するハンドルを保持してメッセージを送信するBpBinder、としてスレッドプールであるIPCThreadStateとProcessStateで構成されています。

//image[6_1][スレッドプールのレイヤの位置づけ]

また、ネイティブで実装されたシステムサービスは、それをを実行するプロセスのmain関数が多くの場合このレイヤのクラスを走らせて、
自身のサービスをservicemanagerに登録するだけ、というコードになっています。
そこでこの節でシステムサービスの典型的なmain関数についても見ていきます。


なお、システムサービスはSDKのレイヤでは提供出来ません。システムイメージに含める必要があります。
このレイヤを直接使うのはカスタムROM開発者やメーカーの人など、かなり限られた人しか作る事は出来ないと思います。

独自のハードウェアをサービスとして提供したいメーカーの方などは本節の内容はとても貴重な資料となると自負していますが、
そうで無い人でも、スレッドモデルの周辺を理解しているとカーネルのレベルで何が起こるのかを正確にイメージ出来るようになるので、
私はAndroidを深く理解するなら必須の内容と思っています。
//}


== スレッドプールのレイヤの構成要素

スレッドプールのレイヤを構成する中心となるクラスはIPCThreadStateです。
このクラスは各スレッドごとに一つインスタンスが出来るようなスレッドローカルなシングルトンで、IPCThreadState::self()と呼ぶと、
TLSにインスタンスが無ければ生成されます。（TLSについては3.2.2のコラム参照） zzz 参照の仕方相談

ioctlでデータを送受信するには、バイト配列に値をシリアライズしたり、バイト配列から値をデシリアライズする必要があります。
またflat_binder_objectのオフセットを指定したりする必要もあります。
そういったioctlに渡す引数処理を行うためのユーティリティがParcelです。
前節でmemcpyで行っていたような処理を代わりに行ってくれます。

IPCThreadStateはParcelを二つメンバに持ちます。mInとmOutです。
mOutに書いておいたものが、binder_write_readのwrite_bufferの方に、mInはread_bufferの方に使われます。
binder_write_readについては@<hd>{systemcall|binderドライバのioctlと読み書き}などで扱いました。

IPCThreadStateのjoinThreadPool()というメソッドが、ioctlを呼び出して、結果を処理する、というループを回します。
この時に上記のmInとmOutを設定したbinder_write_readを引数とします。

IPCThreadStateは、servicemanagerに登録するオブジェクトはBBinderである、
という前提を設ける事で、ioctlの結果の処理のうち、多くの共通部分を処理してくれます。
これがサービス実装の基底クラスとなります。

そしてサービスの実装がBBinderであるなら、サービスのプロキシに対応するクラスもあります。
それがBpBinderです。
このBpBinderはIPCThreadStateを用いて、ターゲットとなるハンドルに対してioctl呼び出しを行います。
また、共通の基底クラスとしてIBinderが存在します。IBinderの役割はこの時点で述べるのは難しいので、zzzで説明します。

最後に大した事はしないクラスですが良く登場するものにProcessStateという物があります。
これは一プロセス一インスタンスなシングルトンオブジェクトで、IPCThreadStateを立ち上げたりといった処理を行うユーティリティクラスです。

IPCThreadState, Parcel, IBinderとBBinderとBpBinder、そしておまけのProcessStateが、スレッドプールのレイヤを構成しているクラスです。


== Parcelとシリアライズ

Parcelはシリアライズやデシリアライズの機能を備えたバッファです。
つまり内部にバイト配列を持っていて、このバイト配列に値をコピーしたり、
このバイト配列から値を復元したりします。@<hd>{driver_message|servicemanagerによるサービスハンドルの取得}でちょっと登場しました。

大した事をするクラスでは無いのですが、今後良く登場するのでここで少し詳細に見ておきます。
まず、Stirng16("android.os.IServiceManager")をバッファにコピーする事を考えます。
バイト配列にコピーするなら以下のようなコードとなります。

//list[str_barr_copy][バイト配列に文字列をコピーする場合]{
String16 s = String16("android.os.IServiceManager");

byte buf[1024];
memcpy(buf, s.string(), s.size());
//}

これをパーセルに書く場合は以下のようになります。
//list[strcopy_parcel][Parcelに文字列をコピーする場合]{
String16 s = String16("android.os.IServiceManager");

Parcel buf;
buf.writeString16(s);
//}

writeString16の他にwriteInt32や、バイト配列を書きこむwriteなどもあります。
書きこんだ配列を取得するのはdata()メソッドです。

//list[ptr_parcel][Parcelからバッファのポインタを取得]{
byte *ptr = buf.data();
int size = buf.dataSize();
//}

また、Parcelにはflat_binder_objectのサポートがあります。
writeStringBinderというメソッドにBBinderのポインタを渡すと、
内部でflat_binder_objectにラップして書きこみ、書き込んだ場所を覚えておいてくれて、
ipcObjects()というメソッドでオフセットの配列を取得出来ます。

@<hd>{flat_binderobj|オブジェクトの送信とflat_binder_object その1 - ユーザープロセス側}で見たサービスの登録の時に用意するバッファと同じバッファは、以下のように用意出来ます。

//list[svrreg_parcel_ver][サービス登録の三つの引数の書き込み]{
Parcel buf;

// 第一、第二引数の文字列を書く
buf.writeString16(String16("android.os.IServiceManager"));
buf.writeString16(String16("com.example.MyService");

// MyServiceのインスタンスを生成
MyService *service = new MyService;

// 第三引数のflat_binder_objectを生成して書きこみ
buf.writeStrongBinder(service);
//}

Parcelにwriteするメソッドを呼び出していくだけで、簡単です。
こうして出来たバッファをbinder_transaction_dataにセットするのでした。

//image[driver_message/4_1_1][binder_write_read、binder_transaction_data、flat_binder_objectの包含関係、再掲]

そのコードは以下のようになります。

//list[br_parcel_ver][引数の書きこまれたバッファをbinder_transaction_dataに設定]{
struct binder_transaction_data tr;

// servicemanagerのハンドルは0にハードコード
tr.target.handle = 0;

// 呼び出すメソッドのID。今回はサービスの登録なのでADD_SERVICE。
tr.code = SVC_MGR_ADD_SERVICE;

// 引数には上で作ったwritedataを設定
tr.data_size = buf.dataSize();
tr.data.ptr.buffer = buf.data();

// /* 1 */ offsets配列と配列の長さの設定。
tr.data.ptr.offsets = buf.ipcObjects();
tr.data.ofsset_size = buf.ipcObjectsCount();
//}

@<hd>{flat_binderobj|オブジェクトの送信とflat_binder_object その1 - ユーザープロセス側}のコードに比べると、バイト配列の扱いをほとんど気にする必要が無くなっているのが分かると思います。
また、/* 1 */にあるようにoffsetsの配列を作ってくれる所がBinder専用のシリアライザっぽいですね。

最後になりましたが、Parcelはbinder_transaction_dataのバッファを作る時にも、binder_write_readのバッファを作る時にも使えます。

== IPCThreadState概要

IPCThreadStateは各スレッドごとに一つインスタンスが出来るような単位のオブジェクトです。
そしてそのスレッドで、「ioctlを呼んで受信した結果を処理する」という処理をループするオブジェクトです。
GUIのメッセージループに似ていますね。ioctl周辺の処理を全て受け持つクラス、と言えます。

IPCThreadStateはParcel型のmInとmOutというオブジェクトを保持します。
そしてスレッドプールのループで、mOutに書かれている事をwrite_bufferにセットし、そしてread_bufferにmInをセットしてioctlを呼び出します。
つまりmOutを送信し、結果をmInで受け取る訳です。

このようなループが走る事で、コードの他の部分ではmOutに送りたい物を詰めておけばやがて勝手に送信される、という風に出来ます。

IPCThreadStateは名前の通り、スレッドローカルなオブジェクトです。さらにスレッドローカルなシングルトンオブジェクトでもあります。
IPCThreadState::self()と呼び出すと、同一スレッド内ならどこでも同じインスタンスが返ります。<fn>初回呼び出しでインスタンスが作られて、7章で紹介したTLS(スレッドローカルストレージ)にインスタンスが入ります。</fn>

だから次のioctlループで何か送り出してほしい、と思う事があったら、
IPCThreadStateとは全然関係無いクラスの中でも、IPCThreadState()::self()と現在のスレッドのIPCThreadStateインスタンスを取り出して、
そのインスタンスにリクエストなどを依頼する事が出来ます。

具体例は@<hd>{BpBinderの実装 - transact()メソッドとIPCThreadState}のBpBinderで登場します。

IPCThreadStateは、メッセージの受信対象がBBinderのサブクラスである、という前提で処理を行います。
ここで少しメッセージの受信対象について補足しておきます。
ioctlのメッセージ送受信には、必ず送る相手、受け取る相手がいます。
この「相手」はハンドルで指定します。ハンドルが存在するためには、どこかしらでflat_binder_objectかtargetにポインタを入れてbinderドライバに渡してある必要があります。(@<hd>{flat_binderobj|オブジェクトの送信とflat_binder_object その2 - binderドライバと受信側}参照)
通常のケースではハンドルはservicemanagerから取得する物ですが、
幾つかのケースでは引数に渡されたオブジェクトがハンドルとして相手側に渡る事もあります。<fn>7.2.3のApplicationThreadなどがこのケースです。</fn>

どちらにせよ、何かしらの手段でbinderドライバに渡したポインタだけがメッセージを受け取る事になります。
このプロセスにメッセージがやってきた、という事は、このプロセスのそれ以前の場所でポインタをbinderドライバに渡していて、
そのポインタに対してメッセージがやってきている訳です。

この、現在のプロセスで以前にbinderドライバに渡したポインタ、それがBBinderのサブクラスでなくてはいけない、とIPCThreadStateは要求している訳です。
実際Androidでbinderドライバに渡される全ポインタがBBinderのサブクラスとなっています。

話を戻します。
ioctl呼び出しのループを実際に行うメソッドがjoinThreadPool()です。
次項でこのIPCThreadStateの本体とも言える、joinThreadPool()メソッドを見ていきます。


== IPCThreadStateのioctl()呼び出しループ - joinThreadPool()メソッドとBBinder

IPCThreadStateのjoinThreadPool()は、ioctlを呼び出して結果を処理する、というループを行うメソッドです。
メソッドの名前は、このメソッドを呼ぶと以後このスレッドはスレッドプールの一員としてループを処理し続けます、というような意味合いでしょう。
ループのメソッドでは普通ですが、このメソッドも一度呼び出すと終了メッセージまで戻ってきません。

ioctlで受信したメッセージの先頭はコマンドIDになっていました。(@<hd>{driver_message|ドライバに書き込むデータのフォーマットとコマンドID}参照)

//list[cmdid_retrv][コマンドIDの取り出し]{
// ioctl呼び出し後。結果はmInに入っている

int32_t cmd;
cmd = mIn.readInt32();

//}

コマンドIDで、重要なのは以下の物が挙げられます。

 1. BR_TRANSACTION
 2. BR_ACQUIRE
 3. BR_RELEASE

受信側なのでBRで始まっています。
joinThreadPool()メソッドは、ioctlを呼び出してはこれらのコマンドIDに応じた処理を行うメソッドです。

//image[6_4_1][joinThreadPool()メソッドの処理概要]

先頭のBR_TRANSACTION以外はリファレンスカウントなどの寿命管理関連です。
これらの処理は、サービスの基底クラスとなるBBinderだけで全ての処理が実行出来るので、
joinThreadPool()メソッド内で処理が完結し、BBinderを継承するサービスの実装者は別段何もコードを書かなくても必要な処理が行われます。

さて、一番重要なのは残ったBR_TRANSACTIONコマンドです。
BR_TRANSACTIONはサービスのメソッドの処理を行う所です。これはサービス実装者にしか何をやりたいのかは分かりません。
だからIPCThreadStateは皆に共通と思われる事だけをやってくれて、後はサービスの実装者に実装を任せます。

IPCThreadStateがやってくれる、皆に共通と思われる事は大ざっぱには以下の事となります。

 1. メッセージ受信対象オブジェクトと引数のデータを取り出す
 2. メッセージ受信対象オブジェクトのtransact()メソッドに引数データを渡して呼び出す
 3. 結果をBC_REPLYとして送り返す

ようするにバイト配列からオブジェクトを取り出してメソッドを呼び、またバイト配列にして送り返す、という事をやります。
transact()メソッドにするのに必要な事を全部やってくれる、と思っておくのが良いですね。

もう少し厳密に書くと、以下のような手続きとなります。

 1. binder_write_readのread_bufferから、binder_transaction_dataを取り出す
 2. binder_transaction_dataからメソッドを表すIDを取り出す(codeと呼ぶ)
 3. binder_transaction_dataからメソッドの引数に相当するデータをParcelに詰める(bufferと呼ぶ)
 4. 呼び出し元のuidやpidを取り出してフィールドに保存
 5. tr.target.ptrをBBinder*にキャストする<fn>厳密にはtr.target.cookieだが概念的には同じ</fn>
 6. 5でキャストしたBBinderのtransactを呼び出す。引数は2のメソッドを表すID(code)、3のParcel、あとは結果を入れる空のParcel(replyと呼ぶ)
 7. 6を呼び出した結果のreplyを、BC_REPLYコマンドとして呼び出し元に返信する

それほど複雑な処理でもないのでソースコードを読んでみても良いのですが、
上記の説明とそれ程変わらないコードなのでここには載せません。
興味のある方はIPCThreadState.cppのgetAndExecuteCommand()メソッドの周辺を読んでみてください。

とにかく、IPCThreadStateのjoinThreadPool()メソッドによるループは、
コマンドIDに応じて行える処理は全て行って、
BR_TRANSACTIONの時には必要なデシリアライズや結果の返信は引き受けた上で、
transact()メソッドを呼び出して後はサービス実装者に任せる、という振る舞いをします。

そこでサービスを実装する側としては、BBinderのtransact()を実装すればいい訳です。
そしてそのメソッドの処理の結果はreplyという引数のParcelに書き込んでやると、
IPCThreadStateは勝手にBC_REPLYとして結果を返信してくれます。

このように、transact()メソッド以外の部分はBBinderの基底クラスとIPCThreadStateで勝手に処理してくれます。
そこで次には、BBinderのtransact()とはどういうメソッドか？という話になります。

== BBinderのtransact()とonTransact()

BBinderはリファレンスカウントに応じた寿命処理と、transact()メソッドを持ったクラスです。<fn>寿命管理周辺は別段難しい事も無いコードとなっているので本書では扱いません。</fn>

IPCThreadStateによるioctlのメッセージループからは、
BBinderのtransact()メソッドが呼ばれる、と言いました。

BBinderのtransact()メソッドはサービスのtransact()の共通の処理を行います。
そしてサービス固有の処理に関しては、BBinder自身のonTransact()を呼びます。
onTransact()はいわゆるテンプレートメソッドのデザインパターンです。

BBinderのonTransact()メソッドの型を見てみましょう。

//list[ontransact_def][onTransact()メソッドの型]{
status_t BBinder::onTransact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
//}

先頭の引数、codeはbinder_transaction_dataのcodeに入っている、メソッドを表すIDです。
これはサービスが独自に決めます。servicemanagerならSVC_MGR_ADD_SERVICEやSVC_MGR_CHECK_SERVICEなどでした(@<hd>{driver_message|servicemanagerによるサービスハンドルの取得}参照)。

二番目の引数、Parcelのdataはメソッドの引数のデータが入ったParcelです。

三番目の引数のreplyは、結果を書き込むParcelです。onTransactの中でメソッドの結果を書き込みます。
これはreturnの値は正常に成功したかどうかのステータスコードを返す、という決まりになっているから、
通常のメソッド呼び出しのようにreturnで結果を返すという手段が使えない為です。

四番目のflagsは呼び出しがoneway、つまり結果を受け取らなくて良い前提で呼び出されたか、
それとも通常の結果が返るメソッド呼び出しかを表します。

onTransactの実装者としては、第一引数のcodeを見てswitchし、
そのcodeに応じた引数をdataから取り出して、結果をreplyにwriteしていけば良い訳です。

典型的なコードとして、int aとint bの結果を足した物を返すMYSERVICE_ADDと、
引いた結果を返すMYSERVICE_SUBを持ったサービス、MyService1を実装すると以下のようになります。


//list[myservice][addとsubを持つMyService1の実装例]{
// BBinderを継承
class MyService1 : public BBinder {
    enum {
        // メソッドID。IBinder::FIRST_CALL_TRANSACTIONより大きい値を使う約束になっている
        MYSERVICE_ADD = IBinder::FIRST_CALL_TRANSACTION,
        MYSERVICE_SUB
    }
...

    // onTransactをオーバーライド
    virtual status_t onTransact(
        uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags = 0) {
        switch(code) {
            case MYSERVICE_ADD: {
                // dataから引数を取り出し。これはサービスごとに決める。
                // 今回は呼び出し側でint32を二つ並べて書き込んでいる、と仮定している。
                int a = data.readInt32();
                int b = data.readInt32();
                
                // a+bを計算してreplyに書く
                reply->writeInt32(a+b);
                return NO_ERROR;
            }
            case MYSERVICE_SUB: {
                // MYSERVICE_ADDと全く同様。
                int a = data.readInt32();
                int b = data.readInt32();

                // a-bを計算してreplyに書く
                reply->writeInt32(a-b);
                return NO_ERROR;
            }
        }
        return BBinder::onTransact(code, data, reply, flags);
    }
};
//}

このように作ったMyService1をnewしてservicemanagerに登録すれば、
クライアントはこのサービスを呼び出す事が出来るようになります。

引数をdataからreadInt32()したりする、というのは少しまだ低レベルな要素が残っていますが、
このMyService1の実装くらいまで来ると、だいぶ通信回りのコードは無くなって、提供する機能に集中出来るコードとなっていませんか？

== サービスの呼び出し側のコードとプロキシの必要性
#@# TODO: 「プロキシクラス」も見出しに登場させたい（サブタイトルでも。目次に出したい）

さて、上記のように作ったサービスを呼び出そう、と思ったとします。
まずサービスのハンドルは@<hd>{driver_message|servicemanagerによるサービスハンドルの取得}で説明した手順で取れます。
より簡単な取得の方法についてはプロキシを扱った後に触れます(@<hd>{IServiceManagerを用いてサービスのハンドルを簡単に取得する}参照)。

=== プロキシを使わないサービス呼び出しの例

さて、このhandleに対してbinder_transaction_dataを用意し、binder_write_readに詰めてioctlをすれば、
メソッド呼び出しが出来るのでした。(@<hd>{driver_message|SVC_MGR_CHECK_SERVICEを例に、ioctl呼び出しを復習する}など参照)
でもそれはとても長いコードになるので、毎回サービスを呼び出す都度やるのは大変です。
例えばエラー処理などを省いても、以下のようなコードになってしまいます。

//list[svccall_raw][プロキシ無しのサービス呼び出し]{
int handle;

// @<hd>{driver_message|servicemanagerによるサービスハンドルの取得}にあるようなコードでMyService1のハンドルを取得したとする。
// コードは省略。

// 3+4を計算させる。
int a = 3;
int b = 4;

Parcel trbuf;
// /* 1 */ 引数a, bの書き込み
trbuf.writeInt32(a);
trbuf.writeInt32(b);


// ハンドルとメソッドIDを指定
struct binder_transaction_data tr;
tr.target.handle = handle;
// /* 2 */ 呼び出したいメソッドのメソッドID
tr.code = MYSERVICE_ADD;

// 引数を設定
tr.data_size = trbuf.dataSize();
tr.data.ptr.buffer = trbuf.data();

// binder_write_readの初期化開始。
// binder_write_readに使う送信用バッファとしてbwrbufを初期化。
Parcel bwrbuf;

bwrbuf.writeInt32(BC_TRANSACTION);
bwrbuf.write(&tr; sizeof(tr));

// binder_write_readの受信に使うバッファ。
Parcel readbuf;
readbuf.setDataCapacity(256);

// binder_write_readに上記送信用、受信用バッファをセット。
struct binder_write_read bwr;

bwr.write_size = bwrbuf.dataSize();
bwr.write_consumed = 0;
bwr.write_buffer = bwrbuf.data();

bwr.read_size = readbuf.dataCapacity();
bwr.read_consumed = 0;
bwr.read_buffer = readbuf.data();

// ioctl呼び出し 
res = ioctl(fd, BINDER_WRITE_READ, &bwr);

// 結果はreadbuf内のバイト配列内に入っているが、長さを教えてやる必要がある。
readbuf.setDataSize(bwr.read_consumed);
readbuf.setDataPosition(0);

int32_t cmd = readbuf.readInt32();
assert(cmd == BR_REPLY);

// binder_write_readからbinder_transaction_dataを取り出し、その中のバイト配列をresultbufにセットする。
binder_transaction_data tr2;
readbuf.read(&tr2, sizeof(tr2));

Parcel resultbuf;
resultbuf.ipcSetDataReference(tr.data.ptr.buffer, tr.data_size, NULL, 0);

// /* 3 */ a+bの結果の取り出し。つまり7が入ってる
int result = resbuf.readInt32();
//}

こんなコードになってしまいます。binderドライバの復習として全体のコードを見てみたい、という時ならいざ知らず、
ただ3+4を計算させる為に毎回こんなコードを書くのは大変ですよね。

そこでサービスの提供者は上記と同じ事をするコードを、サービスの実装と一緒に提供する事になっています。
それがサービスプロキシです。

=== プロキシを使ったサービス呼び出しの例

サービスのプロキシのインターフェースは、直接呼びたいメソッドの形で定義します。
上記のMyService1のプロキシなら、以下のようになります。

//list[proxyint][プロキシのインターフェース]{
class 何かのクラス {
   int add(int a, int b);
};
//}

このインスタンスを作って、このメソッドをただ呼べば良いように作ります。
名前は、普通はサービスの名前の前にBpをつけた物にする約束です。

//list[bpconv][サービスプロキシの名前は、Bpから始めるコンベンション]{
class BpMyService1 {
    int add(int a, int b);
};
//}    

サービスを呼び出す為にはハンドルが必要です。
そこで、コンストラクタでハンドルを渡すように作る事になっています。

//list[proxy_const][サービスプロキシのコンストラクタを追加]{
class BpMyService1 {
public:
    BpMyService1(int32_t handle);
    int add(int a, int b);
};
//}

実装はおいといて、使う側としては、以下のよに使えます。

//list[proxy_usage][サービスプロキシを用いてサービスを呼び出す例]{
int handle;
// 今回もhandleは@<hd>{driver_message|servicemanagerによるサービスハンドルの取得}のようなコードで習得済みとする。

sp<BpMyService1> myservice = new BpMyService1(handle);

// MYSERVICE_ADDを呼び出す
int result = myservice->add(3, 4);
//}

こんな風に使える、BpMyService1をサービスと一緒に提供します。
このByMyService1をプロキシクラス、と呼びます。

ByMyService1を実装するのは、原理的には先ほどのプロキシ無しのコードと同じ事をすれば良い訳です。
binder_transaction_dataとbinder_write_readを適切に初期化してioctlを呼び、結果を返します。

ただ、これも毎回全部書くのは大変です。これでは大変なのがサービスを呼ぶ人からサービスを実装する人に移っただけです。
そこでプロキシを実装するのを支援してくれるクラスが提供されています。これがBpBinderです。

そこで以下では、このBpBinderについて見ていきましょう。

== サービスのプロキシとBpBinderの使い方

#@# TODO: 前項、を参照に置き換える
前項のメソッドの呼び出しの長いコード「プロキシ無しのサービス呼び出し」を見ていくと、呼び出すメソッド特有な処理は以下の三つだけである事に気づきます。

 1. 引数をtrbufに詰める所
 2. メソッドごとのメソッドID
 3. 結果を取り出す所

これ以外の処理は、基本的にはどのサービスのメソッド呼び出しでも同じです。
そこで上記の作業だけ自分でやって、それ以外はBpBinder::transact()を呼ぶ、というのが、BpBinderの使い方です。

例えばBpBinderを使うプロキシの最小限な物は、以下のようになります。

//list[bpbinder_usage][BpBinderを用いたプロキシの実装例]{
class MyService1Proxy {
public:
    MyService1Proxy(int32_t handle) : mRemote(handle) {}
    BpBinder mRemote;    

    int add(int a, int b) {
        Parcel data, reply;
        
        // 1. 引数をdataに詰める
        data.writeInt32(a);
        data.writeInt32(b);
        
        // 2. BpBinderのtransactをメソッドIDをつけて呼び出す
        mRemote.transact(MYSERVICE_ADD, data, &reply);
 
        // 3. 結果の取り出し       
        return reply.readInt32();
    }
};
//}

addの実装はこれだけです。前項のコードに比べるとずっと短くなりましたね。
そしてメソッドに特有の三つの部分だけの実装となっている事が分かります。

このようなプロキシクラスさえ提供されていれば、MyService1を使うのは簡単です。
MyService1サービスを使う人は、handleをコンストラクタで渡して、このプロキシのaddをよびだせば良いのです。

//list[myproxy_usage][MyService1Proxyを使うコード例]{
int handle;
// handleは@<hd>{driver_message|servicemanagerによるサービスハンドルの取得}のコードで取り出してあるとする。

// プロキシクラスのコンストラクタにhandleを渡す
MyService1Proxy myservice(handle);

// プロキシのメソッド呼び出し
int result = myservice.add(3, 4);
//}

サービスの使用もだいぶ簡単になりました。
このように、BpBinderを用いる事で、サービスのプロキシを簡単に実装出来る事が分かりました。



== BpBinderの実装 - transact()メソッドとIPCThreadState

BpBinderはハンドルを渡して初期化し、transact()メソッドを呼んでメッセージを送信する、という話をしました。

実際の実装は、実はBpBinder自身はioctlを呼び出しません。
その代わり、現在のスレッドのTLSに入っているIPCThreadStateに処理を任せます。

エラー処理を省くと以下のようなコードになっています。

//list[delg_transact][BpBinder::transact()メソッド]{
status_t BpBinder::transact(
    uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags)
{
    status_t status = IPCThreadState::self()->transact(
        mHandle, code, data, reply, flags);
    return status;
}
//}

BpBinderはIPCThreadStateの参照を受け取ったりしていないのですが、
IPCThreadStateはスレッドローカルなシングルトンなので、いつでもこのようにIPCThreadState::self()で取得する事が出来ます。

IPCThreadStateのtransactはmOutに引数の処理を書いた後にjoinThreadPool()メソッドとほぼ同じ処理を一回だけ行う、という振る舞いをします。

こうして、プロキシの実装者から見ると、BpBinderはtransactを呼ぶとbinder_write_readやbinder_transaction_dataを自分で設定して呼び出して結果を取り出す、
という事をやってくれます。BpBinderを使えば実際のioctl周辺の処理を自分で書く事無く、全てこのスレッドプールのレイヤが処理してくれます。

こうしてサービスの実装者も使用者も、細かいプロセス間通信のコードを書く事無く、
サービスの実装とプロキシの実装を提供出来るようになりました。

== servicemanagerのプロキシ - IServiceManager

BpBinderとプロキシの説明を終えたので、servicemanagerのプロキシについて見ておく事にします。
servicemanagerのプロキシは最初からAndroidのフレームワークの中に含まれています。

servicemanagerはハンドル0番に固定されているので、プロキシとして最初からメソッド呼び出しが出来ます。
具体的には@<hd>{driver_message|servicemanagerによるサービスハンドルの取得}で行っているコードと同じ事をすれば良いのですが、これをプロキシ化したクラスに、BpServiceManagerというクラスがあります。
通常はその基底クラスであるIServiceManagerを使う事になっています。

ハンドルを取得せずに使う事が出来るので、IServiceManagerのインスタンスはグローバル関数で簡単に取得出来るようになっています。
そのグローバル関数の名は、defaultServiceManager()です。

//list[def_svcmgr][defaultServiceManagerの宣言]{
namespace android {

    sp<IServiceManager> defaultServiceManager();

}
//}

spはStrongPointerの略で、スマートポインタです。寿命管理をしてくれる以外は普通のポインタとして使えます。@<column>{smartptr}


===[column]{smartptr} リファレンスカウントとスマートポインタ - RefBaseとWeak Referenceとsp
リファレンスの寿命管理として、AndroidではRefBaseというユーティリティクラスが提供されています。
これは寿命管理では良く出てくる、Weak Referenceと通常のリファレンスを管理する基底クラスです。
AndroidではWeak Referenceじゃない通常の所有を、Weak Referenceの反対としてStrong Referenceと呼びます。
RefBaseには、incStrong(), decStrong()のようなリファレンスカウントの上げ下げを行うメソッドと、
incWeak(), decWeak()というWeak Referenceのリファレンスカウントを上げ下げするメソッドがあります。

そしてこれらのリファレンスカウントのメソッドを使って寿命管理をするスマートポインタの一つがspです。
spはStrong Pointerの略で、Weakでないリファレンス、つまりカウントが0になるとそのオブジェクトを削除するオーナーシップを表します。
#@# TODO:「spはStrongPointerの略」は、本文でも出てきていたので、どちらかにおまとめたい
RefBase自身はBinderとは関係無く使える汎用のユーティリティクラスですが、
Binder関連のクラスはRefBaseを継承している物がほとんどなので、
Binder関連のコードではこのspは良く出てきます。
===[/column]

こうして取得したIServiceManagerで、良く使うメソッドは以下の二つです。

//list[svcmgr_methods][IServiceManagerのaddService()とcheckService()メソッドの宣言]{
class IServiceManager {
public:
    // サービス登録に使うメソッド
    virtual status_t            addService( const String16& name,
                                            const sp<IBinder>& service,
                                            bool allowIsolated = false) = 0;

    // サービス取得に使うメソッド
    virtual sp<IBinder>         checkService( const String16& name) const = 0;
    
};
//}

addService()とcheckService()の二つのメソッドです。
これらは、@<hd>{driver_message|servicemanagerによるサービスハンドルの取得}で挙げたservicemanagerの二つのメソッドID、つまりSVC_MGR_ADD_SERVICEとSVC_MGR_CHECK_SERVICEに対応したプロキシメソッドです。

checkService()の結果は、ハンドルを返すのではなく、それをBpBinderにラップした物を返します。
型はそのスーパークラスのIBinderを返す事になっています。
何故BpBinderでなくIBinderなのか、については@<hd>{IBinderとは何か？ - SVC_MGR_CHECK_SERVICEでハンドルが返ってこない場合}で扱います。

なお、何回か自動でリトライするgetService()というラッパも存在します。こちらの方が便利なので通常はこちらを使いますが、本質的にはcheckServiceメソッドと同じ事を行います。


== IServiceManagerを用いてサービスのハンドルを簡単に取得する

比較の為、@<hd>{driver_message|servicemanagerによるサービスハンドルの取得}で述べたservicemanager呼び出しでサービスのハンドルを取得するのと同じコードを、
IServiceManagerでも書いてみましょう。

defaultServiceManager()でプロキシを取得し、checkService()かgetService()で取り出せば良い、という事になります。
@<hd>{driver_message|servicemanagerによるサービスハンドルの取得}と同様に"SurfaceFlinger"サービスを取得する場合のコードは以下のようになります。

//list[getsvcr][getService()の使用例]{
sp<IBinder> binder = defaultServiceManager()->getService(String16("SurfaceFlinger"));
int handle = binder->remoteBinder()->handle();
//}

IBinderやBpBinderの詳細はここでは重要では無いので、すぐにハンドルを取り出してしまいます。
普段はBpBinderを取得すれば十分なのでわざわざhandle()まで取得する事はありませんが、
必要であれば上記2行で簡単にhandleも取得出来ます。


== IBinderとは何か？ - SVC_MGR_CHECK_SERVICEでハンドルが返ってこない場合

getService()メソッドの結果はBpBinderでは無くてIBinderといいう基底クラスだと言いました。
いつもBpBinderであれば最初からBpBinderを返せば良いのですが、実はSVC_MGR_CHECK_SERVICEでは、ポインタが返ってくるケースがあります。
それは検索するサービスが自分のプロセスのサービスの場合です。
この場合はBBinderがそのまま返ります。

例を挙げましょう。例えば以下のコードでは、addService()で渡したポインタがそのまま帰ってきます。

//list[checksvcr][addService()したのと同じ場所でcheckService()する例]{
// /* 1 */ 登録するサービスのポインタ
MyService1* service1 = new MyService1();

sp<IServiceManager> svcmgr = defualtServiceManager();
svcmgr->addService(String16("com.example.MyService1"), service1);

// /* 2 */ checkService()で取得したオブジェクト
sp<IBinder> result = svcmgr->checkService("com.example.MyService1");
//}

この場合、/* 1 */のservice1と/* 2 */のresultは同じインスタンスを指します。

これくらい単純なケースだと、いちいちIServiceManagerに問い合わせたりせず、最初からservice1を使えばいいじゃないか、という気がしてしまいますが、
現実にはもっとずっと複雑なケースもありうるのです。

本質的にはこれはcheckService()に限らず、binderドライバにBINDER_TYPE_HANDLEのflat_binder_objectを送る場合にいつでも起こり得ます。
BINDER_TYPE_HANDLEを送信した先が、そのハンドルの元となるポインタの存在するプロセスの場合、BINDER_TYPE_BINDERに変換されて送信されます。

この事をもう少し詳しく見ていきましょう。
プロセスAにサービスのポインタが存在し、プロセスBとやり取りする場合を考えましょう。

プロセスAから生のポインタをプロセスBに送る時は、flat_binder_objectにBINDER_TYPE_BINDERを入れるのでした。(@<hd>{flat_binderobj|オブジェクトの送信とflat_binder_object その1 - ユーザープロセス側}, @<hd>{flat_binderobj|オブジェクトの送信とflat_binder_object その2 - binderドライバと受信側})
すると、プロセスBの側ではBINDER_TYPE_HANDLEとして渡ってきます。

//image[6_10_1][BINDER_TYPE_BINDERを送るとBINDER_TYPE_HANDLEとして出てくる]

このハンドルを今度は逆にプロセスBからプロセスAに送る場合を考えます。
この場合は、逆にBINDER_TYPE_HANDLEが、BINDER_TYPE_BINDERに変換されてプロセスAの側に出てきます。

//image[6_10_2][BINDER_TYPE_HANDLEを送るとBINDER_TYPE_BINDERとして出てくる]

二つのプロセスの場合では自明にも思えるかもしれないので、プロセスCを足してみます。

プロセスAがBに送ります。するとBINDER_TYPE_BINDERがBINDER_TYPE_HANDLEとなります。

//image[6_10_3][BINDER_TYPE_BINDERを送ると前回同様、BINDER_TYPE_HANDLEとして出てくる]

プロセスBがプロセスCにBINDER_TYPE_HANDLEを送ります。
するとプロセスCでもこれはBINDER_TYPE_HANDLEのままです。

//image[6_10_4][BINDER_TYPE_HANDLEを送ると、今回はBINDER_TYPE_HANDLEのまま出てくる]

@<img>{6_10_4}「BINDER_TYPE_HANDLEを送ると、今回はBINDER_TYPE_HANDLEのまま出てくる」と@<img>{6_10_2}「BINDER_TYPE_HANDLEを送るとBINDER_TYPE_BINDERとして出てくる」の違いに注目してください。
どちらもBINDER_TYPE_HANDLEを送信していますが、受け取る側は@<img>{6_10_2}がBINDER_TYPE_BINDERに変換されるのに対し、
@<img>{6_10_4}はBINDER_TYPE_HANDLEのままです。@<fn>{handleval}

//footnote[handleval][なお、ハンドルの値はプロセスごとに異なります。@<img>{flat_binderobj|5_4_5}を参照]

#@# TODO:別の章の図の参照の貼り方。@<img>{flat_binderobj/5_4_5}ではダメだった。  zzz 5章の図「BとCでは別ハンドルの値となる」

さて、ここからプロセスCからプロセスAにこのハンドルを送るとどうなるか？というと、
この場合はこのハンドルの表す生のポインタが所属するプロセスに戻ってきたという事なので、BINDER_TYPE_BINDERとして名前のポインタが返ります。

//image[6_10_5][BINDER_TYPE_HANDLEを送ると、今度はBINDER_TYPE_BINDERとして出てくる]

このように、ハンドルを送信した結果がハンドルなのかBINDER_TYPE_BINDERに変換されるかは、相手のプロセスによります。

そこでクライアントとしてはサービスの参照に生のポインタでもハンドルでもどちらの場合でも共通に扱えるインターフェースを使う事になります。
これがIBinderです。

IBinderはBBinderとBpBinderの共通の基底クラスです。

//image[6_10_6][IBinder、BBinder、BpBinderのクラス図]

IBinderがBBinderなのかBpBinderなのかを問い合わせるために、localBinder()というメソッドとremoteBinder()というメソッドが存在します。

//list[lc_remote][localBinder()とremoteBinder()の型]{
class IBinder : public virtual RefBase
{
public:
    virtual BBinder*        localBinder();
    virtual BpBinder*       remoteBinder();
}
//}

BBinderはlocalBinderでthisを返し、remoteBinderでNULLを返します。
BpBinderはlodalBinderでNULLを返し、remoteBinderでthisを返します。


//list[loc_rem_imp][localBinder()とremoteBinder()の実装それぞれ]{
class BBinder : public IBinder
{
public:
    virtual BBinder*        localBinder() { return this; }
    virtual BpBinder*       remoteBinder() { return NULL; }
};

class BpBinder : public IBinder
{
public:
    virtual BBinder*        localBinder() { return NULL; }
    virtual BpBinder*       remoteBinder() { return this; }
};
//}

これらのメソッドを用いる事で、どちらのインスタンスかをしる事が出来ます。

ISerivceManagerのcheckService()などは、返ってきたflat_binder_objectがBINDER_TYPE_BINDERかBINDER_TYPE_HANDLEかに応じて、
BBinderのポインタを返すかBpBinderのインスタンスを生成して返すかを判断しています。

つまり以下のようなコードになっている訳です。

//list[chksvc_summary][IServiceManagerのcheckService()の概要]{
flat_binder_object *obj;
// objはbinder_transaction_dataから取り出す。詳細省略

sp<IBinder> out;
switch(obj->type) {
    case BINDER_TYPE_BINDER:
        // obj->ptrとobj->cookieは概念的には同じ物が入っている
        out = reinterpret_cast<IBinder*>(obj->cookie);
        return out;
    case BINDER_TYPE_HANDLE:
        out = new BpBinder(obj->handle);
        return out;
}
//}
#@# TODO: obj->ptrの解説追加するか検討

上記コードでobj->ptrとobj->cookieが出てきますが、概念的には同じインスタンスが入っています。
実際にはobj->ptrにはWeak Referenceが、obj->cookieには実体のポインタが入っています。

== ProcessStateとスレッドプール

ここまででIPCThreadStateのjoinThreadPool()メソッドがioctlを呼び出すループの処理を行っている、という話をしてきました。
ですがこのクラスのどこらへんがスレッドプールなのか、という話はしていません。
肝心のスレッドを開始するのがProcessStateクラスとなります。

ProcessStateはシングルトンオブジェクトで、一プロセスにつき一インスタンスが対応し、
プロセス全体のスレッドプールに関する情報を保持します。

ProcessStateを取得するには、ProcessState::self()を呼び出します。
するとまだインスタンスが出来ていなければ作成し、既にインスタンスがあればそのインスタンスを返します。

ProcessStateクラスは、startThreadPool()というメソッドを持ちます。
これが新しいスレッドを開始し、そのスレッドの中でIPCThreadStateのjoinThreadPool()を呼び出します。

//list[st_tp][startThreadPool()の実装]{
void ProcessState::startThreadPool()
{
    // 新しいスレッドを作って実行。スレッドのクラスはPoolThread。
    sp<Thread> t = new PoolThread(isMain);
    t->run();
}

// PoolThreadは、新しいスレッドの中でIPCThreadStateのjoinThreadPoolを呼ぶだけのスレッド。
class PoolThread : public Thread
{
protected:
    virtual bool threadLoop()
    {
        IPCThreadState::self()->joinThreadPool();
        return false;
    }
};
//}

こうして、ProcessStateのstartThreadPool()メソッドを呼び出すと、新しいスレッドが作られて、
そのスレッドの中ではIPCThreadStateのjoinThreadPool()メソッドが呼ばれます。
このjoinThreadPool()はioctlを呼び出して処理するループでした。(@<hd>{IPCThreadStateのioctl()呼び出しループ - joinThreadPool()メソッドとBBinder})

ProcessStateのstartThreadPool()を何回か呼ぶと、呼んだ回数分だけスレッドが作られて、皆がioctlで待ち状態に入ります。
そしてbinderドライバからメッセージがやってきたら処理を行って、またioctlで待ち状態に入る訳です。
これはまさにスレッドプールです。

このように、実装のほとんどはIPCThreadStateのjoinThreadPool()メソッドではありますが、
そのループをスレッドプールとして管理するのがProcessStateです。


== システムサービスのmain関数とProcessState - 独自のシステムサービスを提供する時のコード例

ProcessStateにはスレッドプールの管理の他に、もう一つ役割があります。
それはbinderドライバのopenとmmapです。

サービスを提供するプロセスは、まずbinderドライバをopenしてmmapしなくてはいけないのでした。(@<chapref>{systemcall})
これらopenとmmapの処理は、ProessStateのコンストラクタで行います。
#@# TODO: 図解

//list[procstat_const][ProcessStateのコンストラクタでbinderドライバのopenとmmapが行われる]{

// open_driver()でopen()が行われる。
ProcessState::ProcessState()
    : mDriverFD(open_driver())
...
{
    if (mDriverFD >= 0) {
        // openに成功していたらmmapを行う。
        mVMStart = mmap(0, BINDER_VM_SIZE, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, mDriverFD, 0);
        ...
    }
}
//}

このように、ProcessStateを作成すれば、binderドライバを使うのに必要な初期化は自動的に行われます。

ProcessStateはシングルトンオブジェクトで、最初にstaticメソッドであるProcessState::self()が呼ばれた時にインスタンスが作成されます。
以後のself()メソッド呼び出しはその作成した同じインスタンスが返されます。

こうしてopenしたファイルディスクリプタはどこのスレッドからも参照出来る為、
IPCThreadStateからioctlを呼ぶ時にもProcessStateから取り出して第一引数に渡す事が出来ます。
具体的にはProcessState::self()->mDriverFDで参照出来ます。

このProcessStateのコンストラクタによるbinderドライバのopenとmmap処理も踏まえると、
典型的なService、例えばMyServiceを実装するC++のコードを走らせるmain()のコードは、簡易的に書くと以下のように書けます。

//list[svcmain][サービスを提供するexeの典型的なmain()の内容]{
void main() {
    // (1) ProcessStateのコンストラクタ呼び出し。
    ProcessState* ps = ProcessState::self();

    // (2) 別スレッドを立ち上げてioctlのメッセージループ開始
    ps->startThreadPool();

    // (3) servicemanagerにサービスを登録する、後述
    defaultServiceManager()->addService(String16("MyServiceName"), new MyService);
    
    // (4) このmainのスレッドもioctlメッセージループを回し続ける事でスレッドプールの一スレッドとして振る舞う。
    IPCThreadState::self()->joinThreadPool();    
}
//}

(1) まずは先頭のProcessState::self()の呼び出しで、ProcessStateのコンストラクタが呼ばれます。
ここで/dev/binderのopenとmmapを行います。

(2) startThreadPool()呼び出しで、新しいスレッドを立ち上げて、ここでioctlをメッセージ受信の為に呼び出して、このスレッドはブロックします。

(3) その次にやるべき事は、MyServiceのインスタンスをnewで作り、
そのポインタをバインダドライバ経由でservicemanagerにSVC_MGR_ADD_SERVICEメソッドIDで送る事で、
このサービスをservicemanagerに登録する事でした。

ここでは@<hd>{servicemanagerのプロキシ - IServiceManager}で出てきたIServiceManagerを使っています。
この過程でMyServiceポインタに対応したbinder_nodeが作られ、別のプロセスからはこのbinder_nodeを参照する事でポインタを識別できます。

(4) サービスのポインタをservicemanagerに登録したので、このメインスレッドもやる事は無くなりました。
そこでこのスレッドも有効利用すべく、ioctl呼び出しを行うループを実行し、スレッドプールの一スレッドとして振る舞います。

このコードでは(2)のstartThreadPool()と(4)を合わせて、メッセージループは二つのスレッドとなりました。

これでサービスを提供するプロセスのやるべき事は終わりです。
実際上記のmain関数とほとんど同じ内容のmain関数のプロセスは、Androidのシステムサービスのプロセスでは良く見かけます。

説明の為にあえて分離しましたが、実際は(1)と(2)は1行で書く事が出来るため、
main関数はたったの3行となります。




== サービスの仕組みとシステムの発展 - サービスの実装とプロセスの分離

ここまでで、Binderという仕組みのうち、スレッドプールのレイヤの解説が終わりました。
先に進む前に、この時点で実現されているサービス、という物について、どういう特徴があるかを少し考えてみたいと思います。

Androidではハードウェアや新たなシステムの機能を追加する時は、システムサービス、という形で追加する事を推奨しています。
システムサービスとはBBinderのサブクラスで、servicemanagerに登録して使うものです。

サービス自身は何かのプロセス上で動きますが、一対一の関係では無く、一つのプロセスで複数のサービスを提供する事は可能ですし、
また良く行われる事でもあります。

システムサービスの実装はBBinderのサブクラスとしてonTransactを実装し、
さらにそれに対応したプロキシをBpBinderを用いて実装するだけです。
このコードにはmain関数で作った何かを参照する必要は一切ありません。
main関数でこのサービスへの依存が発生するのは、addService()の引数だけです。(@<hd>{システムサービスのmain関数とProcessState - 独自のシステムサービスを提供する時のコード例}の(3)に対応)。

システムサービスがそれぞれ別のプロセスに分かれている方がロバストで安全なシステムにしやすいですが、
一方でプロセスはメモリやCPUなどのハードウェア資源を消費する物でもあり、
組み込みのシステムであまり多くのプロセスを立ち上げるのは、重くなりすぎて使いものになりません。
また、プロセス間通信もプロセス内のメソッド呼び出しよりもずっと重くなります。
システムサービス相互のやりとりをなるべくプロセス内のメソッド呼び出しで済ますには、同じプロセスに多くのサービスが存在する方が良いという事になります。

===[column] サーバープロセスとサービス
システムサービスを提供しているプロセスはサーバープロセスと呼ばれますが、サーバーという用語はいろいろな場所で使われていてややこしいので、
特に必要が無ければ本書では「サービスを提供しているプロセス」と呼ぶ事にしています。
ですが、このサーバーという呼称を知っていると、ソースを読む時に便利な事もあります。
例えばSystemServerプロセスは、Androidにおいてサービスを提供している重要なプロセスですが、
本コラムで述べたようなサーバーという名前の使われ方を知っていれば、SystemServerという名前を見ただけでサービスを提供しているプロセスである事が推測出来ます。
===[/column]

Androidのシステムサービスは、どれくらいプロセスを分けるのか、という決断を、あまり実装に影響を与えずに行う事が出来る設計となっています。
次の節で扱いますが、システムサービスの仕組みは、呼び出し側はサービスがどこのプロセスに属しているかを意識せずに使えるように作る事が出来ます。
プロセスを分けていっても他のパートのコードには影響を与えません。
#@# TODO: 参照更新

まだ多くの端末でリソースが限られている時代の場合には一つのプロセスにたくさんのサービスを動かす事により、
分散で無い通常のモノリシックなシステムのようにふるまい、少ないリソースでも動くように出来ます。

そして時代が進みハードウェアが発展してきて、メモリやCPU資源が潤沢になっていくに応じて、
重要なサービスを別のプロセスに分けてハードウェアスレッドを割り当てたり、
障害に対してロバストにしていったりできます。
#@# TODO: 8.5.1項に参照飛ばす

実際、Androidはバージョンを重ねるごとにサービスのプロセスを分けていった歴史があり、
その歴史は現在でも進行中です。

===[column] surfaceflingerサービスにみる、システムの発展
surfaceflingerというシステムサービスがあります。詳細は12章で扱います。
このサービスは、初期の頃はその他のシステムサービスと同様、SystemServerというプロセスに存在していました。
初期の頃はほとんどのサービスがSystemServiceプロセス一つで実行されていました。
少なくともAndroid 2.3のGBまではSystemServerプロセスにありました。

ですが、ハードウェアの進歩に伴い、おそらくHoneycombの頃から@<fn>{whydoubt}surfaceflingerは別プロセスに分かれる事になりました。
ハードウェアスレッドの少ない端末ではパフォーマンスの低下がみられましたが、
十分なハードウェアスレッドの存在する機種では、なめらかで引っかからないアニメーションが実現されるようになりました。

//footnote[whydoubt][おそらく、というのは、Android 3系列はソースが公開されなかったのとデバイスも少なかったので、今となっては正確な経緯を知るのはちょと難しくなってしまっている為です。グラフィックス回りは3.0で刷新したと言われているので、おそらく3.0の時点でプロセスも分かれていたと思います。4.0の時点では既に分かれていました。]

このように、ハードウェアの進歩に合わせて柔軟にシステムのプロセス構成を変更していける、
というのは、Androidというシステムの大きな特徴と言えます。
年々劇的な進歩を遂げてきた携帯電話というハードウェアの上で、時代の激変になんとか対応し続けて来られたのも、
Androidの基盤とも言えるシステムサービスの設計の段階で、このようなハードウェアの進歩に応じたシステムの発展がデザインされていたおかげである、
と言えるでしょう。

また、1巻のコラムで触れるStagefrightバグとその結果のMedia Frameworkの改善も、
時代に合わせたプロセス構成の発展のまさに現在行われている例と言えると思います。

===[/column]

また、スレッドプールにどれだけのスレッドを用意するかもサービスの実装とは独立に決定出来ます。
main関数でたくさんstartThreadPool()を呼べばスレッドプールに割り当てられるスレッドは多くなる訳です。
サービスの実装側ではスレッドプールにスレッドが幾つあるかは、一切気にする必要はありません。

このように、サービスの実装は一切いじらずにプロセスやスレッド数といったリソースやシステム構成の設計を行えるのは、
Androidのシステムサービスという仕組みの重要な特徴と言えます。



