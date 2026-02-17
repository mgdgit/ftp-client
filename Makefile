# ============================================================
# Makefile para el cliente FTP seguro (FTPS) en macOS
# ============================================================
# Un Makefile es un archivo que le dice al programa "make" cómo
# compilar y ejecutar tu código. En lugar de escribir el comando
# largo de gcc cada vez, solo escribes "make" o "make run".
#
# Uso:
#   make       → Solo compila el programa.
#   make run   → Compila y ejecuta el programa.
#   make clean → Elimina el archivo compilado (binario).
# ============================================================

# CC define el compilador que se utilizará.
# gcc (GNU Compiler Collection) es el compilador estándar de C.
CC = gcc

# SSL_FLAGS contiene las banderas necesarias para compilar con OpenSSL.
# -I  → Le dice al compilador DÓNDE buscar los archivos .h (headers) de OpenSSL.
# -L  → Le dice al compilador DÓNDE buscar las librerías compiladas de OpenSSL.
# -lssl -lcrypto → Le dice al compilador que use las librerías libssl y libcrypto de OpenSSL.
#
# Las rutas cambian según el sistema operativo:
#
# ┌─────────────────────────────────────────────────────────────────────────────────┐
# │ macOS (Apple Silicon: M1, M2, M3, M4, etc.) — Instalado con Homebrew          │
# │   -I/opt/homebrew/opt/openssl@3/include                                        │
# │   -L/opt/homebrew/opt/openssl@3/lib                                            │
# │                                                                                 │
# │ macOS (Intel) — Instalado con Homebrew                                          │
# │   -I/usr/local/opt/openssl@3/include                                            │
# │   -L/usr/local/opt/openssl@3/lib                                                │
# │                                                                                 │
# │ Linux (Ubuntu/Debian)                                                           │
# │   OpenSSL viene preinstalado. Solo se necesita: -lssl -lcrypto                  │
# │   Si no está instalado: sudo apt install libssl-dev                             │
# │                                                                                 │
# │ Windows (con MSYS2/MinGW)                                                       │
# │   Instalar: pacman -S mingw-w64-x86_64-openssl                                 │
# │   -IC:/msys64/mingw64/include                                                   │
# │   -LC:/msys64/mingw64/lib                                                       │
# │                                                                                 │
# │ Windows (con vcpkg)                                                             │
# │   Instalar: vcpkg install openssl                                               │
# │   -IC:/vcpkg/installed/x64-windows/include                                      │
# │   -LC:/vcpkg/installed/x64-windows/lib                                          │
# └─────────────────────────────────────────────────────────────────────────────────┘
#
# Este Makefile usa las rutas de macOS Apple Silicon (Homebrew).
SSL_FLAGS = -I/opt/homebrew/opt/openssl@3/include -L/opt/homebrew/opt/openssl@3/lib -lssl -lcrypto

# "main" es una regla que compila el programa.
# La línea "main: main.c" significa: "para crear 'main', necesitas 'main.c'".
# Si main.c no ha cambiado desde la última compilación, make no recompilará (ahorra tiempo).
# $(CC) y $(SSL_FLAGS) se reemplazan por los valores definidos arriba.
main: main.c
	$(CC) main.c -o main $(SSL_FLAGS)

# "run" es una regla que primero compila (porque depende de "main") y luego ejecuta el programa.
# Esto permite compilar y correr con un solo comando: make run
run: main
	./main

# "clean" es una regla que elimina el binario compilado.
# Es útil para forzar una recompilación limpia.
clean:
	rm -f main

# .PHONY le dice a make que "run" y "clean" no son nombres de archivos, sino comandos.
# Sin esto, si existiera un archivo llamado "run" o "clean", make se confundiría.
.PHONY: run clean
