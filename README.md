# E2Eye - Simple Cloud Storage

A minimal end-to-end encrypted cloud storage system with 5-user limit and 20MB storage per user.

## Features

- **5 User Limit**: Maximum 5 registered users
- **20MB Storage**: 20MB storage limit per user
- **End-to-End Encryption**: Files encrypted before storage
- **Cross-Platform**: Works on Linux and Windows
- **Simple TUI**: FTXUI-based terminal interface
- **Ngrok Ready**: Designed for temporary IP hosting

## Technology Stack

- **Networking**: cpp-httplib
- **Encryption**: libsodium
- **UI**: FTXUI
- **Build**: Simple CMake

## Quick Start

### Build
```bash
./build.sh
```

### Test
```bash
./test.sh
```

### Run Server
```bash
cd build
./server
```

### Run Client
```bash
cd build
./client [server_ip] [port]
```

## Usage with Ngrok

1. Start your server locally
2. Expose it with ngrok: `ngrok http 8080`
3. Share the ngrok URL with users
4. Users connect using: `./client [ngrok-url] 80`

## Test Users

Register up to 5 users with any username/password combination.
Each user gets 20MB storage space.

## API Endpoints

- `POST /register` - Register new user
- `POST /login` - User authentication
- `POST /upload` - Upload encrypted file
- `GET /files` - List user files
- `GET /download` - Download encrypted file
- `GET /storage` - Get storage usage info

## Security

- All files encrypted client-side before upload
- Password-based key derivation using libsodium
- Encrypted files stored on server
- No plaintext files accessible on server