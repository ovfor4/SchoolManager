# SchoolManager

SchoolManager is a school grade-management app with a C++23 backend, SQLite storage, and a React frontend.

## Install Dependencies

Frontend npm dependencies are managed in `frontend/package.json` and locked by
`frontend/package-lock.json`.

Install backend system packages, Node.js/npm, and frontend npm packages on
Ubuntu 24.04:

```bash
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.40.5/install.sh | bash
\. "$HOME/.nvm/nvm.sh"
nvm install 24
```


```bash
sudo apt-get update && \
sudo apt-get install -y \
  cmake \
  libdrogon-dev \
  libsqlite3-dev \
  libjsoncpp-dev \
  libmariadb-dev \
  libhiredis-dev \
  libyaml-cpp-dev \
  g++-14\
  build-essential\
  uuid-dev\
  libpq-dev\
  libbrotli-dev\
  libcmark-gfm-dev\
  libhpdf-dev\
  libcmark-gfm-extensions-dev

npm --prefix frontend ci
```

## Build

Backend:

```bash
cmake -S . -B build
cmake --build build -j2
```

Frontend:

```bash
cd frontend
npm run build
```

## Run

Start the backend from the repository root:

```bash
SM_DATA_ROOT=./runtime-data \
SM_OPENAPI_PATH=backend/openapi/openapi.json \
SM_PORT=8080 \
./build/backend/schoolmanager_backend
```

Start the frontend:

```bash
cd frontend
npm run dev
```

Open:

```text
http://localhost:5173/
```

The frontend uses `http://localhost:8080` by default. Override these when needed:

```bash
VITE_API_BASE_URL=http://localhost:8080
VITE_WS_BASE_URL=ws://localhost:8080
```

## Data Location

By default, local data is written to:

```text
./runtime-data
```

Change it with `SM_DATA_ROOT`.

## API Contract

The OpenAPI file is here:

```text
backend/openapi/openapi.json
```

When the backend is running, it is also available at:

```text
http://localhost:8080/api/openapi.json
```


# Note

⚠️ NOT COMPATIBLE WITH WINDOWS

SUPPORT LINUX ONLY (RECOMMEND DEBIAN/UBUNTU/CENTOS/REDHAT)

Copyright (C) _ov4

This is free software; see the source for copying conditions. There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

under GNU AFFERO GENERAL PUBLIC LICENSE (AGPL) Version 3
