#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <openssl/ssl.h>  // Librería principal de OpenSSL: contiene las funciones SSL_new, SSL_connect, SSL_read, SSL_write, etc.
#include <openssl/err.h>  // Librería de manejo de errores de OpenSSL: contiene ERR_print_errors_fp para imprimir errores SSL.

// sockaddr_in es una ficha que define qué datos necesitas para contactar a
// alguien en internet utilizando la red IPv4.
//
// struct sockaddr_in {
//     sa_family_t    sin_family;  // 🌍 Tipo de red (IPv4)
//     in_port_t      sin_port;    // 🔢 Puerto (como número de apartamento)
//     struct in_addr sin_addr;    // 📍 Dirección IP
// };
//

struct sockaddr_in serverAddress; // Ubicación donde recibe el servidor instrucciones del cliente.
struct sockaddr_in serverAddressToFiles; // Ubicación donde el cliente y el servidor se envían información.

void FTPCommand(char* command, int phoneChannel, char* response, int responseSize);
void FTPCommandWithSSL(char* command, SSL* encryptedChannel, char* response, int responseSize);
SSL* openDataChannelWithSSL(SSL* encryptedChannel, SSL_CTX* mainContext);

int main() {
  // === FASE 1: PREPARAR OpenSSL ===
  // SSL_METHOD define qué versión de TLS usar. TLS_client_method() selecciona automáticamente la mejor versión disponible (TLS 1.2 o 1.3).
  const SSL_METHOD* method = TLS_client_method();
  // SSL_CTX (contexto) es la "fábrica" de conexiones seguras. Guarda la configuración global de seguridad (versión TLS, certificados, etc.).
  // Solo se necesita crear UNA fábrica para todo el programa.
  SSL_CTX* context = SSL_CTX_new(method);
  // Nivel de seguridad 0 permite claves DH pequeñas (1024 bits). 
  // Se usa solo para pruebas porque nuestro servidor vsFTPd 3.0.2 genera claves DH de 1024 bits.
  // En producción, se usaría nivel 2 o superior con claves de al menos 2048 bits.
  SSL_CTX_set_security_level(context, 0);

  int commChannel = socket(AF_INET, SOCK_STREAM, 0); // "Línea telefónica" o "canal" que utiliza IPv4, envía paquetes de manera ordenada y utiliza el puerto por defecto.

  bzero(&serverAddress, sizeof(serverAddress)); // Limpia la ficha.
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(21); // Convierte (en caso de que sea necesario) de Little Endian a Big Endian.
  serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

  int call = connect( // Para poder hablar con el servidor, primero hay que "marcarle a su número" o ir a su canal.
    commChannel, 
    (struct sockaddr *) &serverAddress,
    sizeof(serverAddress)
  );

  if (call == -1) { // Si no se pudo realizar la llamada, la función perror() nos dirá por qué.
    perror("Error");
    return 1;
  }

  char connection[1024]; // Lugar donde almacenaremos la respuesta del servidor.
  ssize_t serverResponse = recv(
    commChannel,
    connection,
    sizeof(connection),
    0 // Esto indica que el servidor no haga ninguna operación adicional, que solo mande la información.
  );

  if (serverResponse == -1) { // Si recibimos -1 byte de información, perror() nos dirá por qué.
    perror("Error");
    return 1;
  }
  else if (serverResponse == 0) { // Si recibimos 0 bytes de información es porque se perdió la conexión.
    printf("Lost connection with the server.");
  }
  else {
    connection[serverResponse] = '\0'; // Agrega un carácter nulo al final del mensaje para evitar que se imprima toda la "basura" que hay después del mensaje.
    printf("%s\n", connection); // Si recibimos un mensaje 220 es porque la conexión se realizó de manera éxitosa.

    // === FASE 3: ACTIVAR ENCRIPTACIÓN EN EL CANAL DE CONTROL ===
    // AUTH TLS le dice al servidor: "quiero subir esta conexión a TLS (cifrada)".
    // Se envía en texto plano porque aún no hemos cifrado la conexión.
    // Si el servidor acepta, responde con código 234 ("Proceed with negotiation").
    char activateTransportLayerSecurityCommand[] = "AUTH TLS\r\n";
    char serverResponseToTLS[1024];
    FTPCommand(activateTransportLayerSecurityCommand, commChannel, serverResponseToTLS, sizeof(serverResponseToTLS));

    // SSL_new() crea un "sobre sellado" individual a partir de la fábrica (contexto).
    // Cada conexión que queramos proteger necesita su propio objeto SSL.
    SSL* protectedCommChannel = SSL_new(context);
    // SSL_set_fd() asocia el objeto SSL al socket existente.
    // Es como meter la carta (socket) dentro del sobre sellado (SSL).
    SSL_set_fd(protectedCommChannel, commChannel);
    // SSL_connect() realiza el "handshake" TLS: el cliente y el servidor intercambian
    // códigos de cifrado (claves Diffie-Hellman) para crear un canal seguro.
    // Retorna 1 si el intercambio fue exitoso.
    int cypherCodesExchangedCorrectly = SSL_connect(protectedCommChannel);
    if (cypherCodesExchangedCorrectly != 1) {
      // ERR_print_errors_fp() es la versión de OpenSSL de perror().
      // Imprime el error real de SSL (perror no funciona con errores de OpenSSL).
      ERR_print_errors_fp(stderr);
      return 1;
    }

    // === FASE 4: PROTEGER EL CANAL DE DATOS ===
    // A partir de aquí, TODOS los comandos se envían cifrados con SSL_write/SSL_read
    // en lugar de send/recv. Por eso usamos FTPCommandWithSSL.

    // PBSZ (Protection Buffer Size) establece el tamaño del buffer de protección en 0.
    // TLS ya maneja su propio buffering, así que siempre se envía 0.
    // Es un comando obligatorio que debe ir antes de PROT P.
    char protectionBufferSizeCommand[] = "PBSZ 0\r\n";
    char serverResponseToPBSZ[1024];
    FTPCommandWithSSL(protectionBufferSizeCommand, protectedCommChannel, serverResponseToPBSZ, sizeof(serverResponseToPBSZ));

    // PROT P (Protection Private) le dice al servidor:
    // "quiero que el canal de datos (LIST, RETR, STOR) también viaje cifrado".
    // La "P" significa Private (privado/cifrado). La alternativa "C" sería Clear (sin cifrar).
    char protectionLevelCommand[] = "PROT P\r\n";
    char serverResponseToPROT[1024];
    FTPCommandWithSSL(protectionLevelCommand, protectedCommChannel, serverResponseToPROT, sizeof(serverResponseToPROT));

    // Un servidor FTP se le puede configurar un usuario y contraseña, pero también puede ser anónimo.
    // En este caso, se manejará con usuario y contraseña.
    // === FASE 5: LOGIN (ya cifrado) ===
    // El usuario y contraseña ahora viajan cifrados gracias al handshake TLS.
    // Nadie puede interceptar las credenciales.
    char username[] = "USER usuario_prueba\r\n";
    char serverResponseToUser[1024];
    FTPCommandWithSSL(username, protectedCommChannel, serverResponseToUser, sizeof(serverResponseToUser));

    char password[] = "PASS password123\r\n";
    char serverResponseToPassword[1024];
    FTPCommandWithSSL(password, protectedCommChannel, serverResponseToPassword, sizeof(serverResponseToPassword));

    while (true) { // Bucle que recibe sin interrupciones los comandos del usuario.
      char userCommand[100];
      char serverResponseToUserCommand[1024];
      printf("\nWrite a FTP command (QUIT to exit): ");
      fgets(userCommand, sizeof(userCommand), stdin);  // fgets lee la línea completa que el usuario escribe y agrega un \n al final.
      userCommand[strcspn(userCommand, "\n")] = '\0';  // Quita el \n que agrega fgets por defecto. La función strcspn() devuelve el índice donde se encuentra dicho carácter.
      strcat(userCommand, "\r\n"); // Agrega \r\n al final.

      if (strcasecmp(userCommand, "LIST\r\n") == 0) { // Si el usuario escribió el comando LIST:
        // Paso 1: Abrir canal de datos (PASV + conexión TCP + crear objeto SSL, pero SIN handshake aún).
        SSL* protectedDataChannel = openDataChannelWithSSL(protectedCommChannel, context);
        // Paso 2: Enviar LIST por el canal de control cifrado. El servidor responde con "150" (listo para enviar datos).
        FTPCommandWithSSL(userCommand, protectedCommChannel, serverResponseToUserCommand, sizeof(serverResponseToUserCommand));
        // Paso 3: Ahora sí, realizar el handshake TLS en el canal de datos.
        // IMPORTANTE: El handshake del canal de datos se hace DESPUÉS de enviar el comando (LIST/RETR/STOR),
        // porque el servidor no inicia TLS en el canal de datos hasta recibir el comando.
        int dataProtected = SSL_connect(protectedDataChannel);
        if (dataProtected != 1) {
          ERR_print_errors_fp(stderr);
        }

        char listOfFiles[4096];

        while (true) { // Este bucle se ejecutará hasta que ya no haya más archivos del lado del servidor.
          // SSL_read() es la versión cifrada de recv().
          // Lee los datos del canal de datos protegido y los descifra automáticamente.
          ssize_t filesReceived = SSL_read(
            protectedDataChannel,
            listOfFiles,
            sizeof(listOfFiles) - 1 // Un byte es reservado para el carácter nulo (\0).
          );

          if (filesReceived <= 0) break;

          listOfFiles[filesReceived] = '\0';
          printf("List of files:\n%s\n", listOfFiles);
        }

        // El servidor una vez que finaliza de mandar toda la información que tiene disponible, se desconecta del canal (cierra la conexión). 
        // Pero nosotros seguímos ahí a pesar de que el servidor ya no esté. 
        // Si lo mantenemos abierto, puede llegar a ocurrir un error de tener muchos canales abiertos.
        // Por ende, para evitar este error, hay que cerrar el canal (o línea).
        // === LIMPIEZA DEL CANAL DE DATOS ===
        // Para cerrar un canal SSL correctamente, se necesitan 3 pasos:
        int fd = SSL_get_fd(protectedDataChannel);  // 1. Obtener el número del canal del objeto SSL antes de liberarlo.
        SSL_shutdown(protectedDataChannel);          // 2. Avisarle al servidor que terminamos la conexión TLS.
        SSL_free(protectedDataChannel);              // 3. Liberar la memoria del objeto SSL.
        close(fd);                                   // 4. Cerrar el socket TCP subyacente.

        // Este comando hace que el servidor retorne 2 mensajes: el 150 (manda la lista de archivos) y 226 (que la información ha sido enviada correctamente).
        // Este segundo mensaje hay que guardarlo porque, de lo contrario, desincroniza las respuestas y lanza un error. El error ocurre porque el programa intenta entrar a memoria que no le pertence.
        char transferComplete[1024];
        SSL_read(protectedCommChannel, transferComplete, sizeof(transferComplete) - 1);
      }
      else if (strncasecmp(userCommand, "RETR", 4) == 0) { // Si el usuario utiliza el comando RETR nombre_de_un_archivo.ext, extrae la información que contiene dicho archivo y la guarda en un archivo local.
        char serverFileName[100];
        sscanf(userCommand, "RETR %s", serverFileName);

        FILE* downloadFile = fopen(serverFileName, "wb");
        if (downloadFile == NULL) {
            perror("Error");
            continue;
        }

        SSL* protectedDataChannel = openDataChannelWithSSL(protectedCommChannel, context); // Abre un canal donde se envían los datos.
        FTPCommandWithSSL(userCommand, protectedCommChannel, serverResponseToUserCommand, sizeof(serverResponseToUserCommand));
        int dataProtected = SSL_connect(protectedDataChannel);
        if (dataProtected != 1) {
          ERR_print_errors_fp(stderr);
        }

        char storeData[4096];

        while (true) {
          ssize_t fileData = SSL_read(
            protectedDataChannel,
            storeData,
            sizeof(storeData)
          );

          if (fileData <= 0) break;

          fwrite(storeData, 1, fileData, downloadFile); // Recordemos que el 1 significa "escribe 1 byte (letra, número, símbolo) a la vez".
        }

        fclose(downloadFile);

        int fd = SSL_get_fd(protectedDataChannel);
        SSL_shutdown(protectedDataChannel);
        SSL_free(protectedDataChannel);
        close(fd);

        // Leer el "226 Transfer complete".
        char transferComplete[1024];
        SSL_read(protectedCommChannel, transferComplete, sizeof(transferComplete) - 1);
      }
      else if (strncasecmp(userCommand, "STOR", 4) == 0) { // El comando STOR nombre_del_archivo.ext manda un archivo local al servidor.
        char localFileName[100];
        sscanf(userCommand, "STOR %s", localFileName);

        FILE* localFile = fopen(localFileName, "rb");
        if (localFile == NULL) {
            perror("Error");
            continue;
        }

        SSL* protectedDataChannel = openDataChannelWithSSL(protectedCommChannel, context); // Abre un canal donde se envían los datos.
        FTPCommandWithSSL(userCommand, protectedCommChannel, serverResponseToUserCommand, sizeof(serverResponseToUserCommand));
        int dataProtected = SSL_connect(protectedDataChannel);
        if (dataProtected != 1) {
          ERR_print_errors_fp(stderr);
        }

        char storeData[4096];

        while (true) {
          ssize_t fileData = fread(storeData, 1, sizeof(storeData), localFile);
          if (fileData <= 0) break;

          SSL_write(protectedDataChannel, storeData, fileData);
        }

        fclose(localFile);
        
        int fd = SSL_get_fd(protectedDataChannel);
        SSL_shutdown(protectedDataChannel);
        SSL_free(protectedDataChannel);
        close(fd);

        // Leer el "226 Transfer complete".
        char transferComplete[1024];
        SSL_read(protectedCommChannel, transferComplete, sizeof(transferComplete) - 1);
      }
      else if (strncasecmp(userCommand, "NLST", 4) == 0 || strncasecmp(userCommand, "PORT", 4) == 0) { // Los comandos NSLT y PORT son muy rara vez utilizados, por lo tanto los descartaremos para este cliente FTP.
        printf("Command not supported. Use LIST and PASV instead.\n");
      }
      else {
        FTPCommandWithSSL(userCommand, protectedCommChannel, serverResponseToUserCommand, sizeof(serverResponseToUserCommand));
      }

      char exit[] = "QUIT\r\n";
      int endCall = strcasecmp(userCommand, exit); // La función strcmp() compara 2 strings letra por letra y si obtiene 0 al realizar la resta, es que son iguales. 
      // La función strcasecmp() ignora mayúsculas y minúsculas. 
      // La función strncasecmp() solo toma en cuenta los primeros "n" caracteres.

      if (endCall == 0) {
        break;
      }
    }
  }

  close(commChannel);
  return 0;
}

// Esta función envía el comando FTP al servidor para que la ejecute.
void FTPCommand(char* command, int mainChannel, char* response, int responseSize) {

  ssize_t sendFTPCommand = send(mainChannel, command, strlen(command), 0);

  if (sendFTPCommand == -1) {
    perror("Error");
  }
  else {
    char serverResponse[responseSize];
    ssize_t serverResponseToFTPCommand = recv(
      mainChannel,
      serverResponse,
      sizeof(serverResponse),
      0
    );
  
    if (serverResponseToFTPCommand == -1) {
      perror("Error");
    }
    else {
      serverResponse[serverResponseToFTPCommand] = '\0';
      printf("%s\n", serverResponse);
      snprintf(response, responseSize, "%s", serverResponse);
    }
  
  }

}

// Esta función es la versión cifrada de FTPCommand.
// En lugar de send() y recv(), usa SSL_write() y SSL_read() para enviar y recibir datos cifrados.
// El objeto SSL* ya tiene asociado el socket por dentro (gracias a SSL_set_fd),
// por eso no necesita recibir el socket como parámetro.
void FTPCommandWithSSL(char* command, SSL* encryptedChannel, char* response, int responseSize) {

  // SSL_write() es la versión cifrada de send(). Cifra el comando antes de enviarlo.
  ssize_t sendFTPCommand = SSL_write(encryptedChannel, command, strlen(command));

  if (sendFTPCommand == -1) {
    perror("Error");
  }
  else {
    char serverResponse[responseSize];
    // SSL_read() es la versión cifrada de recv(). Recibe datos cifrados y los descifra.
    ssize_t serverResponseToFTPCommand = SSL_read(
      encryptedChannel,
      serverResponse,
      sizeof(serverResponse)
    );
  
    if (serverResponseToFTPCommand == -1) {
      perror("Error");
    }
    else {
      serverResponse[serverResponseToFTPCommand] = '\0';
      printf("\n%s\n", serverResponse);
      snprintf(response, responseSize, "%s", serverResponse);
    }
  
  }

}

// Esta función crea un canal de datos protegido con SSL.
// Realiza: PASV → conexión TCP → crear objeto SSL (pero NO hace el handshake TLS).
// El handshake TLS (SSL_connect) se hace DESPUÉS, una vez que se envía el comando (LIST/RETR/STOR)
// por el canal de control, porque el servidor no inicia TLS en el canal de datos
// hasta recibir dicho comando.
SSL* openDataChannelWithSSL(SSL* encryptedChannel, SSL_CTX* mainContext) {
  char pasvCommand[] = "PASV\r\n";
  char serverResponseToPASV[1024];
  FTPCommandWithSSL(pasvCommand, encryptedChannel, serverResponseToPASV, sizeof(serverResponseToPASV));

  int host1, host2, host3, host4, port1, port2;
  char* pasvIPAndPort = strchr(serverResponseToPASV, '(');
  sscanf(pasvIPAndPort, "(%d,%d,%d,%d,%d,%d)", &host1, &host2, &host3, &host4, &port1, &port2);
  char ipForFiles[16];
  sprintf(ipForFiles, "%d.%d.%d.%d", host1, host2, host3, host4);
  int portForFiles = (port1 * 256) + port2;

  int channelForFiles = socket(AF_INET, SOCK_STREAM, 0);
  bzero(&serverAddressToFiles, sizeof(serverAddressToFiles)); // Limpia la ficha.
  serverAddressToFiles.sin_family = AF_INET;
  serverAddressToFiles.sin_port = htons(portForFiles); // Convierte (en caso de que sea necesario) de Little Endian a Big Endian.
  serverAddressToFiles.sin_addr.s_addr = inet_addr(ipForFiles);

  int callForFiles = connect(
    channelForFiles,
    (struct sockaddr *) &serverAddressToFiles,
    sizeof(serverAddressToFiles)
  );
  if (callForFiles == -1) {
    perror("Error");
    return NULL;
  }

  // Crear el objeto SSL para el canal de datos usando la misma fábrica (contexto) del canal de control.
  // Solo se asocia al socket (SSL_set_fd), pero NO se hace SSL_connect aquí.
  SSL* channelProtected = SSL_new(mainContext);
  SSL_set_fd(channelProtected, channelForFiles);

  return channelProtected;

}