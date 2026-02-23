# FTP Client (C)

A C-based FTP client project that evolves in 3 phases and ends with an FTPS (FTP over TLS) implementation.

- `phases/phase1.c`: Basic TFTP-style file download over UDP (port 69).
- `phases/phase2.c`: Plain FTP control/data channels over TCP (port 21 + PASV data port).
- `phases/phase3.c`: FTPS with TLS on control and data channels.
- `main.c`: Final FTPS client (same core behavior as phase 3, with reusable helpers).

The client currently connects to `127.0.0.1`, logs in with:
- User: `usuario_prueba`
- Password: `password123`

## Requirements

- A C compiler (`gcc`).
- OpenSSL development libraries (required for `main.c` and `phases/phase3.c`).
- A local FTP/FTPS server listening on localhost (port `21`) and supporting passive mode (`PASV`).
- For phase 1 only: a TFTP server on localhost (port `69`) serving `prueba.txt`.
- Recommended: Docker, to run the FTP/FTPS (and optionally TFTP) server in a reproducible environment.
- Recommended: Wireshark, to inspect and verify FTP/TFTP/FTPS traffic during testing.

## Build and Run

From the project root:

```bash
make
./main
```

Or in one step:

```bash
make run
```

Clean binary:

```bash
make clean
```

## Runtime Usage

When the client is running, you can enter FTP commands interactively, for example:

- `LIST`
- `RETR filename.ext`
- `STOR filename.ext`
- `QUIT`

Notes:
- `NLST` and `PORT` are intentionally not supported in phases 2/3 and `main.c`.
- Transfers use passive mode (`PASV`).
