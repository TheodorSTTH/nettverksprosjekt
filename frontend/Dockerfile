# syntax=docker/dockerfile:1

FROM emscripten/emsdk:latest AS wasm-builder
WORKDIR /src
COPY wasm ./wasm
COPY --from=shared . ./shared
RUN mkdir -p /out && em++ -std=c++20 --bind -O3 -Ishared wasm/GCounterBindings.cpp shared/crdt/GCounter.cpp -o /out/crdt.mjs -s MODULARIZE=1 -s EXPORT_ES6=1 -s ENVIRONMENT=web -s FILESYSTEM=0 -s ALLOW_MEMORY_GROWTH=1

FROM node:lts-alpine

# install simple http server for serving static content
RUN npm install -g http-server

# make the 'app' folder the current working directory
WORKDIR /app

# copy both 'package.json' and 'package-lock.json' (if available)
COPY package*.json ./

# install project dependencies
RUN npm install

# copy project files and folders to the current working directory (i.e. 'app' folder)
COPY . .
RUN mkdir -p public/wasm
COPY --from=wasm-builder /out/crdt.mjs ./public/wasm/crdt.mjs
COPY --from=wasm-builder /out/crdt.wasm ./public/wasm/crdt.wasm

# build app for production with minification
RUN npm run build

EXPOSE 8080
CMD [ "http-server", "dist", "-P", "http://localhost:8080?" ]
