# CookieDoc
[![Backend CI](https://github.com/TheodorSTTH/nettverksprosjekt/actions/workflows/backend-ci.yml/badge.svg)](https://github.com/TheodorSTTH/nettverksprosjekt/actions/workflows/backend-ci.yml)


[![Docker CI](https://github.com/TheodorSTTH/nettverksprosjekt/actions/workflows/docker-ci.yml/badge.svg)](https://github.com/TheodorSTTH/nettverksprosjekt/actions/workflows/docker-ci.yml)


**Deltakere**:
- Benjamin Kvello-Hansen
- Theodor Strøm Thrane
- Mateja Vasic

Nettverksprosjekt - conflict-free replicated data types.
---
CookieDoc er et sanntidsverktøy for samskriving og delt cookie-teller som lar flere anonyme brukere samarbeide over lokalt nettverk. Prosjektet bruker CRDT-er (Conflict-free Replicated Data Types) for å la klienter gjøre endringer lokalt og samkjøre dem automatisk når forbindelsen kommer tilbake, uten at noen sentral instans trenger å avgjøre hvem som "vant".

En CRDT er en datastruktur som kan eksistere på flere enheter samtidig og alltid konvergere til samme tilstand, uavhengig av rekkefølgen endringene kom inn i. Dette betyr at to brukere kan redigere samme dokument samtidig (også uten nettverksforbindelse) og at resultatet alltid blir konsistent når de kobler seg til igjen.

## Funksjonalitet

### Hosting

Hver bruker hoster sin egen server som lagrer dokumentet. Andre brukere kan koble seg til ved å skrive inn WebSocket-lenken din (for eksempel `ws://10.22.172.80:3030/ws`) og deretter redigere det samme dokumentet. 

OBS! Tilkobling fra andre maskiner

Hvis andre på samme nett ikke får koblet seg til din kjørende server, er det som regel ett av disse problemene:

**WSL2 (Windows)**: Hvis du kjører Docker inne i WSL2, eksponeres ikke porten automatisk på Windows-vertens LAN-interface. Du må sette opp portforwarding fra Windows til WSL2 og åpne brannmuren.

**Brannmur (alle plattformer)**: Operativsystemets brannmur kan blokkere innkommende tilkoblinger på portene 3030 og 8080.
Kommandoene under løser begge. Kjør de som gjelder din plattform.

Windows (WSL2 portforward + brannmur, kjør som administrator i CMD):

```
> netsh interface portproxy add v4tov4 listenport=3030 listenaddress=0.0.0.0 connectport=3030 connectaddress=127.0.0.1

> netsh interface portproxy add v4tov4 listenport=8080 listenaddress=0.0.0.0 connectport=8080 connectaddress=127.0.0.1

> netsh advfirewall firewall add rule name="CookieDoc Backend" protocol=TCP dir=in localport=3030 action=allow

> netsh advfirewall firewall add rule name="CookieDoc Frontend" protocol=TCP dir=in localport=8080 action=allow
```

##### macOS (Terminal):

```bash
sudo /usr/libexec/ApplicationFirewall/socketfilterfw --setglobalstate off
```

##### Linux med `ufw` (Ubuntu/Debian):

```bash
sudo ufw allow 3030/tcp
sudo ufw allow 8080/tcp
```

##### Linux med `iptables`:

```bash
sudo iptables -A INPUT -p tcp --dport 3030 -j ACCEPT
sudo iptables -A INPUT -p tcp --dport 8080 -j ACCEPT
```

### Dokumentredigering

Flere brukere kan skrive og slette i samme dokument samtidig. Du kan også se hvor de andre har markøren sin. Markøren er ikke en del av CRDT-en. Den sendes bare mellom klientene som en kort melding, og lagres ikke. Grunnen er at en markør bare skal vise hvor en bruker er akkurat nå — den trenger ikke huskes over tid. Hver server håndterer ett dokument om gangen.

### Offline-redigering

Dersom en bruker mister forbindelsen til serveren kan vedkommende fortsette å redigere dokumentet lokalt. Tekstendringer, tittelendringer og cookie-klikk køes opp i klientens minne (og persisteres til `localStorage` slik at de overlever en page reload). Når forbindelsen kommer tilbake forsøker klienten automatisk å koble til igjen med eksponentiell backoff, og spiller deretter av den lokale køen mot serveren. CRDT-en sørger for at alle klienter konvergerer til samme tilstand uavhengig av hvor lenge brukeren var frakoblet eller hvilke endringer andre brukere gjorde i mellomtiden.

For å gjøre offline-modus enklere å teste finnes en "Go offline / Go online"-knapp i serverpanelet. Den lukker WebSocket-tilkoblingen og blokkerer auto-reconnect inntil den trykkes igjen. Knappen ble lagt til fordi nettleserens innebygde Offline-modus i utviklerverktøyene er upålitelig for WebSocket-tilkoblinger, særlig i Incognito og Firefox, hvor eksisterende tilkoblinger ikke alltid lukkes.

### Cookie clicker

Flere brukere kan trykke på den samme cookie clickeren samtidig, og telleren blir samkjørt mellom klientene. Klikk gjort mens brukeren er frakoblet køes opp lokalt og samkjøres ved reconnect på samme måte som tekstredigering.

### Setting av dokument tittel

Flere brukere kan endre dokumenttittelen samtidig. I motsetning til selve teksten, som flettes tegn-for-tegn via RGA, er tittelen modellert som en LWW-Register. hele tittelen erstattes som en verdi, og ved samtidige endringer vinner skrivet med høyest Lamport-tidsstempel. Hvis to klienter setter tittelen i nøyaktig samme øyeblikk, brukes peer_id som deterministisk tiebreaker, slik at alle replikaer ender på samme verdi.

## Fremtidig arbeid & nåværende svakheter

### Svakheter
- Klienter mottar kun markørposisjon når den endres, ikke når noen akkurat har koblet seg til dokumentet
- Vi mangler støtte for rik tekst. Vi har ikke støtte for fet tekst, ulike tekststørrelser, punktlister osv.
- Det er ikke mulig å ha flere dokumenter på samme server
- Det er ikke mulig å se versjonshistorikk av dokumenter
- Endring av dokument er ikke passordbeskyttet
- Tombstones (slettede tegn) samles opp uten opprydding; minnebruken vokser over tid med antall redigeringer i stedet for faktisk tekststørrelse

### Framtidige endringer
- Få klienter til å sende markørposisjon når en annen klient blir med på å redigere et dokument, slik at nye klienter umiddelbart kan se hvor folk redigerer
- Støtte redigering av rik tekst, gjerne med metodene som brukes i Peritext
- Legge til støtte for flere dokumenter
- Legge til muligheten for å se versjonshistorikk på ulike dokumenter
- Legge til passordbeskyttelse på redigering og lesing av dokumenter
- Implementere garbage collection av tombstones via causal stability for å hindre at minnebruken vokser ubegrenset

## Avhengigheter

### C++
- `nlohmann/json`: Et header-only JSON-bibliotek for C++. Det brukes til å parse og serialisere meldinger og snapshots mellom klient og server.
- `uSockets` og `uWebSockets`: Biblioteker for hendelsesdrevet nettverk og WebSocket-kommunikasjon i C++. De brukes til backend-serveren som tar imot og videresender CRDT-operasjoner. Vi valgte `uWebSockets` fremfor alternativer som `Boost.Beast` og `libwebsockets` fordi det har et minimalt og rett-på-sak API, håndterer HTTP-upgrade, framing og keepalive automatisk, og lar oss kjøre alle tilkoblinger på en event-loop uten egen trådhåndtering. Det passer godt til vårt use-case med få samtidige klienter og korte JSON-meldinger.
- `Catch2`: Et testrammeverk for C++. Det brukes til testing av CRDT-, protokoll- og dokumentlogikken.

### Frontend
- `Vue`: Et JavaScript-rammeverk for komponentbaserte brukergrensesnitt. Det brukes til editorgrensesnittet og tilstandshåndteringen i frontenden.
- `Vite`: Et raskt byggverktøy og en utviklingsserver for moderne frontendprosjekter. Det brukes til å bygge og servere frontendkoden.
- `TypeScript`: Et typesystem over JavaScript. Det brukes til typesikring i frontendkoden og rundt WebAssembly-grensesnittet.
- `npm-run-all2`: Et lite verktøy for å kjøre flere npm-skript i rekkefølge eller parallelt. Det brukes til å koordinere wasm-bygging, type-sjekk og frontend-bygging.

### WASM / build-infrastruktur
- `emscripten/emsdk`: Et verktøysett for å kompilere C++ til WebAssembly. Det brukes til å bygge CRDT-koden i `shared/` slik at samme logikk kan kjøres i nettleseren.
- `emscripten/bind.h` og `emscripten/val.h`: Hjelpebiblioteker fra Emscripten for å koble C++ og JavaScript sammen. De brukes i `rga_wasm_bindings.cpp` for å eksponere C++-grensesnittet til frontenden.
- `http-server`: En enkel statisk filtjener for Node.js. Den brukes til å serve den bygde frontenden i `dist/`.

### Backend
- `CMake`: Et byggsystem for C og C++. Det brukes til å konfigurere og bygge backend, tester og eksterne C++-avhengigheter.
- `Docker`: En plattform for å bygge og kjøre containere. Det brukes til å bygge og kjøre både frontend og backend likt på tvers av maskiner.
- `Zlib`: Et komprimeringsbibliotek. Det trekkes inn som systemavhengighet fordi WebSocket-stakken i backend er bygget mot det.

## Installasjonsinstruksjoner

### Forutsetninger

For å kjøre prosjektet trenger du følgende installert lokalt:

- **Docker** og **Docker Compose** (alt av bygging og kjøring skjer i containere, så du trenger ikke installere C++, CMake, Emscripten eller Node selv). Se [docs.docker.com/get-docker](https://docs.docker.com/get-docker/).
- **Git**, dersom du vil klone repoet i stedet for å pakke ut en zip.

### Hente prosjektet

Klon repoet:

```bash
git clone https://github.com/TheodorSTTH/nettverksprosjekt.git
cd nettverksprosjekt
```

Alternativt, pakk ut den utleverte zip-filen og naviger til prosjektroten.

## Instruksjoner for å bruke løsningen

1. Kjør `docker compose up -d --build`
2. Besøk `http://localhost:8080/` i nettleseren for å nå websiden til applikasjonen
3. (Valgfri) Skriv inn `ws://<lokal-ip-til-annen-bruker->:3030/ws` i serverfeltet for å redigere data hos en annen bruker
4. Rediger tekst, endre tittel, klikk på cookie og lev livet

### Justere forsinkelse på serveroppdateringer

Backenden har en kunstig forsinkelse før den sender CRDT-oppdateringer videre til klientene. Dette gjør det enklere å se hvordan CRDT-ene håndterer samtidige endringer og konflikter.

Verdien kan endres i `backend/server/websocket_server.cpp`:

```cpp
constexpr auto kClientUpdateInterval = std::chrono::seconds(5);
```

Sett tallet høyere for å gjøre oppdateringene tregere og lettere å observere, for eksempel:

```cpp
constexpr auto kClientUpdateInterval = std::chrono::seconds(10);
```

Sett verdien til `0` for at applikasjonen skal oppføre seg mer som live updates:

```cpp
constexpr auto kClientUpdateInterval = std::chrono::seconds(0);
```

Etter at verdien er endret må backend bygges og startes på nytt:

```bash
docker compose up -d --build backend
```

## Hvordan kjøre tester

### Kjøre tester lokalt

Kjør `./run-tests.sh` i roten av prosjektet for å kjøre hele testsuiten. Dette kan ta en stund første gang, siden CMake må laste ned og bygge `Catch2`.

Testene kjøres også automatisk i CI på hver push og pull request mot `dev` og `main`. Se `backend-ci.yaml` for detaljer.

### Rammeverk

Vi bruker **Catch2 v3** for alle C++-tester. Det er et header-only testrammeverk uten ekstern oppsett, integrert i prosjektet via CMake.

### Hva testes

Testene er delt mellom `shared/tests/` (ren CRDT-logikk) og `backend/tests/` (server- og protokollogikk):

- **`test_rga.cpp`**: dekker RGA, både basis-operasjoner, edge cases (sletting av første/siste tegn, innsetting etter tombstones, Unicode), idempotens (samme operasjon to ganger endrer ikke tilstanden), kommutativitet (A før B gir samme resultat som B før A), og konvergens (concurrent inserts på samme parent gir samme resultat på alle replikaer uavhengig av ankomstrekkefølge).
- **`test_lww_register.cpp`**: dekker LWWRegister, inkludert tiebreaker på `peer_id` når tidsstempler er like, og at Lamport-klokken oppdateres riktig ved fjern-skriving.
- **`test_g_counter.cpp`**: dekker GCounter, med vekt på at merge tar maks per stabil replika, og at operasjonen er kommutativ, idempotent og assosiativ.
- **`test_document_state.cpp`**: dekker server-laget. Tester at insert med ukjent parent avvises, at snapshot lagres og lastes korrekt (inkludert tombstones og Unicode), og at cookie-telleren persisteres på tvers av omstart.
- **`test_protocol.cpp`**: dekker JSON-serialisering. Round-trip-tester for alle meldingstyper, validering av innkommende meldinger, og avvisning av malformerte meldinger.


## Arkitektur

### Overordnet

| Teknologivalg | Begrunnelse |
| --------- | --------- |
| Docker compose    | Vi valgte å bruke docker compose for å enkelt kjøre prosjektet på tvers av Fedora KDE, Windows & MacOS. Docker compose gjør det også enkelt å holde styr på porter & restart funksjonalitet |
| WebAssembly    | Vi tok i bruk WASM for å kunne dele CRDT logikk mellom backend & frontend istedenfor å måtte implementere det dobbelt i C++ og typescript. |
| Vue | Teamet var kjent med vue fra tidligere prosjekt sammen. Utover dette støtter vue komponenter, scoped css & kan enkelt kjøres med typescript |
| C++ | Vi valgte C++ fordi det er ett kjapt programmerings språk og gruppen ville utfordre seg |

CookieDoc er bygget som en klient-server-applikasjon der hver bruker hoster sin egen server, og andre brukere kobler seg til via WebSocket. Serveren holder den autoritative tilstanden til dokumentet, mens hver klient holder sin egen lokale kopi for å kunne redigere uten å vente på nettverket.

### Delt CRDT-kode

CRDT-logikken ligger under `shared/` og er skrevet en gang i C++. Den samme koden kjøres to steder:

- Backend kompilerer den direkte inn i serveren via CMake.
- Frontend kompilerer den til WebAssembly med Emscripten, og kaller inn i den fra TypeScript via bindings i `shared/wasm/rga_wasm_bindings.cpp`.

Dette gir en felles implementasjon av merge-logikken på tvers av klient og server.

### Moduler

Koden er delt inn slik at CRDT-logikken er fullstendig isolert fra nettverk og I/O:

```
shared/
  crdt/      ren CRDT-logikk, ingen avhengighet til JSON eller nettverk
  wasm/      bindings som eksponerer CRDT-en til JavaScript via Emscripten

backend/
  protocol/  serialiserer CRDT-operasjoner til/fra JSON, validerer meldinger
  server/    WebSocket-server, tilkoblinger, broadcast, persistens
```

- `shared/crdt/` inneholder de tre CRDT-typene: `RGA` (tekst), `LWWRegister` (tittel) og `GCounter` (cookie-teller). Disse vet ingenting om nettverk eller serialisering, og brukes uendret både av backend og frontend.
- `shared/wasm/` inneholder Emscripten-bindings som lar frontend kalle inn i C++-koden fra TypeScript.
- `backend/protocol/` oversetter mellom CRDT-operasjoner og JSON-meldinger, og validerer innkommende meldinger fra klientene.
- `backend/server/` bruker `uWebSockets` til å ta imot tilkoblinger, og `DocumentState` til å holde den samlede dokumenttilstanden (RGA + LWW + GCounter + snapshot-fil).

Avhengighetsretningen er enveis: `backend/server/` bruker `backend/protocol/` og `shared/crdt/`. `shared/crdt/` bruker ingenting annet. Den er ren logikk og kan testes uten å starte en server eller en nettleser.

### Dataflyt for en redigering

1. Bruker trykker en tast i nettleseren.
2. Frontend kaller inn i WASM, som genererer en `InsertOp` lokalt og oppdaterer den lokale RGA-en umiddelbart.
3. Frontend sender operasjonen som JSON over WebSocket til serveren.
4. Serveren tar låsen, lar `DocumentState` anvende operasjonen på sin RGA, og skriver et snapshot til disk.
5. Serveren sender operasjonen videre til alle andre tilkoblede klienter.
6. Hver mottakende klient anvender operasjonen på sin lokale RGA, og brukergrensesnittet oppdateres.

### Samtidighet på serveren

Serveren benytter `uWebSockets` sin enkelttrådede event-loop `uWS::App`, som multiplekser alle WebSocket-tilkoblinger på én tråd. I tillegg kjører én bakgrunnstråd som forsinker behandlingen av innkommende operasjoner med `kClientUpdateInterval` for å gjøre samtidig redigering observerbar under demonstrasjon, før den leverer batchen tilbake til event-loopen via `loop_->defer`. `DocumentState` er beskyttet av en `std::mutex` for å gardere mot fremtidige utvidelser som introduserer flere tråder.

### Persistens

Hver gang en operasjon anvendes, lagrer serveren et fullt JSON-snapshot av dokumenttilstanden (noder, tombstones, tittel, cookie-teller) til disk. Når serveren starter på nytt, leses snapshotet inn igjen.

### Offline-redigering

Når en klient mister tilkoblingen, fortsetter den å redigere mot sin lokale RGA-kopi. Operasjonene legges i kø. Når tilkoblingen kommer tilbake, sendes køen til serveren, som anvender dem som vanlige operasjoner. CRDT-ens matematiske egenskaper garanterer at alle replikaer konvergerer til samme tilstand uansett hvilken rekkefølge operasjonene kommer i.

### Hvorfor denne arkitekturen
Det viktigste arkitekturvalget i prosjektet er at CRDT-logikken er skrevet en gang i C++ under shared/, og brukes uendret både av backend og frontend. Backend kompilerer den direkte inn i serveren, mens frontend kompilerer den til WebAssembly via Emscripten og kaller inn i den fra TypeScript. Alternativet ville vært å implementere RGA, LWWRegister og GCounter to ganger, en gang i C++ for serveren og en gang i TypeScript for nettleseren. Det ville duplisert logikken, gjort det vanskelig å holde merge-semantikken konsistent over tid, og økt risikoen for at klient og server blir forksjellig ved en endring som bare blir gjort ett sted.

Denne oppdelingen følger prinsippet om Separation of Concerns (SoC): shared/crdt/ har ett ansvar, som er å implementere CRDT-operasjonene riktig, og vet ingenting om nettverk, JSON eller brukergrensesnitt. Den følger også Single Responsibility-prinsippet fra SOLID, ved at hver modul (crdt/, protocol/, server/) har en klart avgrenset rolle, og at avhengighetene går en vei. CRDT-koden blir dermed et selvstendig bibliotek som kan testes isolert og gjenbrukes på tvers av plattformer, uten at det krever endringer i hvordan serveren eller frontenden er bygget.

### Svakheter ved arkitekturen

Arkitekturen har også noen tydelige svakheter som er verdt å nevne:

- **WASM-bindings krever ekstra glue-kode.** Hver C++-klasse som skal nås fra TypeScript må eksponeres manuelt i `rga_wasm_bindings.cpp`. Endrer vi CRDT-API-et, må vi også oppdatere bindings. Dette er en pris vi betaler for å slippe å duplisere CRDT-logikken.
- **En lås rundt hele dokumentet.** All serverarbeid går gjennom samme `std::mutex` rundt `DocumentState`. Dette er enkelt og trygt, men skalerer dårlig dersom mange brukere redigerer samtidig.
- **Tombstones samles opp uten opprydding.** Slettede tegn blir liggende som tombstones for at konkurrerende operasjoner skal kunne finne foreldrenoden sin. Vi har ikke implementert garbage collection, så minnebruken vokser med antall redigeringer over tid, ikke med faktisk tekststørrelse.
- **En server håndterer kun ett dokument.** Dette er bakt inn i `DocumentState`-strukturen, og er ikke noe som kan konfigureres uten en større endring.

## Continuous Integration
CI for dette prosjektet består i hovedsak av to .yaml workflow filer. Begge workflow filene trigges enten på push eller en PR til branchene `dev` og `main`.

### backend-ci.yaml
Ansvarlig for å kjøre backend workflow. Workflow filen er delt opp i følgende steg:

**1:**
    `actions/checkout@v4` kloner repositoriet til github sin egne actions runner `ubuntu-latest`, på den branchen som trigget workflowen.

**2:**
    installerer nødvendige dependencies på github runners sin instansierte VM for jobben. Dependencies er definert i `CMakeLists.txt`, hvor det da kjøres en command for å laste ned alle dependencies fra denne filen:

    
**3:**
    cacher nedlastede C++ dependencies i `.cmake-deps` basert på en hash av `CMakeLists.txt`. Treffer cachen dersom filen er uendret.

**4:**
    kjører CMake for å generere ninja build-filer med tester aktivert (`-DBUILD_TESTS=ON`) og release-optimalisering.

**5:**
    kompilerer backend og testsuiten med den genererte build-konfigurasjonen.

**6:**
    kjører alle Catch2-tester via `ctest`. Printer full output ved feil.


### docker-ci.yaml
Ansvarlig for å kjøre en docker workflow, som sørger for å teste at oppspinning av containere skjer feilfritt og uten problem. Den er delt opp i følgende steg:

**1:**
    kloner repositoriet til runneren på samme måte som for `backend-ci.yaml`

**2:**
    setter opp Docker Buildx. Dette gjøres ved å aktivere et utvidet Docker-byggmiljø som støtter mer avanserte build funksjoner.

**3:**
    bygger backend Docker-imaget. Dette gjøres ved å kjøre Dockerfilen for backend og lagre imaget lokalt på runneren.

**4:**
    bygger frontend Docker-imaget. Gjøres på samme måte som backend, men med Dockerfilen for frontend.

**5:**
    verifiserer at WASM-filene ble produsert under bygget. Dette gjøres ved å starte frontend-imaget og sjekke at de forventede filene faktisk finnes.

**6:**
    starter alle containere. Dette gjøres ved å kjøre `docker compose up -d` som spinner opp både frontend og backend i bakgrunnen.

**7:**
    venter på at containerne er klare. Dette gjøres ved å gi dem noen sekunder før testene starter, slik at portene rekker å bli tilgjengelige.

**8:**
    sjekker at ingen container krasjet ved oppstart. Dette gjøres ved å se etter feilstatuser i `docker compose ps`-outputen.

**9:**
    tester at frontend svarer på HTTP og at backend aksepterer WebSocket-tilkoblinger. Dette gjøres ved å sende forespørsler mot begge endepunkter og verifisere at de svarer som forventet.

**10:**
    dumper container-logger dersom noe feilet. Dette gjøres automatisk kun ved feil, slik at man enkelt kan se hva som gikk galt.

**11:**
    river ned alle containere. Dette gjøres alltid til slutt, uavhengig av om workflowen lyktes eller ikke.

## AsyncAPI

WebSocket-protokollen er dokumentert i `asyncapi.yaml`.

Installer AsyncAPI-verktøyene med:

```bash
npm install
```

Valider spesifikasjonen med:

```bash
npm run asyncapi:validate
```

Forhåndsvis dokumentasjonen lokalt med:

```bash
npm run asyncapi:preview
```

Generer HTML-dokumentasjon med:

```bash
npm run asyncapi:generate
```

## Kom i gang

Kjør hele prosjektet fra prosjektroten med:

```bash
docker compose up --build
```

Åpne appen i nettleseren:

- Frontend: `http://localhost:8080`
- Backend WebSocket: `ws://localhost:3030/ws`

Kjør i bakgrunnen:

```bash
docker compose up -d --build
```

Stopp prosjektet:

```bash
docker compose down
```

Se logger på backend med:

```bash 
docker compose logs backend
```

## Kilder

CRDT-algoritmene er implementert fra bunnen basert på følgende kilder:

- Kleppmann, M. (2020). *CRDTs: The Hard Parts*. Foredrag, tilgjengelig på 
  [YouTube](https://www.youtube.com/watch?v=x7drE24geUw). 
  Hovedkilde for designvalg rundt RGA, tombstones og tie-breaking.

- Kleppmann, M. (2023). *"New algorithms for collaborative text editing"*. 
  Foredrag på Strange Loop 2023, tilgjengelig på 
  [YouTube](https://www.youtube.com/watch?v=Mr0a5KyD6BU). 
  Oppdatert supplement til "The Hard Parts" med nyere utvikling innen kollaborativ tekstredigering.

- Wikipedia: *[Conflict-free replicated data type](https://en.wikipedia.org/wiki/Conflict-free_replicated_data_type)*.
  Brukt som oversiktskilde for klassifisering av CRDT-er og merge-semantikk.

- Sypytkowski, B. *["An introduction to state-based CRDTs"](https://www.bartoszsypytkowski.com/the-state-of-a-state-based-crdts/)*.
  Praktisk gjennomgang av state-based CRDT-er med kodeeksempler. Brukt som referanse for
  G-Counter sin merge-semantikk (`max` per peer).

- Sen, A. (2023). *["Understanding CRDTs and LWW with Swift — A Deep Dive"](https://blog.stackademic.com/understanding-crdts-and-lww-with-swift-a-deep-dive-dc98205e3aae)*, Stackademic, 2. oktober 2023.
  Brukt som referanse for Last-Writer-Wins-register og 
  peer_id som deterministisk tiebreaker.

- Lazaroff, J. *["An Interactive Intro to CRDTs"](https://jakelazaroff.com/words/an-interactive-intro-to-crdts/)*.
  Pedagogisk forklaring av RGA brukt som supplement til Kleppmann.

### Teknisk dokumentasjon

- [uWebSockets](https://github.com/uNetworking/uWebSockets)  WebSocket-serveroppsett.
- [Emscripten Embind](https://emscripten.org/docs/porting/connecting_cpp_and_javascript/embind.html)  C++ og TypeScript-bindings.
- [nlohmann/json](https://github.com/nlohmann/json)  JSON-serialisering.
- [cppreference.com](https://en.cppreference.com/)  Oppslagsverk for C++-standardbiblioteket.
- [Vue.js Guide](https://vuejs.org/guide/)  Offisiell dokumentasjon for Vue 3, brukt under frontend-utviklingen.
- [W3Schools](https://www.w3schools.com/)  Referanse for HTML, CSS og DOM-API-er i frontend.
- [Docker-dokumentasjon](https://docs.docker.com/)  Referanse for Dockerfile- og Docker Compose-oppsett.