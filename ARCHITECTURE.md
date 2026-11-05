# SerialConsole: clickable architecture

The Arduino sketch is the hardware adapter. Console commands and optional views use exact PortableShell header exports; AVR and STM32 selection does not substitute a second renderer.

Click a component to inspect its implementation. Links are pinned to source commit `85c891cbab7e35b71ff370f7c848ce2557f27e6e`, so this map remains inspectable as the code evolves. Arrows describe source-level relationships, not proof of deployment or physical behavior.

## Serial input to hardware action

The loop supplies time and bounded input. LED access occurs through callbacks in the sketch, not in the shared shell components.

```mermaid
flowchart TD
    sdk["Serial and GPIO adapter"]
    console["Command registration and dispatch"]
    editor["Editing / history / suggestions"]
    parse["Argument parser"]
    view["Optional view adapter"]
    shared["Snapshot / Action renderer"]
    telnet["Optional Telnet byte framing"]
    sdk -->|bytes and ticks| console
    console --> editor
    console --> parse
    sdk -->|when enabled| view
    view --> shared
    shared -->|requested action| view
    view -->|validated callback| sdk
    console -->|write and LED callbacks| sdk
    telnet -->|decoded bytes| console
    click sdk "https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/SerialConsole/SerialConsole.ino" "Inspect SerialConsole/SerialConsole.ino" _blank
    click console "https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/SerialConsole/src/portable_console.h" "Inspect SerialConsole/src/portable_console.h" _blank
    click editor "https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/SerialConsole/src/portable-shell/portable_editor.h" "Inspect SerialConsole/src/portable-shell/portable_editor.h" _blank
    click parse "https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/SerialConsole/src/portable-shell/portable_parse.h" "Inspect SerialConsole/src/portable-shell/portable_parse.h" _blank
    click view "https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/SerialConsole/src/portable_console_view.hpp" "Inspect SerialConsole/src/portable_console_view.hpp" _blank
    click shared "https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/SerialConsole/src/portable-shell/portable_device_view.hpp" "Inspect SerialConsole/src/portable-shell/portable_device_view.hpp" _blank
    click telnet "https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/SerialConsole/src/portable_console_telnet.h" "Inspect SerialConsole/src/portable_console_telnet.h" _blank
    classDef component fill:#202936,stroke:#71d5e8,color:#f0f3f7,stroke-width:2px
    class sdk,console,editor,parse,view,shared,telnet component
```

## Exact reuse and evidence

The Telnet helper is not a socket listener: the consumer supplies connection lifetime and authentication. Firmware compilation and simulator checks are separate from physical board acceptance.

```mermaid
flowchart TD
    sync["Export exact canonical Git blobs"]
    lock["Revision and content hashes"]
    verify["Verify vendored components"]
    tests["Native console tests"]
    preview["Rebuild and compare previews"]
    ci["Native / boards / simulator / previews"]
    limits["Measured budgets and open acceptance"]
    sync --> lock
    lock --> verify
    verify --> tests
    verify --> preview
    tests --> ci
    preview --> ci
    ci --> limits
    click sync "https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/tools/sync-portable-shell.cmake" "Inspect tools/sync-portable-shell.cmake" _blank
    click lock "https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/portable-shell.lock" "Inspect portable-shell.lock" _blank
    click verify "https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/tools/verify-portable-shell.cmake" "Inspect tools/verify-portable-shell.cmake" _blank
    click tests "https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/tests/checks.c" "Inspect tests/checks.c" _blank
    click preview "https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/tools/rebuild-previews.cmake" "Inspect tools/rebuild-previews.cmake" _blank
    click ci "https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/.github/workflows/build.yml" "Inspect .github/workflows/build.yml" _blank
    click limits "https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/VALIDATION.md" "Inspect VALIDATION.md" _blank
    classDef component fill:#202936,stroke:#71d5e8,color:#f0f3f7,stroke-width:2px
    class sync,lock,verify,tests,preview,ci,limits component
```

## Source index

These links also work in readers that disable clickable diagram nodes.

- [SerialConsole/SerialConsole.ino](https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/SerialConsole/SerialConsole.ino)
- [SerialConsole/src/portable_console.h](https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/SerialConsole/src/portable_console.h)
- [SerialConsole/src/portable-shell/portable_editor.h](https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/SerialConsole/src/portable-shell/portable_editor.h)
- [SerialConsole/src/portable-shell/portable_parse.h](https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/SerialConsole/src/portable-shell/portable_parse.h)
- [SerialConsole/src/portable_console_view.hpp](https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/SerialConsole/src/portable_console_view.hpp)
- [SerialConsole/src/portable-shell/portable_device_view.hpp](https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/SerialConsole/src/portable-shell/portable_device_view.hpp)
- [SerialConsole/src/portable_console_telnet.h](https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/SerialConsole/src/portable_console_telnet.h)
- [tools/sync-portable-shell.cmake](https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/tools/sync-portable-shell.cmake)
- [portable-shell.lock](https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/portable-shell.lock)
- [tools/verify-portable-shell.cmake](https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/tools/verify-portable-shell.cmake)
- [tests/checks.c](https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/tests/checks.c)
- [tools/rebuild-previews.cmake](https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/tools/rebuild-previews.cmake)
- [.github/workflows/build.yml](https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/.github/workflows/build.yml)
- [VALIDATION.md](https://github.com/DRSDavidSoft/Ardush/blob/85c891cbab7e35b71ff370f7c848ce2557f27e6e/VALIDATION.md)
