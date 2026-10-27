# legit-client

**legit-client** — C++20 DLL-клиент для **Minecraft: Java Edition**, инжектируемый в процесс игры. Использует JNI-мост для взаимодействия с JVM (чтение состояния мира/игрока), хукает графический конвейер (OpenGL / GLFW / WGL / DXGI swap buffers) через MinHook и рисует оверлей на базе **Dear ImGui** с поддержкой FreeType-шрифтов и DX11/OpenGL3 бэкендов.

> Имя мьютекса-гарда: `Local\LumaWareLegitClientSingleton`. Выходной артефакт — `legit-client.dll` (shared library).

## Общая архитектура

```
legit-client.dll (DLL_PROCESS_ATTACH)
└── InitClient (фоновый поток)
    ├── AcquireInstanceGuard()          # защита от повторной загрузки
    ├── Hooks::InitOpenGL()             # поиск окна/контекста, установка хуков
    └── цикл ожидания VK_END (shutdown)
```

- **core/** — точка входа, хуки рендера, JNI-мост.
- **sdk/** — обёртки над JNI-классами Minecraft (MinecraftClient, PlayerEntity, World, Inventory, BlockEntity и др.).
- **features/** — функциональные модули, разделённые по категориям.
- **menu/** — GUI-фреймворк (ImGui + кастомные виджеты, конфиги, шрифты, темы) и сторонние библиотеки.

## Категории фич

### Combat (`features/combat`)
- **hitbox** — расширение/визуализация хитбоксов.
- **no_friend_damage** — запрет урона по союзникам/друзьям.
- **shift_tap** — автоматизация shift-tap (движение/бхоп-подобная механика).
- **triggerbot** — автострельба при наведении на цель.

### Movement (`features/movement`)
- **autosprint** — автоспринт.
- **gui_move** — управление движением через GUI (свободный курсор в меню).

### Misc (`features/misc`)
- **auto_respawn** — авто-респавн после смерти.
- **discord_rpc** — интеграция Discord Rich Presence.
- **friends** — список друзей.
- **item_scroller** — скролл предметов/инвентаря.
- **mcf** — дополнительные misc-функции.

### Render (`features/render`)
- **esp** — ESP-отрисовка игроков/сущностей.
- **hud** — HUD-оверлей (информация на экране).
- **name_protect** — скрытие/замена никнейма.
- **no_render** — отключение рендера отдельных элементов.
- **storage_esp** — ESP по контейнерам (сундуки, бочки, воронки и т.п.).
- **world_tweaks** — настройки мира/рендера.

### Common (`features/common`)
- **feature_runtime** — единый тик фич (вызывается каждый кадр рендера).
- **input_utils** — утилиты ввода.
- **inventory_utils** — работа с инвентарём.
- **minecraft_feature_utils** — общие хелперы для фич.

## JNI-мост (`core/jni_bridge`)

- Ищет загруженную `jvm.dll` в процессе.
- Получает `JavaVM*` через `JNI_GetCreatedJavaVMs`.
- Прикрепляет текущий поток как демон-поток (`AttachCurrentThreadAsDaemon`) и кэширует `JNIEnv*`.

## Рендер-хуки (`core/hooks`)

- Хукает `SwapBuffers` / `wglSwapBuffers` и GLFW swap-buffers.
- Сохраняет/восстанавливает состояние OpenGL (VAO, VBO, EBO, шейдеры, текстуры, фреймбуферы, выравнивание пикселей и т.д.), чтобы ImGui не ломал рендер Minecraft.
- Инициализирует ImGui (Win32 + OpenGL3) и FreeType-шрифты.
- В каждом кадре вызывает `Features::Runtime::Tick()` и рисует меню/оверлей.

## GUI-фреймворк (`menu/framework`)

- Кастомный GUI поверх ImGui: окна, секции, чайлды, кнопки, чекбоксы, слайдеры, дропдауны, color edit, keybind, text field, уведомления.
- Конфиги/сохранение настроек (`helpers/config`).
- Размытие/отрисовка (`helpers/blur`, `helpers/draw`), поиск (`helpers/search`), шрифты (`helpers/fonts`).
- Темы/цвета (`settings/colors`, `settings/variables`, `settings/elements`).

## Сторонние зависимости

- **Dear ImGui** (DX9/DX10/DX11/DX12, OpenGL3, Win32, SDL2 бэкенды) — `menu/thirdparty/imgui`.
- **MinHook** — `core/minhook` (git submodule) + внешний путь `kkkkk/external/minhook`.
- **FreeType** — `menu/thirdparty/freetype`.
- **stb_image** — `menu/thirdparty/stb`.
- **DirectX SDK (DXSDK)** — `menu/thirdparty/dxsdk`.
- **JNI** — `find_package(Java/JNI)`.

## Сборка

Требования: CMake ≥ 3.20, C++20, Windows, JDK (JNI), Visual Studio/MSVC (предполагается, судя по путям и `.lib`).

```bash
cmake -S . -B build
cmake --build build --config Release
```

Важные пути заданы в `CMakeLists.txt` через `BASE_DIR = "C:/Users/Aura/Desktop/Legit"` — там ожидаются `menu/`, `kkkkk/external/minhook`. Перед сборкой нужно либо положить проект по этому пути, либо отредактировать `BASE_DIR`.

Линкуемые библиотеки: `d3d11`, `dxgi`, `dwmapi`, `d3dcompiler`, `opengl32`, `freetype.lib`, JNI.

Определения компиляции: `NOMINMAX`, `WIN32_LEAN_AND_MEAN`.

## Запуск/использование

1. Собрать `legit-client.dll`.
2. Инжектировать DLL в процесс Minecraft (Java Edition) любым DLL-инжектором.
3. После загрузки клиент инициализирует хуки и отрисует меню.
4. Завершение работы — клавиша **END** (VK_END) или через GUI.

## Структура репозитория

```
neamaz/
├── CMakeLists.txt
├── core/
│   ├── main.cpp
│   ├── hooks.cpp / hooks.h
│   ├── jni_bridge.cpp / jni_bridge.h
│   └── minhook/          (git submodule)
├── sdk/
│   └── minecraft_classes.cpp / .h
├── features/
│   ├── combat/
│   ├── common/
│   ├── misc/
│   ├── movement/
│   └── render/
└── menu/
    ├── framework/
    └── thirdparty/
```

## Целевая версия игры

Проект рассчитан на **Minecraft: Java Edition 1.21.11**. Код использует Yarn-маппинги (смешанные имена `net/minecraft/...` и обфусцированные `class_xxxx`/`method_xxxx`/`field_xxxx`), актуальные для этой версии. При обновлении клиента маппинги и идентификаторы классов/методов нужно перепроверять.

## Сборка через GitHub Actions

В репозитории добавлен workflow `.github/workflows/build.yml`. Он собирает проект на `windows-latest`:
- checkout
- установка Temurin JDK 17
- `cmake -S . -B build -A x64`
- `cmake --build build --config Release`
- загрузка `build/Release/legit-client.dll` как artifact

Зависимости (MinHook, FreeType) подтягиваются автоматически через CMake FetchContent.

## Примечания

- Проект ориентирован на Windows (Win32 API, DirectX, OpenGL).
- Singleton-гард предотвращает повторную загрузку DLL в один процесс.
- Все фичи тикают из рендер-хука, поэтому их логика привязана к кадрам рендеринга.
