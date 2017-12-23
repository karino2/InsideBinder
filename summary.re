= この章のまとめ

本章ではBinderについて扱いました。
@<chap>{main_intro}から@<chap>{flat_binderobj}までで、binderドライバとflat_binder_objectについて扱いました。
@<chap>{main_intro}ではbindarドライバの概要、@<chap>{systemcall}ではopen, mmap, ioctlと言ったシステムコールを、
@<chap>{driver_message}ではioctlによるメッセージの送受信の構造を扱い、
@<chap>{flat_binderobj}ではそのioctlを用いてflat_binder_objectを送信する時に何が起こるのかについて詳細に扱いました。

@<chap>{threadpool}ではそのbinderドライバを利用したスレッドプールのレイヤの実現について、
スレッドプールの実装であるIPCthreadStateやProcessStateについて説明し、
そのスレッドプールが想定するサービス実装であるBBinder基底クラスとその使い方について扱いました。
また、プロキシオブジェクトとは何か、という事やその実装に使うBpBinderについても説明しました。


@<chap>{common_intr}ではそのスレッドプールのレイヤのクラス達を用いて、
サービスが同じプロセスに居るか別のプロセスに居るかを吸収する為の共通インターフェースのレイヤとして、IInterfaceやinterface_castについて扱いました。

@<chap>{java_aidl}ではJavaでシステムサービスを実装する為の、AIDLによるインターフェース記述とその生成されるクラスについて、
ネイティブの共通インターフェースとの類似点を元に説明しました。

@<chap>{systemservices}では以上のbinderの仕組みが実際のサービスやアプリのプロセスでどのように使われているかについて、
SurfaceFlingerのmainを見たりActivityThreadとActivityManagerServiceのやり取りを見直す事で調べました。

本書はなかなか膨大な量になってしまいましたが、Binderについて必要な物は全て説明出来たと自負しています。
Binderについては公式のドキュメントがあまり無いので、全体像を解説した本章は、Binderを知りたい人にとってはなかなか貴重な文書だと思います。

