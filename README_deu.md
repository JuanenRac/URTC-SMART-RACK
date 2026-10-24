<p align="center">
  <img src="/images/URTC_SMART_RACK_BANNER.svg" alt="URTC Smart Rack Logo" width="100%">
</p>

# 🗄️ URTC-SMART-RACK

<p align="center"><a href="README.md">🇺🇸 English</a> | <a href="README_spa.md">🇪🇸 Español</a> | <a href="README_fra.md">🇫🇷 Français</a> | <a href="README_ita.md">🇮🇹 Italiano</a> | 🇩🇪 <b>Deutsch</b> | <a href="README_zho.md">🇨🇳 简体中文</a> | <a href="README_jpn.md">🇯🇵 日本語</a> | <a href="README_por.md">🇵🇹 Português</a></p>

### 🤖 Intelligentes Werkzeugkopf-Management mit Lebenszyklus- und Temperaturüberwachung

<p align="center">
  <img src="https://img.shields.io/badge/Licencia-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/MCU-STM32G4-003551.svg" alt="STM32G4">
  <img src="https://img.shields.io/badge/Protocol-CAN%20framing%20only-orange.svg" alt="CAN framing only">
  <img src="https://img.shields.io/badge/Feature-Smart%20Idle-green.svg" alt="Smart Idle">
</p>

**Ehrlichkeitscheck - was heute wirklich funktioniert:** `tool_id.c`, `lifecycle.c`, `preheat.c`, `protocol.c`, `rack_command.c`, `link_watchdog.c`, `tool_limits.c`, `rack_app.c`, `rack_link.c`, `rack_deframer.c`, `rack_can.c`, `preheat_limit.c` und `rack_inventory.c` sind echtes, reines C, abgesichert durch 467 bestandene `TEST_ASSERT`-Prüfungen (`tests/test_*.c`, kompiliert und ausgeführt mit dem eigenen C-Compiler des Hosts, nicht `arm-none-eabi-gcc`) - bestätigt durch Kompilieren und Ausführen genau dieser Host-Testsuite. Das deckt ID-Dekodierung, Nutzungs-/Lebenszyklus-Tracking, Vorheiz-Aktivierung und Zieltemperatur, CRC8-gerahmtes Protokoll-Parsing, Befehlsbereichsvalidierung, Link-Timeout-/Idempotenz-Handling und die Verwaltung des Mehrplatz-Werkzeuginventars (welches Werkzeug wo eingelagert ist, ein echtes Werkzeug nie gleichzeitig in zwei Plätzen) ab - alles Logik, nichts Hardware. Wie die Einleitung des READMEs bereits sagt: für diese Platine existiert noch keine PCB/kein Schaltplan, also hat nichts hier je einen echten GPIO-Pin, F-RAM-Chip, Heizer oder CAN-Transceiver angesteuert - `main.c`/`startup_stm32g4_minimal.c` beweisen nur, dass die Cross-Kompilierung und das Linken für Cortex-M4F gegen ein Platzhalter-Linker-Skript gelingen, nicht dass irgendetwas davon auf echtem Silizium läuft. Siehe `CHANGELOG.md` für das, was bisher genau ausgeliefert wurde. Auch die serielle Leitung wird geprüft: 19 Byteströme mit dem Zeitpunkt jedes Stücks (ein Frame in Teilen, Rauschen, ein falscher CRC, eine Version oder Länge, die nicht passt, eine Pause genau an der Grenze des Timeouts, ein Überlauf des Millisekundentakts) laufen durch den echten Deframer, und die Frames, die herauskommen, und seine Zähler sind die des Modells von URTC-COMMON.

---

> **Stand von Sicherheit (Safety) und Security:** Dieses Ökosystem ist kein zertifiziertes Sicherheitssystem, und es existiert noch keine bestückte URTC-Platine. Die Bootloader prüfen Firmware-Images mit CRC32 und einem HMAC-SHA256 mit gemeinsamem Schlüssel, nicht mit einer asymmetrischen Signatur (geplant); die veröffentlichten Anwendungs-Images sind mit dem öffentlichen Entwicklungsschlüssel signiert (`signing_key_kind: dev`), sodass jeder ein vom Bootloader akzeptiertes Image erzeugen kann; an eine echte Platine wurde noch nichts gesendet. Lesen Sie die [Sicherheitsbasis](https://github.com/JuanenRac/URTC-DOCS/blob/main/docs/SECURITY_BASELINE.md) und das [Bedrohungsmodell](https://github.com/JuanenRac/URTC-DOCS/blob/main/docs/THREAT_MODEL.md), bevor Sie sich für die Sicherheit einer Person auf eine Aussage hier verlassen.

## 📋 Auf einen Blick

| | |
|---|---|
| Version | `0.2.0` |
| Reifegrad | Funktionsfähig |
| Technik | C / STM32G4 / arm-none-eabi-gcc |
| Ziel | Eine Mikrocontroller-Platine (die Firmware); wird auf einem PC gebaut |
| Auf echter Hardware | Noch nicht: für diese Version ist kein Lauf auf echter Hardware vermerkt |

## 1. 🛠️ TECHNISCHER ÜBERBLICK

**URTC-SMART-RACK** ist ein intelligentes Lagersystem für Werkzeugköpfe innerhalb des HYDRA-UMC-Ökosystems. Basierend auf dem STM32G4-Mikrocontroller überwacht und bereitet es Werkzeuge vor, während sie nicht an einem Roboter angebracht sind.

Es ermöglicht "Smart Idle"-Modi, etwa das Vorheizen von T12-Lötspitzen kurz vor einem Werkzeugwechsel, und verfolgt die elektronische ID und die Gesamtzahl der Nutzungszyklen jedes URTC-Kopfes (das Auslesen der Firmware-Version jedes Kopfes ist geplant, nicht implementiert), was optimale Wartung und einen Einsatz in null Sekunden gewährleistet.

Für diese Platine existiert noch keine PCB/kein Schaltplan (siehe `hardware/`), also kann nichts davon echte GPIO/F-RAM/CAN-Hardware ansteuern - aber die *Logik*, auf die sich diese Funktionen reduzieren (eine ID dekodieren, die Nutzung verfolgen, entscheiden, wann und auf welche Temperatur vorgeheizt werden soll), ist real, reines C, heute unit-getestet.

### Hauptmerkmale:
* ✅ **Echtes v0 - ID-, Lebenszyklus- & Vorheiz-Logik:** `tool_id.c` dekodiert eine rohe 5-Bit-ID-Ablesung in eine Werkzeugidentität; `lifecycle.c` verfolgt Nutzungszyklen/-zeit und markiert fällige Wartung; `preheat.c` entscheidet, wann das Smart-Idle-Vorheizen starten soll und auf welche Zieltemperatur. 467 Test-Assertions mit dem eigenen C-Compiler des Hosts - keine PCB, kein GPIO-Treiber, kein F-RAM nötig, um all das auszuführen oder zu testen. Ebenfalls echt: ein Strom-Deframer für die serielle Verbindung (`rack_deframer.c`: sucht das Startbyte, synchronisiert sich nach einem verlorenen Byte neu und verwirft einen halben Frame nach einem Timeout), die CAN-Form des Befehls (`rack_can.c`: acht Datenbytes, Temperatur im Big-Endian; noch keine Platine trägt sie) und eine Grenze, wie lange ein Vorheizen ohne Werkzeugwechsel laufen darf (`preheat_limit.c`: standardmäßig 10 Minuten, konfigurierbar).
* ✅ **Köpfe als Einheiten, thermische Prüfung und vorausschauendes Vorheizen (auf dem Host getestet):** `rack_inventory.c` kann einen Platz über den Typ plus die Seriennummer des Kopfes kennzeichnen, und `rack_units.c` führt Nutzung, Heizzeit und thermische Fehler jedes physischen Kopfes (aus seinen eigenen Zählern, `0x1AA` / `0x1AB`), sodass der Verschleiß dem Kopf folgt und nicht dem Typ; `slot_verify.c` sagt, was eine thermische Messung eines Platzes für das bestellte Vorheizen bedeutet (erreicht, zu langsam, Überschwingen, eine Heizung an in einem leeren Platz); `preheat_plan.c` macht aus der Auftragswarteschlange Vorheizbefehle, standardmäßig ausgeschaltet. Messung und Warteschlange kommen von anderen Programmen, nichts davon lief auf einer Platine, und die Aufheizzeiten sind Schätzungen.
* 🗄️ **Werkzeugverfolgung** — automatische Identifikation von URTC-Köpfen über 5-Bit-ID-Jumper oder F-RAM. *(die ID-Dekodierlogik selbst ist real - siehe oben; echte Jumper/F-RAM auszulesen benötigt die PCB.)*
* 🗂️ **Mehrplatz-Inventar** — `rack_inventory.c` verfolgt, welches Werkzeug in welchem Platz eingelagert ist, hält ein echtes Werkzeug immer in genau einem Platz (ein Umlagern löscht seinen alten Platz, statt zu behaupten, es sei an zwei Orten zugleich) und ein Vorheiz-Ziel pro Platz, adressiert über die Werkzeugidentität. Beim Start gleicht `rack_inventory_reconcile()` es mit einem Anwesenheitssensor pro Platz ab: ein bei ausgeschaltetem Rack entnommenes Werkzeug wird gelöscht (samt Vorheizziel) und ein von Hand eingesetztes zur Identifizierung markiert. *(die Inventarlogik selbst ist real - siehe oben; das Verdrahten mit echten Platzsensoren benötigt die PCB.)*
* 🌡️ **Vorheiz-Logik** — intelligentes Temperaturmanagement für Löt- und Heißluftwerkzeuge. *(die Aktivierungsentscheidung und Zieltemperaturen sind real - siehe oben; einen echten Heizer anzusteuern benötigt die PCB.)*
* 📈 **Lebenszyklus-Protokolle** — zeichnet die Gesamtzahl der Betätigungszyklen und Betriebsstunden im F-RAM des Werkzeugs auf. *(die Zähler und die Logik für fällige Wartung sind real - siehe oben; sie in echtem F-RAM zu persistieren benötigt die PCB.)*
* 📡 **CAN-Integration** — soll mit dem HYDRA-UMC-Kinematik-Gehirn für koordinierten ATC (automatischen Werkzeugwechsel) kommunizieren; das lief nie, da weder ein CAN-Treiber noch eine Platine existiert. *(das Drahtprotokoll selbst - Framing, CRC, Befehlsvalidierung - ist real und auf dem Host getestet, siehe unten; ein echter CAN-Transceiver, um es tatsächlich zu übertragen, wird noch benötigt.)*
* 🔒 **Protokoll-Bereichsprüfungen** — versioniertes Framing mit einer CRC8-Prüfsumme, Aktuierungsbereichs-Validierung und ein Link-Timeout-/Idempotenz-Watchdog mit einem definierten sicheren Zustand. Rahmenversion 2: Jeder Mehrbytewert ist Big-Endian wie beim Bus, und ein Rahmen der Version 1 wird abgelehnt. *(als reines C implementiert und nur auf einem PC getestet; nie auf einer Platine ausgeführt und keine zertifizierte Sicherheitsfunktion)*
* ✅ **Cortex-M4F-Firmware-Toolchain** — ein echtes Bare-Metal-Image (Startup + Linker + `main.c`), das mit `arm-none-eabi-gcc` wirklich kompiliert und gelinkt wird, derselben Toolchain, die auch das Schwester-Repository URTC nutzt. *(implementiert — siehe BUILD unten)*

---

## 2. 🔄 SMART-RACK-WORKFLOW

```mermaid
flowchart TB
    TOOL["URTC-Werkzeugkopf"] -- Ins Rack gesteckt --> RACK["URTC-SMART-RACK"]
    RACK --> IDENT["ID & Lebenszyklusdaten lesen"]
    IDENT --> SYNC["Synchronisierung mit HYDRA-ORCHESTRATOR"]
    SYNC -- Erwartete Aufgabe --> HEAT["VORHEIZEN: Lötspitze auf 200°C"]
    RACK -- Zustandsprüfung --> LOG["Wartungsbericht"]
```

---

## 3. 🧱 ARCHITEKTUR & DESIGNENTSCHEIDUNGEN

* **Warum diese Platine noch kein echtes Pinout/keine echte Hardware-ID definiert hat.** Es gibt noch keine PCB für diese Platine - `src/firmware_common.h` trägt eine Versionsidentität ohne Hardware-ID, und die Startup-/Linker-Dateien sind handgeschriebene Platzhalter, die den eigenen CMSIS/HAL-Start von ST ersetzen, bis ein echtes STM32G4-Bauteil feststeht.
* **Warum es kein Kind von URTC selbst ist.** URTC-SMART-RACK ist ein ergänzendes Werkzeug, kein Kind der URTC-Familie - es ist eine separate physische Platine (ein Werkzeug-Lagerregal, kein Werkzeugkopf), die den eigenen Werkzeug-ID-Raum und die Firmware-Konventionen von URTC teilt, nicht dessen Integrationshierarchie.
* **Warum `bump_version.py` eine direkte Kopie des eigenen Skripts von URTC ist.** Gleiche Kilometerzähler-Versionierungsregel, gleiches Firmware-Header-Format - das exakte Skript wiederzuverwenden statt es neu zu erfinden, hält beide durch Konstruktion synchron.
* **Warum `tool_id.c`/`lifecycle.c`/`preheat.c` vor jedem GPIO/F-RAM/CAN-Treiber kommen.** Eine ID zu dekodieren, Nutzung zu akkumulieren und zu entscheiden, wann vorgeheizt werden soll, sind reine Funktionen bereits vorhandener Daten - sie brauchen keine PCB, um geschrieben oder getestet zu werden, also liefert v0 diese Logik zuerst, host-getestet mit dem eigenen C-Compiler der Maschine statt `arm-none-eabi-gcc`. Die Treiber, die diese Daten tatsächlich von echter Hardware beziehen würden, kommen, sobald die PCB existiert.
* **Wie sich das ins restliche Ökosystem einfügt.** Nutzt das Werkzeug-Ökosystem von URTC (dieselben Werkzeug-IDs; die Frames des Racks sind noch keine CAN-Frames, siehe `docs/OPERATIONS.md`) und bildet ein natürliches Paar mit HYDRA-UMC-DETECTION-HEF, um visuell zu erkennen, welches Werkzeug tatsächlich eingelagert ist.
* **Warum Protokoll, Befehlsvalidierung und Link-Watchdog drei getrennte Module sind.** `protocol.c` kennt nur Bytes, Framing und eine CRC - es hat keine Ahnung, was eine "sichere" Temperatur ist. `rack_command.c` besitzt dieses Urteil, dekodiert und prüft den Bereich eines echten Befehls, ohne sich darum zu kümmern, wie er auf der Leitung ankam. `link_watchdog.c` verfolgt Timeout/Idempotenz unabhängig von beiden - ein beschädigter Frame darf ihn niemals überhaupt erreichen (siehe die eigene echte Assertion von `test_rack_link_scenarios.c`, dass ein Frame mit falscher CRC den Watchdog niemals wiederbelebt). Sie getrennt zu halten macht jedes einzeln host-testbar und hält einen künftigen CAN-Empfangshandler schlank - er ruft jede Schicht der Reihe nach auf, er implementiert das Urteil keiner von ihnen neu.
* **Ratenbegrenzer für Befehle.** Eine Flut von Befehlen für ein Werkzeug wird durch einen Mindestabstand zwischen angenommenen Befehlen gebremst (Standard 20 ms, 0 schaltet ihn ab; `link_watchdog_set_command_interval()`). Ein zurückgehaltener Befehl ändert nichts und wird nicht quittiert, zählt aber als lebendige Verbindung; ein sicherer Zustand oder eine Wiederholung wird nie zurückgehalten. Der Abstand lässt sich in der simulierten Rack-Werkbank von URTC-WEB-STUDIO einstellen.
* **Warum eine unbewiesene Verbindung genau wie eine tote behandelt wird.** `link_watchdog_is_link_lost()` ist sowohl nach einem echten Timeout wahr ALS AUCH bevor der allererste Frame überhaupt angekommen ist - die Diagnosenotizen selbst nennen es "estado seguro al arrancar". Ein Rack, das ohne bereits verbundenen Host hochfährt, muss im sicheren Zustand starten, statt stillschweigend "alles in Ordnung" anzunehmen, bis das Gegenteil bewiesen ist.

---

## 📂 VERZEICHNISSTRUKTUR

```text
URTC-SMART-RACK/
├── src/                            # Firmware-Quellcode
│   ├── firmware_common.h           # FIRMWARE_VERSION_MAJOR/MINOR/PATCH
│   ├── tool_id.h / .c              # Echt: Dekodierung roher 5-Bit-Ablesung -> Werkzeug-ID
│   ├── lifecycle.h / .c            # Echt: Verfolgung Nutzungszyklen/-zeit, Prüfung fälliger Wartung
│   ├── preheat.h / .c              # Echt: Smart-Idle-Aktivierung + Zieltemperatur + Ziel für sicheren Zustand
│   ├── protocol.h / .c             # Echt: versioniertes Frame-Format + CRC8-Parsing/-Kodierung
│   ├── urtc_crc8.h                 # Echt: CRC-8, gemeinsam mit der Vision-Sensorverbindung (aus URTC-COMMON kopiert)
│   ├── urtc_frame.h                # Echt: Rahmenschicht (Startbyte, Version, Länge, CRC), gemeinsam mit der Vision-Sensorverbindung (aus URTC-COMMON kopiert)
│   ├── urtc_rate_limiter.h         # Echt: Begrenzer des Mindestabstands der Befehle an ein Werkzeug (aus URTC-COMMON kopiert)
│   ├── urtc_common.lock.json       # Sperrdatei der aus URTC-COMMON kopierten Dateien
│   ├── rack_command.h / .c         # Echt: Befehlsdekodierung + Aktuierungsgrenzen-Validierung
│   ├── link_watchdog.h / .c        # Echt: Link-Timeout + Befehls-Idempotenz
│   ├── rack_link.h / .c            # Echt: Frame-Dispatch-Entscheidung, die protocol/rack_command/link_watchdog/preheat verbindet
│   ├── rack_inventory.h / .c       # Echt: Mehrplatz-Werkzeuginventar - welches Werkzeug wo eingelagert ist, nie in zwei Plätzen zugleich
│   ├── tool_limits.h / .c          # Echt: höchstes Vorheizziel je Werkzeugprofil (0 = kein Heizer)
│   ├── rack_app.h / .c             # Echt: wendet die Entscheidung des Empfangspfads auf das Inventar an; sicherer Zustand bei verlorener Verbindung
│   ├── rack_deframer.h / .c        # Echt: findet die Rahmen im Bytestrom der seriellen Verbindung (Neusynchronisation, Ruhezeit)
│   ├── rack_can.h / .c             # Echt: wie der Rack-Befehl über CAN laufen wird, sobald eine Platine mit Transceiver existiert
│   ├── preheat_limit.h / .c        # Echt: das längste Vorheizen ohne Werkzeugwechsel
│   ├── preheat_plan.h / .c         # Echt: vorausschauendes Vorheizen aus der Auftragswarteschlange
│   ├── slot_verify.h / .c          # Echt: was eine thermische Messung über einen Slot aussagt
│   ├── rack_units.h / .c           # Echt: Identität und Verschleiß jedes physischen Werkzeugkopfs
│   ├── main.c                      # Minimaler Einstiegspunkt (Lebenszeichen-Schleife)
│   ├── startup_stm32g4_minimal.c   # Vektortabelle + Reset_Handler (noch keine ST-HAL, siehe Datei-Header)
│   └── STM32G4_MINIMAL.ld          # Platzhalter-Linkerskript (Untergrenze 128K FLASH / 32K RAM)
├── tests/                          # Echtes host-natives Test-Harness (tool_id, Werkzeuggrenzen, lifecycle, preheat, protocol, rack_command, link_watchdog, Rack-Link-Szenarien, Rack-Inventar, Robustheit, rack_app) und Python-Prüfungen, dass Build-Skripte und veröffentlichte Testanzahl übereinstimmen
├── docs/                           # Betriebsanleitung (docs/OPERATIONS.md): was die Logik tut, ablehnt und unverifiziert lässt
├── hardware/                       # Hardware-Notizen (README.md) - noch keine Entwurfsdateien (kein Schaltplan, keine PCB)
├── firmware/                       # Build-Ausgabe (.bin/.elf/.hex), von git ignoriert - nur als Assets des GitHub-Releases veröffentlicht
├── build/                          # Zwischen-Build-Objekte (von git ignoriert)
├── images/                         # Medien und Diagramme
├── tools/
│   ├── build_test.py               # Build-/Kompilierprüfung ohne Versionserhöhung
│   ├── gcov_floor.py               # Schlägt fehl, wenn die Host-Tests weniger von src/ ausführen als ein Mindestwert (gcov)
│   ├── publish_release_assets.py   # Listet die Release-Assets auf und gibt den gh-Befehl aus
│   └── ci_validate.py              # Manifest-/CHANGELOG-/Doku-Validierung, von der CI genutzt
├── bump.py                         # Ausdrücklicher Befehl zum Erhöhen der Version (Kompilieren erhöht nie)
├── bump_version.py                 # Versionserhöhung nach Kilometerzähler-Prinzip (generisch, geteilt mit URTC)
├── bump_manifest_version.py        # Synchronisiert die Version von urtc.project.json mit der nativen (--sync)
├── build_firmware.sh / .bat        # Echter Build: Host-Tests + kompilieren + linken + veröffentlichen (ändert nie die Version)
├── build-test.sh / .bat            # Build-/Kompilierprüfung ohne Versionserhöhung
└── README.md
```

---

## 4. ⚙️ BUILD

Erfordert die ARM-GNU-Toolchain (`arm-none-eabi-gcc`, `arm-none-eabi-objcopy`, `arm-none-eabi-size`) und Python 3.

```bash
# Linux/macOS
chmod +x build_firmware.sh   # einmalig
./build_firmware.sh

# Windows
build_firmware.bat
```

Der Build kompiliert und führt zuerst `tests/` mit dem C-Compiler des *Hosts selbst* aus (nie `arm-none-eabi-gcc` - das sind reine Logik-Tests, keine MCU-Register werden angefasst) und lässt den gesamten Build fehlschlagen, falls eine Assertion fehlschlägt. Erst danach erhöht er die Version von `src/firmware_common.h` (Kilometerzähler-Regel, wie im übrigen Ökosystem), kompiliert `main.c` und `startup_stm32g4_minimal.c` für Cortex-M4F, linkt sie gegen die Platzhalter-Speicherkarte `STM32G4_MINIMAL.ld` und veröffentlicht versionierte `.elf`/`.bin`/`.hex`-Dateien in `firmware/`.

Alle Module unter `src/` werden zusätzlich für Cortex-M4F kompiliert und zur Kontrolle alle zusammen gelinkt (auf dem Ziel steht keine C-Bibliothek zur Verfügung), obwohl das Image heute nur `main.c` und die Startdatei enthält. Schlägt der Build fehl, nachdem die Version erhöht wurde, werden Version, Manifest und Changelog zurückgesetzt. Mit `URTC_CI=1` wird nichts Versioniertes verändert.

Die Firmware-Images (`.bin`, `.hex`, `.elf`) werden nicht in git versioniert: ein Release-Build schreibt `firmware/URTC_SMART_RACK_FIRMWARE_v<Version>.*` (von git ignoriert; `build/firmware` im CI-Modus, `URTC_CI=1`), und sie werden nur als Assets des GitHub-Releases der jeweiligen Version veröffentlicht. `python tools/publish_release_assets.py` prüft die Dateien, schreibt die fehlende `.sha256`-Datei und gibt den genauen `gh release create`-Befehl mit den anzuhängenden Dateien aus.

Es gibt noch nichts, was auf echte Hardware geflasht werden könnte - es existiert keine PCB, die das Ziel-STM32G4-Modell, das Pinout oder die realen Flash-/RAM-Größen bestätigt. Die Speicherkarte des Linkerskripts ist ein konservativer Platzhalter (in seinem eigenen Header dokumentiert), der ersetzt wird, sobald echte Hardware existiert - zu demselben Zeitpunkt, an dem die handgeschriebene Vektortabelle von `startup_stm32g4_minimal.c` durch STs eigenen CMSIS/HAL-Startupcode ersetzt wird (nach dem Muster des Schwester-Repos URTC in `src/F303-master/` für dessen STM32F303-Platinen).

Echtes Beispiel - die Host-seitigen Tests laufen auch eigenständig, nützlich um die Logik ohne vollständigen Firmware-Build zu prüfen:

```bash
cc -std=c11 -Wall -Wextra -Isrc -Itests -o build/host_tests \
  tests/test_main.c tests/test_tool_id.c tests/test_lifecycle.c tests/test_preheat.c \
  tests/test_protocol.c tests/test_rack_command.c tests/test_link_watchdog.c tests/test_rack_link_scenarios.c \
  tests/test_rack_inventory.c tests/test_robustness.c tests/test_rack_app.c \
  tests/test_rack_deframer.c tests/test_rack_can.c tests/test_preheat_limit.c \
  src/tool_id.c src/tool_limits.c src/lifecycle.c src/preheat.c src/protocol.c src/rack_command.c src/link_watchdog.c src/rack_link.c \
  src/rack_inventory.c src/rack_app.c src/rack_deframer.c src/rack_can.c src/preheat_limit.c
./build/host_tests
# All tests passed.
```

---

## 5. 📋 CHANGELOG

Siehe [`CHANGELOG.md`](CHANGELOG.md) für die vollständige Versionshistorie — jeder echte Build erhöht automatisch die Version von `src/firmware_common.h` (Kilometerzähler-Regel, siehe BUILD oben), und jede Erhöhung hat dort ihren eigenen Eintrag.

---

## 🔗 Verwandte Projekte

Dieses Projekt ist Teil des HYDRA-UMC-Robotik-Ökosystems desselben Autors (JuanenRac / Electro Hobby 3D). Gut zu wissen, da eine Anfrage eigentlich eines dieser Projekte betreffen könnte statt dieses Repositorys.

**Direkt verwandt**
- **[URTC](https://github.com/JuanenRac/URTC)** — Firmware für die physische Universal-Robot-Tool-Controller-Platine, 25+ Werkzeugprofile über CAN-Bus — dasselbe Werkzeug-Ökosystem, über denselben CAN-Bus.
- **[HYDRA-UMC-DETECTION-HEF](https://github.com/JuanenRac/HYDRA-UMC-DETECTION-HEF)** — echte Registry für kompilierte Modelle mit Hailo-Architektur-/Prüfsummen-Safe-Load-Verifizierung — liefert die visuelle Erkennung der Werkzeuge, die dieses Regal lagert.

**Ebenfalls Teil des Ökosystems**

*Kern-Hardware & Plattform*
- **[HYDRA-UMC](https://github.com/JuanenRac/HYDRA-UMC)** — der Entwurf des Motherboards des Roboterarms: CM5-Host + Dual-Core-STM32H745, gedacht zur Koordination von bis zu 8 Werkzeugarmen über CAN-OTA/SPI-OTA (Firmware in einer frühen Phase; noch keine bestückte Platine).
- **[HYDRA-UMC-OS](https://github.com/JuanenRac/HYDRA-UMC-OS)** — reproduzierbare Raspberry-Pi-OS-Produktschicht für den CM5: schreibgeschützter Agent, validierte Konfiguration/Profile, WiFi-Ersteinrichtung.
- **[HYDRA-UMC-SDK](https://github.com/JuanenRac/HYDRA-UMC-SDK)** — der gemeinsame JSON-Schema-Vertrag und die Software-Auftragsschranke, gegen die jede Bridge ihre Befehle validiert (keine zertifizierte Sicherheitsfunktion).
- **[HYDRA-UMC-CONNECTOR-HUB](https://github.com/JuanenRac/HYDRA-UMC-CONNECTOR-HUB)** — deklaratives Adapter-Manifest-Register und Validator für Konnektoren externer Maschinen; erweitert die eigene Vertragsidee des SDK auf externe Maschinen, ohne die Industrie-Gateway-Projekte zu ersetzen.

*Kern-Backend & Clients*
- **[HYDRA-UMC-SERVER](https://github.com/JuanenRac/HYDRA-UMC-SERVER)** — das reale Headless-Backend (REST/WebSocket), mit dem jeder Steuerungsclient tatsächlich spricht.
- **[HYDRA-UMC-STUDIO](https://github.com/JuanenRac/HYDRA-UMC-STUDIO)** — Web-Steuerungs-Dashboard mit Echtzeit-3D-Visualisierung mehrerer Roboter.
- **[HYDRA-UMC-SUITE](https://github.com/JuanenRac/HYDRA-UMC-SUITE)** — Desktop-Schwarmleitstand (PySide6) für mehrere Server gleichzeitig, verpackt als eigenständige ausführbare Datei.
- **[HYDRA-UMC-ANDROID-CONTROL](https://github.com/JuanenRac/HYDRA-UMC-ANDROID-CONTROL)** — native Android-Steuerungs-App mit biometrischem Login und einer gekoppelten Wear-OS-Begleit-App.
- **[HYDRA-UMC-IOS-CONTROL](https://github.com/JuanenRac/HYDRA-UMC-IOS-CONTROL)** — iOS/iPadOS-Steuerungs-App (Flutter) mit Echtzeit-WebSocket-Synchronisierung.
- **[HYDRA-UMC-DSI](https://github.com/JuanenRac/HYDRA-UMC-DSI)** — native Touch-UI für das eingebaute 7"-DSI-Touchscreen, direkt auf dem CM5 eingebettet.
- **[HYDRA-UMC-EDITOR-URDF](https://github.com/JuanenRac/HYDRA-UMC-EDITOR-URDF)** — grafischer Desktop-URDF-Ersteller/-Editor, der fertige Modelle in STUDIOs eigenen Katalog überträgt.
- **[HYDRA-UMC-EDITOR-STL](https://github.com/JuanenRac/HYDRA-UMC-EDITOR-STL)** — Desktop-STL-Modelleditor, der echte Teile im selben Modellkatalog transformiert/ersetzt/entfernt/hinzufügt, den auch HYDRA-UMC-EDITOR-URDF bearbeitet.
- **[HYDRA-UMC-BRIDGE-AMR](https://github.com/JuanenRac/HYDRA-UMC-BRIDGE-AMR)** — Koordinationsschranke für AGV-/AMR-Flotten über einen VDA-5050-MQTT-Publisher (nur gegen eine emulierte Flotte getestet).
- **[HYDRA-UMC-BRIDGE-CNC](https://github.com/JuanenRac/HYDRA-UMC-BRIDGE-CNC)** — High-Level-Koordinator für CNC-Zellen mit GRBL-Status-/Steuerbyte-Zugriff (nur gegen einen Emulator getestet).
- **[HYDRA-UMC-BRIDGE-DROIDS](https://github.com/JuanenRac/HYDRA-UMC-BRIDGE-DROIDS)** — Koordinationsschranke für laufende/humanoide Droiden, mit einem Boston-Dynamics-Spot-Befehlssender (nur gegen einen Emulator getestet).
- **[HYDRA-UMC-BRIDGE-LASER](https://github.com/JuanenRac/HYDRA-UMC-BRIDGE-LASER)** — Koordinator für Laserzellen, liest 3 Schlüssel-/Gehäuse-/Verriegelungs-GPIO-Signale (nur gegen einen simulierten Chip getestet; kann keinen Laser scharfschalten oder auslösen).
- **[HYDRA-UMC-BRIDGE-OPENPNP](https://github.com/JuanenRac/HYDRA-UMC-BRIDGE-OPENPNP)** — sicherer High-Level-Koordinator für den Leiterplattenfluss von OpenPnP Pick-and-Place.
- **[HYDRA-UMC-BRIDGE-PRINTER3D](https://github.com/JuanenRac/HYDRA-UMC-BRIDGE-PRINTER3D)** — sichere Koordinationsschranke für Moonraker/Klipper-3D-Drucker, mit echten gesicherten Job-Befehlen.
- **[HYDRA-UMC-BRIDGE-ROS2](https://github.com/JuanenRac/HYDRA-UMC-BRIDGE-ROS2)** — Koordinator mit einem träge importierten rclpy-ROS-2-Transport (noch nicht gegen eine ROS-2-Installation ausgeführt).
- **[HYDRA-UMC-BRIDGE-UAV](https://github.com/JuanenRac/HYDRA-UMC-BRIDGE-UAV)** — Koordinationsschranke für kameraausgestattete UAVs, mit einem MAVLink-Befehlssender (nur gegen einen Emulator getestet).

*URTC-Werkzeugplattform*
- **[URTC-FLASHER](https://github.com/JuanenRac/URTC-FLASHER)** — Desktop-GUI-Flash-Tool für URTC-Platinen, CAN-OTA plus Full-Chip-SWD/JTAG.
- **[URTC-COMMON](https://github.com/JuanenRac/URTC-COMMON)** — das CAN-Protokoll einmal als Daten geschrieben (Spezifikation, erzeugte Tabellen, Konformitätsvektoren) und der Code, den die Programme teilen; dieses Repository trägt Kopien der Teile, die es verwendet.
- **[URTC-GATEWAY](https://github.com/JuanenRac/URTC-GATEWAY)** — ein Gateway vor dem CAN-Bus, das nur Kennungen einer Liste durchlässt, ihre Rate begrenzt und einen Sitzungsschlüssel verlangt.
- **[URTC-TESTER](https://github.com/JuanenRac/URTC-TESTER)** — Desktop-Live-CAN-Bus-Diagnosetool für URTC-Platinen, ein Panel pro Werkzeugprofil.
- **[URTC-WEB-STUDIO](https://github.com/JuanenRac/URTC-WEB-STUDIO)** — browserbasierte Alternative zu URTC-TESTER über die Web-Serial-API, ohne lokale Installation.

*Vision-KI-Knoten (Hailo-8)*
- **[HYDRA-UMC-VISION-NODE](https://github.com/JuanenRac/HYDRA-UMC-VISION-NODE)** — Integrationsknoten für die Hailo-8-Vision-Pipeline, mit einer echten stufenweisen Hardware-Bereitschaftsprüfung.
- **[HYDRA-UMC-VISION-STREAMER](https://github.com/JuanenRac/HYDRA-UMC-VISION-STREAMER)** — echter GStreamer-Pipeline- + MediaMTX-Konfigurationsgenerator mit einer echten HailoRT-Integrationsschranke.
- **[HYDRA-UMC-VISUAL-SERVOING-API](https://github.com/JuanenRac/HYDRA-UMC-VISUAL-SERVOING-API)** — Position-Based-Visual-Servoing-Korrekturgesetz mit Freigabe abhängig vom Zonenstatus; noch keine Posenschätzung per Kamera.
- **[HYDRA-UMC-SAFETY-ZONES](https://github.com/JuanenRac/HYDRA-UMC-SAFETY-ZONES)** — v0-Logik für Zonenverletzung, Kalibrierungsaktualität und E-STOP-Anforderung, die Objektpositionen aus einer Datei nimmt; kein Detektor speist sie, und noch kein physischer E-STOP ist angeschlossen.

*Kognitiver KI-Knoten (Hailo-10)*
- **[HYDRA-UMC-COGNITIVE-NODE](https://github.com/JuanenRac/HYDRA-UMC-COGNITIVE-NODE)** — Integrationsknoten für die Hailo-10-Cognitive-Pipeline (LLM-/VLA-/Sprach-Orchestrierung).
- **[HYDRA-UMC-VLA-ENGINE](https://github.com/JuanenRac/HYDRA-UMC-VLA-ENGINE)** — echte Aktions-Token-Kodierung/-Dekodierung und Trajektoriengenerierung für ein Vision-Language-Action-Modell.
- **[HYDRA-UMC-VOICE-UI](https://github.com/JuanenRac/HYDRA-UMC-VOICE-UI)** — echtes Sprach-Frontend (VAD + Intent-Parser) mit einem begrenzten, bestätigungsgesicherten Watch-Relay.
- **[HYDRA-UMC-SEMANTIC-PLANNER](https://github.com/JuanenRac/HYDRA-UMC-SEMANTIC-PLANNER)** — echte regelbasierte Aufgabenzerlegung und semantische Fehlerbehebung über MCU-Fehlercodes.
- **[HYDRA-UMC-DOCS-QA](https://github.com/JuanenRac/HYDRA-UMC-DOCS-QA)** — echte, nur auf der Standardbibliothek basierende TF-IDF-Dokumentensuche über die eigenen Markdown-Dokumente dieses Ökosystems.
- **[HYDRA-UMC-LOCAL-TECHNICIAN](https://github.com/JuanenRac/HYDRA-UMC-LOCAL-TECHNICIAN)** — lokaler, richtlinien-gesteuerter KI-Wartungstechniker für das Ökosystem selbst - beobachtet, diagnostiziert und schlägt Korrekturen vor; die zwei höchsten Risikostufen sind absichtlich noch nicht implementiert.

*Orchestrierung & Schwarm*
- **[HYDRA-UMC-ORCHESTRATOR](https://github.com/JuanenRac/HYDRA-UMC-ORCHESTRATOR)** — Integrationsknoten mit einem echten gRPC/Protobuf-Health-Report-Vertrag und einer Missions-Zustandsmaschine.
- **[HYDRA-UMC-JOB-DISPATCHER](https://github.com/JuanenRac/HYDRA-UMC-JOB-DISPATCHER)** — echte prioritätsbasierte Job-Queue mit Deduplizierung, über eine echte HTTP-API.
- **[HYDRA-UMC-NODE-HEALING](https://github.com/JuanenRac/HYDRA-UMC-NODE-HEALING)** — echter gRPC-basierter Flotten-Health-Watchdog mit Retry/Backoff und Identitäts-Mismatch-Erkennung.
- **[HYDRA-UMC-PATH-PLANNER-3D](https://github.com/JuanenRac/HYDRA-UMC-PATH-PLANNER-3D)** — echter RRT-basierter 3D-Pfadplaner mit echter Hindernis-/Arbeitsraum-Kollisionsvalidierung.
- **[HYDRA-UMC-SWARM-SYNC](https://github.com/JuanenRac/HYDRA-UMC-SWARM-SYNC)** — echte CRDT-LWW-Element-Map-Zustandssynchronisation, eigenschaftsgetestet auf Multi-Zellen-Konvergenz.

*Digitaler Zwilling & Simulation*
- **[HYDRA-UMC-TWIN](https://github.com/JuanenRac/HYDRA-UMC-TWIN)** — Integrationsknoten für die Digital-Twin-Engine, mit einem echten Versionskompatibilitäts-Sync-Vertrag.
- **[HYDRA-UMC-HIL-BRIDGE](https://github.com/JuanenRac/HYDRA-UMC-HIL-BRIDGE)** — Hardware-in-the-Loop-Router mit einer Software-Kollisionsverriegelung zwischen Simulation und echter Hardware (ohne echte Hardware getestet).
- **[HYDRA-UMC-PHYSICS-REPLICA](https://github.com/JuanenRac/HYDRA-UMC-PHYSICS-REPLICA)** — echte Vorwärtskinematik und Gelenkgrenzenvalidierung über eine echte URDF-Teilmenge.
- **[HYDRA-UMC-SYNTHETIC-DATA-GEN](https://github.com/JuanenRac/HYDRA-UMC-SYNTHETIC-DATA-GEN)** — echter prozeduraler 2D-Szenengenerator mit YOLO/COCO-Annotationsexport.

*Daten & Analytik*
- **[HYDRA-UMC-DATALAKE](https://github.com/JuanenRac/HYDRA-UMC-DATALAKE)** — echter sqlite3-gestützter Zeitreihenspeicher mit einer echten Ingest-/Abfrage-HTTP-API.
- **[HYDRA-UMC-ANOMALY-DETECTOR](https://github.com/JuanenRac/HYDRA-UMC-ANOMALY-DETECTOR)** — echter FFT- + statistischer Basislinien-Anomaliedetektor mit Drift-Überwachung.
- **[HYDRA-UMC-PRODUCTION-REPORTS](https://github.com/JuanenRac/HYDRA-UMC-PRODUCTION-REPORTS)** — echte OEE-/Verfügbarkeitsberechnung über den DATALAKE-Verlauf, mit reproduzierbarem CSV-Export.
- **[HYDRA-UMC-TELEMETRY-COLLECTOR](https://github.com/JuanenRac/HYDRA-UMC-TELEMETRY-COLLECTOR)** — echte CAN/WebSocket-Ingestion-Pipeline in DATALAKE, mit Sequenz-Deduplizierung.

*Industrie-Gateway*
- **[HYDRA-UMC-GATEWAY-INDUSTRIAL](https://github.com/JuanenRac/HYDRA-UMC-GATEWAY-INDUSTRIAL)** — Integrationsknoten, der zu Industrieprotokollen weiterleitet, mit einer echten Befehls-Allowlist-/Backpressure-Schicht.
- **[HYDRA-UMC-OPCUA-SERVER](https://github.com/JuanenRac/HYDRA-UMC-OPCUA-SERVER)** — echter OPC-UA-Adressraum, verifiziert mit einer echten Binärprotokoll-Client-Session.
- **[HYDRA-UMC-MQTT-BROKER](https://github.com/JuanenRac/HYDRA-UMC-MQTT-BROKER)** — echter MQTT-Broker mit optionaler Pro-Client-Authentifizierung und Topic-ACLs.
- **[HYDRA-UMC-MTCONNECT-ADAPTER](https://github.com/JuanenRac/HYDRA-UMC-MTCONNECT-ADAPTER)** — echte MTConnect-`/probe`- und `/current`-XML-Endpunkte mit Degraded-Mode-Ausgabe.

*Ergänzende Tools & Ökosystembetrieb*
- **[HYDRA-UMC-DASHBOARD-AI](https://github.com/JuanenRac/HYDRA-UMC-DASHBOARD-AI)** — Smart-Summaries- und Anomaly-Highlighting-Panels über DATALAKE/ANOMALY-DETECTOR, mit einem ehrlichen statistischen Fallback.
- **[HYDRA-UMC-TOOL-CLI](https://github.com/JuanenRac/HYDRA-UMC-TOOL-CLI)** — Flotten-CLI mit einem echten, stabilen Exit-Code-Vertrag, ein echter Live-Client der eigenen API von HYDRA-UMC-SERVER.
- **[HYDRA-UMC-WATCH](https://github.com/JuanenRac/HYDRA-UMC-WATCH)** — WearOS-Begleit-App mit haptischen Alarmmustern und einem Sprach-Relay zum gekoppelten Telefon (noch nicht auf einer physischen Uhr ausgeführt).
- **[URTC-VISION-TOOL](https://github.com/JuanenRac/URTC-VISION-TOOL)** — Firmware plus ein echter Python-Vision-Begleiter für einen Thermal-/RGB-Inspektionswerkzeugkopf.
- **[URTC-UPDATER](https://github.com/JuanenRac/URTC-UPDATER)** — Desktop-Werkzeug, das die Repositories der URTC-Familie erkennt, installiert und aktualisiert, jeweils mit einer ausdrücklichen Aktion.
- **[HYDRA-UMC-UPDATER](https://github.com/JuanenRac/HYDRA-UMC-UPDATER)** — administratives Desktop-Tool, das jedes Repository in diesem Ökosystem entdeckt, klont und aktualisiert.
- **[HYDRA-UMC-OS-REBUILDER](https://github.com/JuanenRac/HYDRA-UMC-OS-REBUILDER)** — Windows/Linux-Desktop-Tool, das ein flashbereites CM5-Image baut, vorgeladen mit den aktuellsten Versionen des Ökosystems, mit Ersteinrichtungs-Konfiguration für WLAN/Benutzer/SSH im Stil von Raspberry Pi Imager.
- **[HYDRA-UMC-OPS-AGENT](https://github.com/JuanenRac/HYDRA-UMC-OPS-AGENT)** — Wartungsvorfall-Koordinator: eine Edge-Rolle mit niedrigem Privileg sammelt einen bereinigten Inventar-/Gesundheits-Snapshot, eine Control-Plane-Rolle rendert ihn schreibgeschützt und bittet einen KI-Anbieter um einen Diagnosevorschlag - wendet nie einen Patch an und stellt nie etwas bereit.
- **[HYDRA-UMC-DEV-SERVER](https://github.com/JuanenRac/HYDRA-UMC-DEV-SERVER)** — reproduzierbarer Entwicklungshost (Raspberry Pi 5 / CM5), der den Quellcode des Ökosystems vorhält und begrenzte Build-/Test-Aufgaben über eine dauerhafte Warteschlange ausführt; eine dedizierte Entwicklerrolle, ausdrücklich kein operativer CM5.


---

## 📚 Dokumentation & Community

- **[CONTRIBUTING.md](CONTRIBUTING.md)** — Technologie-Stack und Coding-Richtlinien für einen Pull Request.
- **[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)** — die in dieser Community erwarteten Verhaltensstandards.
- **[SECURITY.md](SECURITY.md)** — wie man eine Schwachstelle meldet, und die echten Sicherheitsschwerpunkte dieses Projekts.
- **[SUPPORT.md](SUPPORT.md)** — wo man Fragen stellt und Fehler meldet.
- **[LICENSE.md](LICENSE.md)** — die eigene Lizenz dieses Projekts.

## 👤 AUTOR
**JuanenRac** (Electro Hobby 3D)
📧 electrohobby3d@gmail.com
📺 [youtube.com/@electrohobby3d](https://youtube.com/@electrohobby3d)

<!-- urtc-section:compatibility -->
## 🧩 Kompatibilität

Welche Versionen der URTC-Programme zusammenarbeiten und welche Protokollversion jede spricht, steht in der Kompatibilitätstabelle, die URTC-COMMON erzeugt: [docs/COMPATIBILITY.md](https://github.com/JuanenRac/URTC/blob/main/docs/COMPATIBILITY.md).
<!-- /urtc-section:compatibility -->

## 📜 LIZENZ
GPL-3.0-or-later - Siehe LICENSE für Details.
