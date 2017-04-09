= 8.7 共通ネイティブインターフェースのレイヤ - IInterfaceとinterface_cast
#@# 旧8.5

//lead{
前節までで、IPCThreadStateとProcessStateを利用し、BBinderを継承したクラスを用いる事でサービスを提供できる、という話をしました。
また、その提供されたサービスを利用する為のプロキシクラスを実装するのも、BpBinderを用いる事で簡単に出来る、という話をしました。

この節では、IBinderがBBinderなのかBpBinderなのかを気にせずにクライアントが同じコードを使えるようにするための、
共通ネイティブインターフェースのレイヤについて説明します。<fn>なお、スレッドプールのレイヤも共通ネイティブインターフェースのレイヤも、レイヤの名前は私が勝手につけました。この周辺は公式のドキュメントが無いので、公式の呼び名が存在しません。</fn>
クラスとしてはIInterface、BnInterface、BpInterfaceの三つです。

共通ネイティブインターフェースのレイヤはネイティブ実装の時にだけ使われるレイヤで、JavaでAIDLを使う場合はこのレイヤは使用しません。
AIDLとは同じ階層の、兄弟の関係にあるレイヤと言えます。
また、内容自体もこれまでのレイヤに比べると随分とシンプルで、大したことはしません。

大した事はしてないレイヤなのでそれほど重要でもないのですが、
この説明をJavaより先に持ってきたのにも訳があります。

まず、システムサービスは性質上、ネイティブで実装される物が多くあります。
そしてネイティブの実装は、ほとんどがこの共通ネイティブインターフェースのレイヤを用いて実装されている為、
システムサービスの実装ではこのレイヤしか出てきません。(main関数でProcessStateとIPCThreadStateが出てくるくらいです）。
実装をするにせよソースを読むにせよ、この周辺を理解しておくと、分からない事は無くなります。
逆にBBinderやBpBinderなどは、概念としては重要度は高くともソースコード上での登場回数はむしろ少ないかもしれません。

実際にシステムサービスを実装する立場の人は比較的少数だとは思いますが、
その人にとっては、本節は現存する中ではもっともしっかり書かれた文書だと自負しています。

もう一つ、後のJavaによるサービスの実装は、本節の内容をかなり忠実にJavaにマップしています。
そこで本節の理解はJavaの側の理解の大きな助けとなります。
Javaのサービスは言語境界をまたぐ都合で、本節の共通ネイティブインターフェースのソースに比べて理解するのが困難だと思います。
そこで一段簡単な本節で基本的な所を理解しておいて、次のAIDLの方のコードの準備としたい、という狙いもあります。
//}


== 8.7.1 呼び出しコードが共通である意義 - 共通のインターフェースの必要性

8.6.13でも触れた通り、サービスという仕組みは、
時代と共にホストしているプロセスを分離していく、という事が想定されています。

その為には、呼び出し元のコードが呼び出し先のサービスが別のプロセスに居るか同じプロセスに居るかで、
違いが無いようにしたい。

もともと携帯電話のOSにおいては、サービスごとに別々のプロセスとするのはパフォーマンス的に厳しい物がありました。
だからサービスはなるべく一つのプロセスに入れたい、という思惑がありました。
全てを一つのプロセスに入れて、必要なだけスレッドを割り当てれば、
かつてのガラケーなどのITRONなどにみられるRTOSと似たような構成となります。

一方で計算資源が許せば重要なサービスはプロセスが分かれている方が都合が良い事も多いものです。
プロセスが分かれていれば、特定のサービスだけ優先度を上げたい時に、そのプロセスの優先度を上げれば良くなりますし、
ハードウェアスレッドを特定のサービスには多く割り当てられるようにチューニングしたりも出来ます。
プロセッサが複数ある時にキャッシュを汚しにくいように配置も可能です。
また、セキュリティ的にもプロセスが分かれている方が望ましい事は多くあります。
外部の入力を良く扱う所はバッファオーバーフローなどのセキュリティホールをつく事で、
良く悪意のあるコードを挟みこまれがちです。
そういったサービスはプロセスを分けて低い権限で動かす方が、
破られた時の被害がずっと少なくて済みます。

//image[7_1_1][サービスが同一プロセスにある場合と別々のプロセスにある場合]

携帯電話の計算資源が年々増えていくであろう事は以前から誰でも予想出来ました。
一方でAndroidの初期のバージョンが出た頃のスマホは、バイトコードを動かすだけで精一杯で、
サービスをたくさんのプロセスで構成出来るほどの余裕はありませんでした。

そこで初期のAndroidは、サービスという物を一つのプロセスにたくさん集める、という方針でいきます。
そしてハードウェア資源が潤沢になってきたら、状況に応じてサービスを別のプロセスに分けていこう、
と思ったのだと思います。

サービスは別のサービスを使って実装されている事も多くあります。
むしろ機能ごとに別のサービスに分けて、オブジェクト指向プログラミングに則ったシステム設計にする事を推奨しています。
様々なサービスが連携して目的を達成するのは、望ましい事なのです。

この時、自分のサービスが依存しているサービスが同じプロセスなのか、別のプロセスなのか、という事を区別してコードを書かなくてはいけない、となると、
計算資源が潤沢になってきたらサービスを別のプロセスに分けていく、というのが大変になります。

そこで、共通ネイティブインターフェースのレイヤでは、サービスの使用者が、サービスが自分のプロセスに居るのか、
別のプロセスに居るのかを区別せずにコードを書けるようになっています。

その為の仕組みがIInterface関連のクラスです。
#@# TODO: おおまかな図？

#@# TODO: 8.6.10と重複してる部分を整理統合
#@# 旧8.5.2
== 8.7.2 プロセスが同じか別かで変わる所 - IBinderの役割

サービスを呼び出す側のコードが、サービスが同じプロセスが別のプロセスかに関わらず同じになるようにしたい。
その為には何が必要か、という事を考える為に、呼び出すサービスのプロセスが同じか別かで何が変わるか、という所を見ていきたいと思います。

8.6.10でも触れた通り、binderドライバからサービスを受け取る時に、サービスが同じプロセスだとBINDER_TYPE_BINDERとして生のポインタが、
そして別のプロセスだとBINDER_TYPE_HANDLEとしてハンドルが返ってくるのでした。

ポインタの時にはただのオブジェクトなのだから、キャストして普通に使えば良い訳です。
ハンドルの時にはプロキシオブジェクトを生成して、これを呼ぶのでした。(8.6.6)

具体例を見ましょう。
まずサービスの実装として8.6.4のMyService1を用いて説明します。
サービスの取得は、IServiceManagerのgetService()を使えば良いのでした。


//list[getsvcr][IServiceManagerのgetService()を使ってサービスを取得]{
sp<IBinder> service = defaultServiceManager()->getService(String16("com.example.MyService1"));
//}

このserviceがBBinder由来の物かBpBinder由来の物か、つまりBINDER_TYPE_BINDERかBINDER_TYPE_HANDLEかで処理を分けます。

説明の都合で、先にプロキシから見ていきます。
MyService1のプロキシは8.6.6にありました。（都合によりコンストラクタの引数を変更してます）。

//list[proxycase][MyService1Proxyの実装、再掲]{
class MyService1Proxy {
public:
    MyService1Proxy(BpBinder* remote) : mRemote(remote) {}
    BpBinder* mRemote;    

    int add(int a, int b) {
        Parcel data, reply;
        
        data.writeInt32(a);
        data.writeInt32(b);
        
        mRemote->transact(MYSERVICE_ADD, data, &reply);
 
        return reply.readInt32();
    }
};
//}

serviceがBpBinderならこちらを使えば良い、という事になります。
IBinderにはlocalBinderとremoteBinderというメソッドがあって、このIBinderの実体がBBinderかBpBinderのどちらかが問い合わせ出来ます。(8.6.10)

//list[ibinder][IBinderのlocalBinder()とremoteBinder()]{
class IBinder {
    virtual BBinder*        localBinder();
    virtual BpBinder*       remoteBinder();
};
//}

これを用いると、以下のように書けます。

//list[bpbinder_case][BpBinderの時のプロキシ呼び出し]{
// 引数の例
int a = 3; int b = 4;

// 説明の為、結果を入れる変数を作っておく。
int result;


BpBinder* bp = service->remoteBinder();
if (bp != NULL) {
    sp<MyService1Proxy> proxy = new MyService1Proxy(bp);
    result = proxy->add(a, b);
}
//}

さて、問題はbpがNULLの場合です。その時はこのIBinderはMyService1のポインタでした。

//list[bpnull][IBinderに生のポインタが入っている場合、ポインタの取得まで]{
BpBinder* bp = service->remoteBinder();
if (bp != NULL) {
   // 既に述べたプロキシの処理、省略
} else {
   MyService1* ptr = (MyService1*)service->localBinder();
   // ...
}
//}

さて、オブジェクトは無事得られました。
このオブジェクトをどう呼んだら良いのでしょうか？

そこでMyService1のクラスを見ると、8.6.4に実装がありました。以下のコードです。

//list[myserviceimp][MyService1の実装、再掲]{
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

理論上はParcelを使ってtransactを呼び出せば呼び出す事は出来ます。

//list[transactcall_code][transactを使ってローカルのロジックを呼び出す例]{
Parcel arg, reply;

arg.writeInt32(a);
arg.writeInt32(b);

ptr->transact(MyService1::MYSERVICE_ADD, arg, reply);

result = reply.readInt32();
//}

ですが、生のポインタを持っていて型まで分かるのにこの呼び出し方は少し迂遠です。
効率も良くありません。
何より、これではプロキシ側と呼び出しコードがあまりにも違っていて共通化出来ません。


== 8.7.3 サービスをローカルからも使えるようにする

そこで普通に考えれば、addのロジックをリファクタリングして別メソッドにし、
ローカル呼び出しの場合はこちらを呼び出すようにするのが普通です。

//list[before_refct][変更前コード]{
            case MYSERVICE_ADD: {
                int a = data.readInt32();
                int b = data.readInt32();
                
                // /* 1 */ a+bを計算してreplyに書く
                reply->writeInt32(a+b);

                return NO_ERROR;
            }
//}

//list[after_refct][変更後コード（addの実装は省略）]{
            case MYSERVICE_ADD: {
                int a = data.readInt32();
                int b = data.readInt32();
                
                // /* 2 */ a+bを計算してreplyに書く、add呼び出しに変更
                int result = add(a, b);

                // replyに結果を書く
                reply->writeInt32(result);
                
                return NO_ERROR;
            }
//}

/* 1 */のコードを/* 2 */に変える訳です。こうしてaddというメソッドを作っておいて、これをpublicにしておけば、
MyService1のポインタを持ったら、素直にaddを呼ぶ事が出来ます。

//list[ptrcase][ptrを使ってローカルのロジックを呼び出す例]{
result = ptr->add(a, b);
//}

コードの説明ばかり続くと何をやっているのか分からなくなってくるので、
ここでポインタのケースで何をやっていたのかを見直しましょう。

まず、BBinderは通常、IPCThreadStateのioctl呼び出しループから呼び出されて、
メッセージの処理をしていたのでした。これがonTransactメソッドでした。

一方でこのIPCThreadStateからでは無く直接ポインタを取得出来る場合があるので、
その場合の呼び出しをどうしよう？という話で、その場合用にもpublicメソッドを作っておいて、
onTransactからも直接呼び出しからもそのメソッドを使おう、という話です。

//image[7_3_1][IPCThreadStateから呼ばれるケースと直接呼ばれるケースを両方処理]

プロキシと生ポインタ呼び出しのケースのコードを並べると以下のようになります。

//list[handlebothcase][IBinderがどちらかによって処理を分けるコード]{
BpBinder* bp = service->remoteBinder();
if (bp != NULL) {
    sp<MyService1Proxy> proxy = new MyService1Proxy(bp);
    // / * 1 */ プロキシのadd呼び出し
    result = proxy->add(a, b);
} else {
   MyService1* ptr = (MyService1*)service->localBinder();
   // /* 2 */ 生ポインタのadd呼び出し
   result = ptr->add(a, b);
}
//}

このif文を除去するのは、通常のリファクタリングの話となります。

1と2は完全にインターフェースが同じとなっています。
そこでMyService1ProxyとMyService1に共通のインターフェースがあれば、
生成時だけどちらかを見て、以後はその共通のインターフェースにキャストして使えば良い、という事になります。

ここで全ての実装を書いてもいいのですが、それを理解出来る人はたぶん上記の説明だけでも理解出来るし、
上記の説明だけで理解出来ない人はコードを見ても分からないと思うので、説明だけにしておきます。

呼び出す側のコードを共通化するには、以下の条件が必要そうです。

 1. プロキシも生ポインタも、同一のインターフェースを継承
 2. 生ポインタはonTransactのswitch文からこの継承したインターフェースの呼び出しをするようにしておく
 3. IBinderを取得した時にlocalBinderメソッドなどを用いて、プロキシか生ポインタかを選びインターフェースにキャスト

以上の事は別段ライブラリが無くても行う事が出来ますが、
こうした事は紳士協定にしておくのでは無くて型システムで強制する方が全体としては一貫したシステムになります。

この手の強制の仕方はC++の得意分野ですね。テンプレート引数を使ってこの形で書かないとコンパイル出来ないように設計する訳です。
それが共通ネイティブインターフェースのレイヤの仕事で、具体的にはIInterface、BnInterface、BpInterfaceの三つのクラスがやる事です。

== 8.7.4 IInterface関連の三つのクラス - IInterface, BpInterface, BnInterface
#@# 旧 8.5.3

ここからは呼び出し側のコードをローカルかリモートかによらずに同一にする為のレイヤである、
共通ネイティブインターフェースのレイヤについて説明していきます。

このレイヤはIInterface, BnInterface, BpInterfaceの三つのクラスで構成されています。
先頭のI, Bn, Bpで役割を表していると思われます。Iはインターフェース、BpはBinderのプロキシ、つまりプロキシです。
Bnのnはなんだかわかりませんが、たぶんインスタンスか何かのnだと思います。とにかくサービスの実装を表します。

これまで出てきたIBinder、BBinder、BpBinderと役割分担は似ていますが、レイヤが一つ上となります。

//table[class_relation][三つのクラスの対応関係]{
役割	IBinder群	IInterface群	本節で実装するサンプルのクラス
-----------------------------------------------------------------------
インターフェース	IBinder	IInterface	IMyService1
サービス実装の基底クラス	BBinder	BnInterface	BnMyService1
サービスプロキシのクラス	BpBinder	BpInterface	BpMyService1
//}


IInterface、BnInterface、BpInterfaceは、C++のテンプレートを用いる事で、
サービスの実装を共通インターフェースを通じて実装する事を半強制します。
そうする事でローカルで生ポインタを直接使う場合にプロキシと同じインターフェースになるように型システムのレベルで強要します。

これまで作っていたaddをするだけのMyService1をこのIInterface関連クラスで再実装してみましょう。
このクラス群を用いる人は、IInterface、BnInterface、BpInterfaceをそれぞれ継承したクラスを作ります。
ここではそれぞれIMyService1、BnMyService1、BpMyService1とする事にします。

//image[7_4_1][IInterface、BnInterface、BpInterface関連のクラス図]

これを実装する所でどのようにIInterface関連クラスが共通インターフェースを強制するのかを見ていきます。

== 8.7.5 テンプレートによる継承の強制

BnInterfaceもBpInterfaceもテンプレート引数を取ります。
このテンプレート引数はインターフェースのクラスにする約束となっています。

宣言を見ると以下のようになっています。

//list[bnbpdef][BnInterfaceとBpInterfaceの定義]{
template<typename INTERFACE>
class BnInterface : public INTERFACE, public BBinder
{
public:
    virtual sp<IInterface>      queryLocalInterface(const String16& _descriptor);
    virtual const String16&     getInterfaceDescriptor() const;

protected:
    virtual IBinder*            onAsBinder();
};

template<typename INTERFACE>
class BpInterface : public INTERFACE, public BpRefBase
{
public:
                                BpInterface(const sp<IBinder>& remote);

protected:
    virtual IBinder*            onAsBinder();
};
//}

見ての通り、どちらもINTERFACEは継承に使っているだけで、中で参照している場所はありません。
BnInterfaceとBpInterfaceを使う人に、インターフェースを実装する事を強制するためにこのテンプレート引数は存在しています。

MyService1を実装する時に作るクラスの定義の例を見ると、以下のようになります。

//list[mysvcr1def][MyService1関連クラスの定義]{
// インターフェースのクラスはIInterfaceを継承しないといけない
class IMyService1 : public IInterface {
    ...

    // サービスとして実装するメソッド。サービスの実装者が好きに決める
    virtual int add(int arg1, int arg2) = 0;
};


// BnInterfaceを継承する時はインターフェースのクラスを渡す。そうするとこのクラスを継承した事になる。
class BnMyService1 : public BnInterface<IMyService1> {
...
};

// BpInterfaceを継承する時もインターフェースのクラスを渡す。
class BpMyService1: public BpInterface<IMyService1> {
...
};
//}

このようにテンプレート引数を用意する事で、ここにインターフェースのクラスを渡さないと、このクラス群を使えないようにしています。
意図的にこのデザインを迂回する為にダミーのクラスを渡したりする事も可能ではありますが、
普通に既存実装を参考に自分も実装しよう、と考えれば、自然と共通のインターフェースを両者が継承して実装する事になります。

これはドキュメントなどに書いて紳士協定としておくよりも、ずっと良い強制方法です。

このようなスタイルで実装をしていくと、自然と8.7.3のようなスタイルでコードを書く事になります。
あとは、プロキシを生成するかキャストするかを関数で隠して、以後はIMyService1だけ使えば良くなります。

//list[imysvcrconv][IMyService1へ変換する関数例]{
sp<IMyService1> asMyService1Interface(IBinder service) {
    BpBinder* bp = service->remoteBinder();
    if (bp != NULL) {
        return new BpMyService1(bp);
    } else {
        return (BnMyService1*)service->localBinder();
    }
}
//}

この実装はどのサービスでもクラス名以外は全く一緒なので、マクロやテンプレートで自動生成出来ます。
そこで最初から提供されています。それがIMyService1のasInterfaceメソッドとなります。

そこでIMyService1インターフェースクラスについて見ていきましょう

== 8.7.6 IInterfaceとasInterfaceメソッド - IMyService1クラス

インターフェースのクラスであるIMyService1の実装を見てみます。

このクラスを定義する時には、幾つか決まりがあります。

 1. IInterfaceを継承しないといけない
 2. 宣言にDECLARE_META_INTERFACE(インターフェース名); という行を置かないといけない
 3. 実装にIMPLEMENT_META_INTERFACE(インターフェース名, インターフェースディスクリプタ); という行を置かないといけない

これはそういう決まりになっている、という事で、特に中を理解していなくても問題は無いように設計されています。

例えばヘッダファイルは以下のようになります。

//list[imysvcrusedef][IMyService1のヘッダ例]{
// /* 1 */ IMyService1はIInterfaceを継承しないといけない
class IMyService1 : public IInterface {
    // /* 2 */ そしてヘッダにはDECLARE_META_INTERFACEという行が必要。後述
    DECLARE_META_INTERFACE(MyService1);

    // サービスとして実装するメソッド。サービスの実装者が好きに決める
    virtual int add(int arg1, int arg2);
};
//}

addの所は実装したいメソッドです。好きな名前で複数書く事が出来ます。
サービスもそのプロキシも、このIMyService1を（間接的に）継承する約束になっています。

ヘッダの中で、DECLARE_META_INTERFACEというマクロを置かないといけない決まりとなっています。
引数はサービス名です。このマクロの展開結果はgetInterfaceDescriptor()というメソッドとasInterface()というメソッドの宣言となっています。
#@# TODO: 「getInterfaceDescriptor」「asInterface」について、一言ずつ説明

また、このDECLARE_META_INTERFACEに対応するマクロとしてIMPLEMENT_META_INTERFACEというマクロもあり、
このIMPLEMENT_META_INTERFACEマクロは.cppの方に含めなくてはいけません。interfaceといいつつ.cppが必要なのです。

具体的には以下のような行を含めた.cppファイルをリンクする必要があります。

//list[implmacr][.cppファイル内のIMPLEMENT_META_INTERFACEマクロの例]{
IMPLEMENT_META_INTERFACE(MyService1, "com.example.IMyService1")
//}

このIMPLEMENT_META_INTERFACEマクロは、getInterfaceDescriptor()とasInterface()の実装を吐くマクロです。

ここで第二引数の"com.example.IMyService1"はインターフェースディスクリプタと呼ばれる、このインターフェースを表す文字列です。
このAndroidのマシン内で一意である事が期待されます。

このマクロが生成するasInterface()は、このサービスを表すIBInderを受け取って、IBinderがBINDEDR_TYPE_BINDERに対応する方かBINDER_TYPE_HANDLEに対応するかに応じて、

 1. サービスと同じプロセスならBnMyService1を返す
 2. サービスと異なるプロセスならBpMyService1を返す

というふるまいをするメソッドです。どちらもIMyService1にキャストして返すので、外からは区別がつきません。

IMyService1::asInterface()メソッドは、その内部でBnMyService1とBpMyService1と名前を決め打ちしたクラスを参照します。
そこでこのメソッドから参照出来る場所にこの二つのクラスが必要ですし、別の名前で定義する事は出来ません。
MyService1、というサービスの名前を決めたら、三つのクラスは

 1. IMyService1
 2. BnMyService1
 3. BpMyService1

に固定されてしまい、これに従って実装しないとコンパイルエラーとなります。

IMPLEMENT_META_INTERFACEは、具体的には以下のようなコードに展開されます。（簡略化しています）

//list[extract][IMPLEMENT_META_INTERFACEマクロの展開例]{
// このサービスを表す名前。インターフェースディスクリプタと呼ばれる。
const android::String16 IMyService::descriptor("com.example.IMyService1");

// asInterfaceの実装
android::sp<IMyService> IMyService1::asInterface(
    const android::sp<android::IBinder>& obj)
{
    android::sp<IMyService> intr;
    intr = static_cast<IMyService1*>(
        // objのqueryLocalInterfaceを呼び出す。詳細は後述だが、基本的にはlocalBinder()と同じ。
        obj->queryLocalInterface(
            IMyService1::descriptor).get()
            );
            
    // intrがNULLという事はIBinderのlocalBinder()がNULLだったという事なので、このobjはBpBinder。
    // そこでプロキシオブジェクトを作って返す
    if (intr == NULL) {
        intr = new BpMyService1(obj);
    }
    return intr;
}
//}

asInterfaceはちょっとややこしいコードですが、要約すると以下のようなコードになります。

 1. IBinderがBBinderだったらただIMyService1*にキャストして返す
 2. IBinderがBpBinderだったらBpMyService1でラップして返す

インターフェースディスクリプタというのはioctlで送るコマンドの先頭に書く決まりになっていて、どのサービス実装者も書いている物です。
実際8.4.4でもハードコードした文字列、として書いていました。
servicemanagerはこの文字列をチェックして、無かったらエラーとする為です。

ですが、Androidでこれを有効に使っている例を私は見つけられませんでした。
そこで本書では、インターフェースディスクリプタについて、多くは解説しません。
こういう決まりになっていて、みんな書いていて、みんなチェックしている。それ以上の事は私にもわかりません。

queryLocalInterface()はBnInterfaceではインターフェースディスクリプタをチェックした上でthisを返し、
BpInterfaceではNULLを返します。IBinderのlocalBinder()とほぼ同じ実装となっています。
つまりqueryLocalInterface()がNULLじゃなければ、BnInterfaceのサブクラス、ひいてはflat_binder_objectがBINDER_TYPE_BINDERだったケースなので、
これはサービスのポインタという事です。
つまりBnMyServiceのポインタです。そこでIMyServiceにただキャストするだけでインスタンスを直接呼び出せる訳です。

===[column] queryLocalInterface()メソッドとインターフェースディスクリプタの著者の勝手な憶測
本文でも解説している通り、queryLocalInterface()メソッドは現状では完全にlocalBinder()メソッドと同じ機能となっています。
引数の文字列により各インターフェースを保持しているかを問い合わせるケースでは振る舞いが変わるのですが、そういうユースケースは存在しません。
普通の分散オブジェクトのシステムだと、この手の仕組みが必要なのは

 1. 型の無い言語で動的に問い合わせる必要がある場合
 2. インターフェースのバージョニング
 3. 特定のインターフェースを実装しているオブジェクトにだけアスペクトを注入するようなコンテナライブラリの存在

というのが良くあるパターンです。
バージョニングに関しては通常は他のサービスが動き続ける中で自分だけアップデート、みたいな状況でないと、この仕組みが必要とは思えず、
その場合にわざわざ備えておく、というのは、わざわざローカルに特化した分散オブジェクトシステムを作り直したAndroidらしくないと思います。

そこで自分の予想では、これはスクリプト言語対応の為に入れた、というもの。
何故現存しないスクリプト言語対応の為のコードが残っているのかは謎ですが、
Binder周辺の歴史のどこかでスクリプト言語を使っていた環境があって、結構なコードがその環境で書かれた後に、そのコード遺産をそのまま持ってきた結果、ここを修正するのが大変になりそのまま残っているんじゃないでしょうか。
全て私の勝手な憶測なので本当の所は分かりませんが。
===[/column]

NULLだった場合はBpBinderです。つまりハンドルが中に入っているだけで、サービスのクラスのポインタでは無く、ただのint値です。
そこでプロキシクラスであるBpMyService1を作ってIMyService1にキャストして返します。
こうする事で、ByMyService1で8.6.6のような事をするように実装すれば、サービス呼び出しのコードが実現出来ます。

このメソッドのポイントとしては、どちらにせよIMyService1*が返るので、
利用者はこのIBinderがBpBinderなのかBBinderなのかを気にせずに、同じコードでサービスを呼べます。
そこで、あるバージョンのAndroidからそのサービスが別のプロセスに変更になっても、
全くコードを変更する必要はありません。

なお、このasInterface()メソッドですが、これを呼ぶフリーフローティング関数としてinterface_castという関数が用意されています。

//list[intcast][interface_cast()インライン関数]{
template<typename INTERFACE>
inline sp<INTERFACE> interface_cast(const sp<IBinder>& obj)
{
    return INTERFACE::asInterface(obj);
}
//}

見る人が見ればこれだけで使い方は想像がつくと思いますが、詳細はzzzで扱います。
#@# TODO: 参照

このように、IMyService1という物を定義して、決まり通りマクロを置くと、asInterface()メソッドが使えるようになります。


== 8.7.7 サービスの実装とプロキシの実装 - BnInterfaceとBpInterface

基本的にはasInterface()メソッドの説明で、この共通ネイティブインターフェースのレイヤの説明は終わりなのですが、
ここは実際にサービスを実装する人が見るクラスでもあるので、簡単に他のクラスの実装も追って全体像を提示しておきましょう。

サービスの実装はBnInterfaceのサブクラスとして行います。
プロキシの実装はBpInterfaceのサブクラスとして行います。
実装内容はBBinderとBpBinderの時とほとんど変わりません。
つまり、ほとんど8.6.4と8.6.6の二つの節の内容の繰り返しとなります。


=== サービスの実装 - BnInterfaceのサブクラスの実装

まずはサービスの実装側、つまりBnInterfaceを継承したBnMyService1の実装を見てみます。
サービスを実装する人はBnInterfaceを継承し、onTransactを実装しなくてはいけません。
そしてメソッドのIDも定義します。

コードとしては以下のようなコードになります。

//list[bndecr][MyService1の実装側の宣言（つまりBnMyService1の宣言）]{

// /* 1 */ テンプレート引数でIMyService1を渡す
class BnMyService1 : public BnInterface<IMyService1> {

    enum {
        // メソッドID。IBinder::FIRST_CALL_TRANSACTIONより大きい値を使う約束になっている
        MYSERVICE_ADD = IBinder::FIRST_CALL_TRANSACTION
    }
    
    virtual status_t    onTransact( uint32_t code,
                                    const Parcel& data,
                                    Parcel* reply,
                                    uint32_t flags = 0);

};
//}

このonTransactについての説明は8.6.4の内容と完全に同じです。
ただ、BBinderと大きく違う所としては、BnInterfaceには/* 1 */にあるように、インターフェースクラスを受け取るテンプレート引数がある事です。
この結果、BnMyService1はIMyService1も継承するように展開されます。

コードとしては以下のBBinderのコードとほとんど変わりません。

//list[bnsummary][BnMyService1の簡略化した定義]{
class BnMyService1 : public IMyService1, public BBinder {
...
};
//}

IMyServiceを継承した為、addも実装しないといけなくなりました。

//list[addimpl][addの実装]{
int BnMyService1::add(int arg1, int arg2) {
    return arg1+arg2;
}
//}

同じプロセスからこのサービスが呼ばれる場合には、このメソッドが直接呼ばれます。

このようにIMyService1をBnMyService1も継承する事で、同じプロセスなら直接addを呼び、
別のプロセスから来た場合はIPCThreadStateのjoinThreadPoolから呼ばれるtransact呼び出しの仕組みを使いまわす、
という事が一つのクラスで出来ています。

//image[7_7_1][IMyService1から呼ばれる場合とtransactから呼ばれる場合が同じメソッドに行き着く]

BBinder同様、別のプロセスから呼ばれる場合はonTransact()が呼ばれます。
BBinderの時とあまり変わりませんが、一応完全を期す為にonTransactの実装例を書いておきます。

//list[ontransactimpl][onTransactの実装]{

// onTransactをオーバーライド
status_t BnMyService1::onTransact( uint32_t code,
                                    const Parcel& data,
                                    Parcel* reply,
                                    uint32_t flags = 0)　{
    switch(code) {
        case MYSERVICE_ADD: {
            // dataから引数を取り出し。

            // /* 1 */ IInterfaceを使う場合、先頭二つはstrict policyとインターフェースディスクリプタ。後述。
            int strictPolicy = data.readInt32();
            String16 interfaceDescName(data.readString16());
            
            // 残りはサービスごとに決める。
            // 呼び出し側でint32を二つ並べて書き込んでいる、と仮定している。
            int a = data.readInt32();
            int b = data.readInt32();

            // addを呼び出す。
            int result = add(a+b);
            
            reply->writeInt32(result);
            return NO_ERROR;
        }
    }
    return BnInterface::onTransact(code, data, reply, flags);                                   
}
//}

BBbinderの時と違うのは、case文で引数を取り出す時の/* 1 */の所にある最初の二行くらいでしょう。
一応そこだけ軽く説明しておきます。

/* 1 */の所だけのコードを抜き出すと以下のようになっています。

//list[quote1][1の所だけ抜粋]{
    // /* 1 */ IInterfaceを使う場合、先頭二つはStrictModeとインターフェースディスクリプタ。後述。
    int strictPolicy = data.readInt32();
    String16 interfaceDescName(data.readString16());
//}

一行目のstrictPolicyという変数で読みだしているのは、StrictModeという仕組みで使う値です。
基本的には0x100が書かれます。これはthreadが何か意図しないディスクアクセスなどをしないかチェックする為の機構ですが、
本書ではこれ以上は扱いません。以下のリンクを参照ください。

https://developer.android.com/reference/android/os/StrictMode.html

インターフェースディスクリプタはIMPLEMENT_META_INTERFACEで渡した"com.example.IMyService1"が書いてあります。

この二つは別段使い道は無いので、ただスキップして捨てれば十分です。
ですが、これらの値がちゃんと書いてある事を確認するマクロ、CHECK_INTERFACEという物があるので、
自分で読み飛ばすよりは、これらを使う方が望ましいでしょう。

以下のような行を書けば、チェックしつつ読み飛ばした事になります。

//list[chkintrcall][CHECK_INTERFACEマクロの使用例]{
    CHECK_INTERFACE(IHelloService, data, reply);
//}

このマクロを呼び出すと、StrictModeを設定し、インターフェースディスクリプタが正しい事を確認してくれます。

それ以外はBBinderの時と変わりません。
以上でBnInterfaceの説明は終わりです。

=== サービスプロキシの実装 - BpInterfaceのサブクラスの実装

サービスプロキシは、BpInterfaceを継承して実装します。
内容はBpBinderの時の8.6.6とほとんど同じです。

まずはBpInterfaceを継承したBpMyService1の宣言から見てみましょう。

//list[myproxy][MyService1のプロキシ実装の宣言（つまりBpMyService1の宣言）]{
class BpMyService1: public BpInterface<IMyService1>
{
...
};
//}

BnInterfaceと同様にBpInterfaceもテンプレート引数を通じてIMyService1を継承する事になっています。

BpInterfaceにはコンストラクタでIBinderが渡されます。
IBinderは前にも言った通り実体は大きくBpBinderとBBinderのケースがあるのですが、BpInterfaceに渡されるケースはBpBinderの方です。
BBInderの時はBnInterface側のクラスにキャストされるだけなので、こちらのクラスには渡ってきません。

BpInterfaceクラスは、コンストラクタで渡されたBpBinderをremote()というアクセサで参照できます。
このBpBinderのtransact()を用いてプロキシを作るのは、8.6.6で解説した手順とほとんど同じです。

試しにadd()を実装してみましょう。

//list[mysvcproxyimpl][MyService1のプロキシの実装]{


// /* 1 */ テンプレート引数にIMyService1を渡す
class BpMyService1 : public BpInterface<IMyService1>
{
public:
    // /* 2 */ コンストラクタはIBinderを受け取る物を用意する必要あり。実装はスーパークラスを呼び出すのみ。
    BpMyService(const sp<IBinder> &remote) : BpInterface<IMyService1>(remote) {}

    // IMyService1から継承したaddの実装。remote()->transact()を呼ぶ。
    virtual int add(int arg1, int arg2) {

        Parcel data, reply;
        // まずは引数を準備。

        // /* 3 */ 最初はStrictModeとインターフェースディスクリプタを書く決まりになっている。以下の文で書かれる。
        data.writeInterfaceToken(IMyService::getInterfaceDescriptor());
        
        // あとは引数を書く。 intを二つ。
        data.writeInt32(arg1);
        data.writeInt32(arg2);
        
        // メソッドID MYSERVICE_ADDで引数は上で準備した物で呼び出し。        
        remote()->transact(MYSERVICE_ADD, data, &reply);
        
        // 結果を取り出して返す。結果はint。
        return reply.readInt32();            
    }
};
//}


実装としては8.6.6とほとんど同じ内容となっています。
違うのはテンプレート引数、コンストラクタ、writeInterfaceTokenくらいでしょうか。
この3つを簡単に解説します。

/* 1 */ BnInterfaceと同様、BpInterfaceもテンプレート引数でIMyService1を渡します。
こうする事で、IMyService1を間接的に継承します。

/* 2 */ コンストラクタとしては、sp<IBinder>を受け取るコンストラクタが必要です。
というのはIMPLEMENT_META_INTERFACEマクロで生成されるasInterface()のコードで、それを決め打ちで呼ぶからです。
一般的にはconstリファレンスにします。そして実装はだいたいは基底クラスのコンストラクタを呼ぶだけです。

つまり、以下のコードになります。

//list[proxyconstructor][プロキシ実装が要求されるコンストラクタ]{
    BpMyService(const sp<IBinder> &remote) : BpInterface<IMyService>(remote) {}
//}

/* 3 */ add()の中は、writeInterfaceToken()の所だけ8.6.6の実装(@<list>{threadpool|bpbinder_usage})と異なっていますね。
以下のコードです。

//list[proxydiff][前の例(@<list>{threadpool|bpbinder_usage})との差分]{
    data.writeInterfaceToken(IMyService::getInterfaceDescriptor());
//}

IMyService::getInterfaceDescriptor()は、IMLEMENT_META_INTERFACEマクロに渡した文字列、
つまり"com.example.IMyService1"が返ります。

data、つまりParcelのwriteInterfaceToken()にインターフェースディスクリプタを渡して最初に呼ぶ、
というのは、決まりとなっている、と言って良いでしょう。
これを呼ぶとStrictModeとインターフェースディスクリプタという名の文字列が書かれて、
前述のとおりBnInterface側でCHECK_INTERFACEマクロを使うとこれらをチェックしてくれます。

実際にwriteInterfaceToken()で何が書かれていて、CHECK_INTERFACEマクロでなにがチェックされているかはあまり気にする必要は無いと思います。

プロキシで大切なのは、前項のBnMyService1のaddで引数を読み出す処理と、今回のtransactの実装でdataに引数を書き込んでいる所で、
順番や型などを一致させる、という事です。この二つの対応がとれていれば、中はどうでも良いのです。


=== 全てのクラスが揃った時の使い方

以上でIInterface、BnInterface、BpInterfaceの3つのサブクラスが揃いました。
これでasInterface()を使う事が出来ます。

//list[howtouse][三つのサブクラスが揃っている場合の使い方]{
sp<IMyService1> intr = IMyServce1::asInterface(defaultServiceManager()->getService(String16("MyService1"));

int result = intr->add(3, 4);
//}

こうするとasInterface()メソッドは、getService()の結果がBpBinder由来のIBinderだったらばプロキシを作り、
ポインタ由来だったらBnMyService1にキャストして結果を返します。

呼ぶ側としてはこのインターフェースが同じプロセスにある生のポインタなのか、別のプロセスにあるサービスのプロキシなのかを気にする事無く、
ただaddのメソッドを呼べば良くなります。

そしてBBinder由来の場合は、完全にローカルなBnMyServiceのポインタに対してメソッド呼び出しをするだけなので、
分散オブジェクトのオーバーヘッドは一切無い、ただのC++の仮想関数呼び出しで済みます。
#@# TODO: 図解？

さらに上のasInterface()呼び出しと全く同じ事ですが、interface_castという名前のinline関数も定義されています。

//list[intcast_again][再掲: interface_cast()インライン関数]{
template<typename INTERFACE>
inline sp<INTERFACE> interface_cast(const sp<IBinder>& obj)
{
    return INTERFACE::asInterface(obj);
}
//}

これを使うと、以下のように、キャストのように書けます。

//list[intcast_ver][interface_cast()バージョン]{
sp<IMyService1> intr = interface_cast<IMyServce1>(defaultServiceManager()->getService(String16("MyService1"));
//}

普通はこのinterface_castを使う方が読みやすく意図も分かりやすいので良いと思います。
ですがasInterface()もJavaの方でも同名のメソッドがあって役割は同じなので、知っておくと良いでしょう。

== 8.7.8 共通ネイティブインターフェースまとめ

以上で、共通ネイティブインターフェースのレイヤの説明を終わりました。

まず、IInterfaceを継承してインターフェースを作り、決まり事となっているマクロを置きます。
その後BnInterfaceとBpInterfaceを継承したクラスを用意すると、asInterface()メソッドとinterface_cast関数が使えるようになります。
こうして呼ぶ側はサービスが同じプロセスに居るのか別のプロセスに居るのかを気にせずサービスを使う事が出来ます。

そしてサービスがローカルにあると通常のC++のメソッド呼び出しとなるので、これは極めて高速に行われます。
サービスに分けるコストというのはプロセスさえ同じならほとんどないと言って良いでしょう。

共通ネイティブインターフェースの説明が終わったので、これでC++でのシステムサービスの実装にかかわるクラスライブラリの説明は全て終わりです。
ここまでの内容を理解した読者の皆様は、mediaserverなどのネイティブのシステムサービスを読んだり、
自分の端末のファームウェア用に専用のシステムサービスを追加するのに十分な知識を備えた事になります。

次の節からは、Javaでシステムサービスを作る場合の手順に進みます。
ほとんどが本章で解説した共通ネイティブインターフェースのレイヤと同じ構成なのですが、
Javaのシステムサービスの実装では、一つだけ大きく異なる所があります。

それがAIDL（Android Interface Definition Language）からの自動生成です。
これを次に見ていきましょう。


TODO: 以下は一部ボツ。あとで一部（必要なら）サルベージ


サービスの実装と呼び出しは、理論的にはbinderドライバに対してbinder_write_readに適切なデータを入れてioctlを呼べば良い、という事になります。
ですが、それはあまりにも低レベルな為、Androidではそれの上にRPC(Remove Procedure Call)のレイヤがあります。

一般的にRPCは通常クライアント側のプロキシと、サーバー側のコードに分けられます。
クライアントは普通引数をシリアライズして自身のメソッドを表すメソッドIDをメッセージに詰めて送ります。
サーバー側はこのメソッドIDに応じてswitchして、それぞれのメソッドの引数に応じて引数をデシリアライズしてメソッドを呼び出します。
RPCのフレームワークは通常プロキシのコードを自動生成してくれて、また上記のswitchする部分のコードも自動生成してくれます。
サーバーを実装する人は、呼び出されるメソッドだけ実装すれば良い、という形になっています。

Androidにおいてもそれらを自動生成してくれる仕組みがあります。
aidlというIDLでインターフェースを定義すると、プロキシとサービスのディスパッチの部分を行う基底クラスを自動生成してくれます。

ここではそれらの自動生成の仕組みを理解する為に、自動生成される基底クラスに相当する物がどういう事をしているのかを見ていきます。
