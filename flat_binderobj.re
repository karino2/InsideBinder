={flat_binderobj}  binderドライバの内側とオブジェクトの送信 - flat_binder_object

//lead{

前節では、servicemanagerでサービスを検索する例を通じて、
binderドライバを利用して引数が文字列のみの簡単なメソッドを呼び出す手順を見てきました。

さて、サービスを検索するには、サービスが既に登録されていないといけません。
そこで次の話題としては当然、このサービスをどう登録するか、という事に移る訳ですが、
サービスを登録するのは、サービスを検索するよりも、一つだけ難しい所があります。
それは引数にサービスというオブジェクトが含まれてしまう、という事です。

サービスというオブジェクトを含んだ引数をどのように扱うか、というのが本節の主要なテーマとなります。
その為にはbinderドライバ内部で様々なデータをどう管理しているのか、という事を知る必要があります。

binderドライバ内部のデータ管理は、カーネルのメモリ空間で行われるカーネルモードでの話となります。
//}

== オブジェクトを送信すると何が起こるか？

詳細に入る前に、オブジェクトを送信すると何が起こるのか？という事の概要から始めたいと思います。

オブジェクトというのはメソッドという処理が含まれるので、転送する事が出来ません（少なくともBinderでは転送されません）。
ここで例示の為に、オブジェクトの存在しているプロセスをプロセスAと呼び、
それを送りつける先をプロセスBとします。

プロセスAからオブジェクトをドライバに渡すと、ドライバがこのオブジェクトのポインタを覚えておきます。
そしてプロセスBには、このポインタを表すハンドルを渡します。
Bにはオブジェクトを転送される訳では無くハンドルが渡されるのですが、ここでプロセスA内のポインタがドライバに保持される、というのが工夫です。

ポインタという物は概念的にはそのプロセスのアドレス空間内のアドレスです@<fn>{vrmem}
ですからプロセスAのポインタはプロセスAのアドレス空間上でないと有効ではありません。
ですがアドレスをドライバが記憶する事は出来ます（そのアドレスの中身を参照するのはちょっと大変ですが）。

//footnote[vrmem][ARMの場合厳密にはそれを参照するためのディスクリプタですが、この場合の議論は等しく有効です。]

そしてプロセスBがハンドルに対してメソッド呼び出しをしたら、それをプロセスAに転送するのですが、
この時は記憶したポインタのアドレスも戻してやる訳です。
プロセスAのioctlから戻った時というのは、アドレス空間はプロセスAの物となっているので、このポインタはそのまま有効で、
以前送信の時に引数で渡したポインタとなっている訳です。

//image[5_1_1][オブジェクトを送ると、プロキシが作られる]

//image[5_1_2][プロキシのメソッドを呼ぶとオブジェクトが呼ばれるメカニズム]

つまり、オブジェクトは転送されません。
プロセスAからオブジェクトを転送しようとしてもドライバの所で記憶されるだけで、実体はプロセスBには行かないのです。
でも身代わりとしてハンドルというのが代わりに届く事になります。

これがオブジェクトを送信する時に起こる事のイメージです。



== スレッドとプロセスのデータ構造 - binder_procとbinder_thread

binderドライバは、binderドライバを使用するプロセスに関しての情報を、binder_procというデータ構造で管理しています。
これはドライバをopenした時にカーネルによって作られるfile構造体のフィールドに格納されます。
ユーザープロセスの側から見れば、binderドライバをopenした時に返るファイルディスクリプタに格納されています。
ファイルディスクリプタはioctl呼び出しの第一引数で渡し続けるので、
このbinder_procもプロセス内で同じインスタンスが毎回ドライバに渡されます。
@#@ TODO: 図と説明でファイルオブジェクトがopenの都度作られる前提で書いているが後で本当にそうか確認。

また、スレッドを表すデータ構造もあります。
binderはメソッド呼び出しを前提としたプロセス間通信です。
メソッド呼び出しが単なるメッセージングと違うのは、結果が返る、という所です。

メソッド呼び出しを成立させるためには、BC_TRANSACTIONとBC_REPLYの二つのioctl呼び出しが必要です。
そしてBC_REPLYを送る時には対応するBC_TRANSACTIONを送ってきたスレッドに送信しないと、結果が呼び出したスレッドに返りません。
つまり、BC_TRANSACTIONを受け取る側が処理をしている間、送信元のスレッドを覚えておく必要があります。@<fn>{replycase}

//footnote[replycase][BC_REPLYの時にはtargetの指定は必要ありません。binderドライバが自動的に探してくれます。]

そこでbinderドライバは、ioctlを呼び出される都度、呼び出しスレッドに対応するデータ構造を作成して管理します。
このスレッドを表す構造体はbinder_threadという名前です。
binder_threadはbinder_procにツリーとして保持されます。@<fn>{rgbtree}

//footnote[rgbtree][Linuxカーネルが提供している赤黒木で保持されます。]


//image[5_2_1][binder_procとbinder_thread]

#@# TODO: この辺でwaitとwakeup的な話を混ぜたい。次のイテレーションで

ioctlが呼ばれる都度、呼び出し元のスレッドIDを見て、そのスレッドIDに対応するスレッド構造体が、
プロセス構造体に既に入っているかを検索します。無ければ新たに生成してツリーに追加します。
このように、スレッド構造体はioctlを呼び出す都度lazyに、しかも暗黙に作られます。
#@# TODO: 上記一文について、もう少し噛み砕いて（とくに「lazy」「暗黙に」をわかりやすく

こうして、ioctlを呼び出している各スレッドをドライバが管理しているので、
BR_TRANSACTIONを呼び出されている側のユーザープロセスが処理している間は、
呼び出し元のスレッド情報をドライバが覚えておいてくれるので、
その後にBC_REPLYをドライバに送ると、自動的に呼び出し元のスレッドを探し出してそのスレッドに返してくれます。

//image[5_2_2][binder_threadにREPLY先を記憶する]


===[column] プロセス構造体という呼び名とbinder_proc
Linuxカーネルでは、プロセス構造体と言う物があります。この本以外の本では、プロセス構造体と単に言ったら、
このLinuxカーネルのプロセス構造体を指すのが普通です。そしてbinderドライバが持つプロセスのデータを表す構造体は、
区別する為に構造体名であるbinder_procと呼ぶのが普通でしょう。
ですが、本章ではプロセス構造体と言ったらbinder_procを指します。
これは、このbinderドライバの周辺にはプロセス構造体の他にも大量の構造体名が出てくる為、
覚えなくてはならない構造体名を一つでも減らす為の工夫です。
私も初めてこの周辺を読んだ時は構造体名が多すぎて、すぐにどの構造体名が何だったのかを忘れてしまい苦労しました。
幸い、本章ではLinuxカーネルのプロセス構造体に言及する必要は無いので、
どちらか分からなくなる事は無いはずです。
もしLinuxカーネルに詳しいが為にかえってこの工夫がややこしい、という方が居たら、
心の中でこの章のプロセス構造体、という言葉を全部binder_procに置き換えて読んでください。
===[/column]


== オブジェクトの送信とflat_binder_object その1 - ユーザープロセス側

メソッド呼び出しの引数としてオブジェクトを渡す場合の話に入ります。

オブジェクトというのは生のポインタです。そのプロセス内のメモリ空間でだけ意味があります。
この生のポインタをbinderドライバに渡すと相手側ではハンドルとして渡ってきます。

まずは生のポインタが存在しているプロセス側で、送信する時のコードを見てみましょう。

binderドライバに生のポインタを渡す場合は、flat_binder_object構造体に入れて渡します。

サービスの取得の所、つまり受け取る側でも、結果はflat_binder_object構造体として返ってくる、という事を説明しました。(@<hd>{driver_message|サービスハンドルの取得の結果 - メッセージ受信})
送る側もこのflat_binder_objectという構造体を使います。

flat_binder_objectには型を表すフィールドがあり、そこには大きく三つの値が入ります。

 1. BINDER_TYPE_BINDER
 2. BINDER_TYPE_HANDLE
 3. BINDER_TYPE_FD

1がポインタ、2がハンドル、3がファイルディスクリプタです。

たとえばhandleという変数に入ったハンドルの値を保持するflat_binder_objectは以下のように作れます。

//list[fbo_handle][flat_binder_objectにハンドルを入れる場合]{
flat_binder_object obj;
obj.type = BINDER_TYPE_HANDLE;
obj.handle = handle;
//}

ptrというポインタ変数に入ったポインタを保持するflat_binder_objectなら以下のようになります。

//list[fbo_ptr][flat_binder_objectにポインタを入れる場合]{
flat_binder_object obj;
obj.type = BINDER_TYPE_BINDER;
obj.binder = ptr;
//}

//image[5_3_1][flat_binder_objectで送れる物三つ]

基本的にはこのflat_binder_objectを引数を表すバッファに入れて少し補助的な設定をした上でioctlを呼べば良い、という事になります。
引数というのはbinder_transaction_dataのptr.bufferに入れる、という話を@<hd>{driver_message|BC_TRANSACTIONコマンドとbinder_transaction_data}で行いましたが、
いろいろな場所に説明が飛ぶと読む方も大変だと思うので、
一部繰り返しになりますがもう一度ここで全体像を見てみましょう。

サービスのservicemanagerへの登録を例として、オブジェクトの送信を見ていきます。
MyServiceというクラスをサービスとして登録する場合を見ていきます。サービス名は"com.example.MyService"とします。

servicemanagerはハンドルが0という固定値でした。
この0というハンドルにBC_TRANSACTIONでSVC_MGR_ADD_SERVICEというメソッドを呼び出すことで、サービスの登録を行えます。(servicemanagerによるサービスハンドルの取得)

SVC_MGR_ADD_SERVICEは、引数として以下の三つを受け取ります

1. String16型の"android.os.IServiceManager"という固定文字列
2. String16型のサービス名、この場合は"com.example.MyService"
3. サービスのポインタ

1番目と2番目は適当なバイト配列にmemcpyしてやれば良い訳です。
3はflat_binder_objectにオブジェクトのポインタを詰めて、それをmemcpyしてやれば良いのですが、
flat_binder_objectの時にはもう一つ、オフセットの指示という作業が追加で必要となります。
というのは、それ以外の値は全てドライバとしてはバイト列をコピーするだけで良いので中を知っている必要は無いのですが、
flat_binder_objectだけはドライバが変換するので、中身を知っている必要があるのです。


まずはバイト配列を生成し、二つの固定文字列をバイト配列にコピーします。
詳細は省略します。

//list[twostrconst][バイト配列に二つの文字列をコピー]{
byte writedata[1024];
// "android.os.IServiceManager"のString16をwritedataにmemcpyする。省略
...
// "com.example.MyService"のString16をwritedataにmemcpyする。省略
//}

次にflat_binder_objectをmemcpyします。まずはflat_binder_objectの作成から。

//list[fbo_init][バッファにコピーするflat_binder_objectを生成]{
// MyServiceのインスタンスを生成
MyService *service = new MyService;

flat_binder_object obj;
// ポインタの時はタイプはBINDER_TYPE_BINDER
obj.type = BINDER_TYPE_BINDER;
// obj.binderにポインタを入れる
obj.binder = service;
//}

このようにflat_binder_objectという物を作って、writedataにmemcpyします。
なお、文字列二つをmemcpyした時にusedバイトまで既に使ったとします。@<fn>{mem_used}

//footnote[mem_used][usedの実際の値は4+2*sizeof("android.os.IServiceManager")+4+2*sizeof("com.exmaple.MyService")ですが、詳細を気にする必要は無いでしょう。]

//list[fbo_copy][生成したflat_binder_objectをバイト配列にコピー]{
// flat_binder_objectをwritedataのusedバイトより先に書き込む。
memcpy(&writedata[used], &obj, sizeof(flat_binder_object));

// 後で使うのでusedを進めておく。
used += sizeof(flat_binder_object);
//}

こうして出来たwritedataを@<hd>{driver_message|servicemanagerによるサービスハンドルの取得}と同様にbinder_transaction_dataに入れて、それを@<hd>{driver_message|SVC_MGR_CHECK_SERVICEを例に、ioctl呼び出しを復習する}と同様にbinder_write_readに入れれば良い訳ですが、
flat_binder_objectを渡す時は先ほども述べた通り、さらにオフセットの指定という事もやらなくてはいけません。

//image[5_3_2][flat_binder_objectとオフセットの書き方]

一気に三つの事が説明に出てきたので、一つずつ見ていきましょう。

まずはbinder_transaction_dataに上で作ったwritedataやターゲットとなるハンドル等を設定します。

//list[buf_copy_btr][binder_transaction_dataに送り先のハンドルを指定し、ここまで作ったバッファをセット]{
// binder_transaction_dataの設定
struct binder_transaction_data tr;

// servicemanagerのハンドルは0にハードコード
tr.target.handle = 0;

// 呼び出すメソッドのID。今回はサービスの登録なのでADD_SERVICE。
tr.code = SVC_MGR_ADD_SERVICE;

// 引数には上で作ったwritedataを設定
tr.data_size = used;
tr.data.ptr.buffer = writedata;
//}

送り先が0、メソッドIDがSVC_MGR_ADD_SERVICE、引数がwritedata、という訳です。
さらにこの引数データの中で、どこにflat_binder_objectがあるか、という情報も追加してやります。

//list[tr_fbo_offset][binder_transaction_dataの引数の中のうち、どこがflat_binder_objectかを表すoffsetを指定]{
// offsetsとして、今回は送り出すデータの中にはflat_binder_objectは一つなのでサイズは1
size_t offsets[1];
// flat_biner_objectをどこに書いたか。最後の引数なので末尾からsizeof(flat_binder_object)だけ戻った所にあるはず。
offsets[0] = used - sizeof(flat_binder_object);


// offsetsは、今回は長さ1の配列
tr.data.ofsset_size = 1;

// 上で設定したoffsetsを代入
tr.data.ptr.offsets = offsets;
//}

binderドライバにこのptr.bufferの中のこことこことここに変換の必要のあるflat_binder_objectが入っているよ、
と伝える為に、offsetsというメンバとoffset_sizeというメンバを設定します。
offsetsはsize_tの配列で、各要素がflat_binder_objectがptr.bufferの先頭からのオフセットに対応します。
offset_sizeはoffsets配列の長さです。

このようにして設定したbinder_transaction_dataをbuffer_write_readに詰めてBC_TRANSACTIONとしてioctlを呼び出します。

以後は前に説明したサービス取得のコードと全く同じコードになりますが、再掲しておきます。

//list[re_svc_recv][再掲: サービス取得のコード]{
// 送信用のデータのバッファ。コマンドIDと先ほど作ったbinder_transaction_dataを詰める。
byte writebuf[1024];
*((int*)writebuf) = BC_TRANSACTION
memcpy(&writebuf[4], &tr, sizeof(struct binder_transaction_data));

// 結果受け取りの為の受信用バッファ
byte readbuf[1024];

// binder_write_readに送信用と受信用のバッファを設定
struct binder_write_read bwr;

// 送信用データ。長さはコマンドIDのサイズ+binder_transaction_dataのサイズ
bwr.write_size = writebuf;
bwr.write_buffer = sizeof(int)+sizeof(struct binder_transaction_data);

// 受信用バッファ
bwr.read_size = 1024;
bwr.read_buffer = readbuf;

// ioctlでservicemanagerのSVC_MGR_ADD_SERVICEを呼び出す
res = ioctl(fd, BINDER_WRITE_READ, &bwr);
//}

このようにすると、MyServiceという生のポインタを引数にして、servicemanagerにサービスを登録する、というメソッドを呼び出した事になります。
それではこの生のポインタがbinderドライバではどう扱われてservicemanager側に渡るのか？という部分を見ていきましょう。


#@# 旧8.3.3
== オブジェクトの送信とflat_binder_object その2 - binderドライバと受信側

ioctl呼び出しの時にflat_binder_objectを渡すと、ドライバは内部でflat_binder_objectの中身の種類に合わせて適切に変換し、送り先に送ります。

BINDER_TYPE_BINDERのポインタは呼び出し元のメモリ空間でしか有効で無いので、
このポインタを渡されても別のプロセスは困ってしまいます。

@<hd>{スレッドとプロセスのデータ構造 - binder_procとbinder_thread}で解説したように、binderドライバをopenした時のファイルディスクリプタには、
そのプロセスの構造体が格納されています。

そしてこのプロセス構造体には、そのプロセスが保持するBINDER_TYPE_BINDERのポインタを格納するツリーがあります。
このツリーのノードを、binder_nodeと呼んでいます。ツリーになっているのは高速に検索する為です。@<fn>{rgbagain}

//footnote[rgbagain][これもLinuxカーネルの提供する赤黒木となっています。]

あるプロセスが保持しているサービスのポインタの一覧がツリーで管理されていると言いました。
さらに、そのプロセスが参照している外部のサービスの一覧もツリーで管理されています。
これはbinder_refというノードの表すツリーです。
このツリーのノードで、参照しているサービスが表されます。
binder_refのフィールドには、参照しているサービスが所属しているプロセス構造体と、
そのプロセス構造体にあるBINDER_TYPE_BINDERのポインタを参照するbinder_nodeが格納されます。

//image[5_4_1][binder_refが参照している物]

別の角度から同じ事を説明してみましょう。
別のプロセスにBINDER_TYPE_BINDERのポインタを渡す時を考えます。
ポインタが所属しているプロセスをA、送り先のプロセスをBとします。

//image[5_4_2][問題設定。オブジェクトをBに送信したい。]

その時には、以下の事が起こります

 1. Aのプロセス構造体のbinder_nodeツリーに、このポインタを表すノードを追加してポインタを格納
 2. Bのプロセス構造体にこのポインタへの参照を表すbinder_refのノードを追加し、1のノードとAのプロセス構造体を追加
 3. Bのbinder_refのツリーのノードに一意のintのIDを振って、そのIDをハンドルとしてプロセスBに渡す

この3のハンドルこそが、サービスの取得の時に得られたBINDER_TYPE_HANDLEのflat_binder_objectに格納されていた、
そしてbinder_transaction_dataのtarget.handleに格納する、そしてservicemanagerは0とハードコードされているハンドルです。

binder_refとハンドルの関係はちょっとわかりにくいですが、binder_refのノードに順番に1から自然数のIDを振っていて、そのIDがハンドルだ、というとだいたい正しい説明となります。
「だいたい正しい」というのは、途中でノードを削除したりすると抜け番が出来て、新しくノードを作る時にはそれを再利用する処理がある為です。
とにかく、binder_refのノードを一意に識別するint値がハンドルです。

ハンドルはbinder_refのノードを引くキーと言えます。内部実装を忘れれば、ハッシュのような物に格納されていてintのキーでlookup出来ると思っておいて問題ありません。
そしてそのキーがハンドルという訳です。@<fn>{ptr_rgb}

//footnote[ptr_rgb][厳密には内部ではポインタによる赤黒木とハンドル値による赤黒木の二本の木を持つ事でこの構造を実現しています。]

Bのプロセス内でのこのハンドルは、このプロセスBでのみ有効なint値です。
この値があれば、Bのプロセス構造体から素早くbinder_refを検索できます。
そしてbinder_refの中を見ると、このサービスを所持しているプロセス構造体と、このサービスのポインタを所持しているbinder_nodeが得られます。

以上がAからBを呼び出した場合です。

これを逆の順序で見ていくと、Bからこのハンドルに対してBC_TRANSACTIONした時に何が起こるのかが分かります。
BからAを呼び出す事を考えましょう。

//image[5_4_3][BからハンドルをターゲットにAのオブジェクトを呼び出す]

 1. プロセスBがハンドルをtargetにioctlを呼び出す
 2. ドライバがプロセスBのプロセス構造体のbinder_refツリーからハンドルの表すノードを高速に検索して取り出す
 3. binder_refのノードの中にある送り先のプロセス構造体とbinder_nodeを取り出す
 4. 送り先のプロセスのメモリ空間で有効なサービスへのポインタをbinder_nodeから取り出す
 5. binder_transaction_dataのtarget.ptrにこのポインタを詰める
 6. プロセスAのioctlから戻る

という手順になります。1で渡したハンドルが、binder_refを介してBのポインタになってBに渡る訳です。

target以外の所で、引数などにBINDER_TYPE_HANDLEのflat_binder_objectがある場合にも、ほぼ同じ作業が行われます。
唯一の違いは、flat_binder_objectは送り先ではタイプがBINDER_TYPE_BINDERに変更される事です。

ターゲットの場合は送り先のプロセスに必ずオブジェクトが存在するので型を表すフィールドは必要ないのですが、
引数の場合はそのプロセスには存在しない場合も存在するのでハンドルかポインタかを表すフィールドが必要になる訳です。

//image[5_4_4][ポインタが入る二つの場所]

ハンドルはプロセスBの中でしか有効ではありません。
例えばプロセスCがまたプロセスAの同じサービスに対して呼び出しを行う時には、
プロセスCにはプロセスBとは別のbinder_refツリーがあるので、
その辿った時のインデックスも別物となります。

//image[5_4_5][BとCでは別のハンドルの値となる]

こうして、ポインタを送るとそれを管理するbinder_nodeのツリーと、それを参照するbinder_refのツリーが双方のプロセスに作られて、
binder_refのノードを表すハンドルがioctlからは返る事になります。
そしてこのハンドルをターゲットにioctlを呼び出すと、受け取る側ではbinder_nodeからポインタを引いて、
ポインタに戻ってioctlから返ってくる事になります。

このようにして、binderドライバはまるでオブジェクトを送っているかのように見せかけています。

以上でオブジェクトの送信のメカニズムの説明が終わったので、任意のメソッドを呼び出す事が出来るようになりまし８た。
これでbinderドライバの主要なところは一通り解説した事になりますが、おまけとしてファイルディスクリプタの送信の話もしましょう。


== ファイルディスクリプタの送信とflat_binder_object - BINDER_TYPE_FDの場合

binderの特徴でファイルディスクリプタを別のプロセスに送れる、という物があります。
ここまで解説してしまうと大した話でも無いのですが、有名な話でもあるのでここでメカニズムを確認しておきます。

ファイルディスクリプタの話をする為にはLinuxのファイルの話を少しする必要があります。
Linuxの各プロセスには、自身のオープンしているファイルの一覧を保持するテーブルがあります。
このテーブルをファイルディスクリプタテーブルと言います。
このファイルディスクリプタテーブルの各エントリが、カーネルの管理するオープンしているファイルオブジェクトを指しています。
ファイルディスクリプタというのは、通常はこのプロセスごとのファイルディスクリプタテーブルの先頭からのインデックスです。

//image[1_3_1][ファイルディスクリプタとファイルディスクリプタテーブル、再掲]

さて、プロセスAでオープンしているファイル、fd1があったとします。
これをプロセスBに送る場合を考えます。

ファイルディスクリプタを送る場合も、BINDER_TYPE_BINDERでサービスを送る場合と同様に、
flat_binder_objectを使います。

//list[fbo_fd][ファイルディスクリプタもflat_binder_objectで送る]{
flat_binder_object obj;

// ファイルディスクリプタの時はタイプはBINDER_TYPE_FD
obj.type = BINDER_TYPE_FD;

// obj.handleにファイルディスクリプタを入れる
obj.handle = fd1;
//}

さて、こんなfalt_binder_objectを含んだデータをioctlに渡すと、
binderドライバは送り先のプロセスのファイルディスクリプタテーブルから空きを探して、
このファイルディスクリプタテーブルが指しているのと同じファイルのエントリを指すようにして、
この送り先のディスクリプタテーブルにhandleの値を書き換えます。

//image[1_3_2][ファイルディスクリプタを送る場合に起こる事、再掲]

そこのコードを抜き出すと以下のようになっています。

//list[fd_binderdrv][binderドライバ内でのファイルディスクリプタの処理]{
int target_fd;
struct file *file;

// pobjにはflat_binder_objectのポインタが入っているとする。
// カーネルAPI。オープンしているファイルオブジェクトを取ってリファレンスカウントを+1
file = fget(pobj->handle);

// target_procで表しているプロセスのファイルディスクリプタテーブルから空いているディスクリプタを取ってくる
target_fd = task_get_unused_fd_flags(target_proc, O_CLOEXEC);

// target_procのプロセスのファイルディスクリプタテーブルにカーネルのファイルオブジェクトを代入
task_fd_install(target_proc, target_fd, file);

// 送り先のハンドルに変換
pobj->handle = target_fd
//}

task_get_unused_fd_flags()関数とtask_fd_install()関数はbinderドライバで実装されている関数ですが、
名前の通りの事をしているだけなのでここではこれ以上は踏み込みません。

ポイントとしては、flat_binder_objectにBINDER_TYPE_FDとして開いているファイルのファイルディスクリプタを入れてbinder経由でサービスに送ると、
サービス側ではそのサービスの動くプロセス上のファイルディスクリプタテーブル上の同じファイルを指すエントリに自動的に変換してくれる、という事です。

この辺のコードはカーネルの内部構造をいろいろ触る割にはシンプルで読みやすいので、
カーネルの勉強をしたい人などは読んでみると面白いと思います。

以上でbinderドライバの説明は全て終わりました。

サービスは全て、binderドライバさえあれば実装出来ます。
ですが皆が全部をこのioctlで実装していくのは大変だし無駄なので、
このbinderドライバを使うライブラリがこの上に提供されています。

以下ではこのbinderドライバの上に作られているライブラリについて話をしていきます。


