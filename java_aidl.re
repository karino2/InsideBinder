= 8.8 AIDLからの自動生成 - Javaによるシステムサービスの実装

#@# 旧8.6

//lead{
ここまではC++によるネイティブのシステムサービスの実装を見てきました。
ここからは、Javaによるシステムサービスの実装を見ていきます。

サービスのコードは前節で見たように、BpMyService1のtransact()の呼び出しとBnMyService1のonTransact()の呼び出しが対応しています。
SurfaceFlingerサービスのBnSurfaceComposerなどのネイティブサービスは上記の手法で自分でサービスを実装しています。

しかし、BnMyService1もBpMyService1も、かなりの部分がインターフェースからほぼ機械的に決まります。
プロキシであるBpMyService1はIMyService1のメソッドの定義通りに引数をParcelにwriteして送信するコードを書くだけなので、インターフェースから全部のコードが完全に決まります。
実装側であるBnMyService1も、Parcelから引数をreadしてメソッドを呼び出す部分まではインターフェースから機械的に決まり、呼び出されるメソッドだけがインターフェースからは自動では決まらない部分となります。

インターフェースとプロキシ、そして実装のデシリアライズ部分の対応は機械的なので、
人間がやるのは無駄であり、さらには対応が取れてないとバグになる為、
インターフェースを変更すると直し漏れなどでバグが生じやすい所でもあります。
そこで、人間はインターフェースを定義してその実装だけを書いて、自動的に決まる部分はなんらかの方法で自動生成する方が望ましいと言えます。

普通分散オブジェクトのシステムにはこれらのコードを自動生成する仕組みがあります。
多くのシステムは、IDLから生成するか動的にコードを生成するかのどちらかです。

Androidも例外ではなく、AIDLというIDLの方言を使ってインターフェースを記述しておくと、
これをコンパイルしてBnMyServiceとBpMyServiceに相当する部分のJavaコードを自動生成してくれます。
何故か知りませんが、C++のコード生成は無く、Javaしかサポートされていません。

ここからは、Javaによるシステムサービスの実装を見ていきます。

余談になりますが、Javaによるシステムサービスもネイティブによるシステムサービスと同様、ServiceManagerがSDKに公開されていない為、通常のアプリ開発者は行う事が出来ません。
あくまでシステムイメージを作るメーカーやカスタムROMを作る人が行う作業です。
ですがJavaの場合、通常のSDKのサービスの時にも同様の手順でaidlを用いて作業をするので、
かなりの部分はアプリ開発者でも行う事が出来ます。(詳細は8.8.2を参照)

//}

== 8.8.1 Javaのシステムサービスを支える基本的なクラス

Javaのクラスでも、だいたいはネイティブのこれまで解説したクラスに対応したクラスがあります。

//table[javanativerel][Javaとネイティブのクラスの対応]{
javaのクラス名	C++のクラス名	役割
--------------------------------------------------------------------
android.os.IInterface	IInterface	インターフェースの基底クラス
android.os.IBinder	IBinder	サービスのポインタとハンドルの双方の共通基底クラス
android.os.Binder	BBinder	サービスのポインタ
android.os.BinderProxy	BpBinder	サービスのハンドル
//}

android.os.Binderやandroid.os.BinderProxyの実装は非常に分かりにくくて、
ネイティブのシステムサービスの知識が無いとかなり難解ですが、
ネイティブ側を知った上で読めばそのまんまをただJavaでラップしているだけ、という事が分かります。


== 8.8.2 AIDLとコンパイルとサービスの実装手順

IDLとは全般に、引数などの型だけ、つまりインターフェースだけを記述する言語です。普通はパースが簡単で、
普通の言語よりも少しインターフェースに関する情報が多い傾向にあります。

AIDLの詳細については https://developer.android.com/guide/components/aidl.html を参照してください。

Javaのシステムサービスの作り方は以下の手順になります。

 1. AIDLでインターフェースを記述
 2. aidl.exeでコンパイル
 3. 生成されたコードを継承して残りを実装
 4. ServiceManagerに登録

なお、3まではシステムサービスで無い通常の開発者が開発するサービス、いわゆるActivityThreadが管理するSDKのサービスでも同じ手順で作る事が出来ます。
（ただしこの手順でサービスを作るのはスレッドモデルが複雑になり過ぎる為、通常は勧められていません https://developer.android.com/guide/components/aidl.html の冒頭の注意を参考の事)

AIDLは、以下のように書きます。

//list[aidl][AIDLによるMyServiceの定義]{
package com.example.myservice;

interface IMyService {
    int add(int arg1, int arg2);    
}
//}


これをSDKに含まれるaidl.exeに渡すと、IMyServiceというJavaのインターフェースと、
その内部クラスとしてIMyService.Stubというクラス、およびそのさらに内部クラスのIMyService.Stub.Proxyというクラスが実装されます。
なお、AndroidStudioは拡張子aidlのファイルがあると自動でこのファイルを生成します。

生成されるコードの詳細は後に回すとして、まずは実装と使い方だけ見てみましょう。

サービス実装者は、aidlをコンパイルしたら、「インターフェース名.Stub」というクラスを継承してメソッドを実装しなくてはいけません。
今回の例ではインターフェース名はIMyServiceなので、以下のようになります。

//list[aidlimpl][AIDLをコンパイルした後はStubを継承して実装]{
class MyService extends IMyService.Stub {
    public int add(int arg1, int arg2) {
        return arg1+arg2;
    }
}
//}

このクラスをServiceManagerに登録すれば、サービスとして使う事が出来ます。
ServiceManagerはネイティブのIServiceManagerと同じ役割のクラスです。

IServiceManagerと同様、ServiceManagerはメーカーなどのシステム提供者が使う事が前提のクラスで、
一般の開発者が使う物ではありません。ですから、SDKには含まれません。
ROM開発など、Androidのシステムをビルドする時には使えます。

こうして実装したサービスを以下のようにServiceManagerに登録してやると、クライアントから使う事が出来ます。

//list[addsvc][実装したサービスをServiceManagerに登録]{
ServiceManager.addService("myservice", new MyService(context));
//}

サービス側が以上の事をしていると、クライアントはこのサービスを使う事が出来ます。

サービスを使うには、ネイティブの場合と同様に/* 1 */ServiceManagerのgetService()を使ってIBinderを取得し、
/* 2 */それをネイティブの場合のasInterfaceとほぼ同じ役割をする「サービス名.Stub.asInterface()」を使ってインターフェースとして呼び出します。

具体的には以下のようなコードになります。

//list[svcusage][Javaにおけるサービスの使用]{
// /* 1 */ getServiceでservicemanagerからハンドルを取得
IBinder binder = ServiceManager.getService("myservice");

// /* 2 */ Stub.asInterfaceでBpBinderかBBinderかに応じて適切なクラスでラップしてIMyServiceにキャスト
IMyService myservice = IMyService.Stub.asInterface(binder);

// サービス呼び出し
int result = myservice.add(3, 4);
//}

このように、aidlをコンパイルすると、binderを使用した通信回りのコードは全て自動生成されて、実際のインターフェースの実装をするだけで済みます。
使う側も、取得の所を適当なファクトリーメソッドでラップしてしまえば、中を何も理解せずに使う事が出来ます。

ですが、まったく自分で作っていない「インターフェース名.Stub.asInterface()」というのを呼び出さなくてはいけなくて、
このメソッドは本章で解説したような背景知識が無いと何をしているのか分からないので、
自分で実装してみるのは簡単だけど、自分が実装したコードが何をやっているかは分からない、という事になりがちです。
しかし本章をここまで読み進めた読者なら、実装を見なくてもこれが何をしているかは想像できる事でしょう。そして実際にその想像通りの事しかしていません。

#@# TODO: 小まとめの図解？

以下では、軽く自動生成されるJavaのコードを見ていきましょう。
基本的にはネイティブと同じなので自分で読んでも分かると思うので、不要と思う読者はスキップしていただいて構いません。

== 8.8.3 AIDLのコンパイルで生成されるクラス - StubとStub.ProxyとStub.asInterface()

IMyService.aidlをaidl.exeやAndroidStudioでコンパイルすると、
Javaのソースが生成されます。
以下に生成されるクラスの入れ子関係とasInterface()の宣言だけを並べてみましょう。

//list[asinterface][AIDLで生成されるクラスの入れ子関係とasInterface()]{
package com.example.myservice;

// IMyServiceはJavaのinterfaceとなる
public interface IMyService extends IInterface {
    
    // BBinder相当のサービスの実装の基底クラスをStubインナークラスで定義。
    // interfaceでは実装は含めないのでIMyServiceが実装すべき物もここに押し込んである（例: asInterface()）
    public static abstract class Stub extends Binder implements IMyService
    {
        public static IMyService asInterface(IBinder obj)...

        // ProxyクラスはStubクラスの中にある。asInterface()からしか参照しないので、privateで。
        private static class Proxy implements IMyService {
            ...
        }
    }
}
//}

クラスとインターフェースを並べると以下の親子関係になります。

//image[8_3_1][IMyService、Stub、Proxyの包含関係]

IMyService.Stubはネイティブで言う所のBnMyServiceに相当するコードを生成します。ただしサービスのメソッドの実装部分はabstractのままです。
Android以外の分散オブジェクトでStubと言われる物と同様の物です。

IMyService.StubはいわゆるTemplateMethodパターンで、
add()がサブクラスで実装される、という前提でonTransact()の実装を生成します。
このonTransactはやってきたデータから引数をデシリアライズしてこのadd()を呼び出します。
なおこのonTransactは、最終的にはネイティブの時と同様、IPCThreadStateのjoinThreadPoolからBBinderのtransact()を通して呼ばれます。

asInterface()の実装もこのStubという内部クラスに入ります。
本来的にはStub側では無くIMyServiceに含めるべき物な気もしますが、Javaはinterfaceに実装が含めないのでここに入るのでしょう。

IMyService.Stub.Proxyはネイティブの所で解説したBpMyServiceと全く同じ事をするプロキシのクラスです。
BinderProxy由来のIBinderをコンストラクタで受け取り、このBinderProxyに対してメソッド呼び出しに対応する引数とメソッドIDを用意してtransact()を呼び出します。

他の分散オブジェクトシステムに詳しい方の為に補足しておくと、
名前はIMyService.Stub.Proxyですが、いわゆる分散オブジェクトのStubでは無くプロキシです。IMyServiceにasInterface()の実装が置けないのでこうなっているに過ぎません。
Proxy自身はファクトリメソッドの中でだけ触れれば十分で、そこより上はインターフェースを返すので、asInterface()からだけアクセス出来るStub内部クラスの中でprivate宣言されています。
#@# TODO: 分かりにくいので見直す。

IMyService.Stub.ProxyとかIMyService.Stub.asInterface()とか間にStubが入るので難しく見えますが、
ほとんどは単にインターフェースには実装がおけないというJavaの制約を回避する為にStubという内部クラスに関係無い実装まで置いているからに過ぎません。
対応するネイティブの概念を本書で見ながら見ていけば、見た目よりはずっと簡単な物である事が分かります。

以上でaidlをコンパイルした場合のケースも解説が終わりました。
基本的にはサービスの実装者はIMyService.Stubを継承して実装し、呼ぶ側はIBinderをIMyService.Stub.asInterface()でインターフェースに変換して使えば良いだけで、
解説する事もあまりありません。

ネイティブのIInterface関連を使った実装と比べると、実装者としてはonTransactのswitch-caseやプロキシクラスを書かなくても良い分楽になります。
一方で実装の楽さに比べると登場する物の多さと言語境界をまたいだ分の複雑さから、なかなかJavaのレイヤーで全容を理解するのが難しくなっています。

ですが、本章をここまで読み進める事が出来た読者の方は、端から端まで全てが理解出来るのではないでしょうか。

== 8.8.4 実際のACCOUNT_SERVICE呼び出しにみる、システムサービス呼び出しの例

システムサービスの仕組みについてはここまでの話で全て終わっています。

そこで終わっても良いのですが、せっかくなので、実際に普段皆さまがアプリを書く際に書いているコードを見て、
それが実際どういう事だったのか、という事を、これまでの説明に照らし合わせて見ていきましょう。

例として、ACCOUNT_SERVICEからそのユーザーのgoogleアカウントの一覧を取り出す例を考えます。
それは、例えばActivityなどで以下のように書きます。

//list[acmgrusage][AccountManagerサービスを使う例(@<fn>{notrecom})]{

AccountManager accountManager = (AccountManager)getSystemService(ACCOUNT_SERVICE);
Account[] accounts = accountManager.getAccounts();

for (Account account : accounts) {
   if (account.type.equals("com.google")) {
   ...
//}

//footnote[notrecom][一行目のgetSystemServiceを直接呼ぶ方法はAccountManagerに関しては現在では推奨されていなく、実際にはAccountManager.get(context)というメソッドがあってそちらを使う事になっています。ですがやっている事はgetSystemService()を呼ぶ事と同じです。]


このようにAccountManagerというオブジェクトを取り出して、まるで自身のプロセスのクラスであるかのようにgetAccounts()を呼び出す事が出来ます。

これらのありふれたコードと、これまで説明したシステムサービスの仕組みはどう関係しているのでしょうか？

#@# TODO: いきなり図解を入れる方が良い？

ActivityのgetSystemService()は、実際はContextImplで実装されています。
このgetSystemService()から呼び出されるコードは以下のようになっています。

//list[insidegetss][getSystemService()で最終的にサービスを取得している所]{
IBinder b = ServiceManager.getService(ACCOUNT_SERVICE);
IAccountManager service = IAccountManager.Stub.asInterface(b);
return new AccountManager(ctx, service);
//}

AccountManagerはIAccountManagerのラッパとなっています。
このように既知のサービスはContextImpl内でStub.asInterface()がハードコードされているので、アプリ開発者はこのBinderという仕組みを理解していなくても使う事が出来ます。

#@# TODO: 既知のサービスについて、具体例を補足


ServiceManager.getService()でサービスのbinder_nodeのbinder_refを表すハンドルを取得し、IAccountManager.Stub.asInterface(b)で、
このハンドルに対するIAccountManagerのプロキシオブジェクトが生成されます。
このプロキシオブジェクトのメソッドを呼び出すと、サービスの機能を使う事が出来ます。

ですがここでの実装では、直接このIAccountManagerをユーザーに返さずに、このプロキシをさらにAccountManagerというラッパクラスにくるんで、それを返しています。
このクラスは大した事はしません。RemoteExceptionをRuntimeExceptionにしたり、という程度です。

AccountManagerのgetAccounts()の呼び出しは、結局はIAccountManagerのgetAccounts(null)を呼び出します。
これはServiceManagerから取得したbinder_refのハンドルに対して、getAccountsのmethod IDと引数をシリアライズしてtransactを呼び出します。
すると、AccountManagerのサービスの方のIPCThreadState::joinThreadPoolの中からBBinderのonTransact経由でAccountMnaagerのサービスのonTransactが呼ばれて、
適切な動作が行われて、replyとしてアカウントの一覧を返します。

このように、Androidは多くのシステムの機能をシステムサービスという形で実装していて、ユーザーはシステムサービスが独自のプロセスで実装されているのか、
それともライブラリとしてアプリのプロセスで動くのかを気にせず使う事が出来ます。

サービスの仕組みは同一マシンを前提としていて使い方を決め打ちしている都合で、かなりライトウェイトでシンプルな仕組みとなっています。
これがスマホなどのリソースの制約されたシステムにおいても、かなり低レベルな所まで分散オブジェクトをベースとしたシステムを、
リーズナブルなパフォーマンスで構築する事を可能にしている鍵と言えます。

このおかげで、複数のサービスをプロセスのオーバーヘッドを避ける為一つのプロセスにまとめたり、端末のリソースが増えてきたら別のプロセスに移動したり、
はたまたクライアント側のプロセス内でサービスで無くライブラリとして実装したり、と言った風なAndroidのバージョンに応じたシステムの発展を、
アプリのコードを変更せずに実現しているのです。

#@# TODO: 本章序盤でも言及した方が良いかも。



