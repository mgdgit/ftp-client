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
int openDataChannel(int mainChannel);

int main() {
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

    // Un servidor FTP se le puede configurar un usuario y contraseña, pero también puede ser anónimo.
    // En este caso, se manejará con usuario y contraseña.
    char username[] = "USER usuario_prueba\r\n"; // \r\n indica salto de línea en las computadoras de los años 70/80, años donde se creó el FTP.
    char serverResponseToUser[1024];
    FTPCommand(username, commChannel, serverResponseToUser, sizeof(serverResponseToUser));

    char password[] = "PASS password123\r\n";
    char serverResponseToPassword[1024];
    FTPCommand(password, commChannel, serverResponseToPassword, sizeof(serverResponseToPassword));

    while (true) { // Bucle que recibe sin interrupciones los comandos del usuario.
      char userCommand[100];
      char serverResponseToUserCommand[1024];
      printf("\nWrite a FTP command (QUIT to exit): ");
      fgets(userCommand, sizeof(userCommand), stdin);  // fgets lee la línea completa que el usuario escribe y agrega un \n al final.
      userCommand[strcspn(userCommand, "\n")] = '\0';  // Quita el \n que agrega fgets por defecto. La función strcspn() devuelve el índice donde se encuentra dicho carácter.
      strcat(userCommand, "\r\n"); // Agrega \r\n al final.

      if (strcasecmp(userCommand, "LIST\r\n") == 0) { // Si el usuario escribió el comando LIST:
        int dataChannel = openDataChannel(commChannel); // Abre un canal donde se envían los datos.
        FTPCommand(userCommand, commChannel, serverResponseToUserCommand, sizeof(serverResponseToUserCommand)); // Se utiliza el comando LIST.

        char listOfFiles[4096];

        while (true) { // Este bucle se ejecutará hasta que ya no haya más archivos del lado del servidor.
          ssize_t filesReceived = recv(
            dataChannel,
            listOfFiles,
            sizeof(listOfFiles) - 1, // Un byte es reservado para el carácter nulo (\0).
            0
          );

          if (filesReceived <= 0) break;

          listOfFiles[filesReceived] = '\0';
          printf("List of files:\n%s\n", listOfFiles);
        }

        // El servidor una vez que finaliza de mandar toda la información que tiene disponible, se desconecta del canal (cierra la conexión). 
        // Pero nosotros seguímos ahí a pesar de que el servidor ya no esté. 
        // Si lo mantenemos abierto, puede llegar a ocurrir un error de tener muchos canales abiertos.
        // Por ende, para evitar este error, hay que cerrar el canal (o línea).
        close(dataChannel);

        // Este comando hace que el servidor retorne 2 mensajes: el 150 (manda la lista de archivos) y 226 (que la información ha sido enviada correctamente).
        // Este segundo mensaje hay que guardarlo porque, de lo contrario, desincroniza las respuestas y lanza un error. El error ocurre porque el programa intenta entrar a memoria que no le pertence.
        char transferComplete[1024];
        recv(commChannel, transferComplete, sizeof(transferComplete) - 1, 0);
      }
      else if (strncasecmp(userCommand, "RETR", 4) == 0) { // Si el usuario utiliza el comando RETR nombre_de_un_archivo.ext, extrae la información que contiene dicho archivo y la guarda en un archivo local.
        char serverFileName[100];
        sscanf(userCommand, "RETR %s", serverFileName);

        FILE* downloadFile = fopen(serverFileName, "wb");
        if (downloadFile == NULL) {
            perror("Error");
            continue;
        }

        int dataChannel = openDataChannel(commChannel);
        FTPCommand(userCommand, commChannel, serverResponseToUserCommand, sizeof(serverResponseToUserCommand));

        char storeData[4096];

        while (true) {
          ssize_t fileData = recv(
            dataChannel,
            storeData,
            sizeof(storeData),
            0
          );

          if (fileData <= 0) break;

          fwrite(storeData, 1, fileData, downloadFile); // Recordemos que el 1 significa "escribe 1 byte (letra, número, símbolo) a la vez".
        }

        fclose(downloadFile);
        close(dataChannel);

        // Leer el "226 Transfer complete".
        char transferComplete[1024];
        recv(commChannel, transferComplete, sizeof(transferComplete) - 1, 0);
      }
      else if (strncasecmp(userCommand, "STOR", 4) == 0) { // El comando STOR nombre_del_archivo.ext manda un archivo local al servidor.
        char localFileName[100];
        sscanf(userCommand, "STOR %s", localFileName);

        FILE* localFile = fopen(localFileName, "rb");
        if (localFile == NULL) {
            perror("Error");
            continue;
        }

        int dataChannel = openDataChannel(commChannel);
        FTPCommand(userCommand, commChannel, serverResponseToUserCommand, sizeof(serverResponseToUserCommand));

        char storeData[4096];

        while (true) {
          ssize_t fileData = fread(storeData, 1, sizeof(storeData), localFile);
          if (fileData <= 0) break;

          send(dataChannel, storeData, fileData, 0);
        }

        fclose(localFile);
        close(dataChannel);

        // Leer el "226 Transfer complete".
        char transferComplete[1024];
        recv(commChannel, transferComplete, sizeof(transferComplete) - 1, 0);
      }
      else if (strncasecmp(userCommand, "NLST", 4) == 0 || strncasecmp(userCommand, "PORT", 4) == 0) { // Los comandos NSLT y PORT son muy rara vez utilizados, por lo tanto los descartaremos para este cliente FTP.
        printf("Command not supported. Use LIST and PASV instead.\n");
      }
      else {
        FTPCommand(userCommand, commChannel, serverResponseToUserCommand, sizeof(serverResponseToUserCommand));
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
void FTPCommand(char* command, int phoneChannel, char* response, int responseSize) {

  ssize_t sendFTPCommand = send(phoneChannel, command, strlen(command), 0);

  if (sendFTPCommand == -1) {
    perror("Error");
  }
  else {
    char serverResponse[responseSize];
    ssize_t serverResponseToFTPCommand = recv(
      phoneChannel,
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

// Esta función crea el canal donde el cliente y el servidor se envían información, y retorna el número del canal.
// Hay que llamar siempre a esta función cuando se reciba o envíe información, 
// pues el servidor siempre se desconecta del canal una vez el envío ha sido completado.
int openDataChannel(int mainChannel) {
  char pasvCommand[] = "PASV\r\n";
  char serverResponseToPASV[1024];
  FTPCommand(pasvCommand, mainChannel, serverResponseToPASV, sizeof(serverResponseToPASV));

  int host1, host2, host3, host4, port1, port2;
  char* pasvIPAndPort = strchr(serverResponseToPASV, '(');
  sscanf(pasvIPAndPort, "(%d,%d,%d,%d,%d,%d)", &host1, &host2, &host3, &host4, &port1, &port2);
  char ipForFiles[16];
  sprintf(ipForFiles, "%d.%d.%d.%d", host1, host2, host3, host4);
  int portForFiles = (port1 * 256) + port2;
  //printf("%d\n", portForFiles);

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
    return -1;
  }

  return channelForFiles;
}