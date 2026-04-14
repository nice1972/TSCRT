#!/usr/bin/env python3
"""
apply_translations.py

Populate the TSCRT Qt .ts files with translations for all strings that
have a known mapping below. Strings not in the dictionary keep the
`type="unfinished"` marker and fall back to the source language at
runtime.

Supported target languages: ko, ja, zh, de, es, fr, it, pt, ru.

Usage (from repo root):
    python translations/apply_translations.py

Re-run after `lupdate` to refresh translations without losing edits.
"""
import os
import re
import sys

TRANSLATIONS = {
    # key = source string (exactly as it appears in <source>...</source>)
    # value = dict with per-language translations (any can be omitted -> unfinished)

    # ---- File menu ----
    "&amp;File": {
        "ko": "파일(&amp;F)", "ja": "ファイル(&amp;F)", "zh": "文件(&amp;F)",
        "de": "&amp;Datei", "es": "&amp;Archivo", "fr": "&amp;Fichier",
        "it": "&amp;File", "pt": "&amp;Arquivo", "ru": "&amp;Файл",
    },
    "&amp;Close tab": {
        "ko": "탭 닫기(&amp;C)", "ja": "タブを閉じる(&amp;C)", "zh": "关闭标签(&amp;C)",
        "de": "Tab &amp;schließen", "es": "&amp;Cerrar pestaña", "fr": "&amp;Fermer l'onglet",
        "it": "&amp;Chiudi scheda", "pt": "&amp;Fechar aba", "ru": "&amp;Закрыть вкладку",
    },
    "&amp;Export": {
        "ko": "내보내기(&amp;E)", "ja": "エクスポート(&amp;E)", "zh": "导出(&amp;E)",
        "de": "&amp;Exportieren", "es": "&amp;Exportar", "fr": "&amp;Exporter",
        "it": "&amp;Esporta", "pt": "&amp;Exportar", "ru": "&amp;Экспорт",
    },
    "&amp;Import": {
        "ko": "가져오기(&amp;I)", "ja": "インポート(&amp;I)", "zh": "导入(&amp;I)",
        "de": "&amp;Importieren", "es": "&amp;Importar", "fr": "&amp;Importer",
        "it": "&amp;Importa", "pt": "&amp;Importar", "ru": "&amp;Импорт",
    },
    "&amp;Full profile...": {
        "ko": "전체 프로파일(&amp;F)...", "ja": "完全プロファイル(&amp;F)...", "zh": "完整配置(&amp;F)...",
        "de": "&amp;Vollständiges Profil...", "es": "Perfil &amp;completo...", "fr": "Profil &amp;complet...",
        "it": "Profilo &amp;completo...", "pt": "Perfil &amp;completo...", "ru": "&amp;Полный профиль...",
    },
    "&amp;Sessions only...": {
        "ko": "세션만(&amp;S)...", "ja": "セッションのみ(&amp;S)...", "zh": "仅会话(&amp;S)...",
        "de": "Nur &amp;Sitzungen...", "es": "Solo &amp;sesiones...", "fr": "&amp;Sessions uniquement...",
        "it": "Solo &amp;sessioni...", "pt": "Somente &amp;sessões...", "ru": "Только &amp;сеансы...",
    },
    "S&amp;napshots only...": {
        "ko": "스냅샷만(&amp;N)...", "ja": "スナップショットのみ(&amp;N)...", "zh": "仅快照(&amp;N)...",
        "de": "Nur S&amp;chnappschüsse...", "es": "Solo i&amp;nstantáneas...", "fr": "I&amp;nstantanés uniquement...",
        "it": "Solo i&amp;stantanee...", "pt": "Somente i&amp;nstantâneos...", "ru": "Только с&amp;нимки...",
    },
    "&amp;Logs": {
        "ko": "로그(&amp;L)", "ja": "ログ(&amp;L)", "zh": "日志(&amp;L)",
        "de": "&amp;Protokolle", "es": "&amp;Registros", "fr": "&amp;Journaux",
        "it": "&amp;Log", "pt": "&amp;Registros", "ru": "&amp;Журналы",
    },
    "&amp;Open log folder": {
        "ko": "로그 폴더 열기(&amp;O)", "ja": "ログフォルダを開く(&amp;O)", "zh": "打开日志文件夹(&amp;O)",
        "de": "Protokoll&amp;ordner öffnen", "es": "&amp;Abrir carpeta de registros", "fr": "&amp;Ouvrir le dossier des journaux",
        "it": "&amp;Apri cartella log", "pt": "&amp;Abrir pasta de registros", "ru": "&amp;Открыть папку журналов",
    },
    "&amp;Log settings...": {
        "ko": "로그 설정(&amp;L)...", "ja": "ログ設定(&amp;L)...", "zh": "日志设置(&amp;L)...",
        "de": "Protoko&amp;lleinstellungen...", "es": "Con&amp;figuración de registros...", "fr": "&amp;Paramètres des journaux...",
        "it": "Impostazioni &amp;log...", "pt": "Configuração de &amp;registros...", "ru": "Настройки &amp;журналов...",
    },
    "E&amp;xit": {
        "ko": "종료(&amp;X)", "ja": "終了(&amp;X)", "zh": "退出(&amp;X)",
        "de": "&amp;Beenden", "es": "&amp;Salir", "fr": "&amp;Quitter",
        "it": "&amp;Esci", "pt": "&amp;Sair", "ru": "В&amp;ыход",
    },

    # ---- Sessions menu ----
    "&amp;Sessions": {
        "ko": "세션(&amp;S)", "ja": "セッション(&amp;S)", "zh": "会话(&amp;S)",
        "de": "&amp;Sitzungen", "es": "&amp;Sesiones", "fr": "&amp;Sessions",
        "it": "&amp;Sessioni", "pt": "&amp;Sessões", "ru": "&amp;Сеансы",
    },
    "&amp;New": {
        "ko": "새로 만들기(&amp;N)", "ja": "新規(&amp;N)", "zh": "新建(&amp;N)",
        "de": "&amp;Neu", "es": "&amp;Nuevo", "fr": "&amp;Nouveau",
        "it": "&amp;Nuovo", "pt": "&amp;Novo", "ru": "&amp;Создать",
    },
    "&amp;Sessions...": {
        "ko": "세션 관리(&amp;S)...", "ja": "セッション管理(&amp;S)...", "zh": "会话管理(&amp;S)...",
        "de": "&amp;Sitzungen...", "es": "&amp;Sesiones...", "fr": "&amp;Sessions...",
        "it": "&amp;Sessioni...", "pt": "&amp;Sessões...", "ru": "&amp;Сеансы...",
    },

    # ---- Snapshots menu ----
    "S&amp;napshots": {
        "ko": "스냅샷(&amp;N)", "ja": "スナップショット(&amp;N)", "zh": "快照(&amp;N)",
        "de": "Sch&amp;nappschüsse", "es": "I&amp;nstantáneas", "fr": "I&amp;nstantanés",
        "it": "I&amp;stantanee", "pt": "I&amp;nstantâneos", "ru": "С&amp;нимки",
    },
    "&amp;Run on current session": {
        "ko": "현재 세션에서 실행(&amp;R)", "ja": "現在のセッションで実行(&amp;R)", "zh": "在当前会话运行(&amp;R)",
        "de": "In aktueller Sitzung &amp;ausführen", "es": "&amp;Ejecutar en la sesión actual", "fr": "&amp;Exécuter sur la session courante",
        "it": "&amp;Esegui sulla sessione corrente", "pt": "&amp;Executar na sessão atual", "ru": "&amp;Запустить в текущем сеансе",
    },
    "&amp;Manage snapshots...": {
        "ko": "스냅샷 관리(&amp;M)...", "ja": "スナップショット管理(&amp;M)...", "zh": "管理快照(&amp;M)...",
        "de": "Schnappschüsse &amp;verwalten...", "es": "&amp;Gestionar instantáneas...", "fr": "&amp;Gérer les instantanés...",
        "it": "&amp;Gestisci istantanee...", "pt": "&amp;Gerenciar instantâneos...", "ru": "&amp;Управление снимками...",
    },
    "&amp;Automation rules...": {
        "ko": "자동화 규칙(&amp;A)...", "ja": "自動化ルール(&amp;A)...", "zh": "自动化规则(&amp;A)...",
        "de": "&amp;Automatisierungsregeln...", "es": "Reglas de &amp;automatización...", "fr": "Règles d'&amp;automatisation...",
        "it": "Regole di &amp;automazione...", "pt": "Regras de &amp;automação...", "ru": "Правила &amp;автоматизации...",
    },
    "Open snapshot &amp;folder": {
        "ko": "스냅샷 폴더 열기(&amp;F)", "ja": "スナップショットフォルダを開く(&amp;F)", "zh": "打开快照文件夹(&amp;F)",
        "de": "Schnappschuss-&amp;Ordner öffnen", "es": "Abrir &amp;carpeta de instantáneas", "fr": "Ouvrir le &amp;dossier des instantanés",
        "it": "Apri &amp;cartella istantanee", "pt": "Abrir &amp;pasta de instantâneos", "ru": "Открыть &amp;папку снимков",
    },
    "&amp;Browse snapshots...": {
        "ko": "스냅샷 탐색(&amp;B)...", "ja": "スナップショットを閲覧(&amp;B)...", "zh": "浏览快照(&amp;B)...",
        "de": "Schnappschüsse &amp;durchsuchen...", "es": "&amp;Examinar instantáneas...", "fr": "&amp;Parcourir les instantanés...",
        "it": "&amp;Sfoglia istantanee...", "pt": "&amp;Navegar instantâneos...", "ru": "&amp;Обзор снимков...",
    },
    "(no snapshots defined)": {
        "ko": "(정의된 스냅샷 없음)", "ja": "(スナップショット未定義)", "zh": "(未定义快照)",
        "de": "(keine Schnappschüsse definiert)", "es": "(sin instantáneas definidas)", "fr": "(aucun instantané défini)",
        "it": "(nessuna istantanea definita)", "pt": "(nenhum instantâneo definido)", "ru": "(снимки не определены)",
    },
    "(no sessions defined)": {
        "ko": "(정의된 세션 없음)", "ja": "(セッション未定義)", "zh": "(未定义会话)",
        "de": "(keine Sitzungen definiert)", "es": "(sin sesiones definidas)", "fr": "(aucune session définie)",
        "it": "(nessuna sessione definita)", "pt": "(nenhuma sessão definida)", "ru": "(сеансы не определены)",
    },

    # ---- View menu ----
    "&amp;View": {
        "ko": "보기(&amp;V)", "ja": "表示(&amp;V)", "zh": "视图(&amp;V)",
        "de": "&amp;Ansicht", "es": "&amp;Ver", "fr": "&amp;Affichage",
        "it": "&amp;Visualizza", "pt": "&amp;Exibir", "ru": "&amp;Вид",
    },
    "Show &amp;Command Line": {
        "ko": "명령줄 표시(&amp;C)", "ja": "コマンドラインを表示(&amp;C)", "zh": "显示命令行(&amp;C)",
        "de": "&amp;Befehlszeile anzeigen", "es": "Mostrar línea de &amp;comandos", "fr": "Afficher la ligne de &amp;commande",
        "it": "Mostra riga di &amp;comando", "pt": "Mostrar linha de &amp;comando", "ru": "Показать &amp;командную строку",
    },
    "Show Action &amp;Buttons": {
        "ko": "액션 버튼 표시(&amp;B)", "ja": "アクションボタンを表示(&amp;B)", "zh": "显示操作按钮(&amp;B)",
        "de": "Aktions-&amp;Schaltflächen anzeigen", "es": "Mostrar &amp;botones de acción", "fr": "Afficher les &amp;boutons d'action",
        "it": "Mostra &amp;pulsanti di azione", "pt": "Mostrar &amp;botões de ação", "ru": "Показать кнопки &amp;действий",
    },
    "Show &amp;Status Bar": {
        "ko": "상태 표시줄 표시(&amp;S)", "ja": "ステータスバーを表示(&amp;S)", "zh": "显示状态栏(&amp;S)",
        "de": "&amp;Statusleiste anzeigen", "es": "Mostrar barra de e&amp;stado", "fr": "Afficher la barre d'é&amp;tat",
        "it": "Mostra barra di &amp;stato", "pt": "Mostrar barra de &amp;status", "ru": "Показать &amp;строку состояния",
    },
    "&amp;Full Screen\tF11": {
        "ko": "전체 화면(&amp;F)\tF11", "ja": "全画面(&amp;F)\tF11", "zh": "全屏(&amp;F)\tF11",
        "de": "&amp;Vollbild\tF11", "es": "&amp;Pantalla completa\tF11", "fr": "&amp;Plein écran\tF11",
        "it": "&amp;Schermo intero\tF11", "pt": "&amp;Tela cheia\tF11", "ru": "&amp;Полный экран\tF11",
    },

    # ---- Settings menu ----
    "Se&amp;ttings": {
        "ko": "설정(&amp;T)", "ja": "設定(&amp;T)", "zh": "设置(&amp;T)",
        "de": "Eins&amp;tellungen", "es": "A&amp;justes", "fr": "Paramè&amp;tres",
        "it": "Impos&amp;tazioni", "pt": "Configurações (&amp;T)", "ru": "Нас&amp;тройки",
    },
    "&amp;Preferences...": {
        "ko": "환경설정(&amp;P)...", "ja": "環境設定(&amp;P)...", "zh": "首选项(&amp;P)...",
        "de": "&amp;Einstellungen...", "es": "&amp;Preferencias...", "fr": "&amp;Préférences...",
        "it": "&amp;Preferenze...", "pt": "&amp;Preferências...", "ru": "&amp;Параметры...",
    },
    "&amp;Language": {
        "ko": "언어(&amp;L)", "ja": "言語(&amp;L)", "zh": "语言(&amp;L)",
        "de": "&amp;Sprache", "es": "&amp;Idioma", "fr": "&amp;Langue",
        "it": "&amp;Lingua", "pt": "&amp;Idioma", "ru": "&amp;Язык",
    },
    "&amp;Reload profile": {
        "ko": "프로파일 다시 읽기(&amp;R)", "ja": "プロファイル再読込(&amp;R)", "zh": "重新加载配置(&amp;R)",
        "de": "Profil neu &amp;laden", "es": "&amp;Recargar perfil", "fr": "&amp;Recharger le profil",
        "it": "&amp;Ricarica profilo", "pt": "&amp;Recarregar perfil", "ru": "&amp;Перезагрузить профиль",
    },

    # ---- Help menu ----
    "&amp;Help": {
        "ko": "도움말(&amp;H)", "ja": "ヘルプ(&amp;H)", "zh": "帮助(&amp;H)",
        "de": "&amp;Hilfe", "es": "A&amp;yuda", "fr": "&amp;Aide",
        "it": "&amp;Aiuto", "pt": "A&amp;juda", "ru": "&amp;Справка",
    },
    "&amp;About TSCRT...": {
        "ko": "TSCRT 정보(&amp;A)...", "ja": "TSCRT について(&amp;A)...", "zh": "关于 TSCRT(&amp;A)...",
        "de": "&amp;Über TSCRT...", "es": "&amp;Acerca de TSCRT...", "fr": "À &amp;propos de TSCRT...",
        "it": "&amp;Informazioni su TSCRT...", "pt": "&amp;Sobre o TSCRT...", "ru": "&amp;О TSCRT...",
    },
    "About TSCRT": {
        "ko": "TSCRT 정보", "ja": "TSCRT について", "zh": "关于 TSCRT",
        "de": "Über TSCRT", "es": "Acerca de TSCRT", "fr": "À propos de TSCRT",
        "it": "Informazioni su TSCRT", "pt": "Sobre o TSCRT", "ru": "О TSCRT",
    },
    "&amp;Usage guide...": {
        "ko": "사용 설명서(&amp;U)...", "ja": "使い方ガイド(&amp;U)...", "zh": "使用指南(&amp;U)...",
        "de": "&amp;Benutzerhandbuch...", "es": "G&amp;uía de uso...", "fr": "G&amp;uide d'utilisation...",
        "it": "G&amp;uida all'uso...", "pt": "G&amp;uia de uso...", "ru": "&amp;Руководство пользователя...",
    },
    "TSCRT — Usage guide": {
        "ko": "TSCRT — 사용 설명서", "ja": "TSCRT — 使い方ガイド", "zh": "TSCRT — 使用指南",
        "de": "TSCRT — Benutzerhandbuch", "es": "TSCRT — Guía de uso", "fr": "TSCRT — Guide d'utilisation",
        "it": "TSCRT — Guida all'uso", "pt": "TSCRT — Guia de uso", "ru": "TSCRT — Руководство пользователя",
    },
    "&amp;Export diagnostics...": {
        "ko": "진단 정보 내보내기(&amp;E)...", "ja": "診断情報エクスポート(&amp;E)...", "zh": "导出诊断信息(&amp;E)...",
        "de": "Diagnose &amp;exportieren...", "es": "&amp;Exportar diagnósticos...", "fr": "&amp;Exporter les diagnostics...",
        "it": "&amp;Esporta diagnostica...", "pt": "&amp;Exportar diagnósticos...", "ru": "&amp;Экспорт диагностики...",
    },
    "Export diagnostics": {
        "ko": "진단 정보 내보내기", "ja": "診断情報エクスポート", "zh": "导出诊断信息",
        "de": "Diagnose exportieren", "es": "Exportar diagnósticos", "fr": "Exporter les diagnostics",
        "it": "Esporta diagnostica", "pt": "Exportar diagnósticos", "ru": "Экспорт диагностики",
    },
    "Diagnostic bundle (*.txt.gz);;All files (*)": {
        "ko": "진단 번들 (*.txt.gz);;모든 파일 (*)", "ja": "診断バンドル (*.txt.gz);;すべてのファイル (*)", "zh": "诊断包 (*.txt.gz);;所有文件 (*)",
        "de": "Diagnose-Paket (*.txt.gz);;Alle Dateien (*)", "es": "Paquete de diagnóstico (*.txt.gz);;Todos los archivos (*)", "fr": "Paquet de diagnostic (*.txt.gz);;Tous les fichiers (*)",
        "it": "Pacchetto diagnostico (*.txt.gz);;Tutti i file (*)", "pt": "Pacote de diagnóstico (*.txt.gz);;Todos os arquivos (*)", "ru": "Диагностический пакет (*.txt.gz);;Все файлы (*)",
    },
    "Export failed": {
        "ko": "내보내기 실패", "ja": "エクスポートに失敗", "zh": "导出失败",
        "de": "Export fehlgeschlagen", "es": "Error al exportar", "fr": "Échec de l'exportation",
        "it": "Esportazione fallita", "pt": "Falha ao exportar", "ru": "Ошибка экспорта",
    },
    "Could not write diagnostic bundle:\n%1": {
        "ko": "진단 번들을 쓸 수 없습니다:\n%1",
        "ja": "診断バンドルを書き込めませんでした:\n%1",
        "zh": "无法写入诊断包:\n%1",
        "de": "Diagnose-Paket konnte nicht geschrieben werden:\n%1",
        "es": "No se pudo escribir el paquete de diagnóstico:\n%1",
        "fr": "Impossible d'écrire le paquet de diagnostic :\n%1",
        "it": "Impossibile scrivere il pacchetto diagnostico:\n%1",
        "pt": "Não foi possível gravar o pacote de diagnóstico:\n%1",
        "ru": "Не удалось записать диагностический пакет:\n%1",
    },
    "Diagnostics exported": {
        "ko": "진단 정보 내보내기 완료", "ja": "診断情報をエクスポートしました", "zh": "诊断信息已导出",
        "de": "Diagnose exportiert", "es": "Diagnósticos exportados", "fr": "Diagnostics exportés",
        "it": "Diagnostica esportata", "pt": "Diagnósticos exportados", "ru": "Диагностика экспортирована",
    },
    "File: %1\nUncompressed: %2\nCompressed: %3": {
        "ko": "파일: %1\n원본: %2\n압축: %3",
        "ja": "ファイル: %1\n非圧縮: %2\n圧縮: %3",
        "zh": "文件: %1\n未压缩: %2\n已压缩: %3",
        "de": "Datei: %1\nUnkomprimiert: %2\nKomprimiert: %3",
        "es": "Archivo: %1\nSin comprimir: %2\nComprimido: %3",
        "fr": "Fichier : %1\nNon compressé : %2\nCompressé : %3",
        "it": "File: %1\nNon compresso: %2\nCompresso: %3",
        "pt": "Arquivo: %1\nNão compactado: %2\nCompactado: %3",
        "ru": "Файл: %1\nНесжатый: %2\nСжатый: %3",
    },
    "Configured log directory is not writable — opened fallback: %1": {
        "ko": "설정된 로그 디렉터리에 쓸 수 없어 폴백 경로를 열었습니다: %1",
        "ja": "設定されたログディレクトリに書き込めないためフォールバックを開きました: %1",
        "zh": "配置的日志目录不可写,已打开回退目录: %1",
        "de": "Konfiguriertes Log-Verzeichnis nicht beschreibbar — Fallback geöffnet: %1",
        "es": "La carpeta de logs configurada no es escribible — abierto el destino alternativo: %1",
        "fr": "Le dossier de journaux configuré n'est pas accessible en écriture — ouverture du repli : %1",
        "it": "La cartella log configurata non è scrivibile — aperta la cartella di riserva: %1",
        "pt": "Diretório de log configurado não é gravável — aberto destino alternativo: %1",
        "ru": "Настроенный каталог логов недоступен для записи — открыт резервный: %1",
    },
    "Note: log content was truncated to fit the size cap.": {
        "ko": "참고: 크기 제한을 맞추기 위해 로그 내용이 잘렸습니다.",
        "ja": "注: サイズ上限に合わせてログ内容が切り詰められました。",
        "zh": "注意: 为适应大小上限,日志内容已被截断。",
        "de": "Hinweis: Log-Inhalt wurde gekürzt, um die Größenbegrenzung einzuhalten.",
        "es": "Nota: el contenido del log fue truncado para respetar el límite de tamaño.",
        "fr": "Remarque : le contenu du journal a été tronqué pour respecter la limite de taille.",
        "it": "Nota: il contenuto del log è stato troncato per rispettare il limite di dimensione.",
        "pt": "Observação: o conteúdo do log foi truncado para atender ao limite de tamanho.",
        "ru": "Примечание: содержимое журнала усечено для соблюдения ограничения размера.",
    },
    "&lt;p&gt;Usage guide not available for language &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;": {
        "ko": "&lt;p&gt;언어 &lt;b&gt;%1&lt;/b&gt;에 대한 사용 설명서가 없습니다.&lt;/p&gt;",
        "ja": "&lt;p&gt;言語 &lt;b&gt;%1&lt;/b&gt; の使い方ガイドはありません。&lt;/p&gt;",
        "zh": "&lt;p&gt;未提供语言 &lt;b&gt;%1&lt;/b&gt; 的使用指南。&lt;/p&gt;",
        "de": "&lt;p&gt;Benutzerhandbuch für Sprache &lt;b&gt;%1&lt;/b&gt; nicht verfügbar.&lt;/p&gt;",
        "es": "&lt;p&gt;Guía de uso no disponible para el idioma &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;",
        "fr": "&lt;p&gt;Guide d'utilisation non disponible pour la langue &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;",
        "it": "&lt;p&gt;Guida all'uso non disponibile per la lingua &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;",
        "pt": "&lt;p&gt;Guia de uso não disponível para o idioma &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;",
        "ru": "&lt;p&gt;Руководство пользователя недоступно для языка &lt;b&gt;%1&lt;/b&gt;.&lt;/p&gt;",
    },

    # ---- Snapshot browser ----
    "Snapshot browser": {
        "ko": "스냅샷 탐색기", "ja": "スナップショット ブラウザ", "zh": "快照浏览器",
        "de": "Schnappschuss-Browser", "es": "Explorador de instantáneas", "fr": "Navigateur d'instantanés",
        "it": "Sfoglia istantanee", "pt": "Navegador de instantâneos", "ru": "Обозреватель снимков",
    },
    "Snapshot folder is not configured.": {
        "ko": "스냅샷 폴더가 설정되지 않았습니다.", "ja": "スナップショット フォルダが設定されていません。", "zh": "未配置快照文件夹。",
        "de": "Schnappschuss-Ordner ist nicht konfiguriert.", "es": "La carpeta de instantáneas no está configurada.", "fr": "Le dossier d'instantanés n'est pas configuré.",
        "it": "La cartella istantanee non è configurata.", "pt": "A pasta de instantâneos não está configurada.", "ru": "Папка снимков не настроена.",
    },
    "Refresh": {
        "ko": "새로 고침", "ja": "更新", "zh": "刷新",
        "de": "Aktualisieren", "es": "Actualizar", "fr": "Actualiser",
        "it": "Aggiorna", "pt": "Atualizar", "ru": "Обновить",
    },
    "Send to administrator": {
        "ko": "관리자에게 보내기", "ja": "管理者に送信", "zh": "发送给管理员",
        "de": "An Administrator senden", "es": "Enviar al administrador", "fr": "Envoyer à l'administrateur",
        "it": "Invia all'amministratore", "pt": "Enviar ao administrador", "ru": "Отправить администратору",
    },
    "Open": {
        "ko": "열기", "ja": "開く", "zh": "打开",
        "de": "Öffnen", "es": "Abrir", "fr": "Ouvrir",
        "it": "Apri", "pt": "Abrir", "ru": "Открыть",
    },
    "Reveal in file manager": {
        "ko": "파일 탐색기에서 표시", "ja": "ファイル マネージャで表示", "zh": "在文件管理器中显示",
        "de": "Im Dateimanager anzeigen", "es": "Mostrar en el explorador de archivos", "fr": "Afficher dans le gestionnaire de fichiers",
        "it": "Mostra nel gestore file", "pt": "Mostrar no gerenciador de arquivos", "ru": "Показать в файловом менеджере",
    },
    "Delete": {
        "ko": "삭제", "ja": "削除", "zh": "删除",
        "de": "Löschen", "es": "Eliminar", "fr": "Supprimer",
        "it": "Elimina", "pt": "Excluir", "ru": "Удалить",
    },
    "Delete snapshot": {
        "ko": "스냅샷 삭제", "ja": "スナップショットを削除", "zh": "删除快照",
        "de": "Schnappschuss löschen", "es": "Eliminar instantánea", "fr": "Supprimer l'instantané",
        "it": "Elimina istantanea", "pt": "Excluir instantâneo", "ru": "Удалить снимок",
    },
    "Delete &quot;%1&quot;?": {
        "ko": "&quot;%1&quot;을(를) 삭제할까요?", "ja": "&quot;%1&quot; を削除しますか?", "zh": "要删除 &quot;%1&quot; 吗?",
        "de": "&quot;%1&quot; löschen?", "es": "¿Eliminar &quot;%1&quot;?", "fr": "Supprimer &quot;%1&quot; ?",
        "it": "Eliminare &quot;%1&quot;?", "pt": "Excluir &quot;%1&quot;?", "ru": "Удалить &quot;%1&quot;?",
    },
    "Delete failed": {
        "ko": "삭제 실패", "ja": "削除に失敗", "zh": "删除失败",
        "de": "Löschen fehlgeschlagen", "es": "Error al eliminar", "fr": "Échec de la suppression",
        "it": "Eliminazione non riuscita", "pt": "Falha ao excluir", "ru": "Ошибка удаления",
    },
    "Could not delete %1": {
        "ko": "%1 을(를) 삭제할 수 없습니다", "ja": "%1 を削除できません", "zh": "无法删除 %1",
        "de": "%1 konnte nicht gelöscht werden", "es": "No se pudo eliminar %1", "fr": "Impossible de supprimer %1",
        "it": "Impossibile eliminare %1", "pt": "Não foi possível excluir %1", "ru": "Не удалось удалить %1",
    },
    "Select a snapshot file first.": {
        "ko": "먼저 스냅샷 파일을 선택하세요.", "ja": "まずスナップショット ファイルを選択してください。", "zh": "请先选择一个快照文件。",
        "de": "Bitte zuerst eine Schnappschussdatei auswählen.", "es": "Seleccione primero un archivo de instantánea.", "fr": "Veuillez d'abord sélectionner un fichier d'instantané.",
        "it": "Selezionare prima un file di istantanea.", "pt": "Selecione primeiro um arquivo de instantâneo.", "ru": "Сначала выберите файл снимка.",
    },
    "SMTP not configured": {
        "ko": "SMTP 미설정", "ja": "SMTP 未設定", "zh": "未配置 SMTP",
        "de": "SMTP nicht konfiguriert", "es": "SMTP no configurado", "fr": "SMTP non configuré",
        "it": "SMTP non configurato", "pt": "SMTP não configurado", "ru": "SMTP не настроен",
    },
    "Configure an SMTP server under Settings → SMTP before using this action.": {
        "ko": "이 기능을 사용하기 전에 설정 → SMTP 에서 SMTP 서버를 설정하세요.",
        "ja": "この機能を使う前に 設定 → SMTP で SMTP サーバを設定してください。",
        "zh": "使用此功能前，请在 设置 → SMTP 中配置 SMTP 服务器。",
        "de": "Konfigurieren Sie vor der Verwendung dieser Aktion einen SMTP-Server unter Einstellungen → SMTP.",
        "es": "Configure un servidor SMTP en Ajustes → SMTP antes de usar esta acción.",
        "fr": "Configurez un serveur SMTP dans Paramètres → SMTP avant d'utiliser cette action.",
        "it": "Configurare un server SMTP in Impostazioni → SMTP prima di usare questa azione.",
        "pt": "Configure um servidor SMTP em Configurações → SMTP antes de usar esta ação.",
        "ru": "Настройте SMTP-сервер в разделе Настройки → SMTP перед использованием этого действия.",
    },
    "No administrator address": {
        "ko": "관리자 주소 없음", "ja": "管理者アドレスがありません", "zh": "没有管理员地址",
        "de": "Keine Administrator-Adresse", "es": "Sin dirección de administrador", "fr": "Aucune adresse d'administrateur",
        "it": "Nessun indirizzo amministratore", "pt": "Sem endereço de administrador", "ru": "Нет адреса администратора",
    },
    "The SMTP &quot;From&quot; address is empty. Set it in Settings → SMTP; that address is used as the administrator recipient.": {
        "ko": "SMTP &quot;From&quot; 주소가 비어 있습니다. 설정 → SMTP 에서 지정하세요. 이 주소가 관리자 수신자로 사용됩니다.",
        "ja": "SMTP &quot;From&quot; アドレスが空です。設定 → SMTP で設定してください。このアドレスが管理者の宛先として使われます。",
        "zh": "SMTP &quot;From&quot; 地址为空。请在 设置 → SMTP 中设置。此地址将作为管理员收件人。",
        "de": "Die SMTP-&quot;From&quot;-Adresse ist leer. Legen Sie sie in Einstellungen → SMTP fest; diese Adresse wird als Administrator-Empfänger verwendet.",
        "es": "La dirección &quot;From&quot; de SMTP está vacía. Configúrela en Ajustes → SMTP; esa dirección se usa como destinatario del administrador.",
        "fr": "L'adresse SMTP &quot;From&quot; est vide. Définissez-la dans Paramètres → SMTP ; cette adresse sert de destinataire administrateur.",
        "it": "L'indirizzo SMTP &quot;From&quot; è vuoto. Impostarlo in Impostazioni → SMTP; tale indirizzo viene usato come destinatario amministratore.",
        "pt": "O endereço &quot;From&quot; do SMTP está vazio. Defina-o em Configurações → SMTP; esse endereço é usado como destinatário administrador.",
        "ru": "SMTP-адрес &quot;From&quot; пуст. Задайте его в разделе Настройки → SMTP; этот адрес используется как получатель-администратор.",
    },
    "[TSCRT] Snapshot %1": {
        "ko": "[TSCRT] 스냅샷 %1", "ja": "[TSCRT] スナップショット %1", "zh": "[TSCRT] 快照 %1",
        "de": "[TSCRT] Schnappschuss %1", "es": "[TSCRT] Instantánea %1", "fr": "[TSCRT] Instantané %1",
        "it": "[TSCRT] Istantanea %1", "pt": "[TSCRT] Instantâneo %1", "ru": "[TSCRT] Снимок %1",
    },
    "The attached TSCRT snapshot file was forwarded by %1 at %2.\n\nFile: %3\n": {
        "ko": "첨부된 TSCRT 스냅샷 파일은 %1 이(가) %2 에 전달했습니다.\n\n파일: %3\n",
        "ja": "添付の TSCRT スナップショット ファイルは %1 により %2 に転送されました。\n\nファイル: %3\n",
        "zh": "附件中的 TSCRT 快照文件由 %1 于 %2 转发。\n\n文件: %3\n",
        "de": "Die angehängte TSCRT-Schnappschussdatei wurde von %1 um %2 weitergeleitet.\n\nDatei: %3\n",
        "es": "El archivo de instantánea TSCRT adjunto fue reenviado por %1 a las %2.\n\nArchivo: %3\n",
        "fr": "Le fichier d'instantané TSCRT joint a été transféré par %1 à %2.\n\nFichier : %3\n",
        "it": "Il file di istantanea TSCRT allegato è stato inoltrato da %1 alle %2.\n\nFile: %3\n",
        "pt": "O arquivo de instantâneo TSCRT anexado foi encaminhado por %1 em %2.\n\nArquivo: %3\n",
        "ru": "Прикреплённый файл снимка TSCRT был переслан пользователем %1 в %2.\n\nФайл: %3\n",
    },
    "Sent": {
        "ko": "전송됨", "ja": "送信しました", "zh": "已发送",
        "de": "Gesendet", "es": "Enviado", "fr": "Envoyé",
        "it": "Inviato", "pt": "Enviado", "ru": "Отправлено",
    },
    "Snapshot &quot;%1&quot; sent to %2.": {
        "ko": "스냅샷 &quot;%1&quot; 을(를) %2 에 전송했습니다.", "ja": "スナップショット &quot;%1&quot; を %2 に送信しました。", "zh": "已将快照 &quot;%1&quot; 发送给 %2。",
        "de": "Schnappschuss &quot;%1&quot; an %2 gesendet.", "es": "Instantánea &quot;%1&quot; enviada a %2.", "fr": "Instantané &quot;%1&quot; envoyé à %2.",
        "it": "Istantanea &quot;%1&quot; inviata a %2.", "pt": "Instantâneo &quot;%1&quot; enviado para %2.", "ru": "Снимок &quot;%1&quot; отправлен на %2.",
    },
    "Send failed": {
        "ko": "전송 실패", "ja": "送信に失敗", "zh": "发送失败",
        "de": "Senden fehlgeschlagen", "es": "Error al enviar", "fr": "Échec de l'envoi",
        "it": "Invio non riuscito", "pt": "Falha no envio", "ru": "Ошибка отправки",
    },

    # ---- Status bar messages ----
    "Ready · libssh2 %1 · libvterm %2.%3": {
        "ko": "준비됨 · libssh2 %1 · libvterm %2.%3",
        "ja": "準備完了 · libssh2 %1 · libvterm %2.%3",
        "zh": "就绪 · libssh2 %1 · libvterm %2.%3",
        "de": "Bereit · libssh2 %1 · libvterm %2.%3",
        "es": "Listo · libssh2 %1 · libvterm %2.%3",
        "fr": "Prêt · libssh2 %1 · libvterm %2.%3",
        "it": "Pronto · libssh2 %1 · libvterm %2.%3",
        "pt": "Pronto · libssh2 %1 · libvterm %2.%3",
        "ru": "Готово · libssh2 %1 · libvterm %2.%3",
    },
    "Connecting to %1...": {
        "ko": "%1에 연결 중...", "ja": "%1 に接続中...", "zh": "正在连接 %1...",
        "de": "Verbinden mit %1...", "es": "Conectando con %1...", "fr": "Connexion à %1...",
        "it": "Connessione a %1...", "pt": "Conectando a %1...", "ru": "Подключение к %1...",
    },
    "Connected: %1": {
        "ko": "연결됨: %1", "ja": "接続済み: %1", "zh": "已连接: %1",
        "de": "Verbunden: %1", "es": "Conectado: %1", "fr": "Connecté : %1",
        "it": "Connesso: %1", "pt": "Conectado: %1", "ru": "Подключено: %1",
    },
    "Disconnected: %1 (%2)": {
        "ko": "연결 끊김: %1 (%2)", "ja": "切断: %1 (%2)", "zh": "已断开: %1 (%2)",
        "de": "Getrennt: %1 (%2)", "es": "Desconectado: %1 (%2)", "fr": "Déconnecté : %1 (%2)",
        "it": "Disconnesso: %1 (%2)", "pt": "Desconectado: %1 (%2)", "ru": "Отключено: %1 (%2)",
    },
    "Error: %1": {
        "ko": "오류: %1", "ja": "エラー: %1", "zh": "错误: %1",
        "de": "Fehler: %1", "es": "Error: %1", "fr": "Erreur : %1",
        "it": "Errore: %1", "pt": "Erro: %1", "ru": "Ошибка: %1",
    },
    "Session \"%1\" saved.": {
        "ko": "세션 \"%1\" 저장됨.", "ja": "セッション \"%1\" を保存しました。", "zh": "已保存会话 \"%1\"。",
        "de": "Sitzung \"%1\" gespeichert.", "es": "Sesión \"%1\" guardada.", "fr": "Session \"%1\" enregistrée.",
        "it": "Sessione \"%1\" salvata.", "pt": "Sessão \"%1\" salva.", "ru": "Сеанс \"%1\" сохранён.",
    },
    "Sessions saved.": {
        "ko": "세션이 저장되었습니다.", "ja": "セッションを保存しました。", "zh": "会话已保存。",
        "de": "Sitzungen gespeichert.", "es": "Sesiones guardadas.", "fr": "Sessions enregistrées.",
        "it": "Sessioni salvate.", "pt": "Sessões salvas.", "ru": "Сеансы сохранены.",
    },
    "Preferences saved.": {
        "ko": "환경설정이 저장되었습니다.", "ja": "環境設定を保存しました。", "zh": "首选项已保存。",
        "de": "Einstellungen gespeichert.", "es": "Preferencias guardadas.", "fr": "Préférences enregistrées.",
        "it": "Preferenze salvate.", "pt": "Preferências salvas.", "ru": "Параметры сохранены.",
    },
    "Log settings saved.": {
        "ko": "로그 설정이 저장되었습니다.", "ja": "ログ設定を保存しました。", "zh": "日志设置已保存。",
        "de": "Protokolleinstellungen gespeichert.", "es": "Configuración de registros guardada.", "fr": "Paramètres des journaux enregistrés.",
        "it": "Impostazioni log salvate.", "pt": "Configuração de registros salva.", "ru": "Настройки журналов сохранены.",
    },
    "Snapshots saved.": {
        "ko": "스냅샷이 저장되었습니다.", "ja": "スナップショットを保存しました。", "zh": "快照已保存。",
        "de": "Schnappschüsse gespeichert.", "es": "Instantáneas guardadas.", "fr": "Instantanés enregistrés.",
        "it": "Istantanee salvate.", "pt": "Instantâneos salvos.", "ru": "Снимки сохранены.",
    },
    "Profile reloaded.": {
        "ko": "프로파일을 다시 읽었습니다.", "ja": "プロファイルを再読込しました。", "zh": "配置已重新加载。",
        "de": "Profil neu geladen.", "es": "Perfil recargado.", "fr": "Profil rechargé.",
        "it": "Profilo ricaricato.", "pt": "Perfil recarregado.", "ru": "Профиль перезагружен.",
    },
    "Copied session &quot;%1&quot;.": {
        "ko": "세션 &quot;%1&quot;을(를) 복사했습니다.", "ja": "セッション &quot;%1&quot; をコピーしました。", "zh": "已复制会话 &quot;%1&quot;。",
        "de": "Sitzung &quot;%1&quot; kopiert.", "es": "Sesión &quot;%1&quot; copiada.", "fr": "Session &quot;%1&quot; copiée.",
        "it": "Sessione &quot;%1&quot; copiata.", "pt": "Sessão &quot;%1&quot; copiada.", "ru": "Сеанс &quot;%1&quot; скопирован.",
    },

    # ---- Dialogs: common ----
    "Session": {
        "ko": "세션", "ja": "セッション", "zh": "会话",
        "de": "Sitzung", "es": "Sesión", "fr": "Session",
        "it": "Sessione", "pt": "Sessão", "ru": "Сеанс",
    },
    "Sessions": {
        "ko": "세션", "ja": "セッション", "zh": "会话",
        "de": "Sitzungen", "es": "Sesiones", "fr": "Sessions",
        "it": "Sessioni", "pt": "Sessões", "ru": "Сеансы",
    },
    "Snapshot": {
        "ko": "스냅샷", "ja": "スナップショット", "zh": "快照",
        "de": "Schnappschuss", "es": "Instantánea", "fr": "Instantané",
        "it": "Istantanea", "pt": "Instantâneo", "ru": "Снимок",
    },
    "Snapshots": {
        "ko": "스냅샷", "ja": "スナップショット", "zh": "快照",
        "de": "Schnappschüsse", "es": "Instantáneas", "fr": "Instantanés",
        "it": "Istantanee", "pt": "Instantâneos", "ru": "Снимки",
    },
    "Preferences": {
        "ko": "환경설정", "ja": "環境設定", "zh": "首选项",
        "de": "Einstellungen", "es": "Preferencias", "fr": "Préférences",
        "it": "Preferenze", "pt": "Preferências", "ru": "Параметры",
    },
    "Common": {
        "ko": "공통", "ja": "共通", "zh": "通用",
        "de": "Allgemein", "es": "Común", "fr": "Commun",
        "it": "Comune", "pt": "Comum", "ru": "Общие",
    },
    "Buttons": {
        "ko": "버튼", "ja": "ボタン", "zh": "按钮",
        "de": "Schaltflächen", "es": "Botones", "fr": "Boutons",
        "it": "Pulsanti", "pt": "Botões", "ru": "Кнопки",
    },
    "Startup": {
        "ko": "시작", "ja": "起動時", "zh": "启动",
        "de": "Start", "es": "Inicio", "fr": "Démarrage",
        "it": "Avvio", "pt": "Inicialização", "ru": "Запуск",
    },
    "Triggers": {
        "ko": "트리거", "ja": "トリガー", "zh": "触发器",
        "de": "Auslöser", "es": "Disparadores", "fr": "Déclencheurs",
        "it": "Trigger", "pt": "Gatilhos", "ru": "Триггеры",
    },
    "Periodic": {
        "ko": "주기적", "ja": "定期実行", "zh": "定时",
        "de": "Regelmäßig", "es": "Periódico", "fr": "Périodique",
        "it": "Periodico", "pt": "Periódico", "ru": "Периодически",
    },
    "SMTP": {
        "ko": "SMTP", "ja": "SMTP", "zh": "SMTP",
        "de": "SMTP", "es": "SMTP", "fr": "SMTP",
        "it": "SMTP", "pt": "SMTP", "ru": "SMTP",
    },
    "Log": {
        "ko": "로그", "ja": "ログ", "zh": "日志",
        "de": "Protokoll", "es": "Registro", "fr": "Journal",
        "it": "Log", "pt": "Registro", "ru": "Журнал",
    },
    "Logs": {
        "ko": "로그", "ja": "ログ", "zh": "日志",
        "de": "Protokolle", "es": "Registros", "fr": "Journaux",
        "it": "Log", "pt": "Registros", "ru": "Журналы",
    },
    "Automation rules": {
        "ko": "자동화 규칙", "ja": "自動化ルール", "zh": "自动化规则",
        "de": "Automatisierungsregeln", "es": "Reglas de automatización", "fr": "Règles d'automatisation",
        "it": "Regole di automazione", "pt": "Regras de automação", "ru": "Правила автоматизации",
    },

    # ---- Form labels ----
    "Name": {
        "ko": "이름", "ja": "名前", "zh": "名称",
        "de": "Name", "es": "Nombre", "fr": "Nom",
        "it": "Nome", "pt": "Nome", "ru": "Имя",
    },
    "Name:": {
        "ko": "이름:", "ja": "名前:", "zh": "名称:",
        "de": "Name:", "es": "Nombre:", "fr": "Nom :",
        "it": "Nome:", "pt": "Nome:", "ru": "Имя:",
    },
    "New name:": {
        "ko": "새 이름:", "ja": "新しい名前:", "zh": "新名称:",
        "de": "Neuer Name:", "es": "Nuevo nombre:", "fr": "Nouveau nom :",
        "it": "Nuovo nome:", "pt": "Novo nome:", "ru": "Новое имя:",
    },
    "Description:": {
        "ko": "설명:", "ja": "説明:", "zh": "说明:",
        "de": "Beschreibung:", "es": "Descripción:", "fr": "Description :",
        "it": "Descrizione:", "pt": "Descrição:", "ru": "Описание:",
    },
    "Label": {
        "ko": "라벨", "ja": "ラベル", "zh": "标签",
        "de": "Beschriftung", "es": "Etiqueta", "fr": "Étiquette",
        "it": "Etichetta", "pt": "Rótulo", "ru": "Метка",
    },
    "Label:": {
        "ko": "라벨:", "ja": "ラベル:", "zh": "标签:",
        "de": "Beschriftung:", "es": "Etiqueta:", "fr": "Étiquette :",
        "it": "Etichetta:", "pt": "Rótulo:", "ru": "Метка:",
    },
    "Action": {
        "ko": "액션", "ja": "アクション", "zh": "动作",
        "de": "Aktion", "es": "Acción", "fr": "Action",
        "it": "Azione", "pt": "Ação", "ru": "Действие",
    },
    "Action:": {
        "ko": "액션:", "ja": "アクション:", "zh": "动作:",
        "de": "Aktion:", "es": "Acción:", "fr": "Action :",
        "it": "Azione:", "pt": "Ação:", "ru": "Действие:",
    },
    "Command": {
        "ko": "명령", "ja": "コマンド", "zh": "命令",
        "de": "Befehl", "es": "Comando", "fr": "Commande",
        "it": "Comando", "pt": "Comando", "ru": "Команда",
    },
    "Command to repeat:": {
        "ko": "반복할 명령:", "ja": "繰り返すコマンド:", "zh": "重复命令:",
        "de": "Zu wiederholender Befehl:", "es": "Comando a repetir:", "fr": "Commande à répéter :",
        "it": "Comando da ripetere:", "pt": "Comando a repetir:", "ru": "Команда для повтора:",
    },
    "Pattern": {
        "ko": "패턴", "ja": "パターン", "zh": "模式",
        "de": "Muster", "es": "Patrón", "fr": "Motif",
        "it": "Modello", "pt": "Padrão", "ru": "Шаблон",
    },
    "Pattern (substring):": {
        "ko": "패턴(부분 문자열):", "ja": "パターン(部分一致):", "zh": "模式(子串):",
        "de": "Muster (Teilzeichenkette):", "es": "Patrón (subcadena):", "fr": "Motif (sous-chaîne) :",
        "it": "Modello (sottostringa):", "pt": "Padrão (substring):", "ru": "Шаблон (подстрока):",
    },
    "Type": {
        "ko": "유형", "ja": "種類", "zh": "类型",
        "de": "Typ", "es": "Tipo", "fr": "Type",
        "it": "Tipo", "pt": "Tipo", "ru": "Тип",
    },
    "Kind": {
        "ko": "종류", "ja": "種別", "zh": "类别",
        "de": "Art", "es": "Clase", "fr": "Genre",
        "it": "Genere", "pt": "Espécie", "ru": "Вид",
    },
    "Session:": {
        "ko": "세션:", "ja": "セッション:", "zh": "会话:",
        "de": "Sitzung:", "es": "Sesión:", "fr": "Session :",
        "it": "Sessione:", "pt": "Sessão:", "ru": "Сеанс:",
    },
    "Subject:": {
        "ko": "제목:", "ja": "件名:", "zh": "主题:",
        "de": "Betreff:", "es": "Asunto:", "fr": "Objet :",
        "it": "Oggetto:", "pt": "Assunto:", "ru": "Тема:",
    },
    "Password:": {
        "ko": "비밀번호:", "ja": "パスワード:", "zh": "密码:",
        "de": "Passwort:", "es": "Contraseña:", "fr": "Mot de passe :",
        "it": "Password:", "pt": "Senha:", "ru": "Пароль:",
    },
    "Port:": {
        "ko": "포트:", "ja": "ポート:", "zh": "端口:",
        "de": "Port:", "es": "Puerto:", "fr": "Port :",
        "it": "Porta:", "pt": "Porta:", "ru": "Порт:",
    },
    "Interval (s)": {
        "ko": "간격(초)", "ja": "間隔(秒)", "zh": "间隔(秒)",
        "de": "Intervall (s)", "es": "Intervalo (s)", "fr": "Intervalle (s)",
        "it": "Intervallo (s)", "pt": "Intervalo (s)", "ru": "Интервал (с)",
    },
    "Interval (seconds):": {
        "ko": "간격(초):", "ja": "間隔(秒):", "zh": "间隔(秒):",
        "de": "Intervall (Sekunden):", "es": "Intervalo (segundos):", "fr": "Intervalle (secondes) :",
        "it": "Intervallo (secondi):", "pt": "Intervalo (segundos):", "ru": "Интервал (секунды):",
    },
    "Delay (ms)": {
        "ko": "지연(ms)", "ja": "遅延(ms)", "zh": "延迟(ms)",
        "de": "Verzögerung (ms)", "es": "Retraso (ms)", "fr": "Délai (ms)",
        "it": "Ritardo (ms)", "pt": "Atraso (ms)", "ru": "Задержка (мс)",
    },
    "Max wait (ms)": {
        "ko": "최대 대기(ms)", "ja": "最大待機(ms)", "zh": "最大等待(ms)",
        "de": "Max. Wartezeit (ms)", "es": "Espera máx. (ms)", "fr": "Attente max (ms)",
        "it": "Attesa max (ms)", "pt": "Espera máx. (ms)", "ru": "Макс. ожидание (мс)",
    },
    "Cooldown (s)": {
        "ko": "쿨다운(초)", "ja": "クールダウン(秒)", "zh": "冷却(秒)",
        "de": "Abklingzeit (s)", "es": "Enfriamiento (s)", "fr": "Temps de repos (s)",
        "it": "Raffreddamento (s)", "pt": "Tempo de espera (s)", "ru": "Перерыв (с)",
    },
    "Cron expr": {
        "ko": "Cron 표현식", "ja": "Cron 式", "zh": "Cron 表达式",
        "de": "cron-Ausdruck", "es": "Expresión cron", "fr": "Expression cron",
        "it": "Espressione cron", "pt": "Expressão cron", "ru": "Выражение cron",
    },
    "Prompt regex": {
        "ko": "프롬프트 정규식", "ja": "プロンプト正規表現", "zh": "提示符正则",
        "de": "Prompt-Regex", "es": "Regex del prompt", "fr": "Regex d'invite",
        "it": "Regex prompt", "pt": "Regex do prompt", "ru": "Регулярное выражение приглашения",
    },
    "Timeout:": {
        "ko": "타임아웃:", "ja": "タイムアウト:", "zh": "超时:",
        "de": "Zeitüberschreitung:", "es": "Tiempo de espera:", "fr": "Délai :",
        "it": "Timeout:", "pt": "Tempo limite:", "ru": "Тайм-аут:",
    },
    "Security:": {
        "ko": "보안:", "ja": "セキュリティ:", "zh": "安全:",
        "de": "Sicherheit:", "es": "Seguridad:", "fr": "Sécurité :",
        "it": "Sicurezza:", "pt": "Segurança:", "ru": "Безопасность:",
    },
    "SMTP host:": {
        "ko": "SMTP 호스트:", "ja": "SMTP ホスト:", "zh": "SMTP 主机:",
        "de": "SMTP-Host:", "es": "Servidor SMTP:", "fr": "Hôte SMTP :",
        "it": "Host SMTP:", "pt": "Host SMTP:", "ru": "SMTP-хост:",
    },
    "Username:": {
        "ko": "사용자명:", "ja": "ユーザー名:", "zh": "用户名:",
        "de": "Benutzername:", "es": "Nombre de usuario:", "fr": "Nom d'utilisateur :",
        "it": "Nome utente:", "pt": "Nome de usuário:", "ru": "Имя пользователя:",
    },
    "From address:": {
        "ko": "보내는 주소:", "ja": "送信元アドレス:", "zh": "发件人地址:",
        "de": "Absenderadresse:", "es": "Dirección del remitente:", "fr": "Adresse d'expéditeur :",
        "it": "Indirizzo mittente:", "pt": "Endereço do remetente:", "ru": "Адрес отправителя:",
    },
    "From name:": {
        "ko": "보내는 사람 이름:", "ja": "送信者名:", "zh": "发件人名称:",
        "de": "Absendername:", "es": "Nombre del remitente:", "fr": "Nom d'expéditeur :",
        "it": "Nome mittente:", "pt": "Nome do remetente:", "ru": "Имя отправителя:",
    },
    "Scrollback:": {
        "ko": "스크롤백:", "ja": "スクロールバック:", "zh": "回滚:",
        "de": "Rückblättern:", "es": "Desplazamiento inverso:", "fr": "Défilement arrière :",
        "it": "Scrollback:", "pt": "Rolagem:", "ru": "Буфер прокрутки:",
    },
    "Encoding:": {
        "ko": "인코딩:", "ja": "エンコーディング:", "zh": "编码:",
        "de": "Kodierung:", "es": "Codificación:", "fr": "Encodage :",
        "it": "Codifica:", "pt": "Codificação:", "ru": "Кодировка:",
    },
    "Terminal type:": {
        "ko": "터미널 유형:", "ja": "端末種類:", "zh": "终端类型:",
        "de": "Terminaltyp:", "es": "Tipo de terminal:", "fr": "Type de terminal :",
        "it": "Tipo di terminale:", "pt": "Tipo de terminal:", "ru": "Тип терминала:",
    },
    "Log directory:": {
        "ko": "로그 디렉터리:", "ja": "ログディレクトリ:", "zh": "日志目录:",
        "de": "Protokollverzeichnis:", "es": "Directorio de registros:", "fr": "Répertoire des journaux :",
        "it": "Directory log:", "pt": "Diretório de registros:", "ru": "Каталог журналов:",
    },
    "Log settings": {
        "ko": "로그 설정", "ja": "ログ設定", "zh": "日志设置",
        "de": "Protokolleinstellungen", "es": "Configuración de registros", "fr": "Paramètres des journaux",
        "it": "Impostazioni log", "pt": "Configuração de registros", "ru": "Настройки журналов",
    },
    "Host / Device": {
        "ko": "호스트 / 장치", "ja": "ホスト / デバイス", "zh": "主机 / 设备",
        "de": "Host / Gerät", "es": "Host / Dispositivo", "fr": "Hôte / Périphérique",
        "it": "Host / Dispositivo", "pt": "Host / Dispositivo", "ru": "Хост / Устройство",
    },
    "Port / Baud": {
        "ko": "포트 / 보드율", "ja": "ポート / ボーレート", "zh": "端口 / 波特率",
        "de": "Port / baud", "es": "Puerto / baud", "fr": "Port / baud",
        "it": "Porta / baud", "pt": "Porta / baud", "ru": "Порт / baud",
    },
    "User": {
        "ko": "사용자", "ja": "ユーザー", "zh": "用户",
        "de": "Benutzer", "es": "Usuario", "fr": "Utilisateur",
        "it": "Utente", "pt": "Usuário", "ru": "Пользователь",
    },

    # ---- Serial/SSH dialog labels ----
    "SSH": {
        "ko": "SSH", "ja": "SSH", "zh": "SSH",
        "de": "SSH", "es": "SSH", "fr": "SSH",
        "it": "SSH", "pt": "SSH", "ru": "SSH",
    },
    "Serial": {
        "ko": "시리얼", "ja": "シリアル", "zh": "串口",
        "de": "Seriell", "es": "Serie", "fr": "Série",
        "it": "Seriale", "pt": "Serial", "ru": "Последовательный",
    },
    "SSH Sessions": {
        "ko": "SSH 세션", "ja": "SSH セッション", "zh": "SSH 会话",
        "de": "SSH-Sitzungen", "es": "Sesiones SSH", "fr": "Sessions SSH",
        "it": "Sessioni SSH", "pt": "Sessões SSH", "ru": "Сеансы SSH",
    },
    "Serial Sessions": {
        "ko": "시리얼 세션", "ja": "シリアルセッション", "zh": "串口会话",
        "de": "Serielle Sitzungen", "es": "Sesiones serie", "fr": "Sessions série",
        "it": "Sessioni seriali", "pt": "Sessões seriais", "ru": "Последовательные сеансы",
    },
    "&amp;Name:": {
        "ko": "이름(&amp;N):", "ja": "名前(&amp;N):", "zh": "名称(&amp;N):",
        "de": "&amp;Name:", "es": "&amp;Nombre:", "fr": "&amp;Nom :",
        "it": "&amp;Nome:", "pt": "&amp;Nome:", "ru": "&amp;Имя:",
    },
    "&amp;Type:": {
        "ko": "유형(&amp;T):", "ja": "種類(&amp;T):", "zh": "类型(&amp;T):",
        "de": "&amp;Typ:", "es": "&amp;Tipo:", "fr": "&amp;Type :",
        "it": "&amp;Tipo:", "pt": "&amp;Tipo:", "ru": "&amp;Тип:",
    },
    "&amp;Host:": {
        "ko": "호스트(&amp;H):", "ja": "ホスト(&amp;H):", "zh": "主机(&amp;H):",
        "de": "&amp;Host:", "es": "&amp;Host:", "fr": "&amp;Hôte :",
        "it": "&amp;Host:", "pt": "&amp;Host:", "ru": "&amp;Хост:",
    },
    "&amp;Port:": {
        "ko": "포트(&amp;P):", "ja": "ポート(&amp;P):", "zh": "端口(&amp;P):",
        "de": "&amp;Port:", "es": "&amp;Puerto:", "fr": "&amp;Port :",
        "it": "&amp;Porta:", "pt": "&amp;Porta:", "ru": "&amp;Порт:",
    },
    "&amp;Username:": {
        "ko": "사용자명(&amp;U):", "ja": "ユーザー名(&amp;U):", "zh": "用户名(&amp;U):",
        "de": "Ben&amp;utzername:", "es": "Nombre de &amp;usuario:", "fr": "Nom d'&amp;utilisateur :",
        "it": "Nome &amp;utente:", "pt": "Nome de &amp;usuário:", "ru": "Им&amp;я пользователя:",
    },
    "Pass&amp;word:": {
        "ko": "비밀번호(&amp;W):", "ja": "パスワード(&amp;W):", "zh": "密码(&amp;W):",
        "de": "Pass&amp;wort:", "es": "Contra&amp;seña:", "fr": "Mot de &amp;passe :",
        "it": "Pass&amp;word:", "pt": "Sen&amp;ha:", "ru": "Паро&amp;ль:",
    },
    "&amp;Key file:": {
        "ko": "키 파일(&amp;K):", "ja": "鍵ファイル(&amp;K):", "zh": "密钥文件(&amp;K):",
        "de": "Schlüsseldatei (&amp;K):", "es": "Archivo de &amp;clave:", "fr": "Fichier de &amp;clé :",
        "it": "File &amp;chiave:", "pt": "Arquivo de &amp;chave:", "ru": "Файл &amp;ключа:",
    },
    "&amp;Device:": {
        "ko": "장치(&amp;D):", "ja": "デバイス(&amp;D):", "zh": "设备(&amp;D):",
        "de": "&amp;Gerät:", "es": "&amp;Dispositivo:", "fr": "&amp;Périphérique :",
        "it": "&amp;Dispositivo:", "pt": "&amp;Dispositivo:", "ru": "&amp;Устройство:",
    },
    "&amp;Baud rate:": {
        "ko": "보드율(&amp;B):", "ja": "ボーレート(&amp;B):", "zh": "波特率(&amp;B):",
        "de": "&amp;baud-Rate:", "es": "Velocidad en &amp;baud:", "fr": "Vitesse en &amp;baud :",
        "it": "Velocità in &amp;baud:", "pt": "Taxa em &amp;baud:", "ru": "Скорость в &amp;baud:",
    },
    "Data &amp;bits:": {
        "ko": "데이터 비트(&amp;B):", "ja": "データビット(&amp;B):", "zh": "数据位(&amp;B):",
        "de": "Daten&amp;bits:", "es": "&amp;Bits de datos:", "fr": "&amp;Bits de données :",
        "it": "&amp;Bit di dati:", "pt": "&amp;Bits de dados:", "ru": "&amp;Биты данных:",
    },
    "&amp;Stop bits:": {
        "ko": "스톱 비트(&amp;S):", "ja": "ストップビット(&amp;S):", "zh": "停止位(&amp;S):",
        "de": "&amp;Stoppbits:", "es": "Bits de &amp;parada:", "fr": "Bits de &amp;stop :",
        "it": "Bit di &amp;stop:", "pt": "Bits de &amp;parada:", "ru": "&amp;Стоп-биты:",
    },
    "Pa&amp;rity:": {
        "ko": "패리티(&amp;R):", "ja": "パリティ(&amp;R):", "zh": "校验(&amp;R):",
        "de": "Pa&amp;rität:", "es": "Pa&amp;ridad:", "fr": "Pa&amp;rité :",
        "it": "Pa&amp;rità:", "pt": "Pa&amp;ridade:", "ru": "Ч&amp;ётность:",
    },
    "&amp;Flow control:": {
        "ko": "흐름 제어(&amp;F):", "ja": "フロー制御(&amp;F):", "zh": "流控(&amp;F):",
        "de": "&amp;Flusskontrolle:", "es": "Control de &amp;flujo:", "fr": "Contrôle de &amp;flux :",
        "it": "Controllo di &amp;flusso:", "pt": "Controle de &amp;fluxo:", "ru": "Управление &amp;потоком:",
    },
    "None": {
        "ko": "없음", "ja": "なし", "zh": "无",
        "de": "Keine", "es": "Ninguno", "fr": "Aucun",
        "it": "Nessuno", "pt": "Nenhum", "ru": "Нет",
    },
    "Odd": {
        "ko": "홀수", "ja": "奇数", "zh": "奇",
        "de": "Ungerade", "es": "Impar", "fr": "Impair",
        "it": "Dispari", "pt": "Ímpar", "ru": "Нечётный",
    },
    "Even": {
        "ko": "짝수", "ja": "偶数", "zh": "偶",
        "de": "Gerade", "es": "Par", "fr": "Pair",
        "it": "Pari", "pt": "Par", "ru": "Чётный",
    },
    "Hardware": {
        "ko": "하드웨어", "ja": "ハードウェア", "zh": "硬件",
        "de": "Hardware", "es": "Hardware", "fr": "Matériel",
        "it": "Hardware", "pt": "Hardware", "ru": "Аппаратное",
    },
    "Software": {
        "ko": "소프트웨어", "ja": "ソフトウェア", "zh": "软件",
        "de": "Software", "es": "Software", "fr": "Logiciel",
        "it": "Software", "pt": "Software", "ru": "Программное",
    },
    "SSH Password": {
        "ko": "SSH 비밀번호", "ja": "SSH パスワード", "zh": "SSH 密码",
        "de": "SSH-Passwort", "es": "Contraseña SSH", "fr": "Mot de passe SSH",
        "it": "Password SSH", "pt": "Senha SSH", "ru": "Пароль SSH",
    },
    "Password for %1@%2:": {
        "ko": "%1@%2의 비밀번호:", "ja": "%1@%2 のパスワード:", "zh": "%1@%2 的密码:",
        "de": "Passwort für %1@%2:", "es": "Contraseña para %1@%2:", "fr": "Mot de passe pour %1@%2 :",
        "it": "Password per %1@%2:", "pt": "Senha para %1@%2:", "ru": "Пароль для %1@%2:",
    },
    "Key file": {
        "ko": "키 파일", "ja": "鍵ファイル", "zh": "密钥文件",
        "de": "Schlüsseldatei", "es": "Archivo de clave", "fr": "Fichier de clé",
        "it": "File chiave", "pt": "Arquivo de chave", "ru": "Файл ключа",
    },
    "Key files (*.pem *.key);;All files (*)": {
        "ko": "키 파일 (*.pem *.key);;모든 파일 (*)", "ja": "鍵ファイル (*.pem *.key);;すべてのファイル (*)", "zh": "密钥文件 (*.pem *.key);;所有文件 (*)",
        "de": "Schlüsseldateien (*.pem *.key);;Alle Dateien (*)", "es": "Archivos de clave (*.pem *.key);;Todos los archivos (*)", "fr": "Fichiers de clé (*.pem *.key);;Tous les fichiers (*)",
        "it": "File chiave (*.pem *.key);;Tutti i file (*)", "pt": "Arquivos de chave (*.pem *.key);;Todos os arquivos (*)", "ru": "Файлы ключей (*.pem *.key);;Все файлы (*)",
    },
    "Optional private key (OpenSSH .pem)": {
        "ko": "선택 개인 키 (OpenSSH .pem)", "ja": "オプション秘密鍵 (OpenSSH .pem)", "zh": "可选私钥 (OpenSSH .pem)",
        "de": "Optionaler privater Schlüssel (OpenSSH .pem)", "es": "Clave privada opcional (OpenSSH .pem)", "fr": "Clé privée facultative (OpenSSH .pem)",
        "it": "Chiave privata opzionale (OpenSSH .pem)", "pt": "Chave privada opcional (OpenSSH .pem)", "ru": "Необязательный закрытый ключ (OpenSSH .pem)",
    },
    "Select private key": {
        "ko": "개인 키 선택", "ja": "秘密鍵を選択", "zh": "选择私钥",
        "de": "Privaten Schlüssel auswählen", "es": "Seleccionar clave privada", "fr": "Sélectionner une clé privée",
        "it": "Seleziona chiave privata", "pt": "Selecionar chave privada", "ru": "Выбрать закрытый ключ",
    },

    # ---- Buttons / actions ----
    "&amp;Add": {
        "ko": "추가(&amp;A)", "ja": "追加(&amp;A)", "zh": "添加(&amp;A)",
        "de": "&amp;Hinzufügen", "es": "&amp;Añadir", "fr": "&amp;Ajouter",
        "it": "&amp;Aggiungi", "pt": "&amp;Adicionar", "ru": "&amp;Добавить",
    },
    "&amp;Add...": {
        "ko": "추가(&amp;A)...", "ja": "追加(&amp;A)...", "zh": "添加(&amp;A)...",
        "de": "&amp;Hinzufügen...", "es": "&amp;Añadir...", "fr": "&amp;Ajouter...",
        "it": "&amp;Aggiungi...", "pt": "&amp;Adicionar...", "ru": "&amp;Добавить...",
    },
    "&amp;Edit...": {
        "ko": "편집(&amp;E)...", "ja": "編集(&amp;E)...", "zh": "编辑(&amp;E)...",
        "de": "&amp;Bearbeiten...", "es": "&amp;Editar...", "fr": "&amp;Modifier...",
        "it": "&amp;Modifica...", "pt": "&amp;Editar...", "ru": "&amp;Изменить...",
    },
    "&amp;Delete": {
        "ko": "삭제(&amp;D)", "ja": "削除(&amp;D)", "zh": "删除(&amp;D)",
        "de": "&amp;Löschen", "es": "&amp;Eliminar", "fr": "&amp;Supprimer",
        "it": "&amp;Elimina", "pt": "&amp;Excluir", "ru": "&amp;Удалить",
    },
    "&amp;Copy": {
        "ko": "복사(&amp;C)", "ja": "コピー(&amp;C)", "zh": "复制(&amp;C)",
        "de": "&amp;Kopieren", "es": "&amp;Copiar", "fr": "&amp;Copier",
        "it": "&amp;Copia", "pt": "&amp;Copiar", "ru": "&amp;Копировать",
    },
    "&amp;Paste": {
        "ko": "붙여넣기(&amp;P)", "ja": "貼り付け(&amp;P)", "zh": "粘贴(&amp;P)",
        "de": "&amp;Einfügen", "es": "&amp;Pegar", "fr": "C&amp;oller",
        "it": "&amp;Incolla", "pt": "&amp;Colar", "ru": "&amp;Вставить",
    },
    "Add": {
        "ko": "추가", "ja": "追加", "zh": "添加",
        "de": "Hinzufügen", "es": "Añadir", "fr": "Ajouter",
        "it": "Aggiungi", "pt": "Adicionar", "ru": "Добавить",
    },
    "Edit": {
        "ko": "편집", "ja": "編集", "zh": "编辑",
        "de": "Bearbeiten", "es": "Editar", "fr": "Modifier",
        "it": "Modifica", "pt": "Editar", "ru": "Изменить",
    },
    "Edit...": {
        "ko": "편집...", "ja": "編集...", "zh": "编辑...",
        "de": "Bearbeiten...", "es": "Editar...", "fr": "Modifier...",
        "it": "Modifica...", "pt": "Editar...", "ru": "Изменить...",
    },
    "Copy": {
        "ko": "복사", "ja": "コピー", "zh": "复制",
        "de": "Kopieren", "es": "Copiar", "fr": "Copier",
        "it": "Copia", "pt": "Copiar", "ru": "Копировать",
    },
    "Paste": {
        "ko": "붙여넣기", "ja": "貼り付け", "zh": "粘贴",
        "de": "Einfügen", "es": "Pegar", "fr": "Coller",
        "it": "Incolla", "pt": "Colar", "ru": "Вставить",
    },
    "Rename": {
        "ko": "이름 바꾸기", "ja": "名前を変更", "zh": "重命名",
        "de": "Umbenennen", "es": "Renombrar", "fr": "Renommer",
        "it": "Rinomina", "pt": "Renomear", "ru": "Переименовать",
    },
    "Rename...": {
        "ko": "이름 바꾸기...", "ja": "名前を変更...", "zh": "重命名...",
        "de": "Umbenennen...", "es": "Renombrar...", "fr": "Renommer...",
        "it": "Rinomina...", "pt": "Renomear...", "ru": "Переименовать...",
    },
    "Rename Tab": {
        "ko": "탭 이름 바꾸기", "ja": "タブ名を変更", "zh": "重命名标签",
        "de": "Tab umbenennen", "es": "Renombrar pestaña", "fr": "Renommer l'onglet",
        "it": "Rinomina scheda", "pt": "Renomear aba", "ru": "Переименовать вкладку",
    },
    "Rename session": {
        "ko": "세션 이름 바꾸기", "ja": "セッション名を変更", "zh": "重命名会话",
        "de": "Sitzung umbenennen", "es": "Renombrar sesión", "fr": "Renommer la session",
        "it": "Rinomina sessione", "pt": "Renomear sessão", "ru": "Переименовать сеанс",
    },
    "Duplicate": {
        "ko": "복제", "ja": "複製", "zh": "复制",
        "de": "Duplizieren", "es": "Duplicar", "fr": "Dupliquer",
        "it": "Duplica", "pt": "Duplicar", "ru": "Дублировать",
    },
    "Pin": {
        "ko": "고정", "ja": "ピン留め", "zh": "固定",
        "de": "Anheften", "es": "Fijar", "fr": "Épingler",
        "it": "Fissa", "pt": "Fixar", "ru": "Закрепить",
    },
    "Unpin": {
        "ko": "고정 해제", "ja": "ピン留め解除", "zh": "取消固定",
        "de": "Lösen", "es": "Desfijar", "fr": "Détacher",
        "it": "Rimuovi", "pt": "Desafixar", "ru": "Открепить",
    },
    "Detach to New Window": {
        "ko": "새 창으로 분리",
        "ja": "新しいウィンドウに切り離す",
        "zh": "分离到新窗口",
        "de": "In neues Fenster lösen",
        "es": "Separar en nueva ventana",
        "fr": "Détacher dans une nouvelle fenêtre",
        "it": "Stacca in nuova finestra",
        "pt": "Destacar em nova janela",
        "ru": "Открепить в новое окно",
    },
    "Close": {
        "ko": "닫기", "ja": "閉じる", "zh": "关闭",
        "de": "Schließen", "es": "Cerrar", "fr": "Fermer",
        "it": "Chiudi", "pt": "Fechar", "ru": "Закрыть",
    },
    "Browse...": {
        "ko": "찾아보기...", "ja": "参照...", "zh": "浏览...",
        "de": "Durchsuchen...", "es": "Examinar...", "fr": "Parcourir...",
        "it": "Sfoglia...", "pt": "Procurar...", "ru": "Обзор...",
    },
    "Add command": {
        "ko": "명령 추가", "ja": "コマンド追加", "zh": "添加命令",
        "de": "Befehl hinzufügen", "es": "Añadir comando", "fr": "Ajouter une commande",
        "it": "Aggiungi comando", "pt": "Adicionar comando", "ru": "Добавить команду",
    },
    "Delete command": {
        "ko": "명령 삭제", "ja": "コマンド削除", "zh": "删除命令",
        "de": "Befehl löschen", "es": "Eliminar comando", "fr": "Supprimer la commande",
        "it": "Elimina comando", "pt": "Excluir comando", "ru": "Удалить команду",
    },
    "Send test email...": {
        "ko": "테스트 메일 보내기...", "ja": "テストメール送信...", "zh": "发送测试邮件...",
        "de": "Test-E-Mail senden...", "es": "Enviar correo de prueba...", "fr": "Envoyer un e-mail de test...",
        "it": "Invia email di test...", "pt": "Enviar e-mail de teste...", "ru": "Отправить тестовое письмо...",
    },
    "Send test email to:": {
        "ko": "테스트 메일 받을 주소:", "ja": "テストメール送信先:", "zh": "测试邮件接收人:",
        "de": "Test-E-Mail senden an:", "es": "Enviar correo de prueba a:", "fr": "Envoyer l'e-mail de test à :",
        "it": "Invia email di test a:", "pt": "Enviar e-mail de teste para:", "ru": "Отправить тестовое письмо на:",
    },
    "Clear &amp;screen": {
        "ko": "화면 지우기(&amp;S)", "ja": "画面クリア(&amp;S)", "zh": "清屏(&amp;S)",
        "de": "Bild&amp;schirm löschen", "es": "Limpiar &amp;pantalla", "fr": "Effacer l'é&amp;cran",
        "it": "Pulisci &amp;schermo", "pt": "Limpar &amp;tela", "ru": "Очистить &amp;экран",
    },

    # ---- Session Manager ----
    "Session Manager": {
        "ko": "세션 관리자", "ja": "セッションマネージャー", "zh": "会话管理器",
        "de": "Sitzungsmanager", "es": "Administrador de sesiones", "fr": "Gestionnaire de sessions",
        "it": "Gestore sessioni", "pt": "Gerenciador de sessões", "ru": "Менеджер сеансов",
    },
    "Save session": {
        "ko": "세션 저장", "ja": "セッション保存", "zh": "保存会话",
        "de": "Sitzung speichern", "es": "Guardar sesión", "fr": "Enregistrer la session",
        "it": "Salva sessione", "pt": "Salvar sessão", "ru": "Сохранить сеанс",
    },
    "Save session log": {
        "ko": "세션 로그 저장", "ja": "セッションログ保存", "zh": "保存会话日志",
        "de": "Sitzungsprotokoll speichern", "es": "Guardar registro de sesión", "fr": "Enregistrer le journal de session",
        "it": "Salva log sessione", "pt": "Salvar registro da sessão", "ru": "Сохранить журнал сеанса",
    },
    "Save failed": {
        "ko": "저장 실패", "ja": "保存失敗", "zh": "保存失败",
        "de": "Speichern fehlgeschlagen", "es": "Error al guardar", "fr": "Échec de l'enregistrement",
        "it": "Salvataggio non riuscito", "pt": "Falha ao salvar", "ru": "Ошибка сохранения",
    },
    "Delete session": {
        "ko": "세션 삭제", "ja": "セッション削除", "zh": "删除会话",
        "de": "Sitzung löschen", "es": "Eliminar sesión", "fr": "Supprimer la session",
        "it": "Elimina sessione", "pt": "Excluir sessão", "ru": "Удалить сеанс",
    },
    "Delete session &quot;%1&quot;?": {
        "ko": "세션 &quot;%1&quot;을(를) 삭제하시겠습니까?", "ja": "セッション &quot;%1&quot; を削除しますか?", "zh": "删除会话 &quot;%1&quot;?",
        "de": "Sitzung &quot;%1&quot; löschen?", "es": "¿Eliminar la sesión &quot;%1&quot;?", "fr": "Supprimer la session &quot;%1&quot; ?",
        "it": "Eliminare la sessione &quot;%1&quot;?", "pt": "Excluir a sessão &quot;%1&quot;?", "ru": "Удалить сеанс &quot;%1&quot;?",
    },
    "Session error": {
        "ko": "세션 오류", "ja": "セッションエラー", "zh": "会话错误",
        "de": "Sitzungsfehler", "es": "Error de sesión", "fr": "Erreur de session",
        "it": "Errore sessione", "pt": "Erro de sessão", "ru": "Ошибка сеанса",
    },
    "Session name is required.": {
        "ko": "세션 이름이 필요합니다.", "ja": "セッション名が必要です。", "zh": "需要会话名称。",
        "de": "Sitzungsname ist erforderlich.", "es": "Se requiere el nombre de la sesión.", "fr": "Le nom de la session est requis.",
        "it": "Il nome della sessione è obbligatorio.", "pt": "O nome da sessão é obrigatório.", "ru": "Требуется имя сеанса.",
    },
    "Session log directory": {
        "ko": "세션 로그 디렉터리", "ja": "セッションログディレクトリ", "zh": "会话日志目录",
        "de": "Sitzungsprotokoll-Verzeichnis", "es": "Directorio de registros de sesión", "fr": "Répertoire des journaux de session",
        "it": "Directory log sessione", "pt": "Diretório de registros de sessão", "ru": "Каталог журналов сеанса",
    },
    "Session logs (and snapshot captures) are saved under the directory below. Each session writes its own timestamped file; snapshots go into a &lt;b&gt;snapshots/&lt;/b&gt; subfolder.": {
        "ko": "세션 로그(와 스냅샷 캡처)는 아래 디렉터리에 저장됩니다. 각 세션은 타임스탬프가 붙은 파일을 기록하며, 스냅샷은 &lt;b&gt;snapshots/&lt;/b&gt; 하위 폴더에 저장됩니다.",
        "ja": "セッションログ(およびスナップショット)は以下のディレクトリに保存されます。各セッションはタイムスタンプ付きファイルを作成し、スナップショットは &lt;b&gt;snapshots/&lt;/b&gt; サブフォルダに保存されます。",
        "zh": "会话日志(和快照)保存在下面的目录中。每个会话写入带时间戳的文件,快照保存在 &lt;b&gt;snapshots/&lt;/b&gt; 子文件夹中。",
        "de": "Sitzungsprotokolle (und Schnappschuss-Aufnahmen) werden im unten angegebenen Verzeichnis gespeichert. Jede Sitzung schreibt eine eigene zeitgestempelte Datei; Schnappschüsse landen im Unterordner &lt;b&gt;snapshots/&lt;/b&gt;.",
        "es": "Los registros de sesión (y las capturas de instantáneas) se guardan en el directorio siguiente. Cada sesión escribe su propio archivo con marca de tiempo; las instantáneas se guardan en la subcarpeta &lt;b&gt;snapshots/&lt;/b&gt;.",
        "fr": "Les journaux de session (et les captures d'instantanés) sont enregistrés dans le répertoire ci-dessous. Chaque session écrit son propre fichier horodaté ; les instantanés vont dans un sous-dossier &lt;b&gt;snapshots/&lt;/b&gt;.",
        "it": "I log di sessione (e le catture delle istantanee) vengono salvati nella directory sottostante. Ogni sessione scrive il proprio file con marca temporale; le istantanee vanno nella sottocartella &lt;b&gt;snapshots/&lt;/b&gt;.",
        "pt": "Os registros de sessão (e capturas de instantâneos) são salvos no diretório abaixo. Cada sessão grava seu próprio arquivo com data e hora; os instantâneos vão para uma subpasta &lt;b&gt;snapshots/&lt;/b&gt;.",
        "ru": "Журналы сеансов (и снимки) сохраняются в указанном ниже каталоге. Каждый сеанс пишет собственный файл с меткой времени; снимки помещаются во вложенную папку &lt;b&gt;snapshots/&lt;/b&gt;.",
    },

    # ---- Snapshot dialog ----
    "e.g. TSCRT {session} {snapshot} {timestamp}": {
        "ko": "예: TSCRT {session} {snapshot} {timestamp}", "ja": "例: TSCRT {session} {snapshot} {timestamp}", "zh": "例: TSCRT {session} {snapshot} {timestamp}",
        "de": "z. B. TSCRT {session} {snapshot} {timestamp}", "es": "p. ej. TSCRT {session} {snapshot} {timestamp}", "fr": "p. ex. TSCRT {session} {snapshot} {timestamp}",
        "it": "es. TSCRT {session} {snapshot} {timestamp}", "pt": "ex.: TSCRT {session} {snapshot} {timestamp}", "ru": "напр. TSCRT {session} {snapshot} {timestamp}",
    },
    "Send email when finished": {
        "ko": "완료 시 메일 전송", "ja": "完了時にメール送信", "zh": "完成时发送邮件",
        "de": "E-Mail senden, wenn fertig", "es": "Enviar correo al finalizar", "fr": "Envoyer un e-mail à la fin",
        "it": "Invia email al termine", "pt": "Enviar e-mail ao concluir", "ru": "Отправить письмо по завершении",
    },
    "Attach as file (otherwise inline)": {
        "ko": "파일로 첨부(그렇지 않으면 본문)", "ja": "ファイルとして添付(そうでなければ本文)", "zh": "作为文件附加(否则内联)",
        "de": "Als Datei anhängen (sonst inline)", "es": "Adjuntar como archivo (si no, en línea)", "fr": "Joindre en tant que fichier (sinon inline)",
        "it": "Allega come file (altrimenti inline)", "pt": "Anexar como arquivo (caso contrário, inline)", "ru": "Прикрепить как файл (иначе в теле письма)",
    },
    "Commands (sent in order; escape sequences: \\r \\n \\t \\e):": {
        "ko": "명령 (순서대로 전송; 이스케이프: \\r \\n \\t \\e):",
        "ja": "コマンド (順番に送信; エスケープ: \\r \\n \\t \\e):",
        "zh": "命令 (按顺序发送; 转义: \\r \\n \\t \\e):",
        "de": "Befehle (werden der Reihe nach gesendet; Escape-Sequenzen: \\r \\n \\t \\e):",
        "es": "Comandos (enviados en orden; secuencias de escape: \\r \\n \\t \\e):",
        "fr": "Commandes (envoyées dans l'ordre ; séquences d'échappement : \\r \\n \\t \\e) :",
        "it": "Comandi (inviati in ordine; sequenze di escape: \\r \\n \\t \\e):",
        "pt": "Comandos (enviados em ordem; sequências de escape: \\r \\n \\t \\e):",
        "ru": "Команды (отправляются по порядку; escape-последовательности: \\r \\n \\t \\e):",
    },
    "Recipients (one per line):": {
        "ko": "수신자 (한 줄에 하나):", "ja": "宛先 (1 行 1 件):", "zh": "收件人 (每行一个):",
        "de": "Empfänger (einer pro Zeile):", "es": "Destinatarios (uno por línea):", "fr": "Destinataires (un par ligne) :",
        "it": "Destinatari (uno per riga):", "pt": "Destinatários (um por linha):", "ru": "Получатели (по одному в строке):",
    },
    "Rules fire a registered snapshot automatically. Kinds: on_connect, cron, pattern. Leave Session blank to apply to every session.": {
        "ko": "규칙은 등록된 스냅샷을 자동으로 실행합니다. 종류: on_connect, cron, pattern. 세션을 비워 두면 모든 세션에 적용됩니다.",
        "ja": "ルールは登録されたスナップショットを自動実行します。種別: on_connect, cron, pattern. セッションを空欄にするとすべてのセッションに適用されます。",
        "zh": "规则自动运行已注册的快照。类型: on_connect, cron, pattern. 留空会话则应用于所有会话。",
        "de": "Regeln lösen automatisch einen registrierten Schnappschuss aus. Arten: on_connect, cron, pattern. Lassen Sie Sitzung leer, um auf alle Sitzungen anzuwenden.",
        "es": "Las reglas ejecutan una instantánea registrada automáticamente. Clases: on_connect, cron, pattern. Deje Sesión en blanco para aplicar a todas las sesiones.",
        "fr": "Les règles déclenchent automatiquement un instantané enregistré. Genres : on_connect, cron, pattern. Laissez Session vide pour appliquer à toutes les sessions.",
        "it": "Le regole eseguono automaticamente un'istantanea registrata. Generi: on_connect, cron, pattern. Lasciare Sessione vuota per applicarla a tutte le sessioni.",
        "pt": "As regras disparam um instantâneo registrado automaticamente. Espécies: on_connect, cron, pattern. Deixe Sessão em branco para aplicar a todas as sessões.",
        "ru": "Правила автоматически запускают зарегистрированный снимок. Виды: on_connect, cron, pattern. Оставьте поле Сеанс пустым, чтобы применить ко всем сеансам.",
    },
    "Run Snapshot": {
        "ko": "스냅샷 실행", "ja": "スナップショット実行", "zh": "运行快照",
        "de": "Schnappschuss ausführen", "es": "Ejecutar instantánea", "fr": "Exécuter l'instantané",
        "it": "Esegui istantanea", "pt": "Executar instantâneo", "ru": "Запустить снимок",
    },

    # ---- SMTP ----
    "STARTTLS": {
        "ko": "STARTTLS", "ja": "STARTTLS", "zh": "STARTTLS",
        "de": "STARTTLS", "es": "STARTTLS", "fr": "STARTTLS",
        "it": "STARTTLS", "pt": "STARTTLS", "ru": "STARTTLS",
    },
    "SMTPS (implicit TLS)": {
        "ko": "SMTPS (암시적 TLS)", "ja": "SMTPS (暗黙的 TLS)", "zh": "SMTPS (隐式 TLS)",
        "de": "SMTPS (implizites TLS)", "es": "SMTPS (TLS implícito)", "fr": "SMTPS (TLS implicite)",
        "it": "SMTPS (TLS implicito)", "pt": "SMTPS (TLS implícito)", "ru": "SMTPS (неявный TLS)",
    },
    "SMTP test": {
        "ko": "SMTP 테스트", "ja": "SMTP テスト", "zh": "SMTP 测试",
        "de": "SMTP-Test", "es": "Prueba SMTP", "fr": "Test SMTP",
        "it": "Test SMTP", "pt": "Teste SMTP", "ru": "Тест SMTP",
    },
    "SMTP client is busy": {
        "ko": "SMTP 클라이언트 사용 중", "ja": "SMTP クライアント使用中", "zh": "SMTP 客户端忙",
        "de": "SMTP-Client ist beschäftigt", "es": "El cliente SMTP está ocupado", "fr": "Le client SMTP est occupé",
        "it": "Il client SMTP è occupato", "pt": "O cliente SMTP está ocupado", "ru": "SMTP-клиент занят",
    },
    "SMTP host is not configured": {
        "ko": "SMTP 호스트가 설정되지 않음", "ja": "SMTP ホストが未設定", "zh": "SMTP 主机未配置",
        "de": "SMTP-Host ist nicht konfiguriert", "es": "El servidor SMTP no está configurado", "fr": "L'hôte SMTP n'est pas configuré",
        "it": "L'host SMTP non è configurato", "pt": "O host SMTP não está configurado", "ru": "SMTP-хост не настроен",
    },
    "SMTP host not configured — email skipped": {
        "ko": "SMTP 호스트가 설정되지 않음 — 메일 전송 생략",
        "ja": "SMTP ホスト未設定 — メール送信をスキップ",
        "zh": "SMTP 主机未配置 — 跳过邮件",
        "de": "SMTP-Host nicht konfiguriert — E-Mail übersprungen",
        "es": "Servidor SMTP no configurado — correo omitido",
        "fr": "Hôte SMTP non configuré — e-mail ignoré",
        "it": "Host SMTP non configurato — email saltata",
        "pt": "Host SMTP não configurado — e-mail ignorado",
        "ru": "SMTP-хост не настроен — письмо пропущено",
    },
    "SMTP timeout": {
        "ko": "SMTP 타임아웃", "ja": "SMTP タイムアウト", "zh": "SMTP 超时",
        "de": "SMTP-Zeitüberschreitung", "es": "Tiempo de espera de SMTP", "fr": "Délai SMTP dépassé",
        "it": "Timeout SMTP", "pt": "Tempo limite de SMTP", "ru": "Тайм-аут SMTP",
    },
    "No recipients": {
        "ko": "수신자 없음", "ja": "宛先なし", "zh": "无收件人",
        "de": "Keine Empfänger", "es": "Sin destinatarios", "fr": "Aucun destinataire",
        "it": "Nessun destinatario", "pt": "Sem destinatários", "ru": "Нет получателей",
    },
    "Fill in host and From address first.": {
        "ko": "호스트와 보내는 주소를 먼저 입력하세요.", "ja": "ホストと送信元アドレスを先に入力してください。", "zh": "请先填写主机和发件人地址。",
        "de": "Bitte zuerst Host und Absenderadresse ausfüllen.", "es": "Primero complete el servidor y la dirección del remitente.", "fr": "Veuillez d'abord renseigner l'hôte et l'adresse d'expéditeur.",
        "it": "Compilare prima host e indirizzo mittente.", "pt": "Preencha primeiro o host e o endereço do remetente.", "ru": "Сначала заполните хост и адрес отправителя.",
    },
    "Test email sent to %1": {
        "ko": "테스트 메일이 %1로 전송됨", "ja": "テストメールを %1 に送信しました", "zh": "测试邮件已发送至 %1",
        "de": "Test-E-Mail an %1 gesendet", "es": "Correo de prueba enviado a %1", "fr": "E-mail de test envoyé à %1",
        "it": "Email di test inviata a %1", "pt": "E-mail de teste enviado para %1", "ru": "Тестовое письмо отправлено на %1",
    },
    "Send failed: %1": {
        "ko": "전송 실패: %1", "ja": "送信失敗: %1", "zh": "发送失败: %1",
        "de": "Senden fehlgeschlagen: %1", "es": "Error al enviar: %1", "fr": "Échec de l'envoi : %1",
        "it": "Invio non riuscito: %1", "pt": "Falha no envio: %1", "ru": "Ошибка отправки: %1",
    },
    "Email sent: %1": {
        "ko": "메일 전송됨: %1", "ja": "メール送信: %1", "zh": "邮件已发送: %1",
        "de": "E-Mail gesendet: %1", "es": "Correo enviado: %1", "fr": "E-mail envoyé : %1",
        "it": "Email inviata: %1", "pt": "E-mail enviado: %1", "ru": "Письмо отправлено: %1",
    },
    "Email failed: %1": {
        "ko": "메일 실패: %1", "ja": "メール失敗: %1", "zh": "邮件失败: %1",
        "de": "E-Mail fehlgeschlagen: %1", "es": "Error de correo: %1", "fr": "Échec de l'e-mail : %1",
        "it": "Email non riuscita: %1", "pt": "Falha no e-mail: %1", "ru": "Ошибка письма: %1",
    },

    # ---- Import/export ----
    "Export": {
        "ko": "내보내기", "ja": "エクスポート", "zh": "导出",
        "de": "Exportieren", "es": "Exportar", "fr": "Exporter",
        "it": "Esporta", "pt": "Exportar", "ru": "Экспорт",
    },
    "Import": {
        "ko": "가져오기", "ja": "インポート", "zh": "导入",
        "de": "Importieren", "es": "Importar", "fr": "Importer",
        "it": "Importa", "pt": "Importar", "ru": "Импорт",
    },
    "Export profile": {
        "ko": "프로파일 내보내기", "ja": "プロファイルエクスポート", "zh": "导出配置",
        "de": "Profil exportieren", "es": "Exportar perfil", "fr": "Exporter le profil",
        "it": "Esporta profilo", "pt": "Exportar perfil", "ru": "Экспортировать профиль",
    },
    "Import profile": {
        "ko": "프로파일 가져오기", "ja": "プロファイルインポート", "zh": "导入配置",
        "de": "Profil importieren", "es": "Importar perfil", "fr": "Importer le profil",
        "it": "Importa profilo", "pt": "Importar perfil", "ru": "Импортировать профиль",
    },
    "Export sessions": {
        "ko": "세션 내보내기", "ja": "セッションエクスポート", "zh": "导出会话",
        "de": "Sitzungen exportieren", "es": "Exportar sesiones", "fr": "Exporter les sessions",
        "it": "Esporta sessioni", "pt": "Exportar sessões", "ru": "Экспортировать сеансы",
    },
    "Import sessions": {
        "ko": "세션 가져오기", "ja": "セッションインポート", "zh": "导入会话",
        "de": "Sitzungen importieren", "es": "Importar sesiones", "fr": "Importer les sessions",
        "it": "Importa sessioni", "pt": "Importar sessões", "ru": "Импортировать сеансы",
    },
    "Export snapshots": {
        "ko": "스냅샷 내보내기", "ja": "スナップショットエクスポート", "zh": "导出快照",
        "de": "Schnappschüsse exportieren", "es": "Exportar instantáneas", "fr": "Exporter les instantanés",
        "it": "Esporta istantanee", "pt": "Exportar instantâneos", "ru": "Экспортировать снимки",
    },
    "Import snapshots": {
        "ko": "스냅샷 가져오기", "ja": "スナップショットインポート", "zh": "导入快照",
        "de": "Schnappschüsse importieren", "es": "Importar instantáneas", "fr": "Importer les instantanés",
        "it": "Importa istantanee", "pt": "Importar instantâneos", "ru": "Импортировать снимки",
    },
    "Exported: %1": {
        "ko": "내보냄: %1", "ja": "エクスポート: %1", "zh": "已导出: %1",
        "de": "Exportiert: %1", "es": "Exportado: %1", "fr": "Exporté : %1",
        "it": "Esportato: %1", "pt": "Exportado: %1", "ru": "Экспортировано: %1",
    },
    "TSCRT profile (*.profile);;All files (*)": {
        "ko": "TSCRT 프로파일 (*.profile);;모든 파일 (*)", "ja": "TSCRT プロファイル (*.profile);;すべてのファイル (*)", "zh": "TSCRT 配置 (*.profile);;所有文件 (*)",
        "de": "TSCRT-Profil (*.profile);;Alle Dateien (*)", "es": "Perfil TSCRT (*.profile);;Todos los archivos (*)", "fr": "Profil TSCRT (*.profile);;Tous les fichiers (*)",
        "it": "Profilo TSCRT (*.profile);;Tutti i file (*)", "pt": "Perfil TSCRT (*.profile);;Todos os arquivos (*)", "ru": "Профиль TSCRT (*.profile);;Все файлы (*)",
    },

    # ---- Fullscreen / per-session toggles ----
    "Show command line in fullscreen": {
        "ko": "전체 화면에서 명령줄 표시", "ja": "全画面でコマンドラインを表示", "zh": "全屏时显示命令行",
        "de": "Befehlszeile im Vollbildmodus anzeigen", "es": "Mostrar línea de comandos en pantalla completa", "fr": "Afficher la ligne de commande en plein écran",
        "it": "Mostra riga di comando a schermo intero", "pt": "Mostrar linha de comando em tela cheia", "ru": "Показывать командную строку в полноэкранном режиме",
    },
    "Show button bar in fullscreen": {
        "ko": "전체 화면에서 버튼 바 표시", "ja": "全画面でボタンバーを表示", "zh": "全屏时显示按钮栏",
        "de": "Schaltflächenleiste im Vollbildmodus anzeigen", "es": "Mostrar barra de botones en pantalla completa", "fr": "Afficher la barre de boutons en plein écran",
        "it": "Mostra barra dei pulsanti a schermo intero", "pt": "Mostrar barra de botões em tela cheia", "ru": "Показывать панель кнопок в полноэкранном режиме",
    },

    # ---- Misc common messages ----
    "Limit": {
        "ko": "한도", "ja": "上限", "zh": "上限",
        "de": "Grenze", "es": "Límite", "fr": "Limite",
        "it": "Limite", "pt": "Limite", "ru": "Предел",
    },
    "Limit reached": {
        "ko": "한도 도달", "ja": "上限に達しました", "zh": "已达上限",
        "de": "Grenze erreicht", "es": "Límite alcanzado", "fr": "Limite atteinte",
        "it": "Limite raggiunto", "pt": "Limite atingido", "ru": "Достигнут предел",
    },
    "Invalid": {
        "ko": "잘못됨", "ja": "無効", "zh": "无效",
        "de": "Ungültig", "es": "No válido", "fr": "Non valide",
        "it": "Non valido", "pt": "Inválido", "ru": "Недопустимо",
    },
    "Profile error": {
        "ko": "프로파일 오류", "ja": "プロファイルエラー", "zh": "配置错误",
        "de": "Profilfehler", "es": "Error de perfil", "fr": "Erreur de profil",
        "it": "Errore profilo", "pt": "Erro de perfil", "ru": "Ошибка профиля",
    },
    "Cannot initialize profile directory.": {
        "ko": "프로파일 디렉터리를 초기화할 수 없습니다.", "ja": "プロファイルディレクトリを初期化できません。", "zh": "无法初始化配置目录。",
        "de": "Profilverzeichnis kann nicht initialisiert werden.", "es": "No se puede inicializar el directorio del perfil.", "fr": "Impossible d'initialiser le répertoire du profil.",
        "it": "Impossibile inizializzare la directory del profilo.", "pt": "Não é possível inicializar o diretório do perfil.", "ru": "Не удалось инициализировать каталог профиля.",
    },
    "Failed to load profile; defaults will be used.": {
        "ko": "프로파일 로드 실패. 기본값을 사용합니다.", "ja": "プロファイル読込失敗。既定値を使用します。", "zh": "加载配置失败,将使用默认值。",
        "de": "Profil konnte nicht geladen werden; Standardwerte werden verwendet.", "es": "Error al cargar el perfil; se usarán los valores predeterminados.", "fr": "Impossible de charger le profil ; les valeurs par défaut seront utilisées.",
        "it": "Impossibile caricare il profilo; verranno usati i valori predefiniti.", "pt": "Falha ao carregar o perfil; os padrões serão usados.", "ru": "Не удалось загрузить профиль; будут использованы значения по умолчанию.",
    },
    "Name cannot be empty.": {
        "ko": "이름이 비어 있을 수 없습니다.", "ja": "名前を空にできません。", "zh": "名称不能为空。",
        "de": "Name darf nicht leer sein.", "es": "El nombre no puede estar vacío.", "fr": "Le nom ne peut pas être vide.",
        "it": "Il nome non può essere vuoto.", "pt": "O nome não pode estar vazio.", "ru": "Имя не может быть пустым.",
    },
    "Edit button": {
        "ko": "버튼 편집", "ja": "ボタン編集", "zh": "编辑按钮",
        "de": "Schaltfläche bearbeiten", "es": "Editar botón", "fr": "Modifier le bouton",
        "it": "Modifica pulsante", "pt": "Editar botão", "ru": "Изменить кнопку",
    },
    "Edit button...": {
        "ko": "버튼 편집...", "ja": "ボタン編集...", "zh": "编辑按钮...",
        "de": "Schaltfläche bearbeiten...", "es": "Editar botón...", "fr": "Modifier le bouton...",
        "it": "Modifica pulsante...", "pt": "Editar botão...", "ru": "Изменить кнопку...",
    },
    "Action (escape: \\r \\n \\t \\b \\e \\p \\\\):": {
        "ko": "액션 (이스케이프: \\r \\n \\t \\b \\e \\p \\\\):",
        "ja": "アクション (エスケープ: \\r \\n \\t \\b \\e \\p \\\\):",
        "zh": "动作 (转义: \\r \\n \\t \\b \\e \\p \\\\):",
        "de": "Aktion (Escape: \\r \\n \\t \\b \\e \\p \\\\):",
        "es": "Acción (escape: \\r \\n \\t \\b \\e \\p \\\\):",
        "fr": "Action (échappement : \\r \\n \\t \\b \\e \\p \\\\) :",
        "it": "Azione (escape: \\r \\n \\t \\b \\e \\p \\\\):",
        "pt": "Ação (escape: \\r \\n \\t \\b \\e \\p \\\\):",
        "ru": "Действие (escape: \\r \\n \\t \\b \\e \\p \\\\):",
    },
    "Bottom-bar buttons. Escape sequences: \\r \\n \\t \\b \\e \\p \\\\": {
        "ko": "하단 바 버튼. 이스케이프 시퀀스: \\r \\n \\t \\b \\e \\p \\\\",
        "ja": "下部バーボタン。エスケープシーケンス: \\r \\n \\t \\b \\e \\p \\\\",
        "zh": "底部栏按钮。转义序列: \\r \\n \\t \\b \\e \\p \\\\",
        "de": "Schaltflächen in der unteren Leiste. Escape-Sequenzen: \\r \\n \\t \\b \\e \\p \\\\",
        "es": "Botones de la barra inferior. Secuencias de escape: \\r \\n \\t \\b \\e \\p \\\\",
        "fr": "Boutons de la barre inférieure. Séquences d'échappement : \\r \\n \\t \\b \\e \\p \\\\",
        "it": "Pulsanti della barra inferiore. Sequenze di escape: \\r \\n \\t \\b \\e \\p \\\\",
        "pt": "Botões da barra inferior. Sequências de escape: \\r \\n \\t \\b \\e \\p \\\\",
        "ru": "Кнопки нижней панели. Escape-последовательности: \\r \\n \\t \\b \\e \\p \\\\",
    },
    "Tab name:": {
        "ko": "탭 이름:", "ja": "タブ名:", "zh": "标签名:",
        "de": "Tab-Name:", "es": "Nombre de pestaña:", "fr": "Nom de l'onglet :",
        "it": "Nome scheda:", "pt": "Nome da aba:", "ru": "Имя вкладки:",
    },
    "Maximum number of sessions (%1) reached.": {
        "ko": "최대 세션 수(%1)에 도달했습니다.", "ja": "最大セッション数 (%1) に達しました。", "zh": "已达到最大会话数 (%1)。",
        "de": "Maximale Anzahl an Sitzungen (%1) erreicht.", "es": "Se alcanzó el número máximo de sesiones (%1).", "fr": "Nombre maximal de sessions (%1) atteint.",
        "it": "Raggiunto il numero massimo di sessioni (%1).", "pt": "Número máximo de sessões (%1) atingido.", "ru": "Достигнуто максимальное количество сеансов (%1).",
    },
    "Maximum snapshot count reached.": {
        "ko": "최대 스냅샷 수에 도달했습니다.", "ja": "最大スナップショット数に達しました。", "zh": "已达到最大快照数。",
        "de": "Maximale Anzahl an Schnappschüssen erreicht.", "es": "Se alcanzó el número máximo de instantáneas.", "fr": "Nombre maximal d'instantanés atteint.",
        "it": "Raggiunto il numero massimo di istantanee.", "pt": "Número máximo de instantâneos atingido.", "ru": "Достигнуто максимальное количество снимков.",
    },
    "Maximum periodic entries reached.": {
        "ko": "최대 주기 항목 수에 도달했습니다.", "ja": "最大定期実行数に達しました。", "zh": "已达到最大定时项目数。",
        "de": "Maximale Anzahl periodischer Einträge erreicht.", "es": "Se alcanzó el número máximo de entradas periódicas.", "fr": "Nombre maximal d'entrées périodiques atteint.",
        "it": "Raggiunto il numero massimo di voci periodiche.", "pt": "Número máximo de entradas periódicas atingido.", "ru": "Достигнуто максимальное количество периодических записей.",
    },
    "Maximum startup entries reached.": {
        "ko": "최대 시작 항목 수에 도달했습니다.", "ja": "最大起動項目数に達しました。", "zh": "已达到最大启动项目数。",
        "de": "Maximale Anzahl an Starteinträgen erreicht.", "es": "Se alcanzó el número máximo de entradas de inicio.", "fr": "Nombre maximal d'entrées de démarrage atteint.",
        "it": "Raggiunto il numero massimo di voci di avvio.", "pt": "Número máximo de entradas de inicialização atingido.", "ru": "Достигнуто максимальное количество записей запуска.",
    },
    "Maximum trigger entries reached.": {
        "ko": "최대 트리거 수에 도달했습니다.", "ja": "最大トリガー数に達しました。", "zh": "已达到最大触发器数。",
        "de": "Maximale Anzahl an Auslöser-Einträgen erreicht.", "es": "Se alcanzó el número máximo de disparadores.", "fr": "Nombre maximal de déclencheurs atteint.",
        "it": "Raggiunto il numero massimo di trigger.", "pt": "Número máximo de gatilhos atingido.", "ru": "Достигнуто максимальное количество триггеров.",
    },
    "Trigger": {
        "ko": "트리거", "ja": "トリガー", "zh": "触发器",
        "de": "Auslöser", "es": "Disparador", "fr": "Déclencheur",
        "it": "Trigger", "pt": "Gatilho", "ru": "Триггер",
    },
    "Mark": {
        "ko": "마크", "ja": "マーク", "zh": "标记",
        "de": "Markierung", "es": "Marca", "fr": "Marque",
        "it": "Segno", "pt": "Marca", "ru": "Отметка",
    },
    "Loop": {
        "ko": "루프", "ja": "ループ", "zh": "循环",
        "de": "Schleife", "es": "Bucle", "fr": "Boucle",
        "it": "Ciclo", "pt": "Laço", "ru": "Цикл",
    },
    "Highlight pattern (empty to clear):": {
        "ko": "강조 패턴 (비우면 해제):", "ja": "強調パターン (空で解除):", "zh": "高亮模式 (留空清除):",
        "de": "Hervorhebungsmuster (leer zum Löschen):", "es": "Patrón de resaltado (vacío para borrar):", "fr": "Motif de surlignage (vide pour effacer) :",
        "it": "Modello di evidenziazione (vuoto per cancellare):", "pt": "Padrão de destaque (vazio para limpar):", "ru": "Шаблон подсветки (пусто для очистки):",
    },
    "loop": {
        "ko": "루프", "ja": "ループ", "zh": "循环",
        "de": "Schleife", "es": "bucle", "fr": "boucle",
        "it": "ciclo", "pt": "laço", "ru": "цикл",
    },
    "mark": {
        "ko": "마크", "ja": "マーク", "zh": "标记",
        "de": "Markierung", "es": "marca", "fr": "marque",
        "it": "segno", "pt": "marca", "ru": "отметка",
    },
    "loop ●": {
        "ko": "루프 ●", "ja": "ループ ●", "zh": "循环 ●",
        "de": "Schleife ●", "es": "bucle ●", "fr": "boucle ●",
        "it": "ciclo ●", "pt": "laço ●", "ru": "цикл ●",
    },
    "mark ●": {
        "ko": "마크 ●", "ja": "マーク ●", "zh": "标记 ●",
        "de": "Markierung ●", "es": "marca ●", "fr": "marque ●",
        "it": "segno ●", "pt": "marca ●", "ru": "отметка ●",
    },
    "on": {
        "ko": "켜짐", "ja": "オン", "zh": "开",
        "de": "ein", "es": "activado", "fr": "activé",
        "it": "attivo", "pt": "ligado", "ru": "вкл",
    },
    "off": {
        "ko": "꺼짐", "ja": "オフ", "zh": "关",
        "de": "aus", "es": "desactivado", "fr": "désactivé",
        "it": "spento", "pt": "desligado", "ru": "выкл",
    },
    "?": {
        "ko": "?", "ja": "?", "zh": "?",
        "de": "?", "es": "?", "fr": "?",
        "it": "?", "pt": "?", "ru": "?",
    },

    # ---- TSCRT help / crash / misc ----
    "TSCRT help": {
        "ko": "TSCRT 도움말", "ja": "TSCRT ヘルプ", "zh": "TSCRT 帮助",
        "de": "TSCRT-Hilfe", "es": "Ayuda de TSCRT", "fr": "Aide TSCRT",
        "it": "Aiuto TSCRT", "pt": "Ajuda do TSCRT", "ru": "Справка TSCRT",
    },
    "TSCRT - Crash report": {
        "ko": "TSCRT - 크래시 보고", "ja": "TSCRT - クラッシュレポート", "zh": "TSCRT - 崩溃报告",
        "de": "TSCRT - Absturzbericht", "es": "TSCRT - Informe de fallo", "fr": "TSCRT - Rapport de plantage",
        "it": "TSCRT - Rapporto di arresto anomalo", "pt": "TSCRT - Relatório de falha", "ru": "TSCRT - Отчёт о сбое",
    },
    "A crash report from a previous run was found:\n%1\n\nWould you like to keep it for inspection?": {
        "ko": "이전 실행의 크래시 보고서가 발견되었습니다:\n%1\n\n검토를 위해 보관하시겠습니까?",
        "ja": "前回の実行のクラッシュレポートが見つかりました:\n%1\n\n調査のため保持しますか?",
        "zh": "发现上次运行的崩溃报告:\n%1\n\n是否保留以便检查?",
        "de": "Ein Absturzbericht eines früheren Laufs wurde gefunden:\n%1\n\nMöchten Sie ihn zur Prüfung behalten?",
        "es": "Se encontró un informe de fallo de una ejecución anterior:\n%1\n\n¿Desea conservarlo para su revisión?",
        "fr": "Un rapport de plantage d'une exécution précédente a été trouvé :\n%1\n\nSouhaitez-vous le conserver pour examen ?",
        "it": "È stato trovato un rapporto di arresto anomalo da un'esecuzione precedente:\n%1\n\nConservarlo per l'ispezione?",
        "pt": "Foi encontrado um relatório de falha de uma execução anterior:\n%1\n\nDeseja mantê-lo para inspeção?",
        "ru": "Обнаружен отчёт о сбое предыдущего запуска:\n%1\n\nСохранить его для анализа?",
    },

    # ---- Terminal-specific ----
    "Interactive terminal session": {
        "ko": "대화형 터미널 세션", "ja": "対話型端末セッション", "zh": "交互式终端会话",
        "de": "Interaktive Terminalsitzung", "es": "Sesión de terminal interactiva", "fr": "Session de terminal interactive",
        "it": "Sessione di terminale interattiva", "pt": "Sessão de terminal interativa", "ru": "Интерактивный сеанс терминала",
    },
    "Terminal display": {
        "ko": "터미널 표시", "ja": "端末表示", "zh": "终端显示",
        "de": "Terminal-Anzeige", "es": "Pantalla de terminal", "fr": "Affichage du terminal",
        "it": "Visualizzazione terminale", "pt": "Exibição de terminal", "ru": "Отображение терминала",
    },
    "Type a command and press Enter…": {
        "ko": "명령을 입력하고 Enter를 누르세요…", "ja": "コマンドを入力して Enter を押してください…", "zh": "输入命令并按回车…",
        "de": "Befehl eingeben und Enter drücken…", "es": "Escriba un comando y pulse Enter…", "fr": "Tapez une commande et appuyez sur Enter…",
        "it": "Digita un comando e premi Enter…", "pt": "Digite um comando e pressione Enter…", "ru": "Введите команду и нажмите Enter…",
    },

    # ---- Snapshot status messages ----
    "Snapshot \"%1\" started on %2": {
        "ko": "스냅샷 \"%1\" 이(가) %2에서 시작됨", "ja": "スナップショット \"%1\" を %2 で開始", "zh": "快照 \"%1\" 已在 %2 启动",
        "de": "Schnappschuss \"%1\" auf %2 gestartet", "es": "Instantánea \"%1\" iniciada en %2", "fr": "Instantané \"%1\" démarré sur %2",
        "it": "Istantanea \"%1\" avviata su %2", "pt": "Instantâneo \"%1\" iniciado em %2", "ru": "Снимок \"%1\" запущен на %2",
    },
    "Snapshot \"%1\" not found": {
        "ko": "스냅샷 \"%1\" 을(를) 찾을 수 없음", "ja": "スナップショット \"%1\" が見つかりません", "zh": "未找到快照 \"%1\"",
        "de": "Schnappschuss \"%1\" nicht gefunden", "es": "Instantánea \"%1\" no encontrada", "fr": "Instantané \"%1\" introuvable",
        "it": "Istantanea \"%1\" non trovata", "pt": "Instantâneo \"%1\" não encontrado", "ru": "Снимок \"%1\" не найден",
    },
    "Snapshot saved: %1": {
        "ko": "스냅샷 저장: %1", "ja": "スナップショット保存: %1", "zh": "快照已保存: %1",
        "de": "Schnappschuss gespeichert: %1", "es": "Instantánea guardada: %1", "fr": "Instantané enregistré : %1",
        "it": "Istantanea salvata: %1", "pt": "Instantâneo salvo: %1", "ru": "Снимок сохранён: %1",
    },
    "Snapshot failed: %1": {
        "ko": "스냅샷 실패: %1", "ja": "スナップショット失敗: %1", "zh": "快照失败: %1",
        "de": "Schnappschuss fehlgeschlagen: %1", "es": "Instantánea fallida: %1", "fr": "Échec de l'instantané : %1",
        "it": "Istantanea non riuscita: %1", "pt": "Falha no instantâneo: %1", "ru": "Ошибка снимка: %1",
    },

    # ---- Display strings ----
    "SSH · %1 (%2@%3:%4)": {
        "ko": "SSH · %1 (%2@%3:%4)", "ja": "SSH · %1 (%2@%3:%4)", "zh": "SSH · %1 (%2@%3:%4)",
        "de": "SSH · %1 (%2@%3:%4)", "es": "SSH · %1 (%2@%3:%4)", "fr": "SSH · %1 (%2@%3:%4)",
        "it": "SSH · %1 (%2@%3:%4)", "pt": "SSH · %1 (%2@%3:%4)", "ru": "SSH · %1 (%2@%3:%4)",
    },
    "Serial · %1 (%2 %3 baud)": {
        "ko": "시리얼 · %1 (%2 %3 baud)", "ja": "シリアル · %1 (%2 %3 baud)", "zh": "串口 · %1 (%2 %3 baud)",
        "de": "Seriell · %1 (%2 %3 baud)", "es": "Serie · %1 (%2 %3 baud)", "fr": "Série · %1 (%2 %3 baud)",
        "it": "Seriale · %1 (%2 %3 baud)", "pt": "Serial · %1 (%2 %3 baud)", "ru": "Последовательный · %1 (%2 %3 baud)",
    },
    " s": {
        "ko": " 초", "ja": " 秒", "zh": " 秒",
        "de": " s", "es": " s", "fr": " s",
        "it": " s", "pt": " s", "ru": " с",
    },
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
        ("tscrt_win_de.ts", "de"),
        ("tscrt_win_es.ts", "es"),
        ("tscrt_win_fr.ts", "fr"),
        ("tscrt_win_it.ts", "it"),
        ("tscrt_win_pt.ts", "pt"),
        ("tscrt_win_ru.ts", "ru"),
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
