# Prusa ESP32 Cam: versão XIAO ESP32-S3 Sense para a Prusa Core One

Este repositório é um **fork** do firmware oficial
[prusa3d/Prusa-Firmware-ESP32-Cam](https://github.com/prusa3d/Prusa-Firmware-ESP32-Cam)
(versão base **v1.1.2**, commit `136c91c`). Todo o histórico original está mantido e as nossas
alterações estão nos commits por cima dele.

Objetivo: uma câmara fiável para o dia a dia, numa **Prusa Core One**, com uma
**Seeed Studio XIAO ESP32-S3 Sense** (câmara OV2640).

Versão deste firmware: **1.1.2-c1**.

> **Importante:** este firmware **compila**, mas **ainda não foi testado numa placa real**.
> Os primeiros testes na tua impressora são a verificação final. Lê a secção
> [Limitações conhecidas](#limitações-conhecidas).

---

## Índice

1. [O que fizemos (resumo)](#1-o-que-fizemos-resumo)
2. [Os 11 bugs corrigidos](#2-os-11-bugs-corrigidos)
3. [Migração para o Prusa Camera Service](#3-migração-para-o-prusa-camera-service)
4. [O que são os "Core One defaults"](#4-o-que-são-os-core-one-defaults)
5. [O que é o "thermal guard"](#5-o-que-é-o-thermal-guard-proteção-térmica)
6. [PrusaLink: fotos frequentes só durante a impressão](#6-prusalink-fotos-frequentes-só-durante-a-impressão)
7. [Outras melhorias](#7-outras-melhorias)
8. [Como compilar (Arduino IDE ou VS Code)](#8-como-compilar)
9. [Como gravar o firmware na placa](#9-como-gravar-o-firmware-na-placa)
10. [Primeira configuração](#10-primeira-configuração)
11. [Limitações conhecidas](#limitações-conhecidas)
12. [Ficheiros alterados](#12-ficheiros-alterados)
13. [Trazer atualizações do projeto original](#13-trazer-atualizações-do-projeto-original)

---

## 1. O que fizemos (resumo)

1. **Revimos o código todo** do firmware original e encontrámos 11 bugs reais que, numa
   utilização longa (impressões de muitas horas), causam bloqueios, reinícios, fotos perdidas
   ou falhas de segurança. **Corrigimos os 11.**
2. **Migrámos para a nova API da Prusa** (`camera-service.prusa3d.com`), seguindo a
   documentação oficial: formato de dados novo, certificados TLS novos, intervalos de envio
   permitidos e tratamento correto de cada código de erro.
3. **Definições de fábrica para a Core One** (melhor resolução, flash desligado).
4. **Proteção térmica** (*thermal guard*): se o chip aquecer demasiado dentro da câmara
   fechada da impressora, a câmara abranda sozinha.
5. **Integração opcional com o PrusaLink** da impressora: fotos a cada 10 s enquanto imprime,
   uma foto a cada 10 min quando está parada.
6. **Compilação reproduzível**: as bibliotecas exatas estão na pasta `libraries/`, existe o
   script `build_xiao.sh` e os ficheiros já compilados estão em `firmware/xiao-esp32-s3/`.

---

## 2. Os 11 bugs corrigidos

| # | Onde | Problema | O que acontecia | Correção |
|---|------|----------|-----------------|----------|
| 1 | `camera.cpp` (stream) | Se a captura de um frame falhava durante o stream, o código saía **sem libertar o semáforo** (o "cadeado" do buffer da câmara) | A tarefa das fotos ficava bloqueada para sempre e o watchdog **reiniciava a placa** ~40 s depois | O semáforo é sempre libertado, e o stream termina de forma limpa quando a captura falha |
| 2 | `connect.cpp` | A leitura da resposta do servidor não tinha **limite de tempo** e ocupava o processador em ciclo | Com rede lenta ou instável: bloqueio e reinício pelo watchdog | Limite de 10 s, com pausas de 10 ms enquanto espera |
| 3 | `camera.cpp` | Quando faltava memória, o buffer era libertado mas o ponteiro continuava a apontar para ele | Libertação dupla no frame seguinte, corrupção de memória e crash | Ponteiro anulado, e o buffer é reaproveitado em vez de ser alocado a cada frame |
| 4 | `connect.cpp` | Envio durante o stream usava um buffer que podia ser **nulo** | Crash ao enviar a foto logo após abrir o stream | O envio usa agora uma cópia própria da foto (ver ponto 8) |
| 5 | `connect.cpp` | Quando o tamanho da foto era **múltiplo exato de 2048 bytes**, os últimos 2 KB não eram enviados | O servidor recusava a foto (tamanho errado). Raro, mas acontecia | Ciclo de envio reescrito, que também trata envios parciais |
| 6 | `connect.cpp` | Eram enviados **4 bytes extra** depois do corpo do pedido HTTP | Violação do protocolo HTTP (o novo servidor é mais rigoroso) | Removidos |
| 7 | `camera.cpp` | As tentativas de repetir uma foto má **não funcionavam** (o frame mau não era devolvido ao driver), e nesse caminho o **flash ficava ligado** | Fotos falhadas e LED de flash aceso permanentemente | Frame mau devolvido antes de nova tentativa, e o flash é sempre desligado |
| 8 | `WebServer.cpp` | A página `/saved-photo.jpg` marcava a foto como "livre" **antes** de o browser acabar de a descarregar | Imagem corrompida ou misturada se a câmara tirasse outra foto entretanto | O servidor web envia uma **cópia** da foto, que só é apagada no fim do envio |
| 9 | `WebServer.cpp` | O **stream** `/stream.mjpg` **ignorava a password** do login | Qualquer pessoa na tua rede via a impressora, mesmo com login ativo | Stream agora pede login como todas as outras páginas |
| 10 | `connect.cpp`, `cfg.cpp` | O **token** do Prusa Connect era escrito no log, que fica guardado no cartão SD | Fuga do token (quem tem o token pode enviar imagens para a tua conta) | O log mostra só o tamanho do token |
| 11 | `wifi_mngt.cpp` | Quando o servidor da Prusa estava em baixo, a câmara **desligava e religava o Wi-Fi a cada ~28 s** | Stream local interrompido, rede instável, por um problema que nem era do Wi-Fi | Espera progressiva (*backoff*) nos envios. O Wi-Fi só é religado após 10 falhas de ligação seguidas |

**Também corrigido:** mais dois casos de leitura fora do buffer (`AsyncBufferResponse` e
`AsyncFrameResponse` copiavam bytes a mais), e a reinicialização da câmara já não pode
acontecer a meio de uma captura (quando mudas a resolução no browser).

**Uma correção minha que estava errada:** na revisão disse que o LED de estado da XIAO estava
com a polaridade invertida. Ao verificar o código (`sys_led.cpp`) vi que o comportamento
original está certo (pisca curto, como diz a documentação). **Não foi alterado.**

---

## 3. Migração para o Prusa Camera Service

A Prusa criou um serviço novo para câmaras: <https://camera-service.prusa3d.com/docs/>.
Os endereços antigos (`connect.prusa3d.com/c/snapshot` e `/c/info`) ainda funcionam, mas a
documentação diz que código novo deve usar os novos. O que mudou:

| Regra da nova API | Antes | Agora |
|---|---|---|
| Endereços `/v1/snapshot` e `/v1/camera-info` em `camera-service.prusa3d.com` | `/c/snapshot` e `/c/info` em `connect.prusa3d.com` | Novos endereços. Configurações antigas com `connect.prusa3d.com` são **migradas automaticamente** |
| O certificado do servidor é da Google Trust Services | Só confiava no certificado ISRG Root X1 (Let's Encrypt), ou seja, **a ligação falharia** | Adicionados GTS Root R4 e GTS Root R1 (e mantido o ISRG Root X1). Verificado contra o servidor real |
| Dados da câmara com `device: {type: "ESP32", firmware_version, hardware_version}`. Campos desconhecidos são **recusados** | Formato antigo, que seria recusado (erro 422) | Formato novo, exatamente como na documentação |
| Nome da câmara com no máximo 32 caracteres | Até 40 | Cortado a 32 |
| `User-Agent` com nome e versão | `ESP32-CAM` | `PrusaESP32Cam/1.1.2-c1` |
| Só 4 intervalos: 10 s, 30 s, 60 s, 10 min. Máximo 6 envios por minuto | Qualquer valor de 10 a 240 s | Lista com os 4 valores. Valores antigos são convertidos para o mais próximo |
| Fotos com no máximo 1 MB | Sem verificação | Verificado antes de enviar |
| Cada código de erro tem a sua ação | Todos os erros tratados da mesma forma | Ver abaixo |

**O que a câmara faz em cada resposta do servidor:**

- **200**: foto aceite.
- **429 / 503 / 500 / sem ligação**: problema temporário. Espera progressiva:
  intervalo normal, depois o dobro, até ao máximo de 10 min. Volta ao normal após um envio bem-sucedido.
- **401 / 403 / 422 / outros 4xx**: o pedido está errado (por exemplo um token inválido).
  Repete só a cada 10 min, até mudares o token.
- **413**: foto demasiado grande. Mensagem a pedir para baixar a resolução ou a qualidade.

**Duas regras importantes do novo serviço:**

1. **Janela de 15 minutos:** depois de adicionares a câmara no Prusa Connect, a câmara tem de
   fazer o primeiro envio em **menos de 15 minutos**, senão o token deixa de funcionar.
   Ao gravar o token, a câmara envia logo os dados (não espera pelo intervalo).
2. **Fingerprint:** o primeiro envio liga o token à "impressão digital" da placa para sempre.
   O firmware calcula-a a partir do ID do chip e do MAC, por isso **sobrevive a um reset de
   fábrica**. Tem cerca de 48 caracteres (o limite é 16 a 64).

O token é validado na página web: tem de ter exatamente **20 caracteres A-Z, a-z, 0-9**.

---

## 4. O que são os "Core One defaults"

São os **valores de fábrica**, ou seja, as definições que a câmara usa na primeira vez que liga
(ou depois de um reset de fábrica). Escolhi-os a pensar na Core One:

| Definição | Original | Agora | Porquê |
|---|---|---|---|
| Resolução | 320×240 (QVGA) | **1024×768 (XGA)** | 320×240 é demasiado pequeno para ver detalhes da peça. XGA vê bem a peça e fica muito abaixo do limite de 1 MB |
| Qualidade JPEG | 10 | **12** | Ficheiros um pouco mais pequenos e envios mais rápidos, quase sem diferença visível (escala 10–63: menor = melhor) |
| Servidor | `connect.prusa3d.com` | **`camera-service.prusa3d.com`** | A nova API |
| Intervalo | 30 s | 30 s | Igual |
| Flash LED | desligado | desligado | A Core One tem iluminação LED própria na câmara de impressão. A XIAO nem tem flash na placa |
| Balanço de brancos | automático | **automático** | Na revisão sugeri fixar um modo para LEDs. Sem testar com a luz real da tua Core One, o automático é mais seguro. Podes mudar em *Advanced settings* depois de veres as imagens |

Podes mudar tudo depois na página web da câmara. Estes valores só contam no primeiro arranque
ou após um reset de fábrica.

---

## 5. O que é o "thermal guard" (proteção térmica)

**O problema:** a XIAO ESP32-S3 é minúscula e aquece muito (a própria documentação do
projeto avisa disso). Dentro da Core One, que é **fechada** e com a câmara de impressão
aquecida (ASA, ABS, PC...), o ar à volta já está quente. Com Wi-Fi e stream de vídeo a correr,
o chip pode sobreaquecer: imagens com ruído, Wi-Fi instável, reinícios ou dano a longo prazo.

**A solução:** a câmara lê a temperatura interna do chip a cada segundo e:

| Temperatura do chip | O que acontece |
|---|---|
| **≥ 75 °C** | Entra em proteção: o **stream é parado** (e não aceita novos, responde 503), e as fotos para o Prusa Connect passam a ser **no máximo 1 por minuto** |
| **≤ 68 °C** | Volta ao normal |

A diferença entre 75 e 68 (*histerese*) evita ligar e desligar a proteção a cada segundo
quando a temperatura anda perto do limite. Na página **System**, a temperatura aparece com o
aviso "TOO HOT" quando a proteção está ativa.

Estes limites estão em `mcu_cfg.h` (`THERMAL_THROTTLE_TEMP`, `THERMAL_RESUME_TEMP`,
`THERMAL_THROTTLE_INTERVAL`). É a temperatura **interna do chip**, que é sempre mais alta que a
do ar. Se nos teus testes a proteção ativar demasiado cedo ou tarde, ajusta estes valores.

**Recomendações físicas para a Core One:**
- Se possível, monta a câmara **fora da câmara aquecida** (a olhar pela porta ou pelo vidro), ou num ponto com ar a circular.
- Cola um **pequeno dissipador de alumínio** nas costas da XIAO.
- A estrutura de aço da Core One bloqueia o Wi-Fi: põe a **antena externa do lado de fora** da estrutura metálica.

---

## 6. PrusaLink: fotos frequentes só durante a impressão

**Opcional, desligado por defeito.** A Core One tem uma API local (PrusaLink) que diz se está a
imprimir. Com esta função ligada:

| Estado da impressora | Fotos para o Prusa Connect |
|---|---|
| A imprimir, em pausa, à espera de atenção, ou ocupada (calibração...) | No intervalo que escolheste (por exemplo 10 s) |
| Parada, ou impressão terminada | **1 foto a cada 10 min** |
| Começa ou termina uma impressão | **Foto imediata** (vês logo o resultado final) |
| Impressora não responde | Intervalo normal (como se a função estivesse desligada) |

Vantagens: menos calor, menos tráfego, e as fotos frequentes só quando interessam.

**Como configurar** (página **Camera**, secção *PrusaLink printer state*):
1. **Printer IP address**: o IP da Core One (no ecrã da impressora: *Settings → Network*).
2. **PrusaLink user**: normalmente `maker`.
3. **PrusaLink password**: no ecrã da impressora, *Settings → Network → PrusaLink*.
4. Liga o interruptor e carrega em **Save**. O campo *PrusaLink status* deve mostrar `OK`,
   e o *Printer state* o estado atual.

A câmara pergunta `GET /api/v1/status` a cada 10 s, com autenticação *digest* (a que o
PrusaLink usa). A password nunca é enviada para a página web.

---

## 7. Outras melhorias

- **Captura e envio no núcleo 1:** o ESP32-S3 tem 2 núcleos. Antes, tudo corria no núcleo 0
  (o mesmo do Wi-Fi) e o núcleo 1 estava parado. A tarefa das fotos, do TLS e do PrusaLink
  passou para o núcleo 1.
- **Só um cliente de stream de cada vez:** a câmara tem um só buffer de imagem, e dois streams
  "roubavam" frames um ao outro (o que causava os problemas de corrupção). Um segundo stream
  recebe a mensagem *"The stream is already open in another window"*.
- **Página principal:** mostra o **intervalo de envio atual** (que já inclui o backoff, o PrusaLink e a proteção térmica).
- **Página System:** mostra o intervalo de envio atual.
- **Mensagens de erro claras** na página (por exemplo, para o 403 explica a janela de 15 minutos).

---

## 8. Como compilar

O firmware compila com o **core ESP32 3.1.0** e as bibliotecas exatas que estão na pasta
`libraries/` deste repositório:

| Biblioteca | Versão | Licença |
|---|---|---|
| ArduinoJson | 7.3.0 | MIT |
| ArduinoUniqueID | 1.3.0 | MIT |
| AsyncTCP (mathieucarbou) | 3.3.1 | LGPL-3.0 |
| ESPAsyncWebServer (mathieucarbou) | 3.4.5 | LGPL-3.0 |
| DHTNEW | 0.5.2 | MIT |

> Usa **estas versões**. Versões mais recentes do core ESP32 ou do ESPAsyncWebServer
> podem não compilar ou comportar-se de forma diferente.

Antes de compilar, confirma em `ESP32_PrusaConnectCam/mcu_cfg.h` que só a XIAO está ativa
(já está assim neste repositório):

```c
#define CAMERA_MODEL_XIAO_ESP32_S3_CAM  true   // todos os outros: false
```

### Opção A: Arduino IDE 2

1. Instala o [Arduino IDE 2](https://www.arduino.cc/en/software).
2. **File → Preferences → Additional boards manager URLs**, e adiciona:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. **Tools → Board → Boards Manager**: procura **esp32 by Espressif Systems** e instala a
   **versão 3.1.0** (escolhe a versão na lista antes de carregar em Install).
4. Copia **as 5 pastas** de dentro de `libraries/` deste repositório para a pasta de
   bibliotecas do Arduino:
   - Windows: `Documentos\Arduino\libraries\`
   - Linux: `~/Arduino/libraries/`
   - macOS: `~/Documents/Arduino/libraries/`

   (Se já tiveres versões diferentes destas bibliotecas lá, remove-as primeiro.)
5. Abre `ESP32_PrusaConnectCam/ESP32_PrusaConnectCam.ino`.
6. Em **Tools**, escolhe:

   | Opção | Valor |
   |---|---|
   | Board | **XIAO_ESP32S3** |
   | USB CDC On Boot | Enabled |
   | CPU Frequency | 160MHz (WiFi) |
   | Core Debug Level | None |
   | USB DFU On Boot | Disabled |
   | Erase All Flash Before Sketch Upload | **Enabled só na primeira vez**, depois Disabled |
   | Events Run On | Core 1 |
   | Flash Mode | QIO 80MHz |
   | Flash Size | 8MB |
   | JTAG Adapter | Disabled |
   | Arduino Runs On | Core 1 |
   | USB Firmware MSC On Boot | Disabled |
   | Partition Scheme | **Default with spiffs (3MB APP/1.5MB SPIFFS)** |
   | PSRAM | **OPI PSRAM** |
   | Upload Mode | UART0 / Hardware CDC |
   | Upload Speed | 921600 |
   | USB Mode | Hardware CDC and JTAG |

7. Liga a XIAO por USB-C, escolhe a porta em **Tools → Port**, e carrega em **Upload**.

> Se ficares com "Erase All Flash" ligado, **apagas a configuração** (Wi-Fi, token) a cada
> upload. O token fica ligado à placa pela fingerprint, por isso apagar a flash não estraga o
> registo, mas terás de configurar o Wi-Fi e o token outra vez.

### Opção B: VS Code (com arduino-cli no terminal)

Funciona em Linux, macOS e Windows. Foi assim que testei a compilação.

1. Instala o [arduino-cli](https://arduino.github.io/arduino-cli/latest/installation/) (testado com a versão 1.1.1).
2. No terminal do VS Code, uma só vez:
   ```bash
   arduino-cli config init
   arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   arduino-cli core update-index
   arduino-cli core install esp32:esp32@3.1.0
   ```
   Não é preciso instalar bibliotecas: o script usa as da pasta `libraries/`.
3. Compilar:
   - **Linux / macOS:**
     ```bash
     ./build_xiao.sh
     ```
   - **Windows (PowerShell)**, a mesma coisa sem o script:
     ```powershell
     arduino-cli compile -b "esp32:esp32:XIAO_ESP32S3:USBMode=hwcdc,CDCOnBoot=default,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,CPUFreq=160,FlashMode=qio,FlashSize=8M,PartitionScheme=default_8MB,DebugLevel=none,PSRAM=opi,LoopCore=1,EventsCore=1,EraseFlash=none,JTAGAdapter=default" --libraries libraries --output-dir build/xiao-esp32-s3 ESP32_PrusaConnectCam
     ```
4. Os ficheiros `.bin` ficam em `build/xiao-esp32-s3/`. Para gravar, ver a secção 9.

Também podes gravar diretamente pelo arduino-cli (troca a porta):
```bash
arduino-cli upload -p /dev/ttyACM0 -b esp32:esp32:XIAO_ESP32S3 --input-dir build/xiao-esp32-s3 ESP32_PrusaConnectCam
```

### Se mudares a página web

Os ficheiros da página estão em `webpage/`. Depois de os alterares, gera o `WebPage.h`:
```bash
cd webpage && bash webpage_h_generator.sh
```

---

## 9. Como gravar o firmware na placa

Os ficheiros já compilados estão em **`firmware/xiao-esp32-s3/`**. Não precisas de compilar
para usar a câmara.

**Forma mais simples (browser, Chrome ou Edge):**
1. Abre <https://espressif.github.io/esptool-js/>.
2. Baud rate 921600, carrega em **Connect** e escolhe a porta da XIAO.
3. Na primeira vez carrega em **Erase Flash**.
4. Adiciona **um só ficheiro**: `ESP32_PrusaConnectCam.ino.merged.bin` no endereço **0x0**. Carrega em **Program**.

**Com esptool** (Linux/macOS/Windows), primeira gravação (apaga tudo):
```bash
esptool.py --chip esp32s3 -p /dev/ttyACM0 -b 921600 --before default_reset --after hard_reset \
  write_flash --erase-all --flash_mode qio --flash_size 8MB --flash_freq 80m \
  0x0 ESP32_PrusaConnectCam.ino.bootloader.bin \
  0x8000 ESP32_PrusaConnectCam.ino.partitions.bin \
  0x10000 ESP32_PrusaConnectCam.ino.bin
```
Para **atualizar sem perder a configuração**, usa o mesmo comando **sem** `--erase-all`.

Se a placa não for detetada: mantém o botão **B** (boot) carregado, liga o USB e larga o botão.

---

## 10. Primeira configuração

> **Faz por esta ordem:** primeiro o Wi-Fi da câmara, só depois crias a câmara no Prusa Connect.
> Assim não corres o risco de passar a janela de 15 minutos do token.

1. Depois de gravar, a câmara cria uma rede Wi-Fi **`ESP32_camera_` seguida de números** (password `12345678`).
2. Liga-te a essa rede e abre <http://192.168.0.1>.
3. Página **Wi-Fi**: escolhe a tua rede, põe a password e grava. A câmara liga-se à tua rede
   (o IP aparece na mesma página; também funciona `http://prusa-esp32cam.local`).
4. No **Prusa Connect** (<https://connect.prusa3d.com>): abre a tua Core One, vai a **Camera →
   Add new other camera**, e copia o **token**.
5. **Nos 15 minutos seguintes**, cola o token na página **Camera** da câmara e carrega em **Save**.
6. No Prusa Connect deve aparecer a primeira foto em poucos segundos.
7. **Recomendado:** página **Authentication**, ativa o login com uma password tua. Agora também protege o stream.
8. Opcional: configura o **PrusaLink** (secção 6).

**Reset de fábrica:** liga o pino **GPIO2** ao GND, liga a alimentação, espera 10 s até o LED
piscar e desliga o GPIO2 do GND.

---

## Limitações conhecidas

- **Não testado numa placa real.** Compila sem erros, mas o comportamento real (câmara, Wi-Fi,
  temperatura, PrusaLink) só se confirma na tua Core One.
- **PrusaLink na Core One:** a especificação do PrusaLink define `/api/v1/status` e a
  autenticação *digest*. Não confirmei que o firmware da Core One responde exatamente assim.
  Se o *PrusaLink status* não ficar `OK`, abre um issue com a mensagem que aparece.
- **Limites térmicos (75 / 68 °C)** são uma estimativa para a temperatura interna do chip.
  Ajusta-os depois de veres valores reais na página System.
- **Só um cliente de stream** de cada vez (ver secção 7).
- **"Update from cloud"** (na página System) descarrega o firmware **oficial** da Prusa, que
  **substitui** esta versão. Para atualizar esta versão, grava pela USB ou usa a atualização
  por ficheiro com o `.bin` deste repositório.
- Os intervalos possíveis são só 10 s, 30 s, 60 s e 10 min (regra da Prusa).

---

## 12. Ficheiros alterados

| Ficheiro | Alteração |
|---|---|
| `ESP32_PrusaConnectCam/camera.cpp`, `camera.h` | Bugs 1, 3, 7. Cópia segura da foto (`GetPhotoCopy`), semáforo em todas as operações com buffers |
| `ESP32_PrusaConnectCam/connect.cpp`, `connect.h`, `connect_types.h` | Bugs 2, 4, 5, 6, 10. Nova API Camera Service, backoff, códigos de erro |
| `ESP32_PrusaConnectCam/Certificate.h` | Certificados GTS Root R4 e R1 |
| `ESP32_PrusaConnectCam/WebServer.cpp` | Bugs 8, 9. Validação do token e do intervalo, rota `/set_prusalink`, novos campos de estado |
| `ESP32_PrusaConnectCam/WebStream.cpp` | Leituras fora do buffer, fim limpo do stream, proteção térmica |
| `ESP32_PrusaConnectCam/wifi_mngt.cpp` | Bug 11 |
| `ESP32_PrusaConnectCam/system.cpp`, `system.h` | Proteção térmica, cálculo do intervalo, PrusaLink |
| `ESP32_PrusaConnectCam/prusalink.cpp`, `prusalink.h` | **Novo**: cliente PrusaLink |
| `ESP32_PrusaConnectCam/cfg.cpp`, `cfg.h` | Gravação do intervalo e das definições PrusaLink, token mascarado no log |
| `ESP32_PrusaConnectCam/mcu_cfg.h` | Placa XIAO selecionada, versão 1.1.2-c1, constantes novas, Core One defaults |
| `ESP32_PrusaConnectCam/var.cpp`, `var.h` | Estado da proteção térmica |
| `ESP32_PrusaConnectCam/ESP32_PrusaConnectCam.ino` | Tarefa das fotos no núcleo 1, carregar a configuração PrusaLink |
| `webpage/*`, `ESP32_PrusaConnectCam/WebPage.h` | Lista de intervalos, secção PrusaLink, intervalo atual |
| `libraries/` | **Novo**: bibliotecas com as versões exatas |
| `build_xiao.sh` | **Novo**: script de compilação |
| `firmware/xiao-esp32-s3/` | **Novo**: firmware compilado |

Para ver todas as alterações linha a linha em relação ao original:
```bash
git diff 136c91c -- ESP32_PrusaConnectCam webpage
```

---

## 13. Trazer atualizações do projeto original

Este repositório tem o projeto da Prusa configurado como `upstream`:

```bash
git remote add upstream https://github.com/prusa3d/Prusa-Firmware-ESP32-Cam.git   # se ainda não existir
git fetch upstream
git merge upstream/master
```

Se houver conflitos nos ficheiros que alterámos, resolve-os mantendo as correções descritas acima.
