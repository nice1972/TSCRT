#!/usr/bin/env python3
"""
apply_translations.py

Populate the TSCRT Qt .ts files with ko/ja/zh translations for all
strings that have a known mapping below. Strings not in the dictionary
keep the `type="unfinished"` marker and fall back to the source language
at runtime.

Usage (from repo root):
    python translations/apply_translations.py

Re-run after `lupdate` to refresh translations without losing edits.
"""
import os
import re
import sys

TRANSLATIONS = {
    # key = source string (exactly as it appears in <source>...</source>)
    # value = dict with ko/ja/zh translations (any can be omitted -> unfinished)

    # ---- File menu ----
    "&amp;File":                     {"ko": "파일(&amp;F)",           "ja": "ファイル(&amp;F)",     "zh": "文件(&amp;F)"},
    "&amp;Close tab":                {"ko": "탭 닫기(&amp;C)",        "ja": "タブを閉じる(&amp;C)", "zh": "关闭标签(&amp;C)"},
    "&amp;Export":                   {"ko": "내보내기(&amp;E)",       "ja": "エクスポート(&amp;E)", "zh": "导出(&amp;E)"},
    "&amp;Import":                   {"ko": "가져오기(&amp;I)",       "ja": "インポート(&amp;I)",   "zh": "导入(&amp;I)"},
    "&amp;Full profile...":          {"ko": "전체 프로파일(&amp;F)...", "ja": "完全プロファイル(&amp;F)...", "zh": "完整配置(&amp;F)..."},
    "&amp;Sessions only...":         {"ko": "세션만(&amp;S)...",       "ja": "セッションのみ(&amp;S)...",   "zh": "仅会话(&amp;S)..."},
    "S&amp;napshots only...":        {"ko": "스냅샷만(&amp;N)...",     "ja": "スナップショットのみ(&amp;N)...", "zh": "仅快照(&amp;N)..."},
    "&amp;Logs":                     {"ko": "로그(&amp;L)",           "ja": "ログ(&amp;L)",          "zh": "日志(&amp;L)"},
    "&amp;Open log folder":          {"ko": "로그 폴더 열기(&amp;O)", "ja": "ログフォルダを開く(&amp;O)", "zh": "打开日志文件夹(&amp;O)"},
    "&amp;Log settings...":          {"ko": "로그 설정(&amp;L)...",   "ja": "ログ設定(&amp;L)...",       "zh": "日志设置(&amp;L)..."},
    "E&amp;xit":                     {"ko": "종료(&amp;X)",           "ja": "終了(&amp;X)",          "zh": "退出(&amp;X)"},

    # ---- Sessions menu ----
    "&amp;Sessions":                 {"ko": "세션(&amp;S)",           "ja": "セッション(&amp;S)",    "zh": "会话(&amp;S)"},
    "&amp;New":                      {"ko": "새로 만들기(&amp;N)",    "ja": "新規(&amp;N)",          "zh": "新建(&amp;N)"},
    "&amp;Sessions...":              {"ko": "세션 관리(&amp;S)...",   "ja": "セッション管理(&amp;S)...", "zh": "会话管理(&amp;S)..."},

    # ---- Snapshots menu ----
    "S&amp;napshots":                {"ko": "스냅샷(&amp;N)",         "ja": "スナップショット(&amp;N)", "zh": "快照(&amp;N)"},
    "&amp;Run on current session":   {"ko": "현재 세션에서 실행(&amp;R)", "ja": "現在のセッションで実行(&amp;R)", "zh": "在当前会话运行(&amp;R)"},
    "&amp;Manage snapshots...":      {"ko": "스냅샷 관리(&amp;M)...", "ja": "スナップショット管理(&amp;M)...", "zh": "管理快照(&amp;M)..."},
    "&amp;Automation rules...":      {"ko": "자동화 규칙(&amp;A)...", "ja": "自動化ルール(&amp;A)...",       "zh": "自动化规则(&amp;A)..."},
    "Open snapshot &amp;folder":     {"ko": "스냅샷 폴더 열기(&amp;F)", "ja": "スナップショットフォルダを開く(&amp;F)", "zh": "打开快照文件夹(&amp;F)"},
    "(no snapshots defined)":        {"ko": "(정의된 스냅샷 없음)",   "ja": "(スナップショット未定義)", "zh": "(未定义快照)"},
    "(no sessions defined)":         {"ko": "(정의된 세션 없음)",     "ja": "(セッション未定義)",      "zh": "(未定义会话)"},

    # ---- View menu ----
    "&amp;View":                     {"ko": "보기(&amp;V)",           "ja": "表示(&amp;V)",          "zh": "视图(&amp;V)"},
    "Show &amp;Command Line":        {"ko": "명령줄 표시(&amp;C)",    "ja": "コマンドラインを表示(&amp;C)", "zh": "显示命令行(&amp;C)"},
    "Show Action &amp;Buttons":      {"ko": "액션 버튼 표시(&amp;B)", "ja": "アクションボタンを表示(&amp;B)", "zh": "显示操作按钮(&amp;B)"},
    "Show &amp;Status Bar":          {"ko": "상태 표시줄 표시(&amp;S)", "ja": "ステータスバーを表示(&amp;S)", "zh": "显示状态栏(&amp;S)"},
    "&amp;Full Screen\tF11":         {"ko": "전체 화면(&amp;F)\tF11", "ja": "全画面(&amp;F)\tF11",    "zh": "全屏(&amp;F)\tF11"},

    # ---- Settings menu ----
    "Se&amp;ttings":                 {"ko": "설정(&amp;T)",           "ja": "設定(&amp;T)",          "zh": "设置(&amp;T)"},
    "&amp;Preferences...":           {"ko": "환경설정(&amp;P)...",    "ja": "環境設定(&amp;P)...",    "zh": "首选项(&amp;P)..."},
    "&amp;Language":                 {"ko": "언어(&amp;L)",           "ja": "言語(&amp;L)",          "zh": "语言(&amp;L)"},
    "&amp;Reload profile":           {"ko": "프로파일 다시 읽기(&amp;R)", "ja": "プロファイル再読込(&amp;R)", "zh": "重新加载配置(&amp;R)"},

    # ---- Help menu ----
    "&amp;Help":                     {"ko": "도움말(&amp;H)",         "ja": "ヘルプ(&amp;H)",        "zh": "帮助(&amp;H)"},
    "&amp;About TSCRT...":           {"ko": "TSCRT 정보(&amp;A)...",  "ja": "TSCRT について(&amp;A)...", "zh": "关于 TSCRT(&amp;A)..."},
    "About TSCRT":                   {"ko": "TSCRT 정보",             "ja": "TSCRT について",        "zh": "关于 TSCRT"},

    # ---- Status bar messages ----
    "Ready · libssh2 %1 · libvterm %2.%3":
        {"ko": "준비됨 · libssh2 %1 · libvterm %2.%3",
         "ja": "準備完了 · libssh2 %1 · libvterm %2.%3",
         "zh": "就绪 · libssh2 %1 · libvterm %2.%3"},
    "Connecting to %1...":           {"ko": "%1에 연결 중...",        "ja": "%1 に接続中...",        "zh": "正在连接 %1..."},
    "Connected: %1":                 {"ko": "연결됨: %1",             "ja": "接続済み: %1",          "zh": "已连接: %1"},
    "Disconnected: %1 (%2)":         {"ko": "연결 끊김: %1 (%2)",     "ja": "切断: %1 (%2)",         "zh": "已断开: %1 (%2)"},
    "Error: %1":                     {"ko": "오류: %1",               "ja": "エラー: %1",            "zh": "错误: %1"},
    "Session \"%1\" saved.":         {"ko": "세션 \"%1\" 저장됨.",    "ja": "セッション \"%1\" を保存しました。", "zh": "已保存会话 \"%1\"。"},
    "Sessions saved.":               {"ko": "세션이 저장되었습니다.", "ja": "セッションを保存しました。", "zh": "会话已保存。"},
    "Preferences saved.":            {"ko": "환경설정이 저장되었습니다.", "ja": "環境設定を保存しました。", "zh": "首选项已保存。"},
    "Log settings saved.":           {"ko": "로그 설정이 저장되었습니다.", "ja": "ログ設定を保存しました。", "zh": "日志设置已保存。"},
    "Snapshots saved.":              {"ko": "스냅샷이 저장되었습니다.", "ja": "スナップショットを保存しました。", "zh": "快照已保存。"},
    "Profile reloaded.":             {"ko": "프로파일을 다시 읽었습니다.", "ja": "プロファイルを再読込しました。", "zh": "配置已重新加载。"},
    "Copied session &quot;%1&quot;.":{"ko": "세션 &quot;%1&quot;을(를) 복사했습니다.", "ja": "セッション &quot;%1&quot; をコピーしました。", "zh": "已复制会话 &quot;%1&quot;。"},

    # ---- Dialogs: common ----
    "Session":                       {"ko": "세션",                   "ja": "セッション",            "zh": "会话"},
    "Sessions":                      {"ko": "세션",                   "ja": "セッション",            "zh": "会话"},
    "Snapshot":                      {"ko": "스냅샷",                 "ja": "スナップショット",      "zh": "快照"},
    "Snapshots":                     {"ko": "스냅샷",                 "ja": "スナップショット",      "zh": "快照"},
    "Preferences":                   {"ko": "환경설정",               "ja": "環境設定",              "zh": "首选项"},
    "Common":                        {"ko": "공통",                   "ja": "共通",                  "zh": "通用"},
    "Buttons":                       {"ko": "버튼",                   "ja": "ボタン",                "zh": "按钮"},
    "Startup":                       {"ko": "시작",                   "ja": "起動時",                "zh": "启动"},
    "Triggers":                      {"ko": "트리거",                 "ja": "トリガー",              "zh": "触发器"},
    "Periodic":                      {"ko": "주기적",                 "ja": "定期実行",              "zh": "定时"},
    "SMTP":                          {"ko": "SMTP",                   "ja": "SMTP",                  "zh": "SMTP"},
    "Log":                           {"ko": "로그",                   "ja": "ログ",                  "zh": "日志"},
    "Logs":                          {"ko": "로그",                   "ja": "ログ",                  "zh": "日志"},
    "Automation rules":              {"ko": "자동화 규칙",            "ja": "自動化ルール",          "zh": "自动化规则"},

    # ---- Form labels ----
    "Name":                          {"ko": "이름",                   "ja": "名前",                  "zh": "名称"},
    "Name:":                         {"ko": "이름:",                  "ja": "名前:",                 "zh": "名称:"},
    "New name:":                     {"ko": "새 이름:",               "ja": "新しい名前:",           "zh": "新名称:"},
    "Description:":                  {"ko": "설명:",                  "ja": "説明:",                 "zh": "说明:"},
    "Label":                         {"ko": "라벨",                   "ja": "ラベル",                "zh": "标签"},
    "Label:":                        {"ko": "라벨:",                  "ja": "ラベル:",               "zh": "标签:"},
    "Action":                        {"ko": "액션",                   "ja": "アクション",            "zh": "动作"},
    "Action:":                       {"ko": "액션:",                  "ja": "アクション:",           "zh": "动作:"},
    "Command":                       {"ko": "명령",                   "ja": "コマンド",              "zh": "命令"},
    "Command to repeat:":            {"ko": "반복할 명령:",           "ja": "繰り返すコマンド:",     "zh": "重复命令:"},
    "Pattern":                       {"ko": "패턴",                   "ja": "パターン",              "zh": "模式"},
    "Pattern (substring):":          {"ko": "패턴(부분 문자열):",     "ja": "パターン(部分一致):",   "zh": "模式(子串):"},
    "Type":                          {"ko": "유형",                   "ja": "種類",                  "zh": "类型"},
    "Kind":                          {"ko": "종류",                   "ja": "種別",                  "zh": "类别"},
    "Session:":                      {"ko": "세션:",                  "ja": "セッション:",           "zh": "会话:"},
    "Subject:":                      {"ko": "제목:",                  "ja": "件名:",                 "zh": "主题:"},
    "Password:":                     {"ko": "비밀번호:",              "ja": "パスワード:",           "zh": "密码:"},
    "Port:":                         {"ko": "포트:",                  "ja": "ポート:",               "zh": "端口:"},
    "Interval (s)":                  {"ko": "간격(초)",               "ja": "間隔(秒)",              "zh": "间隔(秒)"},
    "Interval (seconds):":           {"ko": "간격(초):",              "ja": "間隔(秒):",             "zh": "间隔(秒):"},
    "Delay (ms)":                    {"ko": "지연(ms)",               "ja": "遅延(ms)",              "zh": "延迟(ms)"},
    "Max wait (ms)":                 {"ko": "최대 대기(ms)",          "ja": "最大待機(ms)",          "zh": "最大等待(ms)"},
    "Cooldown (s)":                  {"ko": "쿨다운(초)",             "ja": "クールダウン(秒)",      "zh": "冷却(秒)"},
    "Cron expr":                     {"ko": "Cron 표현식",            "ja": "Cron 式",               "zh": "Cron 表达式"},
    "Prompt regex":                  {"ko": "프롬프트 정규식",        "ja": "プロンプト正規表現",    "zh": "提示符正则"},
    "Timeout:":                      {"ko": "타임아웃:",              "ja": "タイムアウト:",         "zh": "超时:"},
    "Security:":                     {"ko": "보안:",                  "ja": "セキュリティ:",         "zh": "安全:"},
    "SMTP host:":                    {"ko": "SMTP 호스트:",           "ja": "SMTP ホスト:",          "zh": "SMTP 主机:"},
    "Username:":                     {"ko": "사용자명:",              "ja": "ユーザー名:",           "zh": "用户名:"},
    "From address:":                 {"ko": "보내는 주소:",           "ja": "送信元アドレス:",       "zh": "发件人地址:"},
    "From name:":                    {"ko": "보내는 사람 이름:",      "ja": "送信者名:",             "zh": "发件人名称:"},
    "Scrollback:":                   {"ko": "스크롤백:",              "ja": "スクロールバック:",     "zh": "回滚:"},
    "Encoding:":                     {"ko": "인코딩:",                "ja": "エンコーディング:",     "zh": "编码:"},
    "Terminal type:":                {"ko": "터미널 유형:",           "ja": "端末種類:",             "zh": "终端类型:"},
    "Log directory:":                {"ko": "로그 디렉터리:",         "ja": "ログディレクトリ:",     "zh": "日志目录:"},
    "Log settings":                  {"ko": "로그 설정",              "ja": "ログ設定",              "zh": "日志设置"},
    "Host / Device":                 {"ko": "호스트 / 장치",          "ja": "ホスト / デバイス",     "zh": "主机 / 设备"},
    "Port / Baud":                   {"ko": "포트 / 보드율",          "ja": "ポート / ボーレート",   "zh": "端口 / 波特率"},
    "User":                          {"ko": "사용자",                 "ja": "ユーザー",              "zh": "用户"},

    # ---- Serial/SSH dialog labels ----
    "SSH":                           {"ko": "SSH",                    "ja": "SSH",                   "zh": "SSH"},
    "Serial":                        {"ko": "시리얼",                 "ja": "シリアル",              "zh": "串口"},
    "SSH Sessions":                  {"ko": "SSH 세션",               "ja": "SSH セッション",        "zh": "SSH 会话"},
    "Serial Sessions":               {"ko": "시리얼 세션",            "ja": "シリアルセッション",    "zh": "串口会话"},
    "&amp;Name:":                    {"ko": "이름(&amp;N):",          "ja": "名前(&amp;N):",         "zh": "名称(&amp;N):"},
    "&amp;Type:":                    {"ko": "유형(&amp;T):",          "ja": "種類(&amp;T):",         "zh": "类型(&amp;T):"},
    "&amp;Host:":                    {"ko": "호스트(&amp;H):",        "ja": "ホスト(&amp;H):",       "zh": "主机(&amp;H):"},
    "&amp;Port:":                    {"ko": "포트(&amp;P):",          "ja": "ポート(&amp;P):",       "zh": "端口(&amp;P):"},
    "&amp;Username:":                {"ko": "사용자명(&amp;U):",      "ja": "ユーザー名(&amp;U):",   "zh": "用户名(&amp;U):"},
    "Pass&amp;word:":                {"ko": "비밀번호(&amp;W):",      "ja": "パスワード(&amp;W):",   "zh": "密码(&amp;W):"},
    "&amp;Key file:":                {"ko": "키 파일(&amp;K):",       "ja": "鍵ファイル(&amp;K):",   "zh": "密钥文件(&amp;K):"},
    "&amp;Device:":                  {"ko": "장치(&amp;D):",          "ja": "デバイス(&amp;D):",     "zh": "设备(&amp;D):"},
    "&amp;Baud rate:":                {"ko": "보드율(&amp;B):",       "ja": "ボーレート(&amp;B):",   "zh": "波特率(&amp;B):"},
    "Data &amp;bits:":               {"ko": "데이터 비트(&amp;B):",   "ja": "データビット(&amp;B):", "zh": "数据位(&amp;B):"},
    "&amp;Stop bits:":               {"ko": "스톱 비트(&amp;S):",     "ja": "ストップビット(&amp;S):", "zh": "停止位(&amp;S):"},
    "Pa&amp;rity:":                  {"ko": "패리티(&amp;R):",        "ja": "パリティ(&amp;R):",     "zh": "校验(&amp;R):"},
    "&amp;Flow control:":            {"ko": "흐름 제어(&amp;F):",     "ja": "フロー制御(&amp;F):",   "zh": "流控(&amp;F):"},
    "None":                          {"ko": "없음",                   "ja": "なし",                  "zh": "无"},
    "Odd":                           {"ko": "홀수",                   "ja": "奇数",                  "zh": "奇"},
    "Even":                          {"ko": "짝수",                   "ja": "偶数",                  "zh": "偶"},
    "Hardware":                      {"ko": "하드웨어",               "ja": "ハードウェア",          "zh": "硬件"},
    "Software":                      {"ko": "소프트웨어",             "ja": "ソフトウェア",          "zh": "软件"},
    "SSH Password":                  {"ko": "SSH 비밀번호",           "ja": "SSH パスワード",        "zh": "SSH 密码"},
    "Password for %1@%2:":           {"ko": "%1@%2의 비밀번호:",      "ja": "%1@%2 のパスワード:",   "zh": "%1@%2 的密码:"},
    "Key file":                      {"ko": "키 파일",                "ja": "鍵ファイル",            "zh": "密钥文件"},
    "Key files (*.pem *.key);;All files (*)": {"ko": "키 파일 (*.pem *.key);;모든 파일 (*)", "ja": "鍵ファイル (*.pem *.key);;すべてのファイル (*)", "zh": "密钥文件 (*.pem *.key);;所有文件 (*)"},
    "Optional private key (OpenSSH .pem)": {"ko": "선택 개인 키 (OpenSSH .pem)", "ja": "オプション秘密鍵 (OpenSSH .pem)", "zh": "可选私钥 (OpenSSH .pem)"},
    "Select private key":            {"ko": "개인 키 선택",           "ja": "秘密鍵を選択",          "zh": "选择私钥"},

    # ---- Buttons / actions ----
    "&amp;Add":                      {"ko": "추가(&amp;A)",           "ja": "追加(&amp;A)",          "zh": "添加(&amp;A)"},
    "&amp;Add...":                   {"ko": "추가(&amp;A)...",        "ja": "追加(&amp;A)...",       "zh": "添加(&amp;A)..."},
    "&amp;Edit...":                  {"ko": "편집(&amp;E)...",        "ja": "編集(&amp;E)...",       "zh": "编辑(&amp;E)..."},
    "&amp;Delete":                   {"ko": "삭제(&amp;D)",           "ja": "削除(&amp;D)",          "zh": "删除(&amp;D)"},
    "&amp;Copy":                     {"ko": "복사(&amp;C)",           "ja": "コピー(&amp;C)",        "zh": "复制(&amp;C)"},
    "&amp;Paste":                    {"ko": "붙여넣기(&amp;P)",       "ja": "貼り付け(&amp;P)",      "zh": "粘贴(&amp;P)"},
    "Add":                           {"ko": "추가",                   "ja": "追加",                  "zh": "添加"},
    "Edit":                          {"ko": "편집",                   "ja": "編集",                  "zh": "编辑"},
    "Edit...":                       {"ko": "편집...",                "ja": "編集...",               "zh": "编辑..."},
    "Delete":                        {"ko": "삭제",                   "ja": "削除",                  "zh": "删除"},
    "Copy":                          {"ko": "복사",                   "ja": "コピー",                "zh": "复制"},
    "Paste":                         {"ko": "붙여넣기",               "ja": "貼り付け",              "zh": "粘贴"},
    "Rename":                        {"ko": "이름 바꾸기",            "ja": "名前を変更",            "zh": "重命名"},
    "Rename...":                     {"ko": "이름 바꾸기...",         "ja": "名前を変更...",         "zh": "重命名..."},
    "Rename Tab":                    {"ko": "탭 이름 바꾸기",         "ja": "タブ名を変更",          "zh": "重命名标签"},
    "Rename session":                {"ko": "세션 이름 바꾸기",       "ja": "セッション名を変更",    "zh": "重命名会话"},
    "Duplicate":                     {"ko": "복제",                   "ja": "複製",                  "zh": "复制"},
    "Pin":                           {"ko": "고정",                   "ja": "ピン留め",              "zh": "固定"},
    "Unpin":                         {"ko": "고정 해제",              "ja": "ピン留め解除",          "zh": "取消固定"},
    "Close":                         {"ko": "닫기",                   "ja": "閉じる",                "zh": "关闭"},
    "Open":                          {"ko": "열기",                   "ja": "開く",                  "zh": "打开"},
    "Browse...":                     {"ko": "찾아보기...",            "ja": "参照...",               "zh": "浏览..."},
    "Add command":                   {"ko": "명령 추가",              "ja": "コマンド追加",          "zh": "添加命令"},
    "Delete command":                {"ko": "명령 삭제",              "ja": "コマンド削除",          "zh": "删除命令"},
    "Send test email...":            {"ko": "테스트 메일 보내기...",  "ja": "テストメール送信...",   "zh": "发送测试邮件..."},
    "Send test email to:":           {"ko": "테스트 메일 받을 주소:", "ja": "テストメール送信先:",   "zh": "测试邮件接收人:"},
    "Clear &amp;screen":             {"ko": "화면 지우기(&amp;S)",    "ja": "画面クリア(&amp;S)",    "zh": "清屏(&amp;S)"},

    # ---- Session Manager ----
    "Session Manager":               {"ko": "세션 관리자",            "ja": "セッションマネージャー", "zh": "会话管理器"},
    "Save session":                  {"ko": "세션 저장",              "ja": "セッション保存",        "zh": "保存会话"},
    "Save session log":              {"ko": "세션 로그 저장",         "ja": "セッションログ保存",    "zh": "保存会话日志"},
    "Save failed":                   {"ko": "저장 실패",              "ja": "保存失敗",              "zh": "保存失败"},
    "Delete session":                {"ko": "세션 삭제",              "ja": "セッション削除",        "zh": "删除会话"},
    "Delete session &quot;%1&quot;?":{"ko": "세션 &quot;%1&quot;을(를) 삭제하시겠습니까?", "ja": "セッション &quot;%1&quot; を削除しますか?", "zh": "删除会话 &quot;%1&quot;?"},
    "Session error":                 {"ko": "세션 오류",              "ja": "セッションエラー",      "zh": "会话错误"},
    "Session name is required.":     {"ko": "세션 이름이 필요합니다.", "ja": "セッション名が必要です。", "zh": "需要会话名称。"},
    "Session log directory":         {"ko": "세션 로그 디렉터리",     "ja": "セッションログディレクトリ", "zh": "会话日志目录"},
    "Session logs (and snapshot captures) are saved under the directory below. Each session writes its own timestamped file; snapshots go into a &lt;b&gt;snapshots/&lt;/b&gt; subfolder.":
        {"ko": "세션 로그(와 스냅샷 캡처)는 아래 디렉터리에 저장됩니다. 각 세션은 타임스탬프가 붙은 파일을 기록하며, 스냅샷은 &lt;b&gt;snapshots/&lt;/b&gt; 하위 폴더에 저장됩니다.",
         "ja": "セッションログ(およびスナップショット)は以下のディレクトリに保存されます。各セッションはタイムスタンプ付きファイルを作成し、スナップショットは &lt;b&gt;snapshots/&lt;/b&gt; サブフォルダに保存されます。",
         "zh": "会话日志(和快照)保存在下面的目录中。每个会话写入带时间戳的文件,快照保存在 &lt;b&gt;snapshots/&lt;/b&gt; 子文件夹中。"},

    # ---- Snapshot dialog ----
    "e.g. TSCRT {session} {snapshot} {timestamp}": {"ko": "예: TSCRT {session} {snapshot} {timestamp}", "ja": "例: TSCRT {session} {snapshot} {timestamp}", "zh": "例: TSCRT {session} {snapshot} {timestamp}"},
    "Send email when finished":      {"ko": "완료 시 메일 전송",      "ja": "完了時にメール送信",    "zh": "完成时发送邮件"},
    "Attach as file (otherwise inline)": {"ko": "파일로 첨부(그렇지 않으면 본문)", "ja": "ファイルとして添付(そうでなければ本文)", "zh": "作为文件附加(否则内联)"},
    "Commands (sent in order; escape sequences: \\r \\n \\t \\e):":
        {"ko": "명령 (순서대로 전송; 이스케이프: \\r \\n \\t \\e):",
         "ja": "コマンド (順番に送信; エスケープ: \\r \\n \\t \\e):",
         "zh": "命令 (按顺序发送; 转义: \\r \\n \\t \\e):"},
    "Recipients (one per line):":    {"ko": "수신자 (한 줄에 하나):", "ja": "宛先 (1 行 1 件):",     "zh": "收件人 (每行一个):"},
    "Rules fire a registered snapshot automatically. Kinds: on_connect, cron, pattern. Leave Session blank to apply to every session.":
        {"ko": "규칙은 등록된 스냅샷을 자동으로 실행합니다. 종류: on_connect, cron, pattern. 세션을 비워 두면 모든 세션에 적용됩니다.",
         "ja": "ルールは登録されたスナップショットを自動実行します。種別: on_connect, cron, pattern. セッションを空欄にするとすべてのセッションに適用されます。",
         "zh": "规则自动运行已注册的快照。类型: on_connect, cron, pattern. 留空会话则应用于所有会话。"},
    "Run Snapshot":                  {"ko": "스냅샷 실행",            "ja": "スナップショット実行", "zh": "运行快照"},

    # ---- SMTP ----
    "STARTTLS":                      {"ko": "STARTTLS",               "ja": "STARTTLS",              "zh": "STARTTLS"},
    "SMTPS (implicit TLS)":          {"ko": "SMTPS (암시적 TLS)",     "ja": "SMTPS (暗黙的 TLS)",    "zh": "SMTPS (隐式 TLS)"},
    "SMTP test":                     {"ko": "SMTP 테스트",            "ja": "SMTP テスト",           "zh": "SMTP 测试"},
    "SMTP client is busy":           {"ko": "SMTP 클라이언트 사용 중", "ja": "SMTP クライアント使用中", "zh": "SMTP 客户端忙"},
    "SMTP host is not configured":   {"ko": "SMTP 호스트가 설정되지 않음", "ja": "SMTP ホストが未設定", "zh": "SMTP 主机未配置"},
    "SMTP host not configured — email skipped":
        {"ko": "SMTP 호스트가 설정되지 않음 — 메일 전송 생략",
         "ja": "SMTP ホスト未設定 — メール送信をスキップ",
         "zh": "SMTP 主机未配置 — 跳过邮件"},
    "SMTP timeout":                  {"ko": "SMTP 타임아웃",          "ja": "SMTP タイムアウト",     "zh": "SMTP 超时"},
    "No recipients":                 {"ko": "수신자 없음",            "ja": "宛先なし",              "zh": "无收件人"},
    "Fill in host and From address first.": {"ko": "호스트와 보내는 주소를 먼저 입력하세요.", "ja": "ホストと送信元アドレスを先に入力してください。", "zh": "请先填写主机和发件人地址。"},
    "Test email sent to %1":         {"ko": "테스트 메일이 %1로 전송됨", "ja": "テストメールを %1 に送信しました", "zh": "测试邮件已发送至 %1"},
    "Send failed: %1":               {"ko": "전송 실패: %1",          "ja": "送信失敗: %1",          "zh": "发送失败: %1"},
    "Email sent: %1":                {"ko": "메일 전송됨: %1",        "ja": "メール送信: %1",        "zh": "邮件已发送: %1"},
    "Email failed: %1":              {"ko": "메일 실패: %1",          "ja": "メール失敗: %1",        "zh": "邮件失败: %1"},

    # ---- Import/export ----
    "Export":                        {"ko": "내보내기",               "ja": "エクスポート",          "zh": "导出"},
    "Import":                        {"ko": "가져오기",               "ja": "インポート",            "zh": "导入"},
    "Export profile":                {"ko": "프로파일 내보내기",      "ja": "プロファイルエクスポート", "zh": "导出配置"},
    "Import profile":                {"ko": "프로파일 가져오기",      "ja": "プロファイルインポート", "zh": "导入配置"},
    "Export sessions":               {"ko": "세션 내보내기",          "ja": "セッションエクスポート", "zh": "导出会话"},
    "Import sessions":               {"ko": "세션 가져오기",          "ja": "セッションインポート",  "zh": "导入会话"},
    "Export snapshots":              {"ko": "스냅샷 내보내기",        "ja": "スナップショットエクスポート", "zh": "导出快照"},
    "Import snapshots":              {"ko": "스냅샷 가져오기",        "ja": "スナップショットインポート", "zh": "导入快照"},
    "Exported: %1":                  {"ko": "내보냄: %1",             "ja": "エクスポート: %1",      "zh": "已导出: %1"},
    "TSCRT profile (*.profile);;All files (*)": {"ko": "TSCRT 프로파일 (*.profile);;모든 파일 (*)", "ja": "TSCRT プロファイル (*.profile);;すべてのファイル (*)", "zh": "TSCRT 配置 (*.profile);;所有文件 (*)"},

    # ---- Fullscreen / per-session toggles ----
    "Show command line in fullscreen": {"ko": "전체 화면에서 명령줄 표시", "ja": "全画面でコマンドラインを表示", "zh": "全屏时显示命令行"},
    "Show button bar in fullscreen":   {"ko": "전체 화면에서 버튼 바 표시", "ja": "全画面でボタンバーを表示",    "zh": "全屏时显示按钮栏"},

    # ---- Misc common messages ----
    "Limit":                         {"ko": "한도",                   "ja": "上限",                  "zh": "上限"},
    "Limit reached":                 {"ko": "한도 도달",              "ja": "上限に達しました",      "zh": "已达上限"},
    "Invalid":                       {"ko": "잘못됨",                 "ja": "無効",                  "zh": "无效"},
    "Profile error":                 {"ko": "프로파일 오류",          "ja": "プロファイルエラー",    "zh": "配置错误"},
    "Cannot initialize profile directory.": {"ko": "프로파일 디렉터리를 초기화할 수 없습니다.", "ja": "プロファイルディレクトリを初期化できません。", "zh": "无法初始化配置目录。"},
    "Failed to load profile; defaults will be used.": {"ko": "프로파일 로드 실패. 기본값을 사용합니다.", "ja": "プロファイル読込失敗。既定値を使用します。", "zh": "加载配置失败,将使用默认值。"},
    "Name cannot be empty.":         {"ko": "이름이 비어 있을 수 없습니다.", "ja": "名前を空にできません。", "zh": "名称不能为空。"},
    "Edit button":                   {"ko": "버튼 편집",              "ja": "ボタン編集",            "zh": "编辑按钮"},
    "Edit button...":                {"ko": "버튼 편집...",           "ja": "ボタン編集...",         "zh": "编辑按钮..."},
    "Action (escape: \\r \\n \\t \\b \\e \\p \\\\):":
        {"ko": "액션 (이스케이프: \\r \\n \\t \\b \\e \\p \\\\):",
         "ja": "アクション (エスケープ: \\r \\n \\t \\b \\e \\p \\\\):",
         "zh": "动作 (转义: \\r \\n \\t \\b \\e \\p \\\\):"},
    "Bottom-bar buttons. Escape sequences: \\r \\n \\t \\b \\e \\p \\\\":
        {"ko": "하단 바 버튼. 이스케이프 시퀀스: \\r \\n \\t \\b \\e \\p \\\\",
         "ja": "下部バーボタン。エスケープシーケンス: \\r \\n \\t \\b \\e \\p \\\\",
         "zh": "底部栏按钮。转义序列: \\r \\n \\t \\b \\e \\p \\\\"},
    "Tab name:":                     {"ko": "탭 이름:",               "ja": "タブ名:",               "zh": "标签名:"},
    "Maximum number of sessions (%1) reached.": {"ko": "최대 세션 수(%1)에 도달했습니다.", "ja": "最大セッション数 (%1) に達しました。", "zh": "已达到最大会话数 (%1)。"},
    "Maximum snapshot count reached.": {"ko": "최대 스냅샷 수에 도달했습니다.", "ja": "最大スナップショット数に達しました。", "zh": "已达到最大快照数。"},
    "Maximum periodic entries reached.": {"ko": "최대 주기 항목 수에 도달했습니다.", "ja": "最大定期実行数に達しました。", "zh": "已达到最大定时项目数。"},
    "Maximum startup entries reached.": {"ko": "최대 시작 항목 수에 도달했습니다.", "ja": "最大起動項目数に達しました。", "zh": "已达到最大启动项目数。"},
    "Maximum trigger entries reached.": {"ko": "최대 트리거 수에 도달했습니다.", "ja": "最大トリガー数に達しました。", "zh": "已达到最大触发器数。"},
    "Trigger":                       {"ko": "트리거",                 "ja": "トリガー",              "zh": "触发器"},
    "Mark":                          {"ko": "마크",                   "ja": "マーク",                "zh": "标记"},
    "Loop":                          {"ko": "루프",                   "ja": "ループ",                "zh": "循环"},
    "Highlight pattern (empty to clear):": {"ko": "강조 패턴 (비우면 해제):", "ja": "強調パターン (空で解除):", "zh": "高亮模式 (留空清除):"},
    "loop":                          {"ko": "루프",                   "ja": "ループ",                "zh": "循环"},
    "mark":                          {"ko": "마크",                   "ja": "マーク",                "zh": "标记"},
    "loop ●":                        {"ko": "루프 ●",                 "ja": "ループ ●",              "zh": "循环 ●"},
    "mark ●":                        {"ko": "마크 ●",                 "ja": "マーク ●",              "zh": "标记 ●"},
    "on":                            {"ko": "켜짐",                   "ja": "オン",                  "zh": "开"},
    "off":                           {"ko": "꺼짐",                   "ja": "オフ",                  "zh": "关"},
    "?":                             {"ko": "?",                      "ja": "?",                     "zh": "?"},

    # ---- TSCRT help / crash / misc ----
    "TSCRT help":                    {"ko": "TSCRT 도움말",           "ja": "TSCRT ヘルプ",          "zh": "TSCRT 帮助"},
    "TSCRT - Crash report":          {"ko": "TSCRT - 크래시 보고",    "ja": "TSCRT - クラッシュレポート", "zh": "TSCRT - 崩溃报告"},
    "A crash report from a previous run was found:\n%1\n\nWould you like to keep it for inspection?":
        {"ko": "이전 실행의 크래시 보고서가 발견되었습니다:\n%1\n\n검토를 위해 보관하시겠습니까?",
         "ja": "前回の実行のクラッシュレポートが見つかりました:\n%1\n\n調査のため保持しますか?",
         "zh": "发现上次运行的崩溃报告:\n%1\n\n是否保留以便检查?"},

    # ---- Terminal-specific ----
    "Interactive terminal session":  {"ko": "대화형 터미널 세션",     "ja": "対話型端末セッション",  "zh": "交互式终端会话"},
    "Terminal display":              {"ko": "터미널 표시",            "ja": "端末表示",              "zh": "终端显示"},
    "Type a command and press Enter…": {"ko": "명령을 입력하고 Enter를 누르세요…", "ja": "コマンドを入力して Enter を押してください…", "zh": "输入命令并按回车…"},

    # ---- Snapshot status messages ----
    "Snapshot \"%1\" started on %2": {"ko": "스냅샷 \"%1\" 이(가) %2에서 시작됨", "ja": "スナップショット \"%1\" を %2 で開始", "zh": "快照 \"%1\" 已在 %2 启动"},
    "Snapshot \"%1\" not found":     {"ko": "스냅샷 \"%1\" 을(를) 찾을 수 없음", "ja": "スナップショット \"%1\" が見つかりません", "zh": "未找到快照 \"%1\""},
    "Snapshot saved: %1":            {"ko": "스냅샷 저장: %1",        "ja": "スナップショット保存: %1", "zh": "快照已保存: %1"},
    "Snapshot failed: %1":           {"ko": "스냅샷 실패: %1",        "ja": "スナップショット失敗: %1", "zh": "快照失败: %1"},

    # ---- Display strings ----
    "SSH · %1 (%2@%3:%4)":           {"ko": "SSH · %1 (%2@%3:%4)",    "ja": "SSH · %1 (%2@%3:%4)",   "zh": "SSH · %1 (%2@%3:%4)"},
    "Serial · %1 (%2 %3 baud)":      {"ko": "시리얼 · %1 (%2 %3 baud)", "ja": "シリアル · %1 (%2 %3 baud)", "zh": "串口 · %1 (%2 %3 baud)"},
    " s":                            {"ko": " 초",                    "ja": " 秒",                   "zh": " 秒"},
}


def apply_to_file(path: str, lang_code: str) -> int:
    with open(path, encoding="utf-8") as f:
        content = f.read()

    count = 0

    def repl(match: re.Match) -> str:
        nonlocal count
        block = match.group(0)
        src_match = re.search(r"<source>(.*?)</source>", block, re.DOTALL)
        if not src_match:
            return block
        src = src_match.group(1)
        entry = TRANSLATIONS.get(src)
        if not entry or lang_code not in entry:
            return block
        tr = entry[lang_code]
        replacement = f"<translation>{tr}</translation>"
        # Callback form avoids Python re's backslash-escape interpretation
        # in the replacement template — translations contain \r \n \t \e.
        new_block = re.sub(
            r'<translation[^>]*>.*?</translation>',
            lambda _m: replacement,
            block,
            count=1,
            flags=re.DOTALL,
        )
        if new_block != block:
            count += 1
        return new_block

    content = re.sub(r"<message>.*?</message>", repl, content, flags=re.DOTALL)

    with open(path, "w", encoding="utf-8") as f:
        f.write(content)

    return count


def main():
    root = os.path.dirname(os.path.abspath(__file__))
    targets = [
        ("tscrt_win_ko.ts", "ko"),
        ("tscrt_win_ja.ts", "ja"),
        ("tscrt_win_zh.ts", "zh"),
    ]
    for fname, lang in targets:
        path = os.path.join(root, fname)
        if not os.path.exists(path):
            print(f"skip: {path} (missing)")
            continue
        n = apply_to_file(path, lang)
        print(f"{fname}: {n} strings translated")


if __name__ == "__main__":
    main()
