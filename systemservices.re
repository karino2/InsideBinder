= 様々なプロセスやシステムサービスのBinder関連の初期化を理解する

//lead{
前節までで、システムサービスの実装の仕方は全て説明しました。
そしてなぜBinderを用いた呼び出しが動くのかも、ネイティブはちゃんと説明してありますし、Javaのプロセスでもだいたいは想像がつきます。
Binderの章としてはここで終わりにしても良いのですが、その為には他の各章との間をある程度想像力で補う必要がある事でしょう。
そこで、本書ではそうした想像力に期待して終えるのでは無く、ここで復習も兼ねて他の章との境目となりそうな話をしておきたいと思います。

境目として着目するのは、各システムサービスがどこのプロセスに所属しているか、というプロセス的な側面と、
それぞれのプロセスの初期化の部分です。

#@# TODO: 1章冒頭で言及した方が良い？
システムサービスは、どのプロセスに属しているか、という事を意識せずに使えるシステムです。
ですが、逆にそうであるからこそ、個々のシステムサービスがどこのプロセスに属しているか、という事に着目すると、
Androidというシステムが現在どの程度はハードウェアリソースがあると考えているか、
という判断を理解する事が出来て、面白い視点を提供してくれます。

また、各サービスでなぜBinderを使った呼び出しが動くのか、という事を確認する為に、
JavaのプロセスでProcessStateやIPCThreadState関連のメソッドをどこで呼んでいるか、
ネイティブのサービスでは実際はどうなっているのか、という事も見ておきます。

//}

== SystemServerとそれ以外のサービス - Zygoteから起動されるサービスとinit.rcから起動されるサービス

TODO: 参照更新(0章は一巻か二巻に編入されたはず）。この章は本編への参照が多いので、いちいちTODOは足さずにまとめて直す。

0章で解説した通り、Androidのinitからは、app_processというプロセスがZygoteモードで立ち上がります。
Zygoteモードで立ち上がるapp_processはJava関連の初期化を行い必要なシステムのクラスライブラリをロードした後、
「Zygoteになる直前」に、SystemServerと呼ばれるプロセスをforkします。

//image[9_1_1][initからforkされるサービス達]

SystemServerはJavaで書かれたプログラムが動くapp_procerssでありながら、Zyogeになった後のapp_processからforkする訳では無い、
少しだけ特別なプロセスです。psではsystem_serverと見えて、JavaのクラスとしてはSystemServerクラスとなります。

様々なサービスがどのプロセスにホストしているか、という事を調べる場合、最初に知っておくべき基本となる事としては、
ほとんど全てのシステムサービスは、このSystemServerプロセスにホストされている、という事実です。
SystemServerに何がホストされているか、と考えるよりも、何がSystemServer以外かを調べた方がずっと早いのが現状です。

大まかには、以下のように分かれています。

//image[9_1_2][システムサービスがどこにホストされているか]

では、まずはSystemServer以外のプロセスにホストされているサービスから見ていきましょう。

=== SystemServer以外のプロセスにホストされているサービス

Android 7.0現在では、SystemServer以外にホストされているサービスというと、例えば以下のような者たちになります。

 1. surfaceflingerサービス
 2. audioflingerサービス
 3. cameraサービス
 4. mediaplayerサービス
 5. batterypropertiesサービス

surfaceflingerサービスはsurfaceflingerという名前の単独のプロセスにホストされています。実行ファイルのパスは/system/bin/surfaceflingerにあります。

audioflingerとcameraとmediaplayerなどは、7.0で大きく刷新された所です。
本章を書き始めた時には、これらのサービスは全部mediaserverという名前のプロセスにホストされていました。
ところがセキュリティ的に大きな問題となったstagefrightバグなどの影響で、
現在はそれぞれ個別のプロセスに分けられて、audioserver, cameraserver, mediaserverという別個の実行ファイルから起動されます。
MediaFrameworkの刷新とstagefrightバグについては12.6.2のコラムも参照ください。
#@# TODO: 参照更新

batterypropertiesサービスはhealthdというデーモンにホストされていて、実行ファイルは/sbin/healthdにあります。
どれもinit.rc関連のファイルから起動されています。

全体的に、高い権限を必要とするかリアルタイム性を要求される物が多い傾向にあります。
音声は途切れると目立ちますし、画面をスクロールした時の引っかかりなどは小さな事ですが大変目立つので、Androidではバージョンを重ねるごとに画面に割くハードウェアリソースを増しています。

=== SystemServerにホストされているサービス達

一方でSystemServerにホストされるサービスは多すぎて列挙しても見きれない程です。
この本でも良く出てくる見慣れた物を幾つか挙げると以下のようになります。

 1. AccountManagerService
 2. WindowManagerService
 3. InputManagerService
 4. ActivityManagerService

#@# TODO: 本編への参照を追加

その他、ほとんどのサービスがこのSystemServerにホストされています。

現在は別のプロセスに分けられたサービスも、かつてはこのSystemServerプロセスがホストしていました。
Androidはバージョンを重ねるごとに、標準的な端末のCPUのコア数やメモリ等が増えていくに応じて、SystemServerにいるサービスを、
別のプロセスを作ってそこに移動していく、という事を繰り返しています。

例えばsurfaceflingerは<ref>surfaceflingerサービスにみる、システムの発展</ref>で見たように、
Android 2.3の頃はSystemServer内で行っていた処理が、
Androidの3.0の頃に現在のように別のsurfaceflingerプロセスになったと思われます。

また、Android 7.0で行われたMediaFrameworkの刷新でmediaserverプロセス一つにホストされていた様々なサービスが個別のプロセスに分けられたのも、
システムの発展に伴いプロセスを分けていく例の一つと言えます。(詳細は12.6.2のコラムも参照ください)
MediaFrameworkの刷新は、サービスという仕組みを根底に据える事でシステムの発展に伴いプロセスを分けていく事を可能にしている、という事が、
まさに本書執筆時点でも有効に活用されている例と言えます。
Binderを用いたシステムサービスの仕組みは、stagefrightバグに代表されるより高度なセキュリティ対策の必要性を、
初期のAndroidの頃から見越して作られている仕組み、と言えます。
この辺の出来事と対応をリアルタイムで見ていると、システムの最初に織り込まれた哲学という物の凄みを感じます。

将来もコアの数などが増えるに従い、もっと多くのシステムサービスが、だんだんと別のプロセスに分かれていく事でしょう。

== surfaceflingerのmainを見る - SystemServer以外のシステムサービスのmain関数

まずはSystemServer以外のサービスの、立ち上がる所から見てみましょう。
一番簡単で本書での出番も多いsurfaceflingerが説明にも都合が良いので、surfaceflingerのmain関数を見てみます。
コメントだけ和訳して、まずは全コードを載せてみます。

//list[surfaceflgrmain][SurfaceFlingerのmain()]{
int main(int, char**) {
    // surfaceflingerのスレッドの最大数は4に制限しておく
    ProcessState::self()->setThreadPoolMaxThreadCount(4);

    // スレッドプール開始
    sp<ProcessState> ps(ProcessState::self());
    ps->startThreadPool();

    // surfaceflingerをインスタンシエート
    sp<SurfaceFlinger> flinger = new SurfaceFlinger();

#if defined(HAVE_PTHREADS)
    setpriority(PRIO_PROCESS, 0, PRIORITY_URGENT_DISPLAY);
#endif
    set_sched_policy(0, SP_FOREGROUND);

    // クライアントコードが接続してくる前の初期化
    flinger->init();

    // surface flingerをservicemanagerに登録して公開
    sp<IServiceManager> sm(defaultServiceManager());
    sm->addService(String16(SurfaceFlinger::getServiceName()), flinger, false);

    // このスレッドでrunする。
    flinger->run();

    return 0;
}
//}

いろいろとしてますね。この章に関連する所だけ順番に見ていきましょう。
全体的に、@<hd>{threadpool|システムサービスのmain関数とProcessState - 独自のシステムサービスを提供する時のコード例}で説明したのと同じ構造となっているので、そちらの説明も参考にしてください。

まずはProcessStateのself()を呼んでますね。

//list[prcstat][ProcessStateのself()呼び出し]{
    sp<ProcessState> ps(ProcessState::self());
//}

このProcessState::self()の呼び出しでBinderドライバがopenされてmmapされるのでした。
次にスレッドプールを開始しています。(@<hd>{threadpool|ProcessStateとスレッドプール}参照)

//list[prcstatthpool][スレッドプールの開始]{
    ps->startThreadPool();
//}

この呼び出しでは、新しいスレッドが作られて、そこでIPCThreadState::self()のjoinThreadPool()が呼ばれているのでした。
このjoinThreadPool()の中はioctlを呼んでそれを処理するメッセージループとなっています。(@<hd>{threadpool|IPCThreadStateのioctl()呼び出しループ - joinThreadPool()メソッドとBBinder}参照)

次にSurfaceFlingerのインスタンスをnewして初期化しています。

//list[sflinginit][SurfaceFlingerをインスタンシエートして初期化]{
    sp<SurfaceFlinger> flinger = new SurfaceFlinger();
    flinger->init();
//}

ここでSurfaceFlingerの型を見てみましょう。

//list[surfaceflingdef][SurfaceFlingerの型定義]{
class SurfaceFlinger : public BnSurfaceComposer,
                       private IBinder::DeathRecipient,
                       private HWComposer::EventHandler
//}

いろいろな物を継承していますが、BnSurfaceComposerは名前からBnInterfaceを継承したクラスと予想出来ます。
つまりこのtransactがIPCThreadStateのjoinThreadPoolから呼ばれて、
その中から呼ばれるonTransactをオーバーライドして実装しているのでしょう。(@<hd>{common_intr|サービスの実装とプロキシの実装 - BnInterfaceとBpInterface}参照)

一応BnSurfaceComposerの宣言を見てみます。

//list[bnsfcecompdef][BnSurfaceComposerの定義]{
class BnSurfaceComposer: public BnInterface<ISurfaceComposer> {
//}

予想通りBnInterfaceを継承していて、ISurfaceComposerをテンプレートパラメータ経由で間接的に継承しています。

つまりサービスとしてはBnSurfaceComposerが普通のサービス実装のようですね。
SurfaceFlingerはこれを継承して、さらに機能を足している事が読み取れます。

mainのコードに戻って、続きを読んでいきましょう。
次はSurfaceFlingerをdefaultServiceManager()のaddService()を呼び出して、
servicemanagerに登録するコードです。

//list[sfcfligreg][SurfaceFlingerをservicemanagerに登録して公開]{
    sp<IServiceManager> sm(defaultServiceManager());
    sm->addService(String16(SurfaceFlinger::getServiceName()), flinger, false);
//}

これも@<hd>{threadpool|IBinderとは何か？ - SVC_MGR_CHECK_SERVICEでハンドルが返ってこない場合}とほとんど同様で、唯一の違いは最後にfalseを渡している事です。
このフラグはallowIsolatedという変数名で、これは制限の強いサンドボックスからのアクセスは弾く、というフラグです。

こうしてIServiceManagerに登録すれば、このスレッドの役割は終わりなので、
通常のサービスのmain関数のパターンではIPCThreadStateのjoinThreadPool()を呼び出して、
このスレッドもスレッドプールに加えてしまうのですが、surfaceflingerはちょっとここが違います。

このmain関数のスレッドでも、スレッドプールによるioctlのループとは別のループを回すようです。

//list[flingerrun][SurfaceFlingerのループ開始]{
    // このスレッドでrunする。
    flinger->run();
//}

このrun()の中身の詳細はここでは説明しませんが、SurfaceFlingerは通常のサービスのイベントループとは別に、
Looperを使ったループを持っていて、別のスレッドからメッセージベースでアクセスする事を可能にしています。

#@# TODO: 図解、Looperへの参照

以上でsurfaceflingerサービスのmain関数を見ました。
大筋は@<hd>{threadpool|システムサービスのmain関数とProcessState - 独自のシステムサービスを提供する時のコード例}で解説したのと同じ処理をしている事が分かります。

== Javaのプロセスの開始とProcessState - AppRuntime::onZygoteInit()

Javaのプロセスの開始の詳細については9.6のZygoteの所で詳細に扱いますが、
ここではBinder関連の初期化の部分だけ見ておきましょう。

Javaのプロセスは、基本的にはZygoteというシステムサービスが生成するか、またはSystemServerプロセスかの、二種類しかありません。
そしてZygoteが生成する場合もSystemServerプロセスも、初期化時にAppRuntimeのonZygoteInit()というメソッドを呼ぶ事は同様です。
つまり、JavaのプロセスならこのAppRuntimeのonZygoteInit()がいつも呼ばれている、という事になります。

そのAppRuntimeのonZygoteInit()メソッドは以下のようになっています。

//list[apprtinit][AppRuntimeのonZygoteInit()メソッド]{

virtual void onZygoteInit()
{
...
    // いつものProcessStateのコンストラクタ呼び出し。binderドライバがopenされてmmapされる
    sp<ProcessState> proc = ProcessState::self();
    ALOGV("App process: starting thread pool.\n");
    // 新しいスレッドを作り、中でIPCThreadState::self()->joinThreadPool()が呼ばれる
    proc->startThreadPool();
}
//}

ProcessStateのself()とstartThreadPoolO()が呼ばれています。
これで、新しいスレッドがioctlをメッセージ受信の為に呼び出して、その結果がBBinderによる呼び出しだったらこのポインタのtransact()を呼ぶ、
というioctlのループが始まります。


このように、Javaのプロセスの場合、つまりSystemServerでもZygoteにより生成されたプロセスでも、
最終的にはネイティブのシステムサービスで出てきたProcessStateのself()でbinderドライバが初期化され、
startThreadPool()でioctl呼び出しと、その結果が戻ってきたらBBinderのtransact呼び出しを行っていく訳です。
こうして、Javaのプロセスでも、外部からのサービス呼び出しが処理されます。

大分マニアックな話ではありますが、SystemServerのサービスの呼び出しのスレッドがどうなっているか、
を正確に理解する事は、Androidを一段深く理解する上ではなかなか重要な所です。
9.6と合わせてここを理解しておくと、OSのスレッドのレベルでサービス呼び出しが理解出来るようになります。
#@# TODO: 図解

== ActivityManagerServiceはどうやってActivityThreadのメソッドを呼び出すのか？ - ApplicationThreadにみるシステムサービスでないクラスの呼び出し

ZygoteのプロセスでProcessState::self()->startThreadPool()呼び出しが行われている、という話を見たので、
これまで保留にしてきた、ApplicationThread呼び出しの所を最後に見てみます。

6章と7章において、ActivityManagerServiceはActivityThreadと協調してActivityを開始する様子を見ました。
その時に、例えば7.3のattach()メソッドなどで、ActivityManagerServiceから、ActivityThreadのscheduleXXX群のメソッドを呼ぶ、
という話をしました。
これはBinderを用いて行われているのですが、その仕組みについて、最後に見ていきたいと思います。
最後という事なので、復習も兼ねて細かい話を再度、通してみていきます。

この例をわざわざ見るのは、少し特殊な事があるからでもあります。その特殊な事とは、ActivityThreadは別にサービスでは無い、という事です。
実際、ActivityThreadはservicemanagerにも登録されていません。
それが、一体どこでioctlを待ちBBinderのtransact()を呼んでいるのか、このBBinderは誰か、という事を確認していきます。

=== ActivityThread, ApplicationThread, ActivityManagerServiceの確認


7.2.3で扱ったように、、ActivityThreadにはApplicationThreadというインナークラスがあります。
このApplicationThreadは、間にいろいろ挟まりますが基本的にはJavaのBinderクラスを継承しています。
復習しておくと、JavaのBinderクラスはネイティブのBBinderに対応しているクラスでした。

ActivityManagerService自身は@<hd>{SystemServerとそれ以外のサービス - Zygoteから起動されるサービスとinit.rcから起動されるサービス}で確認した通り、SystemServerにホストされているサービスです。
ですから、通常のサービス呼び出しとして別のプロセスからメソッドを呼び出す事が出来ます。
そこでActivityThreadは、attach()の呼び出しの所で、このApplicationThreadインスタンスを引数にActivityManagerServiceのattachApplication()というメソッドを呼んでいます。

//image[9_4_1][attachApplication()でAplicationThreadのインスタンスを渡す]

ActivityManagerServiceはサービスの実装クラスであり、ActivityThreadはそれとは別のプロセスからこのサービスを呼ぶクライアントなので、
普通に考えるとこの呼び出しはActivityManagerServiceのプロキシクラスを使って行います。実際コードを確認してもそうなります。

そのサービスプロキシの呼び出しの引数にJavaのBinderを継承したクラスを渡している訳です。するとどうなるか？

=== ApplicationThreadは、flat_binder_objectで送られる


サービスプロキシのメソッド呼び出しは、最終的にはBpBinderのtransactを呼び出すのでした。
このBpBinderのtransactに、JavaのBinderを継承したオブジェクトが、flat_binder_objectに詰められて渡されます。
@<fn>{nativeref}
そしてActivityManagerServiceのあるプロセスにこのflat_binder_objectが送られる。

//footnote[nativeref][厳密に言うと、JavaのBinderオブジェクトは、そのJavaのオブジェクトと一対一に対応したネイティブのBBinderのポインタを保持しているのですが、そのBBinderのポインタがflat_binder_objectに入ります。]


//image[9_4_2][ApplicationThreadをflat_binder_objectに詰めて送信する]

すると@<hd>{flat_binderobj|オブジェクトの送信とflat_binder_object その2 - binderドライバと受信側}で扱ったflat_binder_objectの変換のメカニズムで、ActivityThreadのあるプロセスにはbinder_nodeというツリーのノードがドライバ内に生成されて、
ActivityManagerServiceのあるスレッド側にはこのbinder_nodeを参照するbinder_refのツリーにノードが追加され、
このインデックスがハンドルとしてActivityManagerServiceのattachApplicationの引数には入る訳です。
このハンドルをActivityRecordの持つプロセス構造体に紐づけるのです。

//image[9_4_3][ActivityManagerService側にはハンドルがやってくる]
//image[9_4_4][実際は複数のアプリがある]


このように、ActivityThreadはservicemanagerに登録していないのですが、それでもシステムサービスと同様に、flat_binder_objectにポインタをセットして渡せば、相手にはハンドルとして渡ります。

=== アプリのプロセスがメソッド呼び出しを受け付けられる理由 - onZygoteInit()メソッドの処理

一見するとこれはどうという事のない話のように見えますが、これを実現させる為には、二つほど追加の条件が必要です。
#@# TODO: ちょっとハイコンテキスト過ぎるので補足したい。

 1. ActivityThread側も、binderドライバをopenしてmmapしてある
 2. ActivityThread側も、ioctl呼び出しでブロックして待っている

@<hd>{flat_binderobj|オブジェクトの送信とflat_binder_object その2 - binderドライバと受信側}で解説した通り、binder_nodeはbinderドライバのプロセス構造体が保持しています。
このプロセス構造体は、binderドライバファイルをopenした時のファイルディスクリプタに対応して作られる物でした。
つまり、binder_nodeが生成される為には、ActivityThreadのプロセス、今まさにZygoteからforkして、アプリのapkからクラスをロードするそのプロセスも、
binderドライバをどこかでopenしてmmapしてある必要があります。これが(1)に相当します。

また、メッセージを受け取るのにも準備が必要でした。

サービスのメソッドを呼び出す事、つまりActivityManagerServiceのメソッドを呼び出す側は、メッセージの送信だけで済みます。
ハンドルに対するメッセージの送信は、特に難しい事はありません。
ハンドルに対するメッセージ送信はBpBinderというラッパのクラスで行うのでした。(JavaではBinderProxyクラス）
中身としてもbinder_write_readに適切に初期化したデータを入れてioctlを呼べば良いのでした。

ですが、メソッド呼び出しを受け付ける側、つまり呼び出される側となるには、もう少し難しい処理が必要です。
メッセージを受け取る以上は、どこかで受信の為に誰かがioctlを呼び出して、ブロック状態になっていないといけません。
そうでないと、メッセージを受け取るスレッドが居ない事になってしまいます。
メッセージを受け付ける為にioctl呼び出しでブロック状態になるのは、IPCThreadStateのjoinThreadPoolの中で行われるのでした。
IPCThreadStateのjoinThreadPoolを、Zygoteでforkした後、誰かがどこかで呼び出していないといけません。
これが(2)に相当します。

この(1)と(2)を行っているのが、前項の@<hd>{Javaのプロセスの開始とProcessState - AppRuntime::onZygoteInit()}で扱ったAppRuntimeのonZygoteInit()メソッドだった、という訳です。

AppRuntimeのonZygoteInit()メソッド呼び出しは、Zygoteのループで、ソケットからのコマンドを待って子プロセスをforkしている所で行っています。
詳細は9.6.4で扱いますが、ここでは新しく生成されたプロセスの初期化の所で前項で説明したAppRuntime.onZygoteInit()が呼ばれる、という事が重要です。
このAppRuntimeのonZygoteInit()メソッドからProcessState::self()->startThreadPool()が呼ばれて、あらなたスレッドが作られてそのスレッドがスレッドプールの一員として振る舞います。
これらの初期化は、ActivityThreadのmainを呼び出す前に行われます。

つまり、アプリのプロセスは、システムサービスでは無い通常のアプリであるにも関わらず、必ずProsessState::self()->startThreadPool()が呼ばれているのです。
言い換えると全てのアプリのプロセスでは、サービスを呼び出そうと呼び出さなかろうと、必ずioctlによるループを回しているスレッドが一つは存在しています。
@<fn>{svcmust}

//footnote[svcmust][「サービスを呼び出そうと呼び出さなかろうと」と言いましたが、アプリが起動するにはActivityManagerService呼び出しが必須なので、実際にはサービスを呼び出さないアプリのプロセスという物は存在しません。]


このように、Zygoteをforkしたプロセスは必ず最初にProcessState::self()を呼び出し、
そこでbinderドライバファイルをopenしてmmapし、スレッドを一つ作り、その中でioctlのループを回しています。
このスレッドで、ioctlからBC_TRANSACTIONが来たら、binder_nodeで管理されているポインタのtransact()を呼び出す訳です。
このポインタはApplicationThreadと紐づいたネイティブのポインタという事になります。
それは巡り巡ってApplicationThreadのscheduleLaunchActivity()などの、scheduleXXX系列のメソッドを呼び出す事になります。

ですから、Zygoteをforkしたプロセスでは、BBinderのサブクラスをflat_binder_objectに詰めて別のプロセスに送り付ければ、
何もしなくても別のプロセスからのメソッド呼び出しを受け取る事が出来る訳です。

=== メソッド呼び出しを受け付けるスレッドはGUIスレッドでは無い

少し細かい話となりますが、このBBinderが呼び出されるスレッドは、AppRuntimeのonZygoteInit()メソッドで作られたスレッドプールのスレッドであって、
いわゆるLooper.loop()などを実行しているUIスレッドではありません。
ですから、Binder越しのメソッド呼び出しはGUIスレッドでは無いスレッドで実行される訳です。

そこで、GUIスレッドで処理を行わせる為に、7章で説明したHandlerのpostMessage()の仕組みを使って、UIスレッドでActivityのコンストラクタや、
ライフサイクルにかかわるstart()やstop()などのメソッドを呼び出していくのです。

TODO: 図解

このようにApplicationThreadのポインタがbinderドライバによって管理されて、そのハンドルがActivityManagerServiceにわたる。
ActivityManagerServiceは渡されたハンドルに対してメッセージを送り、
するとbinderドライバがこのflat_binder_objectをBINDER_TYPE_BINDERのポインタ、つまりApplicationThreadのポインタに置き換えて、
ActivityThreadの存在しているプロセスのProcessStateのstartThreadPool()で作られたスレッドがioctlでブロックして待っているのを起こし、
起こされたスレッドはBC_TRANSACTIONなのを見てBBinderにキャストしてtransact()を呼ぶ訳です。

TODO: 図解

最後はずいぶんと長く複雑な話になりましたが、このようにbinderドライバ、およびそれの上に構築されたシステムサービスという仕組みは、
Androidの根幹を支えている技術と言えます。多くのハードウェア的な機能やシステムの機能は、システムサービスという形で提供されていて、
呼ぶ側はサービスがどこのプロセスに居るのか、という事を気にせずに呼ぶごとが出来ます。
そして同じプロセスの呼び出しは通常のC++のメソッド呼び出しまで最適化される為、比較的低いコストで複数のサービスに分割してシステムを構築する事が出来ます。
そして、ActivityManagerServiceとActivityThreadというAndroidの中でもとても重要な部分でも、このbinderとその上のシステムサービスの仕組みはふんだんに使われています。

===[column] サービスという言葉の定義のむずかしさ

2.3.4のコラムでも書いた通り、Androidには様々な所でサービスという言葉が使われます。
そして、システムサービスとActivityThreadの管理するサービス、という二つの区別は、なかなか難しい物があります。
本書では前者をシステムサービスと呼び、後者をSDKのサービス、と呼んでいますが、ここまでの説明を理解すると、そこまで単純には分けられない事も分かってきます。
たとえば、ApplicationThreadはシステムサービスなのでしょうか？servicemanagerに登録されないので違う気がします。
一方でBinder、BinderProxy、IInterfaceといった仕組みは全て使われていて、システムサービスとの違いが技術的に何なのか、というのは極めてあいまいです。
また、ActivityThreadに管理されるSDKのサービスも、Binderを継承する事が出来て、binderドライバを通してアプリと通信する事が出来ます。
ActivityからbindServiceを呼ぶと、このサービスのIBinderが取得出来ます。
このサービスが別のプロセスならば、この章で述べたのとほぼ同じ仕組みで使う事になります（ただしAIDLで作成した別のプロセスのサービスにbindService()を呼び出すのは、
Androidでは推奨されていません https://developer.android.com/guide/components/bound-services.html ）。
システムサービスとSDKのサービスは、初めて見た時は良く区別が分からなくて、少し理解してくると全然違う物に見えて、
さらに深く理解するとまた区別が分からなくなる、そんな関係にあります。
最初の頃は区別して仕組みを学んでいき、一定以上理解が進んだら両者の区別はあまり気にしなくて良いんじゃないか、と私は思っています。

===[/column]
