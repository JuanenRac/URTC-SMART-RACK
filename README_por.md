<p align="center">
  <img src="/images/URTC_SMART_RACK_BANNER.svg" alt="Logótipo do URTC Smart Rack" width="100%">
</p>

# 🗄️ URTC-SMART-RACK

<p align="center"><a href="README.md">🇺🇸 English</a> | <a href="README_spa.md">🇪🇸 Español</a> | <a href="README_fra.md">🇫🇷 Français</a> | <a href="README_ita.md">🇮🇹 Italiano</a> | <a href="README_deu.md">🇩🇪 Deutsch</a> | <a href="README_zho.md">🇨🇳 简体中文</a> | <a href="README_jpn.md">🇯🇵 日本語</a> | 🇵🇹 <b>Português</b></p>

### 🤖 Gestão Inteligente de End-Effectors com Acompanhamento de Ciclo de Vida e Temperatura

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/MCU-STM32G4-003551.svg" alt="STM32G4">
  <img src="https://img.shields.io/badge/Protocol-CAN%20framing%20only-orange.svg" alt="CAN framing only">
  <img src="https://img.shields.io/badge/Tests-531-2ea44f.svg" alt="Tests">
  <img src="https://img.shields.io/badge/Feature-Smart%20Idle-green.svg" alt="Smart Idle">
</p>

---

**Verificação de honestidade - o que realmente funciona hoje:** `tool_id.c`, `lifecycle.c`, `preheat.c`, `protocol.c`, `rack_command.c`, `link_watchdog.c`, `tool_limits.c`, `rack_app.c`, `rack_link.c`, `rack_deframer.c`, `rack_can.c`, `preheat_limit.c` e `rack_inventory.c` são C puro e real, apoiado por 531 verificações `TEST_ASSERT` aprovadas (`tests/test_*.c`, compiladas e executadas com o próprio compilador de C do anfitrião, não com `arm-none-eabi-gcc`) - confirmado ao compilar e executar exatamente essa bateria de testes no anfitrião. Isso cobre a descodificação de ID, o acompanhamento de utilização/ciclo de vida, a ativação do pré-aquecimento e a temperatura alvo, a análise do protocolo com trama CRC8, a validação de intervalos dos comandos, a gestão de timeout da ligação/idempotência e o acompanhamento do inventário de ferramentas em vários compartimentos (que ferramenta está guardada em que compartimento, nunca uma mesma ferramenta física em dois compartimentos ao mesmo tempo) - tudo isto é lógica, nada é hardware. Como a própria introdução do README já diz: ainda não existe PCB/esquema para esta placa, pelo que nada aqui alguma vez acionou um pino GPIO, um chip F-RAM, uma resistência de aquecimento ou um transcetor CAN reais - `main.c`/`startup_stm32g4_minimal.c` só provam que a compilação cruzada e a ligação para Cortex-M4F têm êxito com um linker script provisório, não que tudo isto funcione em silício real. Consulte `CHANGELOG.md` para ver exatamente o que já foi entregue até agora. A linha série também é testada: 19 fluxos de bytes com a hora de chegada de cada pedaço (uma trama em pedaços, ruído, um CRC mau, uma versão ou um comprimento que não serve, um silêncio mesmo no limite do tempo de espera, a volta do relógio de milissegundos) passam pelo deframer real, e as tramas que saem e os seus contadores são os do modelo de URTC-COMMON.

---

> **Estado de segurança (safety e security):** este ecossistema não é um sistema de segurança certificado e ainda não existe nenhuma placa URTC montada. Os bootloaders verificam as imagens de firmware com CRC32 e um HMAC-SHA256 de chave partilhada, não com uma assinatura assimétrica (prevista); as imagens de aplicação publicadas estão assinadas com a chave pública de desenvolvimento (`signing_key_kind: dev`), pelo que qualquer pessoa pode produzir uma imagem que o bootloader aceita; nada foi transmitido a uma placa real. Leia a [base de segurança](https://github.com/JuanenRac/URTC-DOCS/blob/main/docs/SECURITY_BASELINE.md) e o [modelo de ameaças](https://github.com/JuanenRac/URTC-DOCS/blob/main/docs/THREAT_MODEL.md) antes de confiar em qualquer afirmação aqui para a segurança de uma pessoa.

## 📋 Num relance

| | |
|---|---|
| Versão | `0.2.1` |
| Maturidade | Funcional |
| Testes | 531 |
| Tecnologia | C / STM32G4 / arm-none-eabi-gcc |
| Destino | Uma placa com microcontrolador (o firmware); compila-se num PC |
| Em hardware real | Ainda não: não há registo de execução em hardware real para esta versão |

**Ir para:** [Visão geral](#-visão-geral) · [Arquitetura](#-arquitetura) · [Segurança](#-segurança-e-proteção) · [Interfaces](#-interfaces) · [Início rápido](#-início-rápido) · [Testes](#-testes) · [Estrutura](#-estrutura-do-repositório) · [Relacionados](#-projetos-relacionados)

<!-- urtc-section:status_smart_rack -->
## 🚦 Estado do projeto

🟢 feito e verificado · 🟡 funciona, com limites conhecidos · 🟠 a meio ou por verificar · 🔴 por fazer ou a não funcionar. Nada é verde se nunca correu em hardware real.

| Parte | Estado | O que o repositório consegue demonstrar |
| --- | :---: | --- |
| Software | 🟡 | Funciona num PC e contra o simulador; nunca foi usado com uma placa real. |
| Testes | 🟢 | A bateria de testes passa na CI em cada envio. |
| Hardware real | 🟠 | Ainda não correu numa placa real: o ensaio de bancada continua pendente. |
| Documentação | 🟡 | README em oito línguas e documentos de referência; as traduções não foram revistas por falantes nativos. |

<!-- /urtc-section:status_smart_rack -->

## 🎯 Visão geral

O **URTC-SMART-RACK** é um sistema inteligente de armazenamento de end-effectors dentro do ecossistema HYDRA-UMC. Baseado no microcontrolador STM32G4, monitoriza e prepara as ferramentas enquanto não estão acopladas a um robô.

Permite modos "Smart Idle", como o pré-aquecimento de pontas de soldar T12 imediatamente antes de uma troca de ferramenta, e regista o ID eletrónico e o total de ciclos de utilização de cada cabeça URTC (ler a versão de firmware de cada cabeça está planeado, não implementado), garantindo uma manutenção ótima e uma colocação em serviço em zero segundos.

Ainda não existe PCB/esquema para esta placa (ver `hardware/`), pelo que nada do que se segue pode acionar hardware real de GPIO/F-RAM/CAN - mas a *lógica* a que essas funcionalidades se resumem (descodificar um ID, acompanhar a utilização, decidir quando pré-aquecer e a que temperatura) é C puro e real, testado por testes unitários hoje.

### Funcionalidades principais:
* ✅ **v0 real - lógica de ID da ferramenta, ciclo de vida e pré-aquecimento:** `tool_id.c` descodifica uma leitura bruta de ID de 5 bits numa identidade de ferramenta; `lifecycle.c` regista os ciclos/o tempo de utilização e assinala quando a manutenção é devida; `preheat.c` decide quando o pré-aquecimento Smart Idle deve começar e a que temperatura alvo. 531 asserções de teste com o próprio compilador de C do anfitrião - não é necessário PCB, controlador de GPIO nem F-RAM para executar ou testar nada disto. Também real: um desencapsulador de fluxo para a ligação série (`rack_deframer.c`: procura o byte de início, ressincroniza-se após um byte perdido e descarta meia trama após um tempo limite), a forma CAN do comando (`rack_can.c`: oito bytes de dados, temperatura em big-endian; ainda nenhuma placa a transporta) e um limite para o tempo que um pré-aquecimento pode durar sem troca de ferramenta (`preheat_limit.c`: 10 minutos por predefinição, configurável).
* ✅ **Cabeças como unidades, verificação térmica e pré-aquecimento preditivo (testado no anfitrião):** `rack_inventory.c` pode identificar uma posição pelo tipo mais o número de série da cabeça, e `rack_units.c` guarda o uso, o tempo de aquecedor e as falhas térmicas de cada cabeça física (a partir dos seus próprios contadores, `0x1AA` / `0x1AB`), de modo que o desgaste acompanha a cabeça e não o tipo; `slot_verify.c` diz o que uma leitura térmica de uma posição significa para o pré-aquecimento ordenado (atingido, demasiado lento, ultrapassagem, um aquecedor ligado numa posição vazia); `preheat_plan.c` transforma a fila de trabalhos em ordens de pré-aquecimento, desligado por predefinição. A leitura e a fila vêm de outros programas, nada disto funcionou numa placa e os tempos de aquecimento são estimativas.
* 🗄️ **Identificação de Ferramentas** — identificação automática das cabeças URTC através de jumpers de ID de 5 bits ou de F-RAM. *(a própria lógica de descodificação de ID é real - ver acima; ler jumpers/F-RAM reais requer a PCB.)*
* 🗂️ **Inventário Multi-Compartimento** — `rack_inventory.c` regista que ferramenta está fisicamente guardada em que compartimento, mantém uma ferramenta real em exatamente um compartimento de cada vez (ao guardá-la noutro sítio, o compartimento anterior é libertado em vez de declarar que está em dois lugares ao mesmo tempo) e um alvo de pré-aquecimento por compartimento, obtido pela identidade da ferramenta. No arranque, `rack_inventory_reconcile()` confronta-o com um sensor de presença por compartimento: uma ferramenta retirada enquanto o rack estava desligado é libertada (incluindo o alvo de pré-aquecimento) e uma ferramenta colocada à mão é assinalada para identificação. *(a própria lógica do inventário é real - ver acima; ligá-la a sensores de presença reais por compartimento requer a PCB.)*
* 🌡️ **Lógica de Pré-Aquecimento** — gestão térmica inteligente para ferramentas de soldadura e de ar quente. *(a decisão de ativação e as temperaturas alvo são reais - ver acima; acionar uma resistência de aquecimento real requer a PCB.)*
* 📈 **Registos de Ciclo de Vida** — regista o total de ciclos de atuação e as horas de utilização na F-RAM da ferramenta. *(os contadores e a lógica de manutenção devida são reais - ver acima; guardá-los numa F-RAM real requer a PCB.)*
* 📡 **Integração CAN** — destina-se a comunicar com o Kinematic Brain do HYDRA-UMC para um ATC (Auto Tool Change) coordenado; isto nunca foi executado, porque não existe nem um driver CAN nem uma placa. *(o próprio protocolo de linha - tramas, CRC, validação de comandos - é real e testado no anfitrião, ver abaixo; continua a ser necessário um transcetor CAN real para o transportar efetivamente.)*
* 🔒 **Verificações de Intervalo do Protocolo** — tramas versionadas com soma de verificação CRC8, validação de intervalos de atuação e um watchdog de timeout/idempotência da ligação com um estado seguro definido. Trama na versão 2: todo o valor de vários bytes é big-endian, como o barramento, e uma trama da versão 1 é recusada. *(implementado em C puro e testado apenas num PC; nunca executado numa placa e não é uma função de segurança certificada)*
* ✅ **Toolchain de firmware Cortex-M4F** — uma imagem bare-metal real (startup + linker + `main.c`) que compila de forma cruzada e liga com `arm-none-eabi-gcc`, a mesma toolchain que o repositório irmão URTC utiliza. *(implementado — ver BUILD abaixo)*

## 🧩 Papel no URTC

O URTC-SMART-RACK é a placa que guarda as cabeças de ferramenta, não uma cabeça de ferramenta: um suporte que identifica cada cabeça que contém (o ID de ferramenta de 5 bits que o URTC usa), regista o uso e as falhas térmicas de cada cabeça física, sabe que ferramenta está em que posição e pré-aquece uma ferramenta de soldadura ou de ar quente mesmo antes de uma troca de ferramenta (Smart Idle). Partilha os IDs de ferramenta e as convenções de firmware do URTC, mas é uma ferramenta complementar, não filha do URTC, e destina-se a ser comandado pelo controlador de robô HYDRA-UMC. Ainda não existe nenhuma placa: o que é real hoje é a lógica e o protocolo do suporte, testados num computador.

## 🏛️ Arquitetura

<p align="center">
  <img src="images/architecture.svg" alt="Arquitetura de URTC-SMART-RACK: quem o usa, o que contém e de que depende" width="100%">
</p>

Como as partes deste repositório se encaixam: quem o usa, o que contém e de que depende. A imagem é desenhada a partir de `docs/architecture.json` e verificada com o código.

<details>
<summary>Versão em texto do diagrama (Mermaid)</summary>

```mermaid
flowchart TB
    TOOL["URTC Tool Head"] -- Plugged into Rack --> RACK["URTC-SMART-RACK"]
    RACK --> IDENT["Read ID & Lifetime Data"]
    IDENT --> SYNC["Sync with HYDRA-ORCHESTRATOR"]
    SYNC -- Anticipated Task --> HEAT["PRE-HEAT: Soldering Tip to 200°C"]
    RACK -- Health Check --> LOG["Maintenance Report"]
```

</details>

### ARQUITETURA E DECISÕES DE DESIGN

* **Porque é que esta placa ainda não tem pinout/ID de hardware definido.** Ainda não existe PCB para esta placa - `src/firmware_common.h` transporta uma identidade de versão sem ID de hardware, e os ficheiros de startup/linker são marcadores de posição escritos à mão que substituem o startup CMSIS/HAL da própria ST até que uma peça STM32G4 real seja definida.
* **Porque é que não é um filho do próprio URTC.** O URTC-SMART-RACK é uma Ferramenta Complementar, não um filho da família URTC - é uma placa física separada (um rack de armazenamento de ferramentas, não uma cabeça de ferramenta) que por acaso partilha o espaço de IDs de ferramenta e as convenções de firmware do próprio URTC, mas não a sua hierarquia de integração.
* **Porque `bump_version.py` é uma cópia direta do do URTC.** O mesmo formato de cabeçalho de firmware - reutilizar o script exato em vez de o reinventar mantém os dois em sincronia por construção.
* **Porque é que `tool_id.c`/`lifecycle.c`/`preheat.c` são entregues antes de qualquer controlador de GPIO/F-RAM/CAN.** Descodificar um ID, acumular utilização e decidir quando pré-aquecer são funções puras de dados já disponíveis - não precisam de PCB para serem escritas ou testadas, pelo que a v0 entrega primeiro essa lógica, testada no anfitrião com o compilador de C da própria máquina em vez de `arm-none-eabi-gcc`. Os controladores que obteriam efetivamente esses dados do hardware real chegam quando a PCB existir.
* **Como isto se integra no resto do ecossistema.** Usa o ecossistema de ferramentas do próprio URTC (os mesmos IDs de ferramenta; as tramas do rack ainda não são tramas CAN, ver `docs/OPERATIONS.md`) e forma um par natural com o HYDRA-UMC-DETECTION-HEF para reconhecer visualmente que ferramenta está efetivamente guardada.
* **Porque é que o protocolo, a validação de comandos e o watchdog da ligação são três módulos separados.** `protocol.c` só conhece bytes, tramas e um CRC - não faz ideia do que é uma temperatura "segura". `rack_command.c` é responsável por esse juízo, descodificando e verificando os intervalos de um comando real sem se preocupar com a forma como chegou ao fio. `link_watchdog.c` acompanha o timeout/idempotência independentemente de ambos - uma trama corrompida nunca deve sequer chegar até ele (ver a asserção real de `test_rack_link_scenarios.c` de que uma trama com CRC errado nunca reanima o watchdog). Mantê-los separados é o que torna cada um testável isoladamente no anfitrião e mantém um futuro tratador de receção CAN fino - chama cada camada por ordem, não volta a implementar nenhum dos seus juízos.
* **Limitador de taxa de comandos.** Uma enxurrada de comandos para a mesma ferramenta é travada por um intervalo mínimo entre comandos aceites (20 ms por defeito, 0 desativa; `link_watchdog_set_command_interval()`). Um comando retido não altera nada e não recebe confirmação, mas conta como ligação viva; um estado seguro ou uma retransmissão nunca são retidos. O intervalo pode ser ajustado na bancada simulada do rack do URTC-WEB-STUDIO.
* **Porque é que uma ligação não comprovada é tratada exatamente como uma ligação morta.** `link_watchdog_is_link_lost()` é verdadeiro tanto após um timeout real COMO antes de a primeira trama alguma vez ter chegado - o "estado seguro al arrancar" das próprias notas de diagnóstico. Um rack que arranca sem nenhum anfitrião ainda ligado deve começar no estado seguro, e não assumir silenciosamente que "está tudo bem" até prova em contrário.

## 🔒 Segurança e proteção

| Ameaça | Defesa |
|---|---|
| Uma ordem fora do intervalo para uma ferramenta | `rack_command.c` descodifica e verifica o intervalo de cada ordem sem se preocupar com a forma como chegou; `protocol.c` só conhece bytes, tramas e um CRC; `tool_limits.c` guarda o limite de cada perfil de ferramenta |
| Uma trama corrompida ou antiga | Uma trama versionada com CRC-8; na versão 2 todo o valor de vários bytes é big-endian como o barramento e uma trama da versão 1 é recusada |
| Uma enxurrada de ordens | Um intervalo mínimo entre ordens aceites (20 ms por omissão, 0 desativa); uma ordem retida não altera nada e não recebe confirmação, mas conta como ligação viva; um estado seguro ou uma retransmissão nunca é retido |
| Uma ligação perdida ou não provada | `link_watchdog_is_link_lost()` é verdadeiro depois de um verdadeiro timeout e também antes de chegar a primeira trama: um suporte que liga sem anfitrião arranca em estado seguro |
| Pré-aquecer demasiado tempo ou demasiado quente | `preheat_limit.c` limita quanto tempo um pré-aquecimento pode durar sem troca de ferramenta (10 minutos por omissão, configurável) e `tool_limits.c` o alvo máximo de cada perfil de ferramenta |
| Uma ferramenta em duas posições | `rack_inventory.c` mantém uma ferramenta real em exatamente uma posição; guardá-la noutra limpa a anterior |

Tudo isto é C puro testado num computador; nunca correu numa placa e não é uma função de segurança certificada.

## 🔌 Interfaces

* **A trama série do suporte:** versão 2, big-endian como o barramento, com o CRC-8 fornecido pelo URTC-COMMON (`src/urtc_crc8.h`, `src/urtc_frame.h`); `protocol.c` interpreta-a e codifica-a, `rack_deframer.c` encontra tramas num fluxo de bytes (ressincronização após um byte perdido, meia trama descartada após um timeout) e `rack_link.c` decide o que fazer a cada trama. Os formatos estão em [docs/OPERATIONS.md](docs/OPERATIONS.md).
* **A forma CAN da ordem:** `rack_can.c` (oito bytes de dados, temperatura big-endian). Nenhuma placa a transporta ainda: as tramas do suporte ainda não são tramas CAN.
* **Partilhado com o URTC:** o ID de ferramenta de 5 bits (`tool_id.c`) e os contadores `0x1AA` / `0x1AB` que guardam o uso de cada cabeça física (`rack_units.c`). Os vetores que a camada de tramas tem de reproduzir são os do URTC-COMMON (`conformance/rack_vectors.json`).

## 🚀 Início rápido

Requer a ARM GNU Toolchain (`arm-none-eabi-gcc`, `arm-none-eabi-objcopy`, `arm-none-eabi-size`) e Python 3.

```bash
# Linux/macOS
chmod +x build_firmware.sh   # one-time
./build_firmware.sh

# Windows
build_firmware.bat
```

Exemplo real - os testes do lado do anfitrião também se executam de forma autónoma, útil para verificar a lógica sem uma compilação completa do firmware:

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

<!-- urtc-section:compatibility -->
### Requisitos e compatibilidade

As versões dos programas URTC que funcionam em conjunto, e a versão do protocolo que cada uma fala, constam da tabela de compatibilidade que o URTC-COMMON gera: [docs/COMPATIBILITY.md](https://github.com/JuanenRac/URTC/blob/main/docs/COMPATIBILITY.md).
<!-- /urtc-section:compatibility -->

## 🧪 Testes

Os testes de host são programas C em `tests/` (531 verificações `TEST_ASSERT` em 18 ficheiros) compilados e executados com o compilador C do próprio computador, nunca com `arm-none-eabi-gcc`: ID de ferramenta, limites de ferramenta, ciclo de vida, pré-aquecimento, a trama, a verificação das ordens, o cão de guarda da ligação, cenários completos da ligação, o inventário, a robustez contra entradas erradas, a camada de aplicação, o desenquadrador (19 fluxos de bytes com o instante de chegada de cada pedaço: uma trama em pedaços, ruído, um CRC errado, uma versão ou comprimento errados, um silêncio no limite do timeout, uma volta do relógio de milissegundos), a forma CAN, o limite de pré-aquecimento e as unidades de cada cabeça; os vetores são os do URTC-COMMON. Cinco ficheiros de teste Python (25 testes) verificam que os scripts de compilação, os ativos da release, os ficheiros de versão e o número de testes publicado concordam. A CI compila os testes C com AddressSanitizer e UBSan e falha abaixo de um mínimo de cobertura das linhas executadas de `src/` (`tools/gcov_floor.py`). `bash build_firmware.sh` executa-os antes da compilação cruzada e falha a compilação se uma asserção falhar. O anel de bytes que uma interrupção de receção preenche (`rack_ring.c`) e o armazenamento em duas cópias do ciclo de vida e do inventário (`rack_persist.c`, testado com um corte de energia em cada byte de uma gravação) também são lógica pura; o chip de memória por trás do armazenamento ainda não está escolhido.

## 🏗️ Compilação e publicação

A compilação começa por compilar e executar `tests/` com o compilador C *do próprio computador* (nunca `arm-none-eabi-gcc`: são testes de lógica pura, sem tocar em registos do MCU) e falha por completo se alguma asserção falhar. Só então compila `main.c` e `startup_stm32g4_minimal.c` para Cortex-M4F, liga-os ao mapa de memória provisório `STM32G4_MINIMAL.ld` e publica ficheiros `.elf`/`.bin`/`.hex` com versão em `firmware/`. Compilar nunca altera a versão.

Cada módulo de `src/` é também compilado para Cortex-M4F e todos são ligados em conjunto como verificação (não há biblioteca C no alvo), embora hoje a imagem só contenha `main.c` e o ficheiro de arranque. Com `URTC_CI=1` nenhum ficheiro versionado é tocado.

As imagens de firmware (`.bin`, `.hex`, `.elf`) não são versionadas no git: uma compilação de release escreve `firmware/URTC_SMART_RACK_FIRMWARE_v<versão>.*` (ignorado pelo git; `build/firmware` em modo CI, `URTC_CI=1`) e são publicadas apenas como assets da release do GitHub de cada versão. `python tools/publish_release_assets.py` verifica os ficheiros, escreve o ficheiro `.sha256` em falta e imprime o comando `gh release create` exato com os ficheiros a anexar.

Ainda não há nada para gravar em hardware real — não existe PCB que confirme a peça STM32G4 de destino, o pinout ou os tamanhos reais de flash/RAM. O mapa de memória do linker script é um marcador de posição conservador (documentado no seu próprio comentário de cabeçalho) que será substituído quando existir hardware real, o mesmo momento em que a tabela de vetores escrita à mão de `startup_stm32g4_minimal.c` é substituída pelo código de startup CMSIS/HAL da própria ST (espelhando o `src/F303-master/` do repositório irmão URTC para as suas placas STM32F303).

Veja [`CHANGELOG.md`](CHANGELOG.md) para o histórico completo de versões; cada versão tem aí a sua própria entrada.

## 📂 Estrutura do repositório

<p align="center">
  <img src="images/repository-structure.svg" alt="Estrutura do repositório URTC-SMART-RACK" width="100%">
</p>

<details>
<summary>Versão em texto da árvore</summary>

```text
URTC-SMART-RACK/
├── src/                                               o firmware: lógica em C puro, as cópias verificadas do URTC-COMMON, o ponto de entrada e o script de ligação provisório
├── tests/                                             testes de host em C (531 verificações) e verificações em Python dos scripts de compilação
├── docs/                                              OPERATIONS.md: o que a lógica faz, recusa e deixa por verificar
├── hardware/                                          notas: ainda sem esquema nem PCB
├── tools/                                             verificação de compilação, mínimo de cobertura, ativos da release e ferramentas partilhadas de CI e imagens
├── images/                                            faixa e imagens deste README
├── .github/                                           fluxo de CI e modelos de issues e pull requests
├── build-test.sh build-test.bat                       scripts de compilação e testes que não alteram nada no repositório
├── test.sh test.bat                                   executam os testes
├── urtc.project.json                                  manifesto: versão, papel, maturidade e comando de compilação
├── CHANGELOG.md                                       o que mudou em cada versão
├── SECURITY.md SUPPORT.md LICENSE                     política de segurança, suporte e licença
├── CONTRIBUTING.md CODE_OF_CONDUCT.md                 como contribuir e código de conduta
├── .gitattributes .gitignore                          definições do Git
├── build_firmware.sh build_firmware.bat               testes de host, compilação cruzada, ligação e publicação das imagens
├── bump.py bump_version.py bump_manifest_version.py   scripts que sobem a versão
└── LICENSE.md ruff.toml                               licença da documentação e definições de lint
```

</details>

A lista anotada de cada ficheiro de código, uma linha por ficheiro, está em [docs/SOURCE_MAP.md](docs/SOURCE_MAP.md).

## 🗺️ Roteiro e limites conhecidos

**Limites conhecidos hoje**

* Ainda não existe PCB nem esquema para esta placa (veja `hardware/`): nada disto alguma vez comandou um pino GPIO real, uma F-RAM, um aquecedor ou um transcetor CAN. `main.c` e o ficheiro de arranque apenas provam que a compilação cruzada e a ligação para Cortex-M4F funcionam com um script de ligação provisório.
* Ler a versão do firmware de cada cabeça está planeado, não implementado.
* A forma CAN da ordem nunca correu: não existe nem um controlador CAN nem uma placa, e as tramas do suporte ainda não são tramas CAN.
* Os tempos de aquecimento do pré-aquecimento preditivo são estimativas e o seu interruptor está desligado por omissão.

**A seguir**

* Projetar a placa, fixar o componente STM32G4 e substituir o script de ligação e o ficheiro de arranque provisórios pelos da ST; escrever os controladores de GPIO, F-RAM e CAN que fornecem os dados à lógica.

## 🌍 Idiomas

O README e os textos existem em inglês, espanhol, francês, italiano, alemão, chinês, japonês e português europeu.

## 🔗 Projetos relacionados

Este projeto faz parte do ecossistema de robótica HYDRA-UMC do mesmo autor (JuanenRac / Electro Hobby 3D). Vale a pena conhecê-los, pois um pedido pode, na verdade, dizer respeito a um destes e não a este repositório.

**Diretamente Relacionados**
- **[URTC](https://github.com/JuanenRac/URTC)** — firmware da PCB física do Universal Robot Tool Controller, mais de 25 perfis de ferramenta sobre barramento CAN — o mesmo ecossistema de ferramentas, sobre o mesmo barramento CAN.
- **[HYDRA-UMC-DETECTION-HEF](https://github.com/JuanenRac/HYDRA-UMC-DETECTION-HEF)** — registo real de modelos compilados com verificação segura de carregamento por arquitetura Hailo/checksum — fornece o reconhecimento visual das ferramentas que este rack armazena.

**Também Fazem Parte do Ecossistema**

*Hardware e Plataforma Base*
- **[HYDRA-UMC](https://github.com/JuanenRac/HYDRA-UMC)** — o desenho da placa-mãe do braço robótico: anfitrião CM5 + STM32H745 de dois núcleos, pensado para orquestrar até 8 braços de ferramenta por CAN-OTA/SPI-OTA (firmware numa fase inicial; ainda sem placa montada).
- **[HYDRA-UMC-OS](https://github.com/JuanenRac/HYDRA-UMC-OS)** — camada de produto reproduzível de Raspberry Pi OS para o CM5: agente só de leitura, configuração/perfis validados, provisionamento WiFi de primeiro contacto.
- **[HYDRA-UMC-SDK](https://github.com/JuanenRac/HYDRA-UMC-SDK)** — o contrato JSON-Schema partilhado e a fronteira de software de controlo de trabalhos contra a qual cada bridge valida os seus comandos (não é uma função de segurança certificada).
- **[HYDRA-UMC-CONNECTOR-HUB](https://github.com/JuanenRac/HYDRA-UMC-CONNECTOR-HUB)** — registo e validador declarativo de manifestos de adaptadores para conectores de máquinas externas; estende a própria ideia de contrato do SDK a máquinas externas sem substituir os projetos de gateway industrial.

*Backend e Clientes Base*
- **[HYDRA-UMC-SERVER](https://github.com/JuanenRac/HYDRA-UMC-SERVER)** — o verdadeiro backend headless (REST/WebSocket) com o qual todos os clientes de controlo realmente comunicam.
- **[HYDRA-UMC-STUDIO](https://github.com/JuanenRac/HYDRA-UMC-STUDIO)** — painel de controlo web com visualização 3D multi-robô em tempo real.
- **[HYDRA-UMC-SUITE](https://github.com/JuanenRac/HYDRA-UMC-SUITE)** — centro de comando de enxame para desktop (PySide6) para vários servidores em simultâneo, empacotado como executável autónomo.
- **[HYDRA-UMC-ANDROID-CONTROL](https://github.com/JuanenRac/HYDRA-UMC-ANDROID-CONTROL)** — aplicação nativa de controlo para Android com início de sessão biométrico e uma aplicação companheira emparelhada para Wear OS.
- **[HYDRA-UMC-IOS-CONTROL](https://github.com/JuanenRac/HYDRA-UMC-IOS-CONTROL)** — aplicação de controlo para iOS/iPadOS (Flutter) com sincronização WebSocket em tempo real.
- **[HYDRA-UMC-DSI](https://github.com/JuanenRac/HYDRA-UMC-DSI)** — interface tátil nativa para o ecrã tátil DSI de 7" integrado, embebida no próprio CM5.
- **[HYDRA-UMC-EDITOR-URDF](https://github.com/JuanenRac/HYDRA-UMC-EDITOR-URDF)** — criador/editor gráfico de URDF para desktop que envia os modelos terminados para o próprio catálogo do STUDIO.
- **[HYDRA-UMC-EDITOR-STL](https://github.com/JuanenRac/HYDRA-UMC-EDITOR-STL)** — editor de modelos STL para desktop, que transforma/substitui/remove/adiciona peças reais no mesmo catálogo de modelos que o HYDRA-UMC-EDITOR-URDF também edita.
- **[HYDRA-UMC-BRIDGE-AMR](https://github.com/JuanenRac/HYDRA-UMC-BRIDGE-AMR)** — fronteira de coordenação para frotas AGV/AMR através de um publicador MQTT VDA 5050 (testado apenas contra uma frota emulada).
- **[HYDRA-UMC-BRIDGE-CNC](https://github.com/JuanenRac/HYDRA-UMC-BRIDGE-CNC)** — coordenador de células CNC de alto nível com acesso ao estado GRBL e aos bytes de controlo (testado apenas contra um emulador).
- **[HYDRA-UMC-BRIDGE-DROIDS](https://github.com/JuanenRac/HYDRA-UMC-BRIDGE-DROIDS)** — fronteira de coordenação para droides com patas/humanoides, com um emissor de comandos para o Boston Dynamics Spot (testado apenas contra um emulador).
- **[HYDRA-UMC-BRIDGE-LASER](https://github.com/JuanenRac/HYDRA-UMC-BRIDGE-LASER)** — coordenador de células laser que lê 3 sinais GPIO (chave, caixa e interlock) (testado apenas contra um chip simulado; não pode armar nem disparar um laser).
- **[HYDRA-UMC-BRIDGE-OPENPNP](https://github.com/JuanenRac/HYDRA-UMC-BRIDGE-OPENPNP)** — coordenador seguro de alto nível do fluxo de placas para o pick-and-place OpenPnP.
- **[HYDRA-UMC-BRIDGE-PRINTER3D](https://github.com/JuanenRac/HYDRA-UMC-BRIDGE-PRINTER3D)** — fronteira de coordenação segura para impressoras 3D Moonraker/Klipper, com comandos de trabalho reais protegidos por barreira.
- **[HYDRA-UMC-BRIDGE-ROS2](https://github.com/JuanenRac/HYDRA-UMC-BRIDGE-ROS2)** — coordenador com um transporte ROS 2 baseado em rclpy importado sob demanda (ainda não executado contra uma instalação ROS 2).
- **[HYDRA-UMC-BRIDGE-UAV](https://github.com/JuanenRac/HYDRA-UMC-BRIDGE-UAV)** — fronteira de coordenação para UAV com câmara, com um emissor de comandos MAVLink (testado apenas contra um emulador).

*Plataforma de Ferramentas URTC*
- **[URTC-FLASHER](https://github.com/JuanenRac/URTC-FLASHER)** — ferramenta de gravação com interface gráfica para desktop para placas URTC, CAN-OTA mais SWD/JTAG de chip completo.
- **[URTC-COMMON](https://github.com/JuanenRac/URTC-COMMON)** — o protocolo CAN escrito uma só vez como dados (especificação, tabelas geradas, vetores de conformidade) e o código que os programas partilham; este repositório traz cópias das partes que usa.
- **[URTC-GATEWAY](https://github.com/JuanenRac/URTC-GATEWAY)** — uma gateway à frente do barramento CAN que só deixa passar identificadores de uma lista, limita a sua taxa e pede uma chave de sessão.
- **[URTC-TESTER](https://github.com/JuanenRac/URTC-TESTER)** — ferramenta de diagnóstico em direto do barramento CAN para desktop para placas URTC, um painel por perfil de ferramenta.
- **[URTC-WEB-STUDIO](https://github.com/JuanenRac/URTC-WEB-STUDIO)** — alternativa no navegador ao URTC-TESTER através da Web Serial API, sem necessidade de instalação local.

*Nó de IA de Visão (Hailo-8)*
- **[HYDRA-UMC-VISION-NODE](https://github.com/JuanenRac/HYDRA-UMC-VISION-NODE)** — hub de integração do pipeline de visão Hailo-8, com uma verificação real de prontidão de hardware por etapa.
- **[HYDRA-UMC-VISION-STREAMER](https://github.com/JuanenRac/HYDRA-UMC-VISION-STREAMER)** — pipeline GStreamer real + gerador de configuração MediaMTX com uma fronteira de integração HailoRT real.
- **[HYDRA-UMC-VISUAL-SERVOING-API](https://github.com/JuanenRac/HYDRA-UMC-VISUAL-SERVOING-API)** — lei de correção de servovisão baseada em posição com autorização condicionada ao estado da zona; ainda sem estimativa de pose por câmara.
- **[HYDRA-UMC-SAFETY-ZONES](https://github.com/JuanenRac/HYDRA-UMC-SAFETY-ZONES)** — lógica v0 de violação de zonas, atualidade da calibração e pedido de E-STOP que lê as posições dos objetos de um ficheiro; nenhum detetor a alimenta e ainda não há E-STOP físico ligado.

*Nó de IA Cognitiva (Hailo-10)*
- **[HYDRA-UMC-COGNITIVE-NODE](https://github.com/JuanenRac/HYDRA-UMC-COGNITIVE-NODE)** — hub de integração do pipeline cognitivo Hailo-10 (orquestração de LLM/VLA/voz).
- **[HYDRA-UMC-VLA-ENGINE](https://github.com/JuanenRac/HYDRA-UMC-VLA-ENGINE)** — codificação/descodificação real de tokens de ação e geração de trajetórias para um modelo Vision-Language-Action.
- **[HYDRA-UMC-VOICE-UI](https://github.com/JuanenRac/HYDRA-UMC-VOICE-UI)** — front-end de voz real (VAD + analisador de intenções) com um relé limitado para o Watch, protegido por confirmação.
- **[HYDRA-UMC-SEMANTIC-PLANNER](https://github.com/JuanenRac/HYDRA-UMC-SEMANTIC-PLANNER)** — decomposição real de tarefas baseada em regras e recuperação semântica de erros sobre códigos de erro do MCU.
- **[HYDRA-UMC-DOCS-QA](https://github.com/JuanenRac/HYDRA-UMC-DOCS-QA)** — pesquisa documental TF-IDF real, apenas com a biblioteca padrão, sobre a própria documentação Markdown deste ecossistema.
- **[HYDRA-UMC-LOCAL-TECHNICIAN](https://github.com/JuanenRac/HYDRA-UMC-LOCAL-TECHNICIAN)** — técnico de manutenção de IA local e regido por políticas para o próprio ecossistema - observa, diagnostica e propõe correções; os dois níveis de risco mais elevados não estão deliberadamente implementados ainda.

*Orquestração e Enxame*
- **[HYDRA-UMC-ORCHESTRATOR](https://github.com/JuanenRac/HYDRA-UMC-ORCHESTRATOR)** — hub de integração com um contrato real de relatório de saúde gRPC/Protobuf e uma máquina de estados de missão.
- **[HYDRA-UMC-JOB-DISPATCHER](https://github.com/JuanenRac/HYDRA-UMC-JOB-DISPATCHER)** — fila real de trabalhos baseada em prioridades com deduplicação, sobre uma API HTTP real.
- **[HYDRA-UMC-NODE-HEALING](https://github.com/JuanenRac/HYDRA-UMC-NODE-HEALING)** — watchdog real de saúde da frota baseado em gRPC, com retry/backoff e deteção de incompatibilidade de identidade.
- **[HYDRA-UMC-PATH-PLANNER-3D](https://github.com/JuanenRac/HYDRA-UMC-PATH-PLANNER-3D)** — planeador real de trajetórias 3D baseado em RRT, com validação real de colisões com obstáculos/espaço de trabalho.
- **[HYDRA-UMC-SWARM-SYNC](https://github.com/JuanenRac/HYDRA-UMC-SWARM-SYNC)** — sincronização real de estado CRDT LWW-Element-Map, testada por propriedades quanto à convergência multicélula.

*Gémeo Digital e Simulação*
- **[HYDRA-UMC-TWIN](https://github.com/JuanenRac/HYDRA-UMC-TWIN)** — hub de integração do motor de gémeo digital, com um contrato real de sincronização por compatibilidade de versões.
- **[HYDRA-UMC-HIL-BRIDGE](https://github.com/JuanenRac/HYDRA-UMC-HIL-BRIDGE)** — encaminhador hardware-in-the-loop com um interlock de software de colisão entre a simulação e o hardware real (testado sem hardware real).
- **[HYDRA-UMC-PHYSICS-REPLICA](https://github.com/JuanenRac/HYDRA-UMC-PHYSICS-REPLICA)** — cinemática direta real e validação de limites de juntas sobre um subconjunto real de URDF.
- **[HYDRA-UMC-SYNTHETIC-DATA-GEN](https://github.com/JuanenRac/HYDRA-UMC-SYNTHETIC-DATA-GEN)** — gerador procedimental real de cenas 2D com exportação de anotações YOLO/COCO.

*Dados e Análise*
- **[HYDRA-UMC-DATALAKE](https://github.com/JuanenRac/HYDRA-UMC-DATALAKE)** — armazenamento real de séries temporais com sqlite3 e uma API HTTP real de ingestão/consulta.
- **[HYDRA-UMC-ANOMALY-DETECTOR](https://github.com/JuanenRac/HYDRA-UMC-ANOMALY-DETECTOR)** — detetor real de anomalias com FFT + linha de base estatística e monitorização de deriva.
- **[HYDRA-UMC-PRODUCTION-REPORTS](https://github.com/JuanenRac/HYDRA-UMC-PRODUCTION-REPORTS)** — cálculo real de OEE/disponibilidade sobre o histórico do DATALAKE, com exportação CSV reproduzível.
- **[HYDRA-UMC-TELEMETRY-COLLECTOR](https://github.com/JuanenRac/HYDRA-UMC-TELEMETRY-COLLECTOR)** — pipeline real de ingestão CAN/WebSocket para o DATALAKE, com deduplicação por sequência.

*Gateway Industrial*
- **[HYDRA-UMC-GATEWAY-INDUSTRIAL](https://github.com/JuanenRac/HYDRA-UMC-GATEWAY-INDUSTRIAL)** — hub de integração que retransmite para protocolos industriais, com uma camada real de lista de comandos permitidos/contrapressão.
- **[HYDRA-UMC-OPCUA-SERVER](https://github.com/JuanenRac/HYDRA-UMC-OPCUA-SERVER)** — espaço de endereçamento OPC-UA real, verificado com uma sessão real de cliente de protocolo binário.
- **[HYDRA-UMC-MQTT-BROKER](https://github.com/JuanenRac/HYDRA-UMC-MQTT-BROKER)** — broker MQTT real com autenticação opcional por cliente e ACL de tópicos.
- **[HYDRA-UMC-MTCONNECT-ADAPTER](https://github.com/JuanenRac/HYDRA-UMC-MTCONNECT-ADAPTER)** — endpoints XML reais MTConnect `/probe` e `/current` com saída em modo degradado.

*Ferramentas Complementares e Operações do Ecossistema*
- **[HYDRA-UMC-DASHBOARD-AI](https://github.com/JuanenRac/HYDRA-UMC-DASHBOARD-AI)** — painéis de Smart Summaries e Anomaly Highlighting sobre o DATALAKE/ANOMALY-DETECTOR, com uma alternativa estatística honesta.
- **[HYDRA-UMC-TOOL-CLI](https://github.com/JuanenRac/HYDRA-UMC-TOOL-CLI)** — CLI de frota com um contrato real e estável de códigos de saída, um verdadeiro cliente em direto da própria API do HYDRA-UMC-SERVER.
- **[HYDRA-UMC-WATCH](https://github.com/JuanenRac/HYDRA-UMC-WATCH)** — aplicação companheira WearOS com padrões de alerta tátil e um relé de voz para o telemóvel emparelhado (ainda não executada num relógio físico).
- **[URTC-VISION-TOOL](https://github.com/JuanenRac/URTC-VISION-TOOL)** — firmware mais um verdadeiro companheiro de visão em Python para uma cabeça de ferramenta de inspeção térmica/RGB.
- **[URTC-UPDATER](https://github.com/JuanenRac/URTC-UPDATER)** — ferramenta de ambiente de trabalho que deteta, instala e atualiza os repositórios da família URTC, uma ação explícita de cada vez.
- **[HYDRA-UMC-UPDATER](https://github.com/JuanenRac/HYDRA-UMC-UPDATER)** — ferramenta administrativa para desktop que descobre, clona e atualiza todos os repositórios deste ecossistema.
- **[HYDRA-UMC-OS-REBUILDER](https://github.com/JuanenRac/HYDRA-UMC-OS-REBUILDER)** — ferramenta para desktop Windows/Linux que constrói uma imagem CM5 pronta a gravar, pré-carregada com as versões mais atuais do ecossistema, com configuração de Wi-Fi/utilizador/SSH no primeiro arranque ao estilo do Raspberry Pi Imager.
- **[HYDRA-UMC-OPS-AGENT](https://github.com/JuanenRac/HYDRA-UMC-OPS-AGENT)** — coordenador de incidentes de manutenção: uma função edge de baixos privilégios recolhe um instantâneo higienizado de inventário/saúde, uma função de control-plane apresenta-o só em leitura e pede a um fornecedor de IA que sugira um diagnóstico - nunca aplica um patch nem implementa nada.
- **[HYDRA-UMC-DEV-SERVER](https://github.com/JuanenRac/HYDRA-UMC-DEV-SERVER)** — anfitrião de desenvolvimento reproduzível (Raspberry Pi 5 / CM5) que armazena o código-fonte do ecossistema e executa tarefas limitadas de compilação/teste numa fila durável; uma função dedicada de desenvolvimento, explicitamente não um CM5 operacional.

## 📚 Documentação

- **[CONTRIBUTING.md](CONTRIBUTING.md)** — stack tecnológica e diretrizes de código para um pull request.
- **[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)** — as normas de comportamento esperadas nesta comunidade.
- **[SECURITY.md](SECURITY.md)** — como comunicar uma vulnerabilidade e as áreas de foco de segurança reais deste projeto.
- **[SUPPORT.md](SUPPORT.md)** — onde colocar questões e comunicar erros.
- **[LICENSE.md](LICENSE.md)** — a licença deste projeto.

## 🤝 Contribuir e comunidade

* Antes de um pull request execute `URTC_CI=1 python tools/ci_validate.py`: tem de passar. Veja [CONTRIBUTING.md](CONTRIBUTING.md) e o [código de conduta](CODE_OF_CONDUCT.md).
* O README existe em oito idiomas: uma alteração ao texto faz-se nos oito ficheiros, e as secções partilhadas com outros repositórios são escritas por `tools/readme_sections.py` do URTC-COMMON.
* Uma alteração ao protocolo CAN começa no URTC-COMMON (`spec/can_protocol.json`) e chega a este repositório através da cópia verificada.
* Não descreva como a funcionar numa placa aquilo que só foi compilado.
* Comunique uma vulnerabilidade em privado: [SECURITY.md](SECURITY.md). Perguntas, ideias e relatórios: electrohobby3d@gmail.com ([SUPPORT.md](SUPPORT.md)).

## 👤 AUTOR

**JuanenRac** (Electro Hobby 3D)
📧 electrohobby3d@gmail.com
📺 [youtube.com/@electrohobby3d](https://youtube.com/@electrohobby3d)

## 📜 LICENÇA

GPL-3.0-or-later - Consulte LICENSE para mais detalhes.
