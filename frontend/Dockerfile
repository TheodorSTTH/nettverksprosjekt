FROM emscripten/emsdk:3.1.74 AS wasm

WORKDIR /src

COPY shared ./shared
COPY frontend/public ./frontend/public

RUN mkdir -p frontend/public/wasm \
    && emcc shared/wasm/rga_wasm_bindings.cpp shared/crdt/RGA.cpp \
      -I shared \
      --bind \
      -std=c++17 \
      -O2 \
      -sMODULARIZE=1 \
      -sEXPORT_ES6=1 \
      -sEXPORT_NAME=createCrdtWasmModule \
      -sENVIRONMENT=web \
      -sALLOW_MEMORY_GROWTH=1 \
      -sFILESYSTEM=0 \
      -o frontend/public/wasm/crdt_wasm.js

FROM node:lts-alpine AS build

WORKDIR /app

COPY frontend/package*.json ./
RUN npm ci

COPY frontend ./
COPY --from=wasm /src/frontend/public/wasm ./public/wasm

RUN npm run type-check \
    && npm run build-only

FROM node:lts-alpine AS runtime

RUN npm install -g http-server

WORKDIR /app
COPY --from=build /app/dist ./dist

EXPOSE 8080
CMD ["http-server", "dist", "-p", "8080", "-a", "0.0.0.0", "-c-1"]
