# SchoolManager

SchoolManager is a school grade-management app with a C++23 backend, SQLite storage, and a React frontend.

## Install Dependencies

Backend packages on Ubuntu 24.04:

```bash
sudo apt-get update
sudo apt-get install -y \
  cmake \
  libdrogon-dev \
  libsqlite3-dev \
  libjsoncpp-dev \
  libmariadb-dev \
  libhiredis-dev \
  libyaml-cpp-dev
```

Frontend packages:

```bash
cd frontend
npm install
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
