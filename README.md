# CookieDoc
Nettverksprosjekt - conflict-free replicated data types.
---
CookieDoc er et sanntidsverktøy for samskriving og delt cookie-teller som lar flere anonyme brukere samarbeide over lokalt nettverk. Prosjektet bruker CRDT-er for å la klienter gjøre endringer lokalt og samkjøre dem når forbindelsen kommer tilbake.

## Funksjonalitet

### Hosting

Hver bruker hoster sin egen server som lagrer dokumentet. Andre brukere kan koble seg til ved å skrive inn WebSocket-lenken din (for eksempel `ws://10.22.172.80:3030/ws`) og deretter redigere det samme dokumentet. Brukere kan se tilkoblingsstatusen sin og bruke `Ctrl+Z` for å angre lokale endringer.

### Dokumentredigering

Flere brukere kan redigere samme tekst samtidig. Dette innebærer både sletting og skriving hvor som helst i dokumentet. Brukerne kan også se hvor andre brukere har markøren sin. Hver server håndterer i dag ett dokument. Dersom en bruker mister nettverkstilkoblingen og senere kobler seg opp igjen, blir lokale endringer samkjørt med endringer som andre brukere gjorde mens brukeren var frakoblet.

### Cookie clicker

Flere brukere kan trykke på den samme cookie clickeren samtidig, og telleren blir samkjørt mellom klientene. Dersom en bruker mister nettverkstilkoblingen og senere kobler seg opp igjen, blir lokale endringer samkjørt med endringer som andre brukere gjorde mens brukeren var frakoblet.

### Setting av dokument tittel

Flere brukere kan endre tittel. Siste person som setter dokument tittel bestemmer den. Endringer blir altså ikke samkjørte.

## Fremtidig arbeid & nåværende svakheter

### Svakheter
- Klienter mottar kun markørposisjon når den endres, ikke når noen akkurat har koblet seg til dokumentet
- Vi mangler støtte for rik tekst. Vi har ikke støtte for fet tekst, ulike tekststørrelser, punktlister osv.
- Det er ikke mulig å ha flere dokumenter på samme server
- Det er ikke mulig å se versjonshistorikk av dokumenter
- Endring av dokument er ikke passordbeskyttet

### Framtidige endringer
- Få klienter til å sende markørposisjon når en annen klient blir med på å redigere et dokument, slik at nye klienter umiddelbart kan se hvor folk redigerer
- Støtte redigering av rik tekst, gjerne med metodene som brukes i Peritext
- Legge til støtte for flere dokumenter
- Legge til muligheten for å se versjonshistorikk på ulike dokumenter
- Legge til passordbeskyttelse på redigering og lesing av dokumenter

## Avhengigheter

### C++
- `nlohmann/json`: Et header-only JSON-bibliotek for C++. Det brukes til å parse og serialisere meldinger og snapshots mellom klient og server.
- `uSockets` og `uWebSockets`: Biblioteker for hendelsesdrevet nettverk og WebSocket-kommunikasjon i C++. De brukes til backend-serveren som tar imot og videresender CRDT-operasjoner.
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

1. klon repo eller unzip

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

Kjør `./run-tests.sh` i roten av prosjektet for å kjøre tester. Dette kan ta en stund.

## Arkitektur

### Overordnet

| Teknologivalg | Begrunnelse |
| --------- | --------- |
| Docker compose    | Vi valgte å bruke docker compose for å enkelt kjøre prosjektet på tvers av Fedora KDE, Windows & MacOS. Docker compose gjør det også enkelt å holde styr på porter & restart funksjonalitet |
| WebAssembly    | Vi tok I bruk WASM for å kunne dele CRDT logikk mellom backend & frontend istedenfor å måtte implementere det dobbelt i c++ og typescript. |
| Vue | Teamet var kjent med vue fra tideligere prosjekt sammen. Utover dette støtter vue komponenter, scoped css & kan enkelt kjøres med typescript |
| c++ | Vi valgte dette for å få en høyere karakter og fordi det er kjapt |


### Shared

Både frontend og backend bruker C++-koden under `shared/` for å håndtere CRDT-logikken likt. Frontenden bruker denne koden via WebAssembly, mens backenden kompilerer den direkte inn i serveren.

### Frontend

Vi har frontend under `frontend/` i roten av prosjektet.
Vi kjører frontenden med `frontend/Dockerfile`.
Frontenden bruker Vue og bygger CRDT WebAssembly fra delt C++-kode under Docker-builden.

### Backend

Vi har backend under `backend/` i roten av prosjektet.
Vi kjører backenden med `backend/Dockerfile`.
Backenden bygges med CMake og bruker delt CRDT/protokoll/server-kode fra `shared/`.

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
