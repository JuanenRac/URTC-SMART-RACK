<p align="center">
  <img src="/images/URTC_SMART_RACK_BANNER.svg" alt="URTC Smart Rack Logo" width="100%">
</p>

# 🗄️ URTC-SMART-RACK

<p align="center"><a href="README.md">🇺🇸 English</a> | <a href="README_spa.md">🇪🇸 Español</a> | <a href="README_fra.md">🇫🇷 Français</a> | 🇮🇹 <b>Italiano</b> | <a href="README_deu.md">🇩🇪 Deutsch</a> | <a href="README_zho.md">🇨🇳 简体中文</a> | <a href="README_jpn.md">🇯🇵 日本語</a></p>

### 🤖 Gestione Intelligente delle Testine Utensile con Tracciamento del Ciclo di Vita e Termico

<p align="left">
  <img src="https://img.shields.io/badge/Licencia-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/MCU-STM32G4-003551.svg" alt="STM32G4">
  <img src="https://img.shields.io/badge/Protocol-CAN%20%2F%20FDCAN-orange.svg" alt="CAN">
  <img src="https://img.shields.io/badge/Feature-Smart%20Idle-green.svg" alt="Smart Idle">
</p>

---

## 1. 🛠️ PANORAMICA TECNICA

**URTC-SMART-RACK** è un sistema di stoccaggio intelligente per testine utensile all'interno dell'ecosistema HYDRA-UMC. Basato sul microcontrollore STM32G4, monitora e prepara gli utensili mentre non sono collegati a un robot.

Abilita modalità "Smart Idle", come il preriscaldamento delle punte saldanti T12 poco prima di un cambio utensile, e traccia l'ID elettronico, la versione del firmware e i cicli totali di utilizzo di ogni testina URTC, garantendo una manutenzione ottimale e un dispiegamento a zero secondi.

Non esiste ancora un PCB/schematico per questa scheda (vedi `hardware/`), quindi nulla di sottostante può pilotare GPIO/F-RAM/CAN reali - ma la *logica* a cui queste funzionalità si riducono (decodificare un ID, tracciare l'utilizzo, decidere quando preriscaldare e a quale temperatura) è reale, C puro, testata unitariamente oggi stesso.

### Caratteristiche Principali:
* ✅ **v0 reale - logica ID, ciclo di vita e preriscaldamento:** `tool_id.c` decodifica una lettura ID grezza a 5 bit in un'identità utensile; `lifecycle.c` traccia cicli/tempo di utilizzo e segnala la manutenzione dovuta; `preheat.c` decide quando deve iniziare il preriscaldamento Smart Idle e a quale temperatura target. 25 asserzioni di test sul compilatore C proprio dell'host - nessun PCB, driver GPIO o F-RAM necessario per eseguire o testare tutto questo.
* 🗄️ **Tracciamento Utensili** — identificazione automatica delle testine URTC tramite jumper ID a 5 bit o F-RAM. *(la logica di decodifica ID in sé è reale - vedi sopra; leggere jumper/F-RAM reali richiede il PCB.)*
* 🌡️ **Logica di Preriscaldamento** — gestione termica intelligente per utensili di saldatura e aria calda. *(la decisione di attivazione e le temperature target sono reali - vedi sopra; pilotare un riscaldatore reale richiede il PCB.)*
* 📈 **Registri del Ciclo di Vita** — registra i cicli totali di attuazione e le ore di utilizzo nella F-RAM dell'utensile. *(i contatori e la logica di manutenzione dovuta sono reali - vedi sopra; persisterli su F-RAM reale richiede il PCB.)*
* 📡 **Integrazione CAN** — comunica direttamente con il Cervello Cinematico di HYDRA-UMC per un ATC coordinato (cambio utensile automatico). *(il protocollo stesso - framing, CRC, validazione dei comandi - è reale, vedi sotto; serve ancora un vero transceiver CAN per trasportarlo davvero.)*
* 🔒 **Limiti di Sicurezza del Protocollo** — framing versionato reale con checksum CRC8, validazione reale dell'intervallo di attuazione, e un watchdog reale di timeout del collegamento/idempotenza con uno stato sicuro definito. *(implementato)*
* ✅ **Toolchain firmware Cortex-M4F** — un'immagine bare-metal reale (avvio + linker + `main.c`) che compila e collega davvero con `arm-none-eabi-gcc`, la stessa toolchain usata dal repository gemello URTC. *(implementato — vedi COMPILAZIONE sotto)*

---

## 2. 🔄 FLUSSO DI LAVORO DELLO SMART RACK

```mermaid
flowchart TB
    TOOL["Testina URTC"] -- Inserita nel Rack --> RACK["URTC-SMART-RACK"]
    RACK --> IDENT["Legge ID e Dati sul Ciclo di Vita"]
    IDENT --> SYNC["Sincronizza con HYDRA-ORCHESTRATOR"]
    SYNC -- Attivita Anticipata --> HEAT["PRERISCALDA: Punta Saldante a 200°C"]
    RACK -- Controllo Salute --> LOG["Rapporto di Manutenzione"]
```

---

## 3. 🧱 ARCHITETTURA E DECISIONI DI PROGETTAZIONE

* **Perché questa scheda non ha ancora un pinout/ID hardware reale definito.** Non esiste ancora un PCB per questa scheda - `src/firmware_common.h` porta un'identità di versione senza ID hardware, e i file di avvio/linker sono scritti a mano come segnaposto in sostituzione dell'avvio CMSIS/HAL proprio di ST finché non viene fissato un vero componente STM32G4.
* **Perché non è un figlio di URTC stesso.** URTC-SMART-RACK è uno Strumento Complementare, non un figlio della famiglia URTC - è una scheda fisica separata (un rack di stoccaggio utensili, non una testa utensile) che condivide il bus CAN e le convenzioni firmware di URTC, ma non la sua gerarchia di integrazione.
* **Perché `bump_version.py` è una copia diretta di quello proprio di URTC.** Stessa regola di versionamento contachilometri, stesso formato di intestazione firmware - riutilizzare lo script esatto invece di reinventarlo mantiene i due sincronizzati per costruzione.
* **Perché `tool_id.c`/`lifecycle.c`/`preheat.c` arrivano prima di qualsiasi driver GPIO/F-RAM/CAN.** Decodificare un ID, accumulare l'utilizzo e decidere quando preriscaldare sono funzioni pure di dati già disponibili - non hanno bisogno di alcun PCB per essere scritte o testate, quindi la v0 consegna prima questa logica, testata con il compilatore C della macchina stessa anziché `arm-none-eabi-gcc`. I driver che effettivamente reperirebbero quei dati da hardware reale arriveranno quando esisterà il PCB.
* **Come si inserisce nel resto dell'ecosistema.** Condivide il bus CAN/ecosistema utensili proprio di URTC, e forma un abbinamento naturale con HYDRA-UMC-DETECTION-HEF per riconoscere visivamente quale utensile è realmente in rastrelliera.
* **Perché il protocollo, la validazione dei comandi e il watchdog del collegamento sono tre moduli separati.** `protocol.c` conosce solo byte, framing e un CRC - non ha idea di cosa sia una temperatura "sicura". `rack_command.c` possiede quel giudizio, decodificando e verificando l'intervallo di un comando reale senza preoccuparsi di come sia arrivato sul filo. `link_watchdog.c` traccia il timeout/l'idempotenza indipendentemente da entrambi - un frame corrotto non deve mai nemmeno raggiungerlo (vedi la stessa asserzione reale di `test_rack_link_scenarios.c` secondo cui un frame con CRC errato non fa mai rivivere il watchdog). Tenerli separati è ciò che rende ciascuno testabile autonomamente sull'host, e mantiene snello un futuro handler di ricezione CAN - chiama ogni livello in ordine, non reimplementa il giudizio di nessuno di essi.
* **Perché un collegamento non provato viene trattato esattamente come uno morto.** `link_watchdog_is_link_lost()` è vero sia dopo un vero timeout SIA prima che sia mai arrivato il primissimo frame - lo stesso audit di promozione lo chiama "estado seguro al arrancar". Una rastrelliera che si accende senza che sia ancora connesso alcun host deve avviarsi nello stato sicuro, non assumere silenziosamente che vada "bene" finché non sia dimostrato il contrario.

---

## 📂 STRUTTURA DELLE DIRECTORY

```text
URTC-SMART-RACK/
├── src/                            # Codice sorgente del firmware
│   ├── firmware_common.h           # FIRMWARE_VERSION_MAJOR/MINOR/PATCH
│   ├── tool_id.h / .c              # Reale: decodifica lettura grezza 5 bit -> ID utensile
│   ├── lifecycle.h / .c            # Reale: tracciamento cicli/tempo di utilizzo, verifica manutenzione dovuta
│   ├── preheat.h / .c              # Reale: attivazione Smart Idle + temperatura target + target di stato sicuro
│   ├── protocol.h / .c             # Reale: formato di frame versionato + parsing/codifica CRC8
│   ├── rack_command.h / .c         # Reale: decodifica comandi + validazione dei limiti di attuazione
│   ├── link_watchdog.h / .c        # Reale: timeout del collegamento + idempotenza dei comandi
│   ├── main.c                      # Punto di ingresso minimo (ciclo di battito di vita)
│   ├── startup_stm32g4_minimal.c   # Tabella dei vettori + Reset_Handler (ancora senza HAL ST, vedi intestazione del file)
│   └── STM32G4_MINIMAL.ld          # Linker script placeholder (base 128K FLASH / 32K RAM)
├── tests/                          # Harness di test reale host-native (tool_id, lifecycle, preheat, protocol, rack_command, link_watchdog, scenari di collegamento del rack)
├── docs/                           # Documentazione e manuale utente
├── hardware/                       # File di progettazione hardware (PCB, 3D) - vuoto, nessuno schematico ancora
├── firmware/                       # Output di build versionato (.bin/.elf/.hex), commesso come il repository gemello URTC
├── build/                          # Oggetti di build intermedi (ignorato da git)
├── images/                         # Media e diagrammi
├── scripts/                        # Script di utilità
├── bump_version.py                 # Incremento versione stile contachilometri (generico, condiviso con URTC)
├── build_firmware.sh / .bat        # Build reale: test host + incrementa versione + compila + collega + pubblica
└── README.md
```

---

## 4. ⚙️ COMPILAZIONE

Richiede la toolchain ARM GNU (`arm-none-eabi-gcc`, `arm-none-eabi-objcopy`, `arm-none-eabi-size`) e Python 3.

```bash
# Linux/macOS
chmod +x build_firmware.sh   # una tantum
./build_firmware.sh

# Windows
build_firmware.bat
```

Il build compila ed esegue prima `tests/` con il compilatore C dell'*host stesso* (mai `arm-none-eabi-gcc` - sono test di logica pura, nessun registro MCU toccato) e fallisce l'intero build se un'asserzione fallisce. Solo allora incrementa la versione di `src/firmware_common.h` (regola contachilometri, come il resto dell'ecosistema), compila `main.c` e `startup_stm32g4_minimal.c` per Cortex-M4F, li collega con la mappa di memoria placeholder `STM32G4_MINIMAL.ld`, e pubblica file `.elf`/`.bin`/`.hex` versionati in `firmware/`.

Non c'è ancora nulla da flashare su hardware reale - non esiste un PCB che confermi la parte STM32G4 target, il pinout, o le sue reali dimensioni di flash/RAM. La mappa di memoria del linker script è un placeholder prudente (documentato nella propria intestazione) che verrà sostituito quando esisterà hardware reale, lo stesso momento in cui la tabella dei vettori scritta a mano di `startup_stm32g4_minimal.c` verrà sostituita dal codice di avvio CMSIS/HAL proprio di ST (seguendo il modello del repository gemello URTC in `src/F303-master/` per le sue schede STM32F303).

Esempio reale - i test lato host si eseguono anche da soli, utile per verificare la logica senza un build firmware completo:

```bash
cc -std=c11 -Wall -Wextra -Isrc -Itests -o build/host_tests \
  tests/test_main.c tests/test_tool_id.c tests/test_lifecycle.c tests/test_preheat.c \
  src/tool_id.c src/lifecycle.c src/preheat.c
./build/host_tests
# All tests passed.
```

---

## 🔗 Progetti Correlati

Questo progetto fa parte di un ecosistema robotico più ampio dello stesso autore (JuanenRac / Electro Hobby 3D), che copre firmware, software di controllo, nodi IA e strumenti di flotta. Utile saperlo, perché una richiesta potrebbe in realtà riguardare uno di questi progetti anziché questo repository.

### Relazione Diretta

- **[URTC](https://github.com/JuanenRac/URTC)** — stesso ecosistema di strumenti / bus CAN.
- **[HYDRA-UMC-DETECTION-HEF](https://github.com/JuanenRac/HYDRA-UMC-DETECTION-HEF)** — riconoscimento visivo degli strumenti conservati in questo rack.

### Resto dell'Ecosistema

**Piattaforma HYDRA-UMC** — la cella di micro-fabbrica multi-robot
- **[HYDRA-UMC](https://github.com/JuanenRac/HYDRA-UMC)** — la scheda madre CM5 + STM32H745 che orchestra fino a 8 bracci robotici.
- **[HYDRA-UMC-SERVER](https://github.com/JuanenRac/HYDRA-UMC-SERVER)** — il backend Express/WebSocket con cui parla ogni client di controllo.
- **[HYDRA-UMC-STUDIO](https://github.com/JuanenRac/HYDRA-UMC-STUDIO)** — dashboard di controllo web, visualizzazione 3D multi-robot.
- **[HYDRA-UMC-ANDROID-CONTROL](https://github.com/JuanenRac/HYDRA-UMC-ANDROID-CONTROL)** — app di controllo Android via Wi-Fi/Bluetooth.
- **[HYDRA-UMC-IOS-CONTROL](https://github.com/JuanenRac/HYDRA-UMC-IOS-CONTROL)** — app di controllo iOS/iPadOS costruita in Flutter.
- **[HYDRA-UMC-SUITE](https://github.com/JuanenRac/HYDRA-UMC-SUITE)** — centro di comando sciame desktop (Python/PySide6).
- **[HYDRA-UMC-EDITOR-URDF](https://github.com/JuanenRac/HYDRA-UMC-EDITOR-URDF)** — editor desktop di modelli URDF per il catalogo robot.
- **[HYDRA-UMC-DSI](https://github.com/JuanenRac/HYDRA-UMC-DSI)** — interfaccia touch nativa per lo schermo DSI a bordo.

**Piattaforma URTC** — il controller della testa utensile che ogni braccio HYDRA-UMC porta con sé
- **[URTC](https://github.com/JuanenRac/URTC)** — controller testa utensile su bus CAN, 25 profili utensile.
- **[URTC-FLASHER](https://github.com/JuanenRac/URTC-FLASHER)** — strumento desktop di flashing CAN-OTA + SWD/JTAG.
- **[URTC-TESTER](https://github.com/JuanenRac/URTC-TESTER)** — strumento desktop di diagnostica CAN live.
- **[URTC-WEB-STUDIO](https://github.com/JuanenRac/URTC-WEB-STUDIO)** — alternativa basata su browser via Web Serial API.

**🎥 Nodo di Visione IA (Hailo-8)**
- [HYDRA-UMC-VISION-NODE](https://github.com/JuanenRac/HYDRA-UMC-VISION-NODE)
- [HYDRA-UMC-VISION-STREAMER](https://github.com/JuanenRac/HYDRA-UMC-VISION-STREAMER)
- [HYDRA-UMC-DETECTION-HEF](https://github.com/JuanenRac/HYDRA-UMC-DETECTION-HEF)
- [HYDRA-UMC-SAFETY-ZONES](https://github.com/JuanenRac/HYDRA-UMC-SAFETY-ZONES)
- [HYDRA-UMC-VISUAL-SERVOING-API](https://github.com/JuanenRac/HYDRA-UMC-VISUAL-SERVOING-API)

**🧠 Nodo IA Cognitiva (Hailo-10)**
- [HYDRA-UMC-COGNITIVE-NODE](https://github.com/JuanenRac/HYDRA-UMC-COGNITIVE-NODE)
- [HYDRA-UMC-VLA-ENGINE](https://github.com/JuanenRac/HYDRA-UMC-VLA-ENGINE)
- [HYDRA-UMC-VOICE-UI](https://github.com/JuanenRac/HYDRA-UMC-VOICE-UI)
- [HYDRA-UMC-SEMANTIC-PLANNER](https://github.com/JuanenRac/HYDRA-UMC-SEMANTIC-PLANNER)
- [HYDRA-UMC-DOCS-QA](https://github.com/JuanenRac/HYDRA-UMC-DOCS-QA)

**🐝 Orchestrazione e Sciame**
- [HYDRA-UMC-ORCHESTRATOR](https://github.com/JuanenRac/HYDRA-UMC-ORCHESTRATOR)
- [HYDRA-UMC-SWARM-SYNC](https://github.com/JuanenRac/HYDRA-UMC-SWARM-SYNC)
- [HYDRA-UMC-PATH-PLANNER-3D](https://github.com/JuanenRac/HYDRA-UMC-PATH-PLANNER-3D)
- [HYDRA-UMC-JOB-DISPATCHER](https://github.com/JuanenRac/HYDRA-UMC-JOB-DISPATCHER)
- [HYDRA-UMC-NODE-HEALING](https://github.com/JuanenRac/HYDRA-UMC-NODE-HEALING)

**🎮 Gemello Digitale e Simulazione**
- [HYDRA-UMC-TWIN](https://github.com/JuanenRac/HYDRA-UMC-TWIN)
- [HYDRA-UMC-PHYSICS-REPLICA](https://github.com/JuanenRac/HYDRA-UMC-PHYSICS-REPLICA)
- [HYDRA-UMC-HIL-BRIDGE](https://github.com/JuanenRac/HYDRA-UMC-HIL-BRIDGE)
- [HYDRA-UMC-SYNTHETIC-DATA-GEN](https://github.com/JuanenRac/HYDRA-UMC-SYNTHETIC-DATA-GEN)

**📊 Dati e Analisi**
- [HYDRA-UMC-DATALAKE](https://github.com/JuanenRac/HYDRA-UMC-DATALAKE)
- [HYDRA-UMC-TELEMETRY-COLLECTOR](https://github.com/JuanenRac/HYDRA-UMC-TELEMETRY-COLLECTOR)
- [HYDRA-UMC-ANOMALY-DETECTOR](https://github.com/JuanenRac/HYDRA-UMC-ANOMALY-DETECTOR)
- [HYDRA-UMC-PRODUCTION-REPORTS](https://github.com/JuanenRac/HYDRA-UMC-PRODUCTION-REPORTS)

**🏭 Gateway Industriale**
- [HYDRA-UMC-GATEWAY-INDUSTRIAL](https://github.com/JuanenRac/HYDRA-UMC-GATEWAY-INDUSTRIAL)
- [HYDRA-UMC-OPCUA-SERVER](https://github.com/JuanenRac/HYDRA-UMC-OPCUA-SERVER)
- [HYDRA-UMC-MQTT-BROKER](https://github.com/JuanenRac/HYDRA-UMC-MQTT-BROKER)
- [HYDRA-UMC-MTCONNECT-ADAPTER](https://github.com/JuanenRac/HYDRA-UMC-MTCONNECT-ADAPTER)

**🛠️ Strumenti Complementari**
- [URTC-VISION-TOOL](https://github.com/JuanenRac/URTC-VISION-TOOL)
- [HYDRA-UMC-WATCH](https://github.com/JuanenRac/HYDRA-UMC-WATCH)
- [HYDRA-UMC-TOOL-CLI](https://github.com/JuanenRac/HYDRA-UMC-TOOL-CLI)
- [HYDRA-UMC-DASHBOARD-AI](https://github.com/JuanenRac/HYDRA-UMC-DASHBOARD-AI)


## 👤 AUTORE
**JuanenRac** (Electro Hobby 3D)
📧 electrohobby3d@gmail.com
📺 [youtube.com/@electrohobby3d](https://youtube.com/@electrohobby3d)

## 📜 LICENZA
GPL-3.0 - Vedi LICENSE per i dettagli.
