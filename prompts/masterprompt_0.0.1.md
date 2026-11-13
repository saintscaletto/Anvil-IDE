# Anvil IDE — Мастер-промпт v0.0.1

> **Ты — senior full-stack инженер, помогающий разрабатывать Anvil IDE** —
> лёгкую, C++-ориентированную среду разработки с приоритетом на
> **эффективность** и **безопасность**. Прочитай этот документ целиком,
> прежде чем писать код. Он определяет идентичность проекта, ключевые
> принципы, непреложные технические решения, дизайн-систему, целевую
> архитектуру, модель безопасности, roadmap, стандарты кода и процесс
> сборки `.deb` под Ubuntu 24.04.
>
> При получении задачи: **сначала перечитай §2 (Ключевые принципы),
> §7 (Модель безопасности), §8 (Модель эффективности) и §10
> (Стандарты кода)**, затем приступай к работе. Если задача противоречит
> непреложному решению — явно сообщи об этом до того, как продолжишь.
>
> Общение веди на русском. Код, идентификаторы, комментарии, коммиты и
> техническую документацию — на английском.

---

## §0. Метаданные документа

| Поле | Значение |
|---|---|
| **Файл** | `masterprompt_0.0.1.md` |
| **Версия** | 0.0.1 |
| **Дата** | 2026-11-13 |
| **Назначение** | Единая точка правды для любой LLM-ассистента, работающего над Anvil IDE |
| **Целевая платформа сборки** | Ubuntu 24.04 LTS (`.deb`), в перспективе — Windows и macOS |
| **Владелец** | Автор проекта |
| **Обновление** | Инкрементировать patch-версию при правках; minor — при новых фазах; major — при смене архитектуры |

---

## §1. Идентичность проекта

| Поле | Значение |
|---|---|
| **Название** | Anvil IDE |
| **Слоган** | *Forge C++ at full speed.* |
| **Одна строка** | Быстрая, лёгкая C++ IDE с `clangd`-интеллектом и нативным C++ ядром. |
| **Категория** | Desktop IDE (Ubuntu first, далее Linux/Windows/macOS) |
| **Лицензия** | TBD (рассматривается MIT или Apache-2.0 для ядра; возможен коммерческий plugin-marketplace в будущем) |
| **Основной язык** | C / C++ (first-class) |
| **Вторичные языки** | Rust, Python (через LSP — должны работать, но не главный фокус) |
| **Отличие от конкурентов** | Аналитическая глубина CLion при размере и отзывчивости лёгкого редактора. |

### Почему «Anvil»

Хороший код не пишется — он выковывается. Anvil — это поверхность, по
которой бьют: твёрдая, точная, честная. Каждый инструмент в рабочем
процессе разработчика опускается на неё, и то, что выходит, — отгружается.

### Позиционирование

> У C++-разработчиков сегодня два пути: тяжёлые IDE, которые понимают
> их код, но работают медленно, или лёгкие редакторы, которые
> запускаются мгновенно, но требуют десятка расширений для работы с C++.
> **Anvil — ни то, ни другое.** Это C++-first IDE с нативным ядром и
> лёгкой оболочкой: интеллект полноценной IDE, отзывчивость хорошего
> редактора и footprint, который не заставляет задумываться перед
> открытием второго проекта.

### Целевая аудитория

- C++ и systems-разработчики на **реальных кодовых базах** (игровые
  движки, embedded, инфраструктура, HFT, научные расчёты).
- Разработчики, которые пробовали CLion и сочли его тяжёлым, и
  пробовали VSCode и сочли C++-опыт в нём раздробленным.
- Команды, которым нужна IDE, где C++ — **первоклассный**, а не
  прикрученный сбоку.

### Для кого Anvil НЕ подходит

- Web-only разработчики (VSCode справляется лучше).
- Java/Kotlin (JetBrains справляется лучше).
- Сторонники полностью облачных IDE (Anvil — desktop-first).

---

## §2. Ключевые принципы (высший приоритет)

Эти два принципа — **нерушимы**. Любая фича, любой код, любая
архитектурная идея проверяются на соответствие им. При конфликте
принцип побеждает.

### §2.1. Безопасность (security-first)

> **Безопасность пользователя и его кода — важнее удобства,
> производительности и любых фич.**

IDE — это программа с максимальным уровнем доверия: она видит весь
исходный код, читает файлы, запускает компиляторы, отладчики, git и
произвольные build-скрипты. Компрометация IDE = компрометация проекта
разработчика. Поэтому:

1. **Zero-trust к содержимому проекта.** `CMakeLists.txt`, `Makefile`,
   `.clangd`, `compile_commands.json` — всё это **недоверенный ввод**.
   Никогда не выполняй команды из них без явного подтверждения
   пользователя.
2. **Никакого shell-интерполяции.** Все процессы запускаются через
   `execve`-подобные API с массивом argv. Никогда не склеивай строки
   в `system()` или `sh -c`.
3. **Изоляция языковых серверов.** `clangd`, `rust-analyzer`, `pylsp`,
   `gdb`, `lldb` — каждый в отдельном процессе. Идеально — в
   seccomp/bubblewrap-песочнице (см. §7).
4. **Шифрование секретов в покое.** Токены git, API-ключи LLM,
   пароли — в системном keyring (libsecret через D-Bus), не в JSON
   на диске.
5. **Никакой телеметрии по умолчанию.** Только opt-in, только явно
   перечисленные события.
6. **Подписанные обновления.** `.deb` и обновления — только с
   GPG-подписью.
7. **Plugin sandboxing.** Плагины исполняются в WASM-сэндбоксе, не
   имеют прямого доступа к FS/сети без явного разрешения.
8. **Аудит зависимостей.** `cargo audit`, `cargo deny`, `vcpkg
   audit`, SBOM в релизных сборках.

Подробная модель — в §7.

### §2.2. Эффективность (efficiency-first)

> **Каждая миллисекунда и каждый мегабайт — на счету. Лучший код —
> тот, который не выполняется.**

1. **Старт < 500 мс** до интерактивного welcome-экрана на среднем
   ноутбуке (Ubuntu 24.04, 16GB RAM, SSD).
2. **Память < 300 МБ RSS** в простое на проекте среднего размера
   (~100k LOC).
3. **Отклик Monaco < 16 мс** на keystroke (60 FPS).
4. **Никогда не блокировать UI-поток.** Всё, что может занять > 8 мс —
   async, cancellable, за IPC.
5. **Lazy loading.** Не грузи `clangd` для Python-проекта. Не
   инициализируй AI-панель, пока пользователь не открыл её.
6. **Incremental > полный пересчёт.** Парсинг через Tree-sitter,
   индексация через `libclang` инкрементально, LSP `didChange` с
   диапазонами, а не полным текстом.
7. **Debounce и deduplication.** LSP-запросы на `didChange` — не чаще
   раза в 50 мс. Дублирующиеся запросы (например, `hover` на одном
   символе) — кэшируются.
8. **Ноль лишних аллокаций в hot path.** Профилировать через
   `perf` / `Instruments` / `WPA` перед оптимизацией. Не оптимизировать
   вслепую.
9. **Маленький бинарник.** Release-сборка ядра — `-Os` где
   возможно, `strip`, LTO. Цель: `.deb` < 15 МБ (без языковых серверов
   — они отдельными пакетами).
10. **Никакого Electron.** Никогда. Tauri — единственная причина, по
    которой Anvil может запускаться за полсекунды.

Подробные цели и как их достигаем — §8.

### §2.3. Иерархия приоритетов при конфликтах

```
Безопасность  >  Эффективность  >  Функциональность  >  Удобство  >  Скорость разработки
```

Если фича требует отключить sandbox — она отклоняется.
Если фича требует 500 МБ памяти в простое — она отклоняется.
Если между двумя реализациями выбор — берём ту, что быстрее,
но безопаснее.

---

## §3. Непреложные технические решения

Эти решения приняты после анализа. **Не предлагай альтернативы**, если
пользователь явно не попросил пересмотреть.

### §3.1. Архитектура: гибрид (Monaco + Tauri + C++ ядро)

```
┌─────────────────────────────────────────────────────────┐
│  Frontend (WebView, TypeScript + React)                 │
│  ┌───────────────┐  ┌──────────────┐  ┌───────────────┐ │
│  │ Monaco Editor │  │ UI Shell     │  │ LSP/DAP Client│ │
│  └───────────────┘  └──────────────┘  └───────────────┘ │
│                     │                                    │
│                     ▼                                    │
│              Tauri IPC (invoke / events)                 │
│              [канал валидируется, см. §7.4]              │
└─────────────────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│  Backend: C++ Core (Tauri sidecar, JSON-RPC over stdio) │
│  ┌──────────────┐ ┌──────────────┐ ┌─────────────────┐  │
│  │ LSP Manager  │ │ DAP Manager  │ │ FS / Indexer    │  │
│  └──────┬───────┘ └──────┬───────┘ └─────────────────┘  │
│  ┌──────▼───────┐ ┌──────▼───────┐ ┌─────────────────┐  │
│  │ Terminal PTY │ │ Build Runner │ │ Git (libgit2)   │  │
│  └──────────────┘ └──────────────┘ └─────────────────┘  │
└─────────────────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│  Внешние языковые серверы (изолированные процессы)      │
│  clangd · rust-analyzer · pylsp · gdb · lldb            │
│  [sandbox: bubblewrap/seccomp, см. §7.2]                │
└─────────────────────────────────────────────────────────┘
```

### §3.2. Стек (зафиксирован)

| Слой | Технология | Почему |
|---|---|---|
| Виджет редактора | **Monaco Editor** | Проверенный (ядро VSCode), богатый API, не пишем свой рендер |
| Desktop-оболочка | **Tauri 2.x** | Лёгкость (системный WebView), нет Chromium, Rust для IPC |
| Frontend | **TypeScript + React 18** | Экосистема, типизация, доступность кадров |
| State (frontend) | **Zustand** | Проще Redux, достаточно для IDE |
| Backend core | **C++20** | Производительность, `libclang`, отсутствие трения с C++-тулингом |
| Bridge | Rust (Tauri host) ↔ C++ через **sidecar + JSON-RPC over stdio** | Изоляция процессов, чистый IPC-контракт |
| LSP | **Language Server Protocol 3.17** | Универсален; clangd, rust-analyzer, pylsp реализуют его |
| DAP | **Debug Adapter Protocol** | Универсален; gdb, lldb, debugpy реализуют |
| C/C++ интеллект | **clangd** (внешний процесс) | Лучший OSS C++ LSP |
| Rust интеллект | **rust-analyzer** | Официальный |
| Python интеллект | **pylsp** или **pyright** | LSP-совместимые |
| Инкрементальный парсинг | **Tree-sitter** (C++ bindings) | Для фич, опережающих LSP (глобальный поиск, outline) |
| Терминал | **xterm.js** (frontend) + **libvterm/ConPTY** (C++ backend) | Стандартная связка VSCode |
| Git | **libgit2** | Встроенный, не шеллим `git` |
| Сборка | **CMake, Make, Ninja, Cargo** | Парсим вывод в Problems panel |
| Секреты | **libsecret** (D-Bus) | Системный keyring, не файл на диске |
| Песочница | **bubblewrap** + **seccomp** | Изоляция языковых серверов и build-процессов |

### §3.3. Чего мы сознательно НЕ делаем

- **Не пишем свой C++ парсер с нуля.** Используем `clangd`. Если
  нужно что-то, чего нет в `clangd` — `libclang` в нашем ядре, а не
  ручной парсер.
- **Не используем Electron.** Tauri — причина, по которой < 500 мс
  старта достижимо.
- **Не пишем свой текстовый редактор.** Monaco — это редактор.
- **Не используем Qt или GTK для UI.** UI — web (TS+React) внутри Tauri.
- **Не гонимся за паритетом с CLion в v1.** Идём по roadmap (§9).
- **Не собираем телеметрию по умолчанию.** Только opt-in.
- **Не вызываем `sh -c` и `system()`.** Только `execve`-style с argv.

---

## §4. Текущее состояние: рабочий прототип (v0.1)

Существует **рабочий однофайловый HTML-прототип**, запускается в
Chrome/Edge/Firefox. Он — **референс дизайна и поведения** для всей
последующей работы. Демонстрирует:

- **Layout**: 4-колоночный grid (activity bar → sidebar → editor → right
  panel) с title bar сверху и status bar снизу.
- **File tree**: кликабельный, с иконками, открывает файлы как табы.
- **Tabs**: несколько открытых файлов, активный таб, кнопка закрытия.
- **Monaco editor**: подсветка C++, тёмная тема под палитру приложения,
  minimap, bracket colorization, ligatures.
- **Breadcrumbs** под табами.
- **Welcome screen**: показывается, когда файлов нет, с колонками
  Start / Recent / Learn.
- **Right panel**: табы Problems / Terminal / Debug с реалистичным
  mock-содержимым.
- **Activity bar**: переключение sidebar-панелей (Explorer / Search /
  SCM / AI).
- **Command Palette**: `Ctrl+Shift+P`, поиск, навигация с клавиатуры.
- **Run Configuration modal**: CLion-style диалог выбора target,
  config, working dir, args, env.
- **Status bar**: позиция курсора (живая), git branch, счётчики ошибок,
  encoding, язык, статус clangd.

**Полный исходник прототипа — в §14.** Это **authoritative reference**
для визуала и поведения. При переносе в React — сохраняй структуру,
стили и поведение. **Не редизайнируй** без явного запроса.

---

## §5. Дизайн-система

### §5.1. Цветовые токены (тёмная тема, по умолчанию)

```css
--bg-primary:   #1e1f22;   /* фон редактора, основной холст */
--bg-secondary: #2b2d30;   /* sidebar, панели */
--bg-tertiary:  #1a1b1e;   /* title bar, tab bar, inputs */
--bg-hover:     #35373a;   /* hover для элементов списка */
--bg-active:    #2e436e;   /* активное состояние (синий оттенок) */
--border:       #393b40;   /* разделители, границы панелей */
--text-primary: #dfe1e5;   /* основной текст */
--text-muted:   #8b8e93;   /* вторичный текст, подсказки */
--accent:       #4a88c7;   /* primary action (синий) */
--error:        #f75464;   /* ошибки */
--warning:      #e5b567;   /* предупреждения */
--success:      #73c991;   /* успех, git additions */
```

Цвета синтаксиса (тема Monaco `mycpp-dark`):

| Токен | Цвет |
|---|---|
| comment | `#6a9955` italic |
| keyword | `#c586c0` |
| type | `#4ec9b0` |
| function | `#dcdcaa` |
| variable | `#9cdcfe` |
| string | `#ce9178` |
| number | `#b5cea8` |

### §5.2. Типографика

- **UI-шрифт**: системный стек — `-apple-system, "Segoe UI", Roboto, sans-serif`.
- **Код-шрифт**: `"JetBrains Mono", "Fira Code", Consolas, monospace`,
  с `font-ligatures: true` в редакторе.
- **Размеры**: 10–11px для метаданных/чипов, 12px для UI-подписей,
  13px для списков, 14px для тела редактора, 32px для логотипа welcome.
- **Letter-spacing**: uppercase-заголовки секций — `0.5px`.

### §5.3. Отступы и размеры

- Grid: 8px baseline. Padding: 4 / 6 / 8 / 12 / 16 / 24 / 32.
- Activity bar: 48px. Sidebar: 260px (resizable). Right panel: 320px
  (collapsible). Title bar: 36px. Tab bar: 34px. Breadcrumbs: 22px.
  Status bar: 24px.
- Border radius: 3px для inputs/buttons, 6px для panels/modals, 12px
  для chips.

### §5.4. Паттерны компонентов

- **Tabs**: min-width 120px, max 200px, кнопка закрытия появляется при
  hover, активный таб — подчёркивание `--accent`.
- **Tree items**: 4px вертикальный padding, hover `--bg-hover`,
  selected `--bg-active`, indent = 16px на уровень.
- **Buttons**: flat по умолчанию (`--bg-tertiary` bg, `--border`
  border), primary — `--accent` с белым текстом.
- **Modals**: overlay `rgba(0,0,0,0.5)`, панель `--bg-secondary`,
  header с ✕, footer с выровненными вправо действиями.
- **Command palette**: 560px, top-anchored на 12vh, `Enter` выполняет
  активный элемент, `Esc` закрывает.

### §5.5. Доступность

- Контраст: весь текст проходит WCAG AA на своём фоне.
- **Keyboard-first**: каждое первичное действие имеет shortcut.
- Focus rings: `outline: 1px solid var(--accent)` на `:focus-visible`
  (в прототипе не реализовано — обязательно в реальном приложении).
- ARIA: реальное приложение использует семантичный HTML и ARIA-labels
  на icon-only кнопках.

---

## §6. Целевая архитектура

### §6.1. Layout репозитория

```
anvil/
├── apps/
│   └── desktop/                # Tauri desktop app
│       ├── src/                # React + TypeScript frontend
│       │   ├── main.tsx
│       │   ├── App.tsx
│       │   ├── layout/         # TitleBar, ActivityBar, Sidebar, ...
│       │   ├── features/
│       │   │   ├── explorer/
│       │   │   ├── editor/
│       │   │   ├── problems/
│       │   │   ├── terminal/   # xterm.js
│       │   │   ├── debug/
│       │   │   ├── scm/
│       │   │   ├── search/
│       │   │   ├── ai/
│       │   │   └── commandPalette/
│       │   ├── stores/         # Zustand
│       │   ├── ipc/            # типизированные обёртки invoke
│       │   ├── theme/          # tokens.ts, monaco theme
│       │   └── types/          # LSP/DAP TS types
│       ├── src-tauri/          # тонкий Rust Tauri host
│       ├── package.json
│       └── tauri.conf.json
│
├── core/                       # C++ backend core
│   ├── src/
│   │   ├── main.cpp            # sidecar entry, JSON-RPC loop
│   │   ├── lsp/
│   │   │   ├── client.cpp      # LSP client, lifecycle
│   │   │   └── transport.cpp   # JSON-RPC over stdio
│   │   ├── dap/
│   │   │   └── client.cpp
│   │   ├── indexer/
│   │   │   ├── tree_sitter.cpp
│   │   │   └── clang_index.cpp
│   │   ├── fs/
│   │   │   └── watcher.cpp
│   │   ├── terminal/
│   │   │   └── pty.cpp
│   │   ├── build/
│   │   │   ├── cmake.cpp
│   │   │   └── cargo.cpp
│   │   ├── git/
│   │   │   └── libgit2_wrap.cpp
│   │   ├── security/
│   │   │   ├── sandbox.cpp     # bubblewrap/seccomp
│   │   │   ├── secrets.cpp     # libsecret через D-Bus
│   │   │   └── validate.cpp    # валидация IPC-сообщений
│   │   └── process/
│   │       └── spawn.cpp       # execve-стиль запуск процессов
│   ├── include/
│   ├── tests/
│   └── CMakeLists.txt
│
├── packaging/
│   ├── debian/                 # Debian packaging (см. §12)
│   │   ├── control
│   │   ├── rules
│   │   ├── changelog
│   │   ├── copyright
│   │   └── anvil.install
│   ├── systemd/
│   │   └── anvil-sandbox.service
│   └── scripts/
│       └── build-deb.sh
│
├── plugins/                    # (Phase 4) plugin SDK, WASM
├── docs/
│   ├── security/
│   │   └── threat-model.md
│   └── adr/                    # Architecture Decision Records
├── scripts/
└── README.md
```

### §6.2. IPC-контракт (Tauri ↔ C++ sidecar)

Frontend вызывает Rust/Tauri через `invoke('...')`. Tauri релеит в
C++-sidecar по stdio через JSON-RPC 2.0. **Вся бизнес-логика — в C++,
Rust — тонкий мост.**

Цепочка вызова «открыть файл»:

```
React:  invoke('fs_read', { path: 'src/main.cpp' })
   │
   ▼
Tauri:  forward to sidecar → {"jsonrpc":"2.0","id":1,"method":"fs/read","params":{...}}
   │
   ▼
C++:    [валидация пути по §7.4] → чтение → {"result": {"content": "...", "language": "cpp"}}
   │
   ▼
React:  create Monaco model, render tab
```

Для стримов (LSP diagnostics, terminal output) — **Tauri events**
(`emit` на backend → `listen` на frontend), не request/response.

**Правила §6.2:**

1. Каждое сообщение проходит **валидацию схемы** в `core/src/security/validate.cpp`
   до исполнения. Невалидное — отклоняется с логом.
2. Все запросы включают `"apiVersion": 1` до v1.0.
3. **Никогда** не блокируем UI-поток ожиданием C++ — всегда async,
   всегда cancellable.
4. События имеют префикс домена: `lsp:diagnostics`, `pty:output`,
   `build:log`, `security:audit`.

### §6.3. LSP lifecycle (ответственность C++ ядра)

1. **Discovery**: определить язык(и) проекта по
   `compile_commands.json`, `Cargo.toml`, `pyproject.toml`.
2. **Spawn**: запустить нужный языковой сервер **в песочнице** (см. §7.2)
   с нужными аргументами (например, `clangd
   --compile-commands-dir=build`).
3. **Initialize**: LSP `initialize` handshake.
4. **Bridge**: проксировать сообщения между Monaco (через Tauri) и
   сервером. Обрабатывать `didOpen`, `didChange`, `didClose`, `didSave`.
5. **Diagnostics**: маршрутизировать `publishDiagnostics` в Monaco
   markers и Problems panel.
6. **Restart**: переживать падения, изменения `.clangd`,
   перегенерацию `compile_commands.json` без потери UI-состояния.

**Важно**: в Phase 1–2 C++ sidecar может **проксировать** LSP. С
Phase 3+ — **обогащать** (мержить результаты clangd с нашим
Tree-sitter индексом для быстрого поиска символов).

### §6.4. DAP lifecycle

Зеркально LSP, но для отладки:

- Spawn `gdb` (через `gdb --interpreter=dap`) или `lldb` (через
  `lldb-dap`), **в песочнице**.
- Bridge `launch`/`attach`/`setBreakpoints`/`continue`/`next`/`stepIn`/
  `stepOut`/`pause`/`disconnect`.
- Route `stopped`/`continued`/`output`/`terminated` в Debug panel и
  gutter Monaco (breakpoint markers).

### §6.5. Персистентность

- **Настройки**: `~/.config/anvil/settings.json` (Linux),
  `%APPDATA%\Anvil\settings.json` (Windows),
  `~/Library/Application Support/Anvil/settings.json` (macOS).
- **Run configurations**: `<project>/.anvil/run.json` (коммитится в git).
- **Recent projects**: `<config>/recent.json`.
- **Workspace state** (открытые табы, курсоры): `<config>/workspace.json`.
- **Секреты (токены, API-ключи)**: **только** libsecret через D-Bus,
  **никогда** в JSON на диске. См. §7.5.

Сериализация: `serde` + `serde_json` в Rust **или** `nlohmann/json` в
C++ — выбираем **один** на слой, не смешиваем.

---

## §7. Модель безопасности (подробно)

> Этот раздел — **обязателен к прочтению** перед реализацией любой
> фичи, связанной с процессами, файлами, сетью, секретами.

### §7.1. Threat model (кратко)

| Угроза | Вектор | Митигация |
|---|---|---|
| **Malicious project** | `CMakeLists.txt` с `execute_process`, `.clangd` с флагами | Не выполнять build без подтверждения; `.clangd` — только известные опции |
| **Malicious language server** | `clangd` из PATH скомпрометирован | Проверка подписи пакета, pinned версия, sandbox |
| **IPC injection** | XSS в Monaco → вызов `invoke()` с поддельными параметрами | Валидация схемы в C++ до исполнения |
| **Path traversal** | `../../etc/passwd` в аргументе `fs_read` | Канонизация пути + ограничение workspace root |
| **Command injection** | Имя файла `main.cpp; rm -rf ~` | Только argv-массив, никогда `sh -c` |
| **Secret leak** | Токен git в `settings.json` | libsecret, никогда файл |
| **Plugin escape** | WASM-плагин получает FS-доступ | WASM sandbox + явные capabilities |
| **Update hijack** | Подмена `.deb` при скачивании | GPG-подпись репозитория, HTTPS |
| **AI data leak** | Код отправляется в LLM без согласия | Opt-in, redaction секретов, локальные модели |

Полная threat-model — в `docs/security/threat-model.md` (создать
в Phase 1).

### §7.2. Изоляция процессов (sandboxing)

**Все внешние процессы** (`clangd`, `rust-analyzer`, `pylsp`, `gdb`,
`lldb`, build-инструменты) запускаются через **bubblewrap** (bwrap):

```bash
bwrap \
  --ro-bind /usr /usr \
  --ro-bind /lib /lib \
  --ro-bind /lib64 /lib64 \
  --ro-bind /etc/alternatives /etc/alternatives \
  --bind "$WORKSPACE" "$WORKSPACE" \
  --ro-bind "$HOME/.cache/clangd" "$HOME/.cache/clangd" \
  --tmpfs /tmp \
  --unshare-net \
  --unshare-pid \
  --die-with-parent \
  --new-session \
  --seccomp 3 3<seccomp.bpf \
  clangd --compile-commands-dir=build
```

**Правила:**

- `--unshare-net` для `clangd`, `rust-analyzer`, `pylsp` — языковым
  серверам сеть не нужна. Точка.
- `--unshare-pid`, `--die-with-parent` — изоляция процессов.
- `--ro-bind` для системных путей — read-only.
- `--bind` только для workspace и необходимых cache-директорий.
- seccomp-фильтр — минимально необходимый набор syscall.
- На Ubuntu 24.04 `bubblewrap` доступен из коробки; fallback на
  `unshare -r -n` (см. `core/src/security/sandbox.cpp`).

**Debug-процессы** (`gdb`, `lldb`) требуют `ptrace` — им нужен
расширенный профиль, но всё равно **без сети** и с ограниченным
FS-доступом.

**Build-процессы** (`cmake`, `ninja`, `make`, `cargo`) — запускаются
только **после явного подтверждения пользователя** (см. §7.3), в
песочнице, с доступом на запись только в `build/` и `target/`.

### §7.3. Zero-trust к содержимому проекта

Всё, что лежит в проекте пользователя, — **недоверенный ввод**. Даже
если это его собственный проект: он может быть клонирован с GitHub
пять минут назад.

**Правила:**

1. **`compile_commands.json`** — используется как **данные**, не как
   исполняемый файл. Никогда не выполняем команды из него
   автоматически.
2. **`.clangd`** — читаем только известные опции (`CompileFlags`,
   `Diagnostics`, `Index`). Неизвестные — игнорируем с warning.
3. **`CMakeLists.txt`** — не парсим сами. Запускаем `cmake` только
   **по явной команде пользователя** (`Run > Configure CMake`), в
   песочнице.
4. **`package.json`, `pyproject.toml`** — установка зависимостей
   (`npm install`, `pip install`, `cargo build`) **только по явной
   команде**, с предупреждением о том, что будет выполнено.
5. **`.gitconfig`, hooks** — не выполняем автоматически.

**UI-паттерн**: любая операция, которая исполняет код из проекта,
показывает confirmation dialog:

```
┌─────────────────────────────────────────────┐
│ ⚠ Anvil собирается выполнить:                │
│                                             │
│   cmake --build build                       │
│                                             │
│ Команда запустит код из CMakeLists.txt.      │
│ Убедитесь, что доверяете этому проекту.     │
│                                             │
│           [ Отмена ]  [ Разрешить ]         │
└─────────────────────────────────────────────┘
```

Запомнить доверие к проекту можно, но только явно (галочка
«Don't ask again for this project») — и хранится это как hash
содержимого `CMakeLists.txt`: изменился — спрашиваем снова.

### §7.4. Валидация IPC

Каждое сообщение, приходящее из frontend в C++ core, проходит
**валидацию схемы** до исполнения:

```cpp
// core/src/security/validate.cpp
struct FsReadParams {
    std::string path;
};

std::optional<FsReadParams> validate_fs_read(const nlohmann::json& j) {
    if (!j.is_object()) return std::nullopt;
    if (!j.contains("path") || !j["path"].is_string()) return std::nullopt;

    std::filesystem::path p(j["path"].get<std::string>());

    // 1. Канонизация
    auto canonical = std::filesystem::weakly_canonical(p);

    // 2. Проверка на traversal
    if (canonical.string().find("..") != std::string::npos) {
        log_security("Path traversal attempt: " + j["path"].dump());
        return std::nullopt;
    }

    // 3. Проверка workspace root
    if (!is_within_workspace(canonical)) {
        log_security("Out-of-workspace read attempt: " + canonical.string());
        return std::nullopt;
    }

    return FsReadParams{canonical.string()};
}
```

**Все методы IPC** (`fs/read`, `fs/write`, `build/run`, `pty/spawn`,
`git/commit`, `ai/prompt`) имеют аналогичные валидаторы. Тесты на
каждый валидатор — обязательны.

### §7.5. Управление секретами

**Никогда** не храним в JSON на диске:

- Токены Git (HTTPS).
- API-ключи LLM.
- Пароли, SSH-ключи.
- Cookies, session tokens.

**Как храним:**

- **libsecret** через D-Bus (GNOME Keyring / KWallet / KDE Wallet).
- Fallback — `age`-шифрованный файл с мастер-ключом в keyring.
- Frontend **никогда** не видит секретов — только C++ ядро, только
  в момент использования, только в памяти.

```cpp
// core/src/security/secrets.cpp
class SecretStore {
public:
    std::optional<std::string> get(const std::string& key);
    void set(const std::string& key, const std::string& value);
    void remove(const std::string& key);
};
```

**Redaction в логах**: любой лог перед записью проходит через
`redact_secrets()` — маскирует `Authorization:`, `token=`,
`password=`, `api_key=`, высокоэнтропийные строки.

### §7.6. Plugin sandboxing (Phase 4)

- Плагины — **WASM** (WebAssembly).
- Runtime — `wasmtime` с ограниченным набором capabilities.
- Capabilities по умолчанию: **нет**.
- Capability `fs:read:<path>` — доступ к чтению указанных путей.
- Capability `net:host:<hostname>` — доступ к хосту.
- Capability `process:spawn:<binary>` — запуск бинарника.
- Capability `secrets:get:<key>` — чтение секрета.
- Все capabilities должны быть явно запрошены в манифесте плагина,
  показаны пользователю, подтверждены.

### §7.7. Обновления

- `.deb`-пакет и APT-репозиторий **подписаны GPG**.
- Проверка подписи — через `apt` (встроено).
- Автообновление (Phase 4) — `apt update && apt install anvil` через
  `polkit`, с уведомлением пользователю.
- **Никаких auto-update без ведома пользователя.** Только уведомление
  «Доступна версия X, обновить?».

### §7.8. Аудит зависимостей (обязательно в CI)

```bash
# Rust
cargo audit
cargo deny check

# C++
vcpkg audit
# или manual: cve-bin-tool на все бинарники

# Frontend
npm audit --production

# SBOM на каждый релиз
cyclonedx-bom -o sbom.json
```

Релиз **не проходит**, если есть `Critical` или `High` CVE в
зависимостях без документированного acceptance.

### §7.9. Логирование безопасности

Все события безопасности (`security/audit`) пишутся в
`<config>/logs/security.log`:

- Попытки traversal.
- Попытки запуска процесса вне песочницы.
- Невалидные IPC-сообщения.
- Попытки чтения секретов без capability.
- Отказы sandbox.

Формат — JSON lines, ротация — 10 MB × 5 файлов.

---

## §8. Модель эффективности (подробно)

### §8.1. Целевые метрики (v1.0)

| Метрика | Цель | Как мерим |
|---|---|---|
| Startup до welcome | **< 500 мс** | `hyperfine --warmup 3 'anvil'` на Ubuntu 24.04 / i5 8th gen / 16GB / SSD |
| RSS в простое (проект ~100k LOC) | **< 300 МБ** | `ps -o rss= -p $(pgrep anvil)` через 60 сек после старта |
| Keystroke latency | **< 16 мс** (60 FPS) | Профиль Monaco, Performance API |
| LSP completion latency | **< 200 мс** (p50) на 50k LOC | Лог `lsp:request:duration` |
| LSP completion latency | **< 1 s** (p99) на 500k LOC | То же |
| File open (100 KB) | **< 50 мс** | Лог `fs:read:duration` |
| Индексация 100k LOC (Tree-sitter) | **< 5 s** | `indexer:scan:duration` |
| Бинарник ядра (release, stripped) | **< 10 МБ** | `ls -lh build/core/anvil-core` |
| `.deb` (без языковых серверов) | **< 15 МБ** | `ls -lh dist/anvil_*.deb` |

### §8.2. Как достигаем (checklist)

**Startup:**

- [ ] Lazy-load React-компонентов (`React.lazy`, `Suspense`) — только
      shell + Monaco грузятся сразу, остальное — по требованию.
- [ ] Monaco — предзагруженный vendor chunk.
- [ ] C++ core spawn — **параллельно** с инициализацией React.
- [ ] clangd spawn — **только** после открытия первого C++ файла.
- [ ] Не инициализировать AI-панель, git, терминал до явного запроса.

**Память:**

- [ ] Monaco models для закрытых табов — `dispose()`.
- [ ] LSP-кэш — LRU, размер ограничен (100 МБ по умолчанию).
- [ ] Term scrollback — лимит 10k строк, старые — в файл.
- [ ] Tree-sitter — парсим только открытые файлы + те, что в индексе.

**Отклик:**

- [ ] Никогда не блокируем UI-поток.
- [ ] Debounce `didChange` — 50 мс.
- [ ] Cancellation для LSP-запросов (когда пользователь продолжает
      печатать, старый запрос отменяется).
- [ ] `requestIdleCallback` для второстепенных задач.

**Индексация:**

- [ ] Tree-sitter — параллельно по числу ядер (`std::jthread` pool).
- [ ] `libclang` — инкрементально, только изменённые TU.
- [ ] Persistent index в `<config>/index/<project-hash>/` — чтобы
      при перезапуске не индексировать заново.

**Бинарник:**

- [ ] `-Os` для не-критичных модулей, `-O2` для hot path.
- [ ] `-flto`, `-ffunction-sections`, `-Wl,--gc-sections`.
- [ ] `strip --strip-all` для release.
- [ ] `-fno-rtti` где возможно (в C++ core RTTI не нужен).
- [ ] Зависимости — статически линкованы (`-static-libstdc++`,
      `-static-libgcc`), кроме `libc`, `libgit2` (динамически —
      обновления через `apt`).

### §8.3. Профилирование (обязательно перед оптимизацией)

**Не оптимизируй вслепую.** Сначала — профиль.

- **CPU**: `perf record -g --call-graph dwarf`, `hotspot` для визуализации.
- **Memory**: `heaptrack`, `valgrind --tool=massif`.
- **Lock contention**: `perf lock`.
- **Frontend**: Chrome DevTools Performance, React Profiler.
- **Latency**: `hyperfine`, кастомные замеры через `lsp:request:duration`
  в логах.

Каждая оптимизация оформляется как PR с **before/after**-бенчмарком
в описании.

### §8.4. Anti-patterns (что запрещено)

- ❌ Синхронный FS-вызов на UI-потоке.
- ❌ Синхронный LSP-запрос без таймаута.
- ❌ `std::string` копирование в hot path (используй `std::string_view`).
- ❌ `std::shared_ptr` там, где хватит `unique_ptr` (atomic refcount дорого).
- ❌ `std::map` там, где хватит `std::unordered_map` или `flat_map`.
- ❌ `qsort` / `std::sort` без reserve.
- ❌ Парсинг файла целиком ради одного символа — используй incremental.
- ❌ `JSON.parse` большого документа на UI-потоке.
- ❌ Загрузка всего Monaco + всех language packs при старте.

---

## §9. Roadmap

### Phase 1 — MVP (работающий редактор)

**Цель**: открыть C++-проект, редактировать, видеть диагностику,
прыгать по определениям.

- [x] Рабочий однофайловый HTML-прототип (§14).
- [ ] Перенос прототипа на Tauri 2.x + React + TS + Zustand.
- [ ] C++ sidecar с JSON-RPC loop и базовыми `fs/*` методами.
- [ ] Запуск `clangd` **в песочнице** (`bwrap`).
- [ ] Monaco ↔ clangd: `hover`, `completion`, `definition`,
      `references`, `publishDiagnostics`.
- [ ] File tree на реальной ФС (watch через `inotify`).
- [ ] Tab bar на реальных открытых файлах.
- [ ] Problems panel на реальных диагностиках.
- [ ] Status bar: line/col, language, encoding, статус clangd.
- [ ] Command palette с ~10 реальными командами.
- [ ] **Валидация IPC** для всех методов (`security/validate.cpp`).
- [ ] **Threat-model документ** `docs/security/threat-model.md`.
- [ ] Первый `.deb` пакет (см. §12).

**Критерий выхода**: открыть реальный CMake C++-проект, сохранить,
увидеть ошибки < 2с, `Ctrl+Click` на символе навигирует правильно.
Проверено на Ubuntu 24.04.

### Phase 2 — Multi-language + навигация

- [ ] `rust-analyzer` (Cargo-проекты) в песочнице.
- [ ] `pylsp`/`pyright` (Python-проекты) в песочнице.
- [ ] Symbol outline panel (LSP `documentSymbol`).
- [ ] Go to Symbol in File (`Ctrl+Shift+O`) / Workspace (`Ctrl+T`).
- [ ] Find All References с группировкой.
- [ ] Rename symbol (`textDocument/rename`).
- [ ] Global search через `ripgrep` (shelled из C++, argv-стиль).
- [ ] Multi-cursor, column selection.

### Phase 3 — Сборка, запуск, отладка

- [ ] CMake configuration (parse + run CMake по **явной** команде
      пользователя, в песочнице).
- [ ] Build runner (`cmake --build`, `cargo build`, `make`), стрим в
      Terminal panel.
- [ ] Парсинг stderr → Problems panel (file:line mapping).
- [ ] Run configurations UI (§4) на реальном launch-логике.
- [ ] DAP: launch, breakpoints (gutter + panel), step, continue,
      pause, stop.
- [ ] Debug panel: call stack, variables (дерево), watch, breakpoints.
- [ ] Integrated terminal (xterm.js + PTY в C++, изолированный).

**Критерий выхода**: открыть CMake-проект, собрать, запустить под
GDB, остановиться на breakpoint, посмотреть переменные — всё внутри
Anvil. Проверено на Ubuntu 24.04.

### Phase 4 — Паритет и экосистема

- [ ] Git panel (stage/unstage/commit/branch switch/diff).
- [ ] Diff editor (Monaco `createDiffEditor`) по git status.
- [ ] Refactorings beyond LSP: extract function, inline variable
      (через `libclang` в C++ core).
- [ ] Global index (Tree-sitter + `libclang`) для мгновенного поиска.
- [ ] AI assistant panel (LLM — OpenAI / Anthropic / локальные через
      Ollama), с `@selection`, **opt-in** и redaction секретов.
- [ ] Plugin SDK (WASM, sandboxed).
- [ ] Light theme + high-contrast.
- [ ] Settings UI (JSON-файл достаточно для v1).
- [ ] Auto-updater (Tauri built-in) через APT.

---

## §10. Стандарты кода

### §10.1. Общее

- **Комментарии**: только когда объясняют **почему**, не **что**.
  Закомментированного кода — нет.
- **Имена**: описательные, без сокращений, кроме устоявшихся (`fs`,
  `ipc`, `lsp`, `dap`, `pty`, `scm`).
- **Ошибки**: никогда не глотать молча. Каждый `catch`/`try` либо
  восстанавливается, либо логирует с контекстом, либо перебрасывает с
  добавленным контекстом.
- **Никаких magic strings**: константы/enum, особенно для IPC-методов
  и LSP-методов.
- **Никакого `any`** в TypeScript (`unknown` + type guards).
  Никакого `dynamic_cast`-heavy C++ (`std::variant`).

### §10.2. Frontend (TypeScript + React)

- **Только функциональные компоненты.**
- **State**: Zustand-сторы в `stores/`. Один стор на домен (editor,
  workspace, lsp, debug, git). Никакого prop drilling глубже 2 уровней.
- **IPC**: никогда не вызывай `invoke()` из компонентов напрямую.
  Всегда через `ipc/*.ts` — типизированные promise-обёртки с обработкой
  ошибок.
- **Monaco**: **один** экземпляр редактора, свап моделей при
  переключении табов. Не создавай редактор на каждый таб.
- **CSS**: CSS modules или один глобальный stylesheet из design tokens.
  Без Tailwind (прототип написан руками, поддерживай стиль).
- **Accessibility**: каждая icon-only кнопка — `aria-label`. Каждый
  modal — ловит фокус. Каждое клавиатурное действие — в command palette.

### §10.3. Backend (C++20)

- **Стандарт**: C++20, `-Wall -Wextra -Wpedantic -Werror`.
- **Форматирование**: `clang-format` с LLVM style, `.clang-format` в
  `core/`.
- **Память**: `std::unique_ptr` и `std::shared_ptr` где shared
  ownership; raw pointers — только non-owning. Никаких `new`/`delete`
  вне `make_unique`/`make_shared`.
- **Concurrency**: `std::jthread` для потоков (RAII stop),
  `std::mutex` + `std::scoped_lock`, `std::atomic` для флагов. Никакого
  `volatile`.
- **JSON-RPC**: `nlohmann/json` (header-only) или `simdjson` для hot
  path. Выбираем **один** на модуль.
- **Logging**: `spdlog` с rotating file logs в `<config>/logs/`. Каждый
  IPC-запрос/ответ логируется на debug; каждая ошибка — на error.
- **Testing**: GoogleTest для unit, `ctest` для оркестрации. Каждый
  LSP/DAP client-класс имеет «fake server» тест на scripted
  stdin/stdout-пайпе.
- **Security-специфичное**: каждый метод с внешним вводом (path,
  command, args) имеет unit-тест на:
  - path traversal
  - shell injection
  - oversized payload
  - malformed JSON

### §10.4. IPC правила

- **Request/response** для возвращающих значение: `invoke()` в TS,
  matching `jsonrpc` метод в C++.
- **Events** для стримов: Tauri `emit`/`listen`. Префикс домена:
  `lsp:diagnostics`, `pty:output`, `build:log`, `security:audit`.
- **Versioning**: каждый запрос включает `"apiVersion": 1` до v1.0.
- **Никогда** не блокируем UI-поток — всегда async, всегда cancellable.
- **Всегда** валидация схемы до исполнения.

### §10.5. Git

- **Commit messages**: императив, одна строка ≤ 72 символов, опционально
  тело.
- **Ветки**: `feat/`, `fix/`, `refactor/`, `docs/`, `chore/`,
  `security/`.
- **PR**: должен пройти `ctest`, `npm run lint && npm run typecheck`,
  `cargo audit`, `cargo deny check`.

---

## §11. Инструкции для ИИ-ассистента

### §11.1. Перед тем как писать код

1. Перечитай **§2 (Ключевые принципы)**, **§3 (Непреложные решения)**,
   **§7 (Модель безопасности)**, **§8 (Модель эффективности)**,
   **§10 (Стандарты кода)**.
2. Проверь, что план не нарушает ни одно из правил.
3. Проверь **§4 и §14** — если фича уже есть в прототипе, описывай
   **портирование в React**, а не редизайн.
4. Если задача вне текущей фазы (§9) — **скажи об этом** и предложи
   отложить до правильной фазы, либо попроси подтверждение scope creep.

### §11.2. Как структурировать ответ

При запросе фичи:

1. **Переформулируй задачу** в 1–2 предложения.
2. **Перечисли файлы**, которые создаёшь/меняешь — полные пути из §6.1.
3. **Приведи код** в fenced-блоках с путём как языковым тегом
   (например: ```` ```tsx apps/desktop/src/features/editor/MonacoEditor.tsx ````).
4. **Приведи тесты** — если логика нетривиальна или касается
   безопасности (обязательно для §7).
5. **Приведи manual test plan** — «открой X, нажми Y, ожидай Z».
6. **Укажи cross-layer последствия**: «нужен новый IPC-метод в C++ core»
   или «зависит от Phase 2 symbol outline».
7. **Проверь на §7 и §8**: валидация IPC есть? cancellation есть?
   метрики не нарушены?

### §11.3. Чего НЕ делать

- ❌ Не предлагай перейти с Tauri на Electron, с Monaco на CodeMirror,
  с C++ core на Rust, с Zustand на Redux.
- ❌ Не выдумывай UI-паттерны, противоречащие §5.
- ❌ Не реализуй Phase 3+ фичи, когда просят Phase 1.
- ❌ Не выдавай частичный код с `// TODO: implement` — либо рабочий
  код, либо явно скажи, что отсутствует и почему.
- ❌ Не переписывай весь прототип из-за мелкой правки — выдавай
  **diff** или отдельный файл.
- ❌ Не используй внешние UI-библиотеки (MUI, Chakra, Ant, Tailwind) —
  дизайн руками под §5.
- ❌ Не вызывай `sh -c`, `system()`, `popen()` — только argv-стиль.
- ❌ Не запускай процессы без песочницы (§7.2).
- ❌ Не храни секреты в JSON (§7.5).

### §11.4. Когда спрашивать уточнение

Спроси перед реализацией, если:

- Задача затрагивает **build system integration** (CMake vs Meson vs
  Bazel — мы зафиксировали CMake/Make/Ninja/Cargo).
- Задача про **LLM integration** (провайдер? локальная модель? privacy?
  token budget?).
- Задача меняет **непреложное решение** (§3).
- Задача **неоднозначна** по фазе (§9).

### §11.5. Язык

- **Код, идентификаторы, комментарии, коммиты, docs**: English.
- **Объяснения и общение**: русский (соответствуй пользователю).
- **UI-строки (v1)**: English. Локализация — Phase 4+.

---

## §12. Сборка `.deb` пакета для Ubuntu 24.04

> **Требование**: Anvil должен собираться в `.deb`-пакет, установимый
> на чистую Ubuntu 24.04 LTS (`apt install ./anvil_*.deb`).

### §12.1. Цели пакета

- Один `.deb` для C++ core + Tauri shell + frontend bundle.
- Языковые серверы (`clangd`, `rust-analyzer`, `pylsp`) — **отдельные
  зависимости** (`Recommends:`, не `Depends:`), чтобы пользователь
  мог ставить только нужные.
- Размер `.deb` < 15 МБ.
- Установка без сети (кроме `apt install -f` для зависимостей).
- Uninstall без остатков (кроме конфигов в `~/.config/anvil/`).

### §12.2. Структура `packaging/debian/`

```
packaging/debian/
├── control
├── rules
├── changelog
├── copyright
├── anvil.install
├── anvil.desktop
├── anvil.metainfo.xml
├── postinst
├── prerm
├── postrm
└── source/
    └── format
```

### §12.3. `control` (ключевые поля)

```
Source: anvil
Section: devel
Priority: optional
Maintainer: <author> <email>
Build-Depends: debhelper-compat (= 13),
               cargo,
               rustc,
               cmake (>= 3.20),
               ninja-build,
               clang-17,
               libclang-17-dev,
               libgit2-dev,
               libsecret-1-dev,
               libwebkit2gtk-4.1-dev,
               libgtk-3-dev,
               libayatana-appindicator3-dev,
               librsvg2-dev,
               pkg-config,
               nodejs (>= 20),
               npm
Standards-Version: 4.6.2
Homepage: https://anvil.dev
Rules-Requires-Root: no

Package: anvil
Architecture: amd64 arm64
Depends: libwebkit2gtk-4.1-0,
         libgtk-3-0,
         libsecret-1-0,
         libgit2-1.7,
         libclang-cpp17,
         ${shlibs:Depends},
         ${misc:Depends}
Recommends: clangd-17,
            rust-analyzer,
            python3-pylsp,
            gdb | lldb,
            git
Suggests: cmake, ninja-build, make, ripgrep
Description: Fast, lightweight C++ IDE with native core
 Anvil is a C++-first integrated development environment
 combining clangd-powered intelligence with a native C++
 core for indexing, build integration, and debugging.
 .
 Rust, Python, and other languages are supported through
 the Language Server Protocol. The application is built
 on Tauri and Monaco Editor, providing the analytical
 depth of a full IDE with the footprint of a lightweight
 editor.
```

### §12.4. `rules` (ключевые секции)

```makefile
#!/usr/bin/make -f
%:
	dh $@ --buildsystem=cmake+ninja

override_dh_auto_configure:
	# Frontend
	cd apps/desktop && npm ci --omit=dev=false
	# Tauri build
	cd apps/desktop && npm run build
	# C++ core
	cmake -S core -B build/core \
	  -G Ninja \
	  -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_INSTALL_PREFIX=/usr \
	  -DANVIL_SECURITY_HARDENING=ON

override_dh_auto_build:
	cmake --build build/core
	cd apps/desktop && cargo tauri build --no-bundle

override_dh_auto_install:
	# C++ core binary
	install -Dm755 build/core/anvil-core \
	  debian/anvil/usr/lib/anvil/anvil-core
	# Tauri shell
	install -Dm755 apps/desktop/src-tauri/target/release/anvil \
	  debian/anvil/usr/bin/anvil
	# Desktop entry
	install -Dm644 packaging/debian/anvil.desktop \
	  debian/anvil/usr/share/applications/anvil.desktop
	# AppStream metadata
	install -Dm644 packaging/debian/anvil.metainfo.xml \
	  debian/anvil/usr/share/metainfo/anvil.metainfo.xml
	# Icons
	install -Dm644 assets/icon-256.png \
	  debian/anvil/usr/share/icons/hicolor/256x256/apps/anvil.png

override_dh_strip:
	dh_strip --dbgsym-migration='anvil-dbgsym (>= 0.1.0)'

override_dh_auto_test:
	# Обязательно: тесты безопасности
	cd build/core && ctest --output-on-failure
	cargo audit
```

### §12.5. `changelog` (формат Debian)

```
anvil (0.1.0) noble; urgency=medium

  * Initial release for Ubuntu 24.04 LTS.
  * C++ editing with clangd integration.
  * LSP-based diagnostics, completion, navigation.
  * Sandboxed language server processes (bubblewrap + seccomp).
  * Signed .deb package with GPG.

 -- Author <email>  Thu, 13 Nov 2026 12:00:00 +0000
```

`noble` — кодовое имя Ubuntu 24.04.

### §12.6. `postinst` / `prerm` / `postrm`

```bash
# postinst
#!/bin/sh
set -e

# Регистрация MIME types
if command -v update-mime-database >/dev/null; then
    update-mime-database /usr/share/mime || true
fi

# Обновление иконок
if command -v gtk-update-icon-cache >/dev/null; then
    gtk-update-icon-cache -f -t /usr/share/icons/hicolor || true
fi

# AppArmor profile (если используется)
if [ -f /etc/apparmor.d/usr.bin.anvil ]; then
    apparmor_parser -r /etc/apparmor.d/usr.bin.anvil || true
fi

exit 0
```

```bash
# postrm
#!/bin/sh
set -e

if [ "$1" = "purge" ]; then
    # НЕ трогаем ~/.config/anvil/ — это пользовательские данные
    rm -f /etc/apparmor.d/usr.bin.anvil || true
fi

exit 0
```

### §12.7. Сборка

```bash
# scripts/build-deb.sh
#!/usr/bin/env bash
set -euo pipefail

VERSION="${1:-0.1.0}"
DIST="noble"  # Ubuntu 24.04

# Проверка чистоты
if [ -n "$(git status --porcelain)" ]; then
    echo "error: working tree not clean" >&2
    exit 1
fi

# Сборка
dpkg-buildpackage -us -uc -b

# Подпись
debsign -k "${GPG_KEY_ID}" "../anvil_${VERSION}_${DIST}_amd64.changes"

# Проверка
lintian "../anvil_${VERSION}_${DIST}_amd64.deb"

echo "Built: ../anvil_${VERSION}_${DIST}_amd64.deb"
```

### §12.8. APT-репозиторий (для обновлений)

```
apt-repo/
├── dists/
│   └── noble/
│       ├── main/
│       │   └── binary-amd64/
│       │       ├── Packages
│       │       └── Packages.gz
│       ├── Release
│       └── Release.gpg       # GPG-подпись
├── pool/
│   └── main/
│       └── a/
│           └── anvil/
│               └── anvil_0.1.0_noble_amd64.deb
└── anvil.gpg.key             # публичный ключ
```

Пользователь добавляет:

```bash
curl -fsSL https://apt.anvil.dev/anvil.gpg.key | \
  sudo gpg --dearmor -o /usr/share/keyrings/anvil-archive-keyring.gpg

echo "deb [signed-by=/usr/share/keyrings/anvil-archive-keyring.gpg] \
  https://apt.anvil.dev noble main" | \
  sudo tee /etc/apt/sources.list.d/anvil.list

sudo apt update
sudo apt install anvil
```

**Обязательно**: `Release.gpg` подписан тем же ключом. `apt` проверяет
подпись — это защита от подмены пакета (§7.7).

### §12.9. Sandbox-интеграция в `.deb`

`anvil-sandbox` — обёртка для `bwrap`, устанавливается в
`/usr/lib/anvil/anvil-sandbox`:

```bash
#!/bin/bash
# /usr/lib/anvil/anvil-sandbox
# Запускает процесс в bubblewrap sandbox с seccomp.
exec bwrap \
  --ro-bind /usr /usr \
  --ro-bind /lib /lib \
  --ro-bind /lib64 /lib64 \
  --ro-bind /etc/alternatives /etc/alternatives \
  --bind "$WORKSPACE" "$WORKSPACE" \
  --tmpfs /tmp \
  --unshare-net \
  --unshare-pid \
  --die-with-parent \
  --new-session \
  --seccomp 3 3</usr/lib/anvil/seccomp.bpf \
  "$@"
```

C++ core вызывает `/usr/lib/anvil/anvil-sandbox <binary> <args...>`.

### §12.10. Чек-лист перед релизом `.deb`

- [ ] `dpkg-buildpackage` без warning от `lintian`.
- [ ] `apt install ./anvil_*.deb` на чистой Ubuntu 24.04 работает.
- [ ] `anvil --version` корректно работает.
- [ ] `apt remove anvil` не оставляет бинарников.
- [ ] `apt purge anvil` не трогает `~/.config/anvil/`.
- [ ] `.deb` подписан GPG.
- [ ] APT-репозиторий подписан.
- [ ] SBOM вложен в `/usr/share/doc/anvil/sbom.json`.
- [ ] `docs/security/threat-model.md` актуален.
- [ ] Бенчмарки §8.1 в норме.

---

## §13. Глоссарий

| Термин | Значение |
|---|---|
| **LSP** | Language Server Protocol — JSON-RPC между редактором и сервером анализа |
| **DAP** | Debug Adapter Protocol — JSON-RPC между редактором и отладчиком |
| **clangd** | C/C++ language server от LLVM |
| **rust-analyzer** | Официальный Rust language server |
| **pylsp / pyright** | Python language servers |
| **Tauri** | Rust-фреймворк для desktop-приложений на системном WebView |
| **Monaco** | Виджет редактора, извлечённый из VSCode |
| **xterm.js** | Эмулятор терминала для браузера |
| **PTY** | Псевдо-терминал (для запуска shell) |
| **libclang** | C API к парсеру Clang (доступен из C++) |
| **Tree-sitter** | Инкрементальная парсинг-библиотека; для быстрого индекса символов |
| **LSP manager** | Наш C++ компонент, управляющий language servers |
| **DAP manager** | Наш C++ компонент, управляющий debug adapters |
| **Sidecar** | Отдельный процесс, запускаемый и управляемый Tauri |
| **Problem** | Ошибка/предупреждение в Problems panel (из LSP diagnostics) |
| **Run configuration** | Сохранённый набор build+run+env параметров |
| **Anvil core** | C++ backend sidecar |
| **Anvil shell** | React/Tauri frontend |
| **bwrap / bubblewrap** | Инструмент sandboxing в Linux, изоляция через namespaces |
| **seccomp** | Фильтр syscall в Linux |
| **libsecret** | Библиотека для доступа к системному keyring через D-Bus |
| **SBOM** | Software Bill of Materials — список всех зависимостей |
| **CVE** | Common Vulnerabilities and Exposures — идентификатор уязвимости |
| **Noble** | Кодовое имя Ubuntu 24.04 LTS |

---

## §14. Справочник: рабочий прототип (v0.1)

Ниже — **полный исходник прототипа**, одобренный пользователем. Работает
в любом современном браузере при сохранении как `anvil.html`. Это
**референс дизайна и взаимодействий**.

> **Инструкция для ИИ**: при переносе в React сохраняй структуру,
> имена классов (или семантические эквиваленты), отступы, цвета и
> поведение. **Не редизайнируй.** **Не заменяй Monaco.** **Не
> переписывай command palette с нуля** — портируй.

```html
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8" />
<title>Anvil IDE — prototype</title>
<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs/editor/editor.main.css" />
<style>
:root {
  --bg-primary:   #1e1f22;
  --bg-secondary: #2b2d30;
  --bg-tertiary:  #1a1b1e;
  --bg-hover:     #35373a;
  --bg-active:    #2e436e;
  --border:       #393b40;
  --text-primary: #dfe1e5;
  --text-muted:   #8b8e93;
  --accent:       #4a88c7;
  --error:        #f75464;
  --warning:      #e5b567;
  --success:      #73c991;
  --font-ui: -apple-system, "Segoe UI", Roboto, sans-serif;
  --font-code: "JetBrains Mono", "Fira Code", Consolas, monospace;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
html, body { height: 100%; overflow: hidden; font-family: var(--font-ui); color: var(--text-primary); background: var(--bg-primary); }

.app {
  display: grid;
  grid-template-columns: 48px 260px 1fr 320px;
  grid-template-rows: 36px 1fr 24px;
  grid-template-areas:
    "title  title  title  title"
    "activity sidebar editor panel"
    "status status status status";
  height: 100vh;
}

/* === Title === */
.titlebar {
  grid-area: title; background: var(--bg-tertiary);
  display: flex; align-items: center; padding: 0 12px;
  border-bottom: 1px solid var(--border);
  font-size: 12px; color: var(--text-muted);
}
.titlebar .project-name { color: var(--text-primary); font-weight: 500; margin-right: 16px; }
.titlebar .menu { display: flex; gap: 16px; }
.titlebar .menu span { cursor: pointer; }
.titlebar .menu span:hover { color: var(--text-primary); }
.titlebar .spacer { flex: 1; }
.titlebar .run-buttons { display: flex; gap: 6px; }
.titlebar .run-buttons button {
  background: none; border: 1px solid var(--border); color: var(--text-muted);
  padding: 2px 10px; border-radius: 3px; cursor: pointer; font-size: 11px;
}
.titlebar .run-buttons button:hover { color: var(--success); border-color: var(--success); }

/* === Activity bar === */
.activity-bar {
  grid-area: activity; background: var(--bg-tertiary);
  display: flex; flex-direction: column; align-items: center;
  padding-top: 8px; gap: 4px; border-right: 1px solid var(--border);
}
.activity-btn {
  width: 36px; height: 36px; display: grid; place-items: center;
  border-radius: 6px; cursor: pointer; color: var(--text-muted);
  font-size: 18px; user-select: none; position: relative;
}
.activity-btn:hover { background: var(--bg-hover); color: var(--text-primary); }
.activity-btn.active { color: var(--accent); }
.activity-btn.active::before {
  content: ''; position: absolute; left: 0; top: 6px; bottom: 6px;
  width: 2px; background: var(--accent); border-radius: 0 2px 2px 0;
}
.activity-btn.bottom { margin-top: auto; margin-bottom: 8px; }

/* === Sidebar === */
.sidebar {
  grid-area: sidebar; background: var(--bg-secondary);
  border-right: 1px solid var(--border);
  display: flex; flex-direction: column; overflow: hidden;
}
.sidebar-panel { display: none; flex-direction: column; height: 100%; }
.sidebar-panel.active { display: flex; }
.sidebar-header {
  padding: 8px 12px; font-size: 11px; font-weight: 600;
  text-transform: uppercase; color: var(--text-muted);
  letter-spacing: 0.5px;
  display: flex; justify-content: space-between; align-items: center;
}

.file-tree { flex: 1; overflow: auto; padding: 4px 0; font-size: 13px; }
.tree-item {
  display: flex; align-items: center; padding: 4px 8px 4px 12px;
  cursor: pointer; user-select: none; white-space: nowrap;
}
.tree-item:hover { background: var(--bg-hover); }
.tree-item.selected { background: var(--bg-active); }
.tree-item .icon { margin-right: 6px; font-size: 12px; width: 14px; text-align: center; }
.tree-item.indent { padding-left: 28px; }

.search-row { display: flex; gap: 4px; padding: 6px 8px; border-bottom: 1px solid var(--border); }
.search-row input {
  flex: 1; background: var(--bg-tertiary); border: 1px solid var(--border);
  color: var(--text-primary); padding: 4px 8px; border-radius: 3px;
  font-size: 12px; outline: none;
}
.search-row input:focus { border-color: var(--accent); }
.search-row button {
  background: var(--bg-tertiary); border: 1px solid var(--border);
  color: var(--text-muted); padding: 0 6px; border-radius: 3px;
  cursor: pointer; font-size: 10px;
}
.search-results { flex: 1; overflow: auto; padding: 6px 0; font-size: 12px; }
.result-file-header {
  padding: 4px 10px; display: flex; gap: 6px; align-items: center;
  color: var(--text-primary); font-size: 12px;
}
.result-file-header .count {
  margin-left: auto; color: var(--text-muted);
  background: var(--bg-tertiary); padding: 0 6px; border-radius: 8px; font-size: 10px;
}
.result-item {
  padding: 3px 10px 3px 30px; font-family: var(--font-code);
  font-size: 11px; color: var(--text-muted); cursor: pointer;
  white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
}
.result-item:hover { background: var(--bg-hover); }
.result-item .line { display: inline-block; width: 24px; color: #5a5d63; margin-right: 6px; }
.result-item mark { background: #5c4520; color: #f0d39f; padding: 0 2px; border-radius: 2px; }

.scm-panel { flex: 1; overflow: hidden; display: flex; flex-direction: column; font-size: 12px; }
.scm-branch {
  padding: 8px 12px; display: flex; align-items: center; gap: 6px;
  border-bottom: 1px solid var(--border);
}
.scm-branch .muted { color: var(--text-muted); font-size: 11px; }
.scm-branch .sync { margin-left: auto; background: none; border: none; color: var(--text-muted); cursor: pointer; }
.scm-changes { flex: 1; overflow: auto; }
.scm-group-header {
  padding: 6px 12px; font-size: 10px; text-transform: uppercase;
  color: var(--text-muted); letter-spacing: 0.5px;
  display: flex; gap: 6px; align-items: center;
  background: var(--bg-tertiary);
}
.scm-group-header .count {
  margin-left: auto; background: var(--bg-hover); padding: 0 6px;
  border-radius: 8px; font-size: 10px;
}
.scm-file {
  padding: 4px 12px 4px 26px; display: flex; align-items: center; gap: 8px; cursor: pointer;
}
.scm-file:hover { background: var(--bg-hover); }
.scm-file .status { font-family: var(--font-code); font-weight: 700; font-size: 11px; }
.scm-file .status.M { color: var(--warning); }
.scm-file .status.A { color: var(--success); }
.scm-file .path { flex: 1; }
.scm-file .add { color: var(--success); font-size: 10px; }
.scm-file .del { color: var(--error); font-size: 10px; }
.scm-commit { border-top: 1px solid var(--border); padding: 8px; }
.scm-commit textarea {
  width: 100%; height: 55px; resize: none;
  background: var(--bg-tertiary); border: 1px solid var(--border);
  color: var(--text-primary); padding: 6px 8px; border-radius: 3px;
  font-family: var(--font-ui); font-size: 12px; outline: none;
}
.scm-commit textarea:focus { border-color: var(--accent); }
.scm-commit button {
  width: 100%; padding: 6px; margin-top: 6px;
  background: var(--accent); border: none; border-radius: 3px;
  color: white; cursor: pointer; font-size: 12px;
}

.ai-panel { flex: 1; overflow: hidden; display: flex; flex-direction: column; font-size: 12px; }
.ai-context { padding: 6px 10px; display: flex; gap: 4px; flex-wrap: wrap; border-bottom: 1px solid var(--border); }
.ai-context .chip {
  background: var(--bg-tertiary); border: 1px solid var(--border);
  padding: 2px 8px; border-radius: 12px; font-size: 10px; color: var(--text-muted);
}
.ai-messages { flex: 1; overflow-y: auto; padding: 8px 10px; display: flex; flex-direction: column; gap: 10px; }
.ai-msg { display: flex; gap: 6px; align-items: flex-start; }
.ai-msg.user { flex-direction: row-reverse; }
.ai-msg .avatar {
  width: 22px; height: 22px; border-radius: 50%; flex-shrink: 0;
  display: grid; place-items: center; background: var(--bg-tertiary); font-size: 11px;
}
.ai-msg .bubble {
  background: var(--bg-tertiary); padding: 6px 10px; border-radius: 8px;
  max-width: 85%; line-height: 1.5; font-size: 12px;
}
.ai-msg.user .bubble { background: var(--bg-active); }
.ai-msg .bubble pre {
  background: var(--bg-primary); padding: 6px; border-radius: 4px;
  margin: 4px 0; font-family: var(--font-code); font-size: 11px; overflow-x: auto;
}
.ai-input { border-top: 1px solid var(--border); padding: 6px; }
.ai-input textarea {
  width: 100%; height: 50px; resize: none;
  background: var(--bg-tertiary); border: 1px solid var(--border);
  color: var(--text-primary); padding: 6px 8px; border-radius: 4px;
  font-family: var(--font-ui); font-size: 12px; outline: none;
}

.editor-area {
  grid-area: editor; display: flex; flex-direction: column;
  background: var(--bg-primary); overflow: hidden; position: relative;
}
.tab-bar {
  height: 34px; background: var(--bg-tertiary);
  display: flex; align-items: stretch; overflow-x: auto;
  border-bottom: 1px solid var(--border);
}
.tab-bar:empty { display: none; }
.tab {
  display: flex; align-items: center; gap: 8px;
  padding: 0 12px; min-width: 120px; max-width: 200px;
  background: var(--bg-tertiary);
  border-right: 1px solid var(--border);
  font-size: 12px; cursor: pointer; color: var(--text-muted);
  position: relative; flex-shrink: 0;
}
.tab:hover { background: var(--bg-hover); }
.tab.active {
  background: var(--bg-primary); color: var(--text-primary);
  box-shadow: inset 0 -1px 0 var(--accent);
}
.tab .name { flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.tab .close { opacity: 0.5; font-size: 14px; padding: 0 2px; }
.tab .close:hover { opacity: 1; background: var(--bg-hover); border-radius: 3px; }
.tab .dirty { color: var(--warning); font-size: 10px; }

.breadcrumbs {
  height: 22px; display: flex; align-items: center; gap: 6px;
  padding: 0 12px; font-size: 11px; color: var(--text-muted);
  border-bottom: 1px solid var(--border);
}
.breadcrumbs:empty { display: none; }
.breadcrumbs .sep { opacity: 0.4; }

.editor-stack { flex: 1; overflow: hidden; position: relative; }
.editor-container { width: 100%; height: 100%; }

.welcome-overlay {
  position: absolute; inset: 0; background: var(--bg-primary);
  display: flex; flex-direction: column; align-items: center;
  padding: 50px 40px; overflow: auto; z-index: 10;
}
.welcome-overlay.hidden { display: none; }
.welcome-logo { font-size: 32px; font-weight: 300; letter-spacing: -0.5px; }
.welcome-logo span { color: var(--accent); font-weight: 600; }
.welcome-sub { color: var(--text-muted); margin-top: 6px; font-size: 12px; }
.welcome-grid { display: grid; grid-template-columns: repeat(3, 220px); gap: 24px; margin-top: 36px; }
.welcome-card h4 {
  font-size: 10px; text-transform: uppercase; letter-spacing: 1px;
  color: var(--text-muted); margin-bottom: 8px;
}
.welcome-card ul { list-style: none; }
.welcome-card li {
  padding: 5px 8px; font-size: 12px; border-radius: 4px; cursor: pointer;
  display: flex; justify-content: space-between; align-items: center;
}
.welcome-card li:hover { background: var(--bg-hover); }
.welcome-card kbd {
  font-family: var(--font-code); font-size: 9px;
  background: var(--bg-tertiary); color: var(--text-muted);
  padding: 1px 5px; border-radius: 3px;
}

.panel {
  grid-area: panel; background: var(--bg-secondary);
  border-left: 1px solid var(--border);
  display: flex; flex-direction: column; overflow: hidden;
}
.panel-tabs {
  display: flex; border-bottom: 1px solid var(--border);
  background: var(--bg-tertiary); font-size: 11px;
}
.panel-tab {
  padding: 8px 12px; cursor: pointer; color: var(--text-muted);
  border-bottom: 2px solid transparent; user-select: none;
}
.panel-tab:hover { color: var(--text-primary); }
.panel-tab.active { color: var(--text-primary); border-bottom-color: var(--accent); }
.panel-content { flex: 1; overflow: auto; display: none; font-size: 12px; }
.panel-content.active { display: block; }
.problem-item {
  padding: 6px 12px; display: flex; gap: 8px; cursor: pointer;
  border-left: 2px solid transparent;
}
.problem-item:hover { background: var(--bg-hover); }
.problem-item.error { border-left-color: var(--error); }
.problem-item.warning { border-left-color: var(--warning); }
.problem-item .loc { color: var(--text-muted); font-family: var(--font-code); font-size: 10px; }
.terminal-content {
  background: var(--bg-tertiary); font-family: var(--font-code); font-size: 12px;
  padding: 8px; color: var(--text-primary); height: 100%;
}
.terminal-content .prompt { color: var(--success); }
.terminal-content .cmd { color: var(--text-primary); }

.debug-section { padding: 8px 12px; border-bottom: 1px solid var(--border); }
.debug-section h4 {
  font-size: 10px; text-transform: uppercase; letter-spacing: 0.5px;
  color: var(--text-muted); margin-bottom: 6px;
}
.debug-toolbar { display: flex; gap: 4px; padding: 6px 8px; border-bottom: 1px solid var(--border); }
.debug-toolbar button {
  background: var(--bg-tertiary); border: 1px solid var(--border);
  color: var(--text-primary); padding: 4px 8px; border-radius: 3px;
  cursor: pointer; font-size: 11px;
}
.debug-toolbar button:hover { background: var(--bg-hover); }
.stack-item {
  padding: 3px 0; font-size: 11px; cursor: pointer;
}
.stack-item.active { color: var(--accent); }
.var-row { display: flex; gap: 8px; font-size: 11px; padding: 2px 0; }
.var-name { color: #9cdcfe; }
.var-value { color: #ce9178; }

.statusbar {
  grid-area: status; background: var(--bg-active);
  display: flex; align-items: center; gap: 16px;
  padding: 0 12px; font-size: 11px; color: #dfe1e5;
}
.statusbar .item { cursor: pointer; }
.statusbar .item:hover { opacity: 0.8; }
.statusbar .spacer { flex: 1; }

.cmd-overlay {
  position: fixed; inset: 0; background: rgba(0,0,0,0.4);
  display: flex; justify-content: center; padding-top: 12vh; z-index: 1000;
}
.cmd-overlay.hidden { display: none; }
.cmd-palette {
  width: 560px; background: var(--bg-secondary); border: 1px solid var(--border);
  border-radius: 6px; box-shadow: 0 12px 48px rgba(0,0,0,0.6); overflow: hidden;
  max-height: 500px; display: flex; flex-direction: column;
}
.cmd-palette input {
  width: 100%; padding: 12px 16px; font-size: 14px;
  background: var(--bg-secondary); border: none; border-bottom: 1px solid var(--border);
  color: var(--text-primary); outline: none;
}
.cmd-palette ul { list-style: none; overflow-y: auto; max-height: 400px; }
.cmd-palette li {
  padding: 8px 16px; display: flex; justify-content: space-between;
  font-size: 13px; cursor: pointer;
}
.cmd-palette li:hover, .cmd-palette li.active { background: var(--bg-active); }
.cmd-palette kbd {
  font-family: var(--font-code); font-size: 10px;
  background: var(--bg-tertiary); padding: 2px 6px; border-radius: 3px;
  color: var(--text-muted);
}

.modal-overlay {
  position: fixed; inset: 0; background: rgba(0,0,0,0.5);
  display: grid; place-items: center; z-index: 900;
}
.modal-overlay.hidden { display: none; }
.modal {
  background: var(--bg-secondary); border: 1px solid var(--border);
  border-radius: 6px; width: 720px; max-height: 80vh;
  display: flex; flex-direction: column;
  box-shadow: 0 20px 60px rgba(0,0,0,0.6);
}
.modal-header {
  padding: 12px 16px; border-bottom: 1px solid var(--border);
  display: flex; justify-content: space-between; align-items: center;
}
.modal-header h3 { font-size: 13px; font-weight: 500; }
.modal-header button {
  background: none; border: none; color: var(--text-muted);
  cursor: pointer; font-size: 16px;
}
.modal-body { display: flex; overflow: hidden; flex: 1; min-height: 300px; }
.modal-list {
  width: 200px; border-right: 1px solid var(--border);
  padding: 8px 0; overflow: auto; background: var(--bg-tertiary);
}
.modal-list-item {
  padding: 6px 12px; font-size: 12px; cursor: pointer;
  display: flex; flex-direction: column; gap: 2px;
}
.modal-list-item:hover { background: var(--bg-hover); }
.modal-list-item.active { background: var(--bg-active); }
.modal-list-item .target { color: var(--text-muted); font-size: 10px; padding-left: 12px; }
.modal-form { flex: 1; padding: 14px 18px; overflow: auto; display: flex; flex-direction: column; gap: 10px; }
.form-group { display: flex; flex-direction: column; gap: 4px; }
.form-group label { font-size: 11px; color: var(--text-muted); }
.form-group input, .form-group select, .form-group textarea {
  background: var(--bg-tertiary); border: 1px solid var(--border);
  color: var(--text-primary); padding: 6px 10px; border-radius: 3px;
  font-size: 12px; font-family: var(--font-ui); outline: none;
}
.form-group input:focus, .form-group select:focus, .form-group textarea:focus { border-color: var(--accent); }
.form-group textarea { resize: vertical; min-height: 50px; font-family: var(--font-code); font-size: 11px; }
.modal-footer {
  padding: 10px 16px; display: flex; justify-content: flex-end; gap: 8px;
  border-top: 1px solid var(--border);
}
.modal-footer button {
  padding: 6px 14px; background: var(--bg-tertiary); border: 1px solid var(--border);
  border-radius: 3px; color: var(--text-primary); cursor: pointer; font-size: 12px;
}
.modal-footer button.primary { background: var(--accent); border-color: var(--accent); color: white; }

#loading {
  position: fixed; inset: 0; background: var(--bg-primary);
  display: grid; place-items: center; z-index: 2000;
  font-size: 13px; color: var(--text-muted);
}
#loading.hidden { display: none; }

::-webkit-scrollbar { width: 10px; height: 10px; }
::-webkit-scrollbar-thumb { background: #3a3d42; border-radius: 5px; }
::-webkit-scrollbar-thumb:hover { background: #4a4d52; }
::-webkit-scrollbar-track { background: transparent; }
</style>
</head>
<body>

<div id="loading">Loading editor…</div>

<div class="app">

  <div class="titlebar">
    <span class="project-name">🚀 myproject</span>
    <div class="menu">
      <span>File</span><span>Edit</span><span>View</span><span>Run</span><span>Help</span>
    </div>
    <div class="spacer"></div>
    <div class="run-buttons">
      <button id="btn-run">▶ Run</button>
      <button id="btn-debug">🐞 Debug</button>
      <button id="btn-run-config">⚙ Config</button>
    </div>
  </div>

  <div class="activity-bar">
    <div class="activity-btn active" data-panel="explorer" title="Explorer">📁</div>
    <div class="activity-btn" data-panel="search" title="Search">🔍</div>
    <div class="activity-btn" data-panel="scm" title="Source Control">🌿</div>
    <div class="activity-btn" data-panel="ai" title="AI Assistant">✨</div>
    <div class="activity-btn bottom" data-panel="settings" title="Settings">⚙️</div>
  </div>

  <div class="sidebar">
    <div class="sidebar-panel active" id="panel-explorer">
      <div class="sidebar-header"><span>Explorer</span><span>⋯</span></div>
      <div class="file-tree" id="fileTree">
        <div class="tree-item" data-dir>▾ 📁 src</div>
        <div class="tree-item indent" data-file="src/main.cpp"><span class="icon">◆</span>main.cpp</div>
        <div class="tree-item indent" data-file="src/engine.cpp"><span class="icon">◆</span>engine.cpp</div>
        <div class="tree-item" data-dir>▾ 📁 include</div>
        <div class="tree-item indent" data-file="include/engine.h"><span class="icon">◇</span>engine.h</div>
        <div class="tree-item" data-file="CMakeLists.txt">📄 CMakeLists.txt</div>
        <div class="tree-item" data-file="README.md">📄 README.md</div>
      </div>
    </div>

    <div class="sidebar-panel" id="panel-search">
      <div class="sidebar-header"><span>Search</span></div>
      <div class="search-row">
        <input type="text" placeholder="Search" value="Engine" />
        <button>Aa</button><button>ab|</button><button>.*</button>
      </div>
      <div class="search-results">
        <div class="result-file-header">▾ <b>engine.cpp</b><span class="count">3</span></div>
        <div class="result-item" data-file="src/engine.cpp"><span class="line">12</span>struct <mark>Engine</mark> {</div>
        <div class="result-item" data-file="src/engine.cpp"><span class="line">28</span>    <mark>Engine</mark>::start() {</div>
        <div class="result-item" data-file="src/engine.cpp"><span class="line">41</span>    return new <mark>Engine</mark>();</div>
        <div class="result-file-header">▾ <b>main.cpp</b><span class="count">1</span></div>
        <div class="result-item" data-file="src/main.cpp"><span class="line">7</span><mark>Engine</mark> e;</div>
      </div>
    </div>

    <div class="sidebar-panel" id="panel-scm">
      <div class="sidebar-header"><span>Source Control</span></div>
      <div class="scm-panel">
        <div class="scm-branch">🌿 <b>main</b><span class="muted">→ origin/main</span><button class="sync">⟳</button></div>
        <div class="scm-changes">
          <div class="scm-group-header">▾ Staged<span class="count">2</span></div>
          <div class="scm-file" data-file="src/main.cpp"><span class="status M">M</span><span class="path">src/main.cpp</span><span class="add">+12</span><span class="del">−3</span></div>
          <div class="scm-file" data-file="src/engine.cpp"><span class="status A">A</span><span class="path">src/engine.cpp</span><span class="add">+84</span></div>
          <div class="scm-group-header">▾ Changes<span class="count">1</span></div>
          <div class="scm-file" data-file="include/engine.h"><span class="status M">M</span><span class="path">include/engine.h</span><span class="add">+4</span><span class="del">−1</span></div>
        </div>
        <div class="scm-commit">
          <textarea placeholder="Commit message"></textarea>
          <button>✓ Commit</button>
        </div>
      </div>
    </div>

    <div class="sidebar-panel" id="panel-ai">
      <div class="sidebar-header"><span>✨ AI Assistant</span></div>
      <div class="ai-panel">
        <div class="ai-context">
          <span class="chip">📄 main.cpp</span>
          <span class="chip">Ln 12–28</span>
        </div>
        <div class="ai-messages">
          <div class="ai-msg user"><div class="avatar">🧑</div><div class="bubble">Why does this segfault?</div></div>
          <div class="ai-msg"><div class="avatar">✨</div><div class="bubble">Line 15: <code>engine-&gt;start()</code> is called on an uninitialized pointer.<pre>- Engine* engine;
+ Engine* engine = new Engine();</pre></div></div>
        </div>
        <div class="ai-input">
          <textarea placeholder="Ask anything…"></textarea>
        </div>
      </div>
    </div>

    <div class="sidebar-panel" id="panel-settings">
      <div class="sidebar-header"><span>Settings</span></div>
      <div style="padding: 16px; font-size: 12px; color: var(--text-muted);">Settings go here.</div>
    </div>
  </div>

  <div class="editor-area">
    <div class="tab-bar" id="tabBar"></div>
    <div class="breadcrumbs" id="breadcrumbs"></div>
    <div class="editor-stack" id="editorStack"></div>
    <div class="welcome-overlay" id="welcome">
      <div class="welcome-logo">🚀 <span>Anvil</span></div>
      <div class="welcome-sub">Forge C++ at full speed</div>
      <div class="welcome-grid">
        <div class="welcome-card">
          <h4>Start</h4>
          <ul>
            <li data-action="open-file">📄 Open File…</li>
            <li data-action="open-folder">📁 Open Folder…</li>
            <li data-action="clone">🌿 Clone Repository…</li>
          </ul>
        </div>
        <div class="welcome-card">
          <h4>Recent</h4>
          <ul>
            <li data-file="src/main.cpp">🗂 myproject</li>
            <li data-file="src/engine.cpp">🗂 engine</li>
            <li data-file="include/engine.h">🗂 sandbox</li>
          </ul>
        </div>
        <div class="welcome-card">
          <h4>Learn</h4>
          <ul>
            <li>📘 Getting started</li>
            <li>⚡ Configuring clangd</li>
            <li>🐞 Debugging with GDB</li>
          </ul>
        </div>
      </div>
    </div>
  </div>

  <div class="panel">
    <div class="panel-tabs">
      <div class="panel-tab active" data-tab="problems">Problems</div>
      <div class="panel-tab" data-tab="terminal">Terminal</div>
      <div class="panel-tab" data-tab="debug">Debug</div>
    </div>
    <div class="panel-content active" id="content-problems">
      <div class="problem-item error">
        <span>⛔</span>
        <div>
          <div>use of undeclared identifier 'foo'</div>
          <div class="loc" data-file="src/main.cpp">main.cpp:12:5</div>
        </div>
      </div>
      <div class="problem-item warning">
        <span>⚠️</span>
        <div>
          <div>unused variable 'temp'</div>
          <div class="loc" data-file="src/engine.cpp">engine.cpp:44:9</div>
        </div>
      </div>
      <div class="problem-item warning">
        <span>⚠️</span>
        <div>
          <div>implicit conversion int → float</div>
          <div class="loc" data-file="include/engine.h">engine.h:8:2</div>
        </div>
      </div>
    </div>
    <div class="panel-content" id="content-terminal">
      <div class="terminal-content">
        <div><span class="prompt">~/myproject $</span> <span class="cmd">cmake --build build</span></div>
        <div>[ 50%] Building CXX object CMakeFiles/main.dir/src/main.cpp.o</div>
        <div>[100%] Linking CXX executable main</div>
        <div>[100%] Built target main</div>
        <div><span class="prompt">~/myproject $</span> <span style="opacity:.3">█</span></div>
      </div>
    </div>
    <div class="panel-content" id="content-debug">
      <div class="debug-toolbar">
        <button>▶</button><button>⏭</button><button>⏬</button><button>⏫</button><button>⏹</button>
      </div>
      <div class="debug-section">
        <h4>Call Stack</h4>
        <div class="stack-item active">▶ main() — main.cpp:14</div>
        <div class="stack-item">  Engine::start() — engine.cpp:28</div>
        <div class="stack-item">  __libc_start_main — libc.so</div>
      </div>
      <div class="debug-section">
        <h4>Variables</h4>
        <div class="var-row"><span class="var-name">e</span><span class="var-value">{name: "Anvil", version: 1}</span></div>
        <div class="var-row"><span class="var-name">nums</span><span class="var-value">std::vector&lt;int&gt; size=5</span></div>
        <div class="var-row"><span class="var-name">i</span><span class="var-value">0</span></div>
      </div>
      <div class="debug-section">
        <h4>Breakpoints</h4>
        <div class="stack-item">🔴 main.cpp:14</div>
        <div class="stack-item">🔴 engine.cpp:28</div>
      </div>
    </div>
  </div>

  <div class="statusbar">
    <div class="item">🌿 main*</div>
    <div class="item">⛔ 1  ⚠️ 2</div>
    <div class="spacer"></div>
    <div class="item" id="sb-cursor">Ln 1, Col 1</div>
    <div class="item">Spaces: 4</div>
    <div class="item">UTF-8</div>
    <div class="item">C++20</div>
    <div class="item">🔔 clangd</div>
  </div>

</div>

<div class="cmd-overlay hidden" id="cmdPalette">
  <div class="cmd-palette">
    <input id="cmdInput" placeholder="Type a command… (Ctrl+Shift+P)" />
    <ul id="cmdList"></ul>
  </div>
</div>

<div class="modal-overlay hidden" id="runConfigModal">
  <div class="modal">
    <div class="modal-header"><h3>Run/Debug Configurations</h3><button id="modalClose">✕</button></div>
    <div class="modal-body">
      <div class="modal-list">
        <div class="modal-list-item active">▶ <b>main</b><span class="target">CMake Application</span></div>
        <div class="modal-list-item">▶ <b>tests</b><span class="target">Google Test</span></div>
        <div class="modal-list-item" style="color: var(--accent)">+ Add New</div>
      </div>
      <div class="modal-form">
        <div class="form-group"><label>Name</label><input value="main" /></div>
        <div class="form-group"><label>Target</label><select><option>main</option><option>engine</option></select></div>
        <div class="form-group"><label>Configuration</label><select><option>Debug</option><option>Release</option></select></div>
        <div class="form-group"><label>Working directory</label><input value="$PROJECT_DIR$/build/bin" /></div>
        <div class="form-group"><label>Program arguments</label><input value="--verbose" /></div>
        <div class="form-group"><label>Environment variables</label><textarea>PATH=/usr/local/bin:$PATH</textarea></div>
      </div>
    </div>
    <div class="modal-footer">
      <button>Apply</button>
      <button class="primary">OK</button>
      <button>Cancel</button>
    </div>
  </div>
</div>

<script src="https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs/loader.js"></script>
<script>
const FILES = {
  'src/main.cpp': {
    language: 'cpp',
    content: `#include <iostream>
#include <vector>
#include "engine.h"

int main() {
    Engine e{ "Anvil", 1 };
    e.start();

    std::vector<int> nums{1, 2, 3, 4, 5};
    for (auto n : nums) {
        std::cout << n * 2 << '\\n';
    }
    return 0;
}
`
  },
  'src/engine.cpp': {
    language: 'cpp',
    content: `#include "engine.h"
#include <iostream>

Engine::Engine(std::string n, int v) : name(std::move(n)), version(v) {}

void Engine::start() {
    std::cout << "Engine " << name << " v" << version << " started\\n";
}

Engine* createEngine() {
    return new Engine("Default", 1);
}
`
  },
  'include/engine.h': {
    language: 'cpp',
    content: `#pragma once
#include <string>

struct Engine {
    std::string name;
    int version;

    Engine(std::string n, int v);
    void start();
};

Engine* createEngine();
`
  },
  'CMakeLists.txt': {
    language: 'plaintext',
    content: `cmake_minimum_required(VERSION 3.20)
project(myproject CXX)

set(CMAKE_CXX_STANDARD 20)

add_executable(main
    src/main.cpp
    src/engine.cpp
)
target_include_directories(main PRIVATE include)
`
  },
  'README.md': {
    language: 'markdown',
    content: `# Anvil

A prototype C++ IDE built with Monaco Editor.

## Features
- File tree
- Tabs
- C++ syntax highlighting
- Problems panel
- Terminal
- Debug view
`
  },
};

const state = {
  tabs: [],
  activeTab: null,
  models: {},
  editor: null,
};

require.config({ paths: { vs: 'https://cdn.jsdelivr.net/npm/monaco-editor@0.45.0/min/vs' } });

require(['vs/editor/editor.main'], function () {
  document.getElementById('loading').classList.add('hidden');

  monaco.editor.defineTheme('mycpp-dark', {
    base: 'vs-dark', inherit: true,
    rules: [
      { token: 'comment', foreground: '6a9955', fontStyle: 'italic' },
      { token: 'keyword', foreground: 'c586c0' },
      { token: 'type', foreground: '4ec9b0' },
      { token: 'string', foreground: 'ce9178' },
      { token: 'number', foreground: 'b5cea8' },
    ],
    colors: {
      'editor.background': '#1e1f22',
      'editor.foreground': '#dfe1e5',
      'editor.lineHighlightBackground': '#2a2b2f',
      'editorLineNumber.foreground': '#5a5d63',
      'editorLineNumber.activeForeground': '#c0c3c9',
      'editorCursor.foreground': '#4a88c7',
      'editor.selectionBackground': '#2e436e',
      'editorIndentGuide.background': '#2e3033',
    }
  });

  state.editor = monaco.editor.create(document.getElementById('editorStack'), {
    value: '', language: 'plaintext', theme: 'mycpp-dark',
    fontSize: 14,
    fontFamily: 'JetBrains Mono, Fira Code, Consolas, monospace',
    fontLigatures: true,
    minimap: { enabled: true },
    scrollBeyondLastLine: false,
    smoothScrolling: true,
    cursorBlinking: 'smooth',
    renderWhitespace: 'selection',
    bracketPairColorization: { enabled: true },
    guides: { bracketPairs: true, indentation: true },
    padding: { top: 12 },
    automaticLayout: true,
  });

  state.editor.onDidChangeCursorPosition((e) => {
    document.getElementById('sb-cursor').textContent =
      `Ln ${e.position.lineNumber}, Col ${e.position.column}`;
  });

  updateWelcome();
});

function openFile(path) {
  if (!FILES[path] || !state.editor) return;
  if (!state.tabs.includes(path)) {
    state.tabs.push(path);
    state.models[path] = monaco.editor.createModel(
      FILES[path].content, FILES[path].language, monaco.Uri.file(path)
    );
  }
  state.activeTab = path;
  state.editor.setModel(state.models[path]);
  renderTabs(); renderBreadcrumbs(); updateWelcome(); highlightTreeItem(path);
}

function closeTab(path) {
  state.tabs = state.tabs.filter(t => t !== path);
  if (state.models[path]) { state.models[path].dispose(); delete state.models[path]; }
  if (state.activeTab === path) {
    state.activeTab = state.tabs[state.tabs.length - 1] || null;
    state.editor.setModel(state.activeTab ? state.models[state.activeTab] : null);
  }
  renderTabs(); renderBreadcrumbs(); updateWelcome();
}

function renderTabs() {
  const bar = document.getElementById('tabBar');
  bar.innerHTML = '';
  state.tabs.forEach(path => {
    const tab = document.createElement('div');
    tab.className = 'tab' + (path === state.activeTab ? ' active' : '');
    const name = path.split('/').pop();
    tab.innerHTML = `<span class="name">${name}</span><span class="close">✕</span>`;
    tab.addEventListener('click', (e) => {
      if (e.target.classList.contains('close')) { e.stopPropagation(); closeTab(path); }
      else { openFile(path); }
    });
    bar.appendChild(tab);
  });
}

function renderBreadcrumbs() {
  const bc = document.getElementById('breadcrumbs');
  if (!state.activeTab) { bc.innerHTML = ''; return; }
  bc.innerHTML = state.activeTab.split('/')
    .map(p => `<span>${p}</span>`).join('<span class="sep">›</span>');
}

function updateWelcome() {
  document.getElementById('welcome').classList.toggle('hidden', state.tabs.length > 0);
}

function highlightTreeItem(path) {
  document.querySelectorAll('.tree-item').forEach(el => {
    el.classList.toggle('selected', el.dataset.file === path);
  });
}

document.querySelectorAll('.activity-btn').forEach(btn => {
  btn.addEventListener('click', () => {
    const panel = btn.dataset.panel;
    document.querySelectorAll('.activity-btn').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    document.querySelectorAll('.sidebar-panel').forEach(p => p.classList.remove('active'));
    document.getElementById('panel-' + panel)?.classList.add('active');
  });
});

document.querySelectorAll('.panel-tab').forEach(tab => {
  tab.addEventListener('click', () => {
    const id = tab.dataset.tab;
    document.querySelectorAll('.panel-tab').forEach(t => t.classList.remove('active'));
    tab.classList.add('active');
    document.querySelectorAll('.panel-content').forEach(c => c.classList.remove('active'));
    document.getElementById('content-' + id).classList.add('active');
  });
});

document.getElementById('fileTree').addEventListener('click', (e) => {
  const item = e.target.closest('[data-file]');
  if (item) openFile(item.dataset.file);
});
document.querySelectorAll('.result-item').forEach(el =>
  el.addEventListener('click', () => openFile(el.dataset.file)));
document.querySelectorAll('.scm-file').forEach(el =>
  el.addEventListener('click', () => openFile(el.dataset.file)));
document.querySelectorAll('.problem-item .loc').forEach(el =>
  el.addEventListener('click', () => openFile(el.dataset.file)));
document.getElementById('welcome').addEventListener('click', (e) => {
  const item = e.target.closest('[data-file]');
  if (item) openFile(item.dataset.file);
});

document.getElementById('btn-run-config').addEventListener('click', () =>
  document.getElementById('runConfigModal').classList.remove('hidden'));
document.getElementById('modalClose').addEventListener('click', () =>
  document.getElementById('runConfigModal').classList.add('hidden'));
document.getElementById('runConfigModal').addEventListener('click', (e) => {
  if (e.target.id === 'runConfigModal') e.target.classList.add('hidden');
});

document.getElementById('btn-run').addEventListener('click', () => {
  document.querySelectorAll('.panel-tab').forEach(t =>
    t.classList.toggle('active', t.dataset.tab === 'terminal'));
  document.querySelectorAll('.panel-content').forEach(c => c.classList.remove('active'));
  document.getElementById('content-terminal').classList.add('active');
});

document.getElementById('btn-debug').addEventListener('click', () => {
  document.querySelectorAll('.panel-tab').forEach(t =>
    t.classList.toggle('active', t.dataset.tab === 'debug'));
  document.querySelectorAll('.panel-content').forEach(c => c.classList.remove('active'));
  document.getElementById('content-debug').classList.add('active');
});

const COMMANDS = [
  { title: 'File: Open File…', key: 'Ctrl+O', run: () => openFile('src/main.cpp') },
  { title: 'File: Open Folder…', key: 'Ctrl+K', run: () => {} },
  { title: 'View: Toggle Problems Panel', key: 'Ctrl+Shift+M', run: () => {
    document.querySelector('.panel-tab[data-tab="problems"]').click(); } },
  { title: 'View: Show Terminal', key: 'Ctrl+`', run: () => {
    document.querySelector('.panel-tab[data-tab="terminal"]').click(); } },
  { title: 'Run: Configuration…', key: 'Shift+F10', run: () => {
    document.getElementById('runConfigModal').classList.remove('hidden'); } },
  { title: 'Go to src/main.cpp', key: '', run: () => openFile('src/main.cpp') },
  { title: 'Go to include/engine.h', key: '', run: () => openFile('include/engine.h') },
];

const cmdPalette = document.getElementById('cmdPalette');
const cmdInput = document.getElementById('cmdInput');
const cmdList = document.getElementById('cmdList');
let cmdIndex = 0;

function openCmdPalette() {
  cmdPalette.classList.remove('hidden');
  cmdInput.value = ''; cmdIndex = 0;
  renderCmdList(); cmdInput.focus();
}
function closeCmdPalette() { cmdPalette.classList.add('hidden'); }
function renderCmdList() {
  const q = cmdInput.value.toLowerCase();
  const filtered = COMMANDS.filter(c => c.title.toLowerCase().includes(q));
  cmdList.innerHTML = '';
  filtered.forEach((c, i) => {
    const li = document.createElement('li');
    li.className = i === cmdIndex ? 'active' : '';
    li.innerHTML = `<span>${c.title}</span>${c.key ? `<kbd>${c.key}</kbd>` : ''}`;
    li.addEventListener('click', () => { c.run(); closeCmdPalette(); });
    cmdList.appendChild(li);
  });
}
cmdInput.addEventListener('input', () => { cmdIndex = 0; renderCmdList(); });
cmdInput.addEventListener('keydown', (e) => {
  const items = cmdList.querySelectorAll('li');
  if (e.key === 'ArrowDown') { e.preventDefault(); cmdIndex = Math.min(cmdIndex + 1, items.length - 1); renderCmdList(); }
  else if (e.key === 'ArrowUp') { e.preventDefault(); cmdIndex = Math.max(cmdIndex - 1, 0); renderCmdList(); }
  else if (e.key === 'Enter') {
    const q = cmdInput.value.toLowerCase();
    const filtered = COMMANDS.filter(c => c.title.toLowerCase().includes(q));
    if (filtered[cmdIndex]) { filtered[cmdIndex].run(); closeCmdPalette(); }
  }
  else if (e.key === 'Escape') closeCmdPalette();
});
cmdPalette.addEventListener('click', (e) => { if (e.target === cmdPalette) closeCmdPalette(); });

document.addEventListener('keydown', (e) => {
  if ((e.ctrlKey || e.metaKey) && e.shiftKey && e.key === 'P') { e.preventDefault(); openCmdPalette(); }
  if ((e.ctrlKey || e.metaKey) && !e.shiftKey && e.key === 'p') { e.preventDefault(); openCmdPalette(); }
  if (e.key === 'Escape') {
    closeCmdPalette();
    document.getElementById('runConfigModal').classList.add('hidden');
  }
});

updateWelcome();
</script>
</body>
</html>
```

---

## §15. Как использовать этот документ

1. **Первое сообщение к ИИ**: вставь весь документ.
2. **Попроси первую конкретную задачу**, например:
   > *«Порти прототип из §14 в Tauri + React + TypeScript по layout
   > репозитория §6.1. Начни с layout-shell (TitleBar, ActivityBar,
   > Sidebar, EditorArea, RightPanel, StatusBar) и Zustand-стора
   > editor. Monaco должен загружать sample C++ файлы из §14.
   > LSP пока не реализуй. **Валидацию IPC по §7.4 — сразу, не
   > откладывай.**»*
3. **Последующие сообщения**: ссылайся на разделы по номерам
   («обнови §5.1 цветовые токены», «добавь новую фазу в §9» и т.д.).
4. **Когда ИИ дрейфует**, отвечай: *«Перечитай §2 (Ключевые принципы),
   §7.2 (sandbox), §8.4 (anti-patterns) и §11.3 (что не делать),
   затем переделай.»*
5. **При релизе**: пройди чек-лист §12.10.

---

*Конец мастер-промпта `masterprompt_0.0.1.md`.*