#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdbool.h>
#include <unistd.h>

// sockaddr_in es una ficha que define qué datos necesitas para contactar a
// alguien en internet utilizando la red IPv4.
//
// struct sockaddr_in {
//     sa_family_t    sin_family;  // 🌍 Tipo de red (IPv4)
//     in_port_t      sin_port;    // 🔢 Puerto (como número de apartamento)
//     struct in_addr sin_addr;    // 📍 Dirección IP
// };
//

struct sockaddr_in serverAddress; // Ubicación donde recibe información el servidor.

int main() {
  int commChannel = socket(AF_INET, SOCK_STREAM, 0); // "Línea telefónica" que utiliza IPv4, envía paquetes de manera ordenada y utiliza el puerto por defecto.

  bzero(&serverAddress, sizeof(serverAddress)); // Limpia la ficha.
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(21); // Convierte (en caso de que sea necesario) de Little Endian a Big Endian.
  serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

  int call = connect( // Para poder hablar con el servidor, primero hay que "marcarle a su número".
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

    char username[] = "USER usuario_prueba\r\n"; // \r\n indica salto de línea en las computadoras de los años 70/80, años donde se creó el FTP. Un servidor FTP se le puede configurar un usuario y contraseña, pero también puede ser anónimo.
    ssize_t sendUser = send(commChannel, username, strlen(username), 0);

    if (sendUser == -1) {
      perror("Error");
      return 1;
    }

    char requestPassword[1024]; // El servidor responderá con el mensaje 331, que es para solicitar la contraseña.
    ssize_t serverResponseToUser = recv(
      commChannel,
      requestPassword,
      sizeof(requestPassword),
      0
    );
    if (serverResponseToUser == -1) {
      perror("Error");
      return 1;
    }
    requestPassword[serverResponseToUser] = '\0';
    printf("%s\n", requestPassword);

    char password[] = "PASS password123\r\n";
    ssize_t sendPassword = send(commChannel, password, strlen(password), 0);

    if (sendPassword == -1) {
      perror("Error");
      return 1;
    }
    
    char loginSuccessful[1024]; // El servidor responderá con el mensaje 230, que indica que el inicio de sesión se realizó de manera éxitosa.
    ssize_t serverResponseToPassword = recv(
      commChannel,
      loginSuccessful,
      sizeof(loginSuccessful),
      0
    );
    if (serverResponseToPassword == -1) {
      perror("Error");
      return 1;
    }
    loginSuccessful[serverResponseToPassword] = '\0';
    printf("%s\n", loginSuccessful);

    while (true) { // FTP tiene una lista de comandos que el cliente (usuario) puede utilizar. Utilizamos un bucle para poder insertar dichos comandos sin interrupciones hasta que el usuario diga que ya no quiere.
      char userCommand[100];
      printf("\nWrite a FTP command (QUIT to exit): ");
      fgets(userCommand, sizeof(userCommand), stdin);  // fgets lee la línea completa que el usuario escribe y agrega un \n al final.
      userCommand[strcspn(userCommand, "\n")] = '\0';  // Quita el \n que agrega fgets por defecto. La función strcspn() devuelve el índice donde se encuentra dicho carácter.
      strcat(userCommand, "\r\n"); // Agrega \r\n al final del comando.
      ssize_t sendCommand = send(commChannel, userCommand, strlen(userCommand), 0);

      if (sendCommand == -1) {
        perror("Error");
        return 1;
      }

      char commandResponse[1024];
      ssize_t serverResponseToCommand = recv(
        commChannel,
        commandResponse,
        sizeof(commandResponse),
        0
      );
      if (serverResponseToCommand == -1) {
        perror("Error");
        return 1;
      }
      commandResponse[serverResponseToCommand] = '\0';
      printf("%s", commandResponse);

      char exit[] = "QUIT\r\n";
      int endCall = strcasecmp(userCommand, exit); // La función strcmp() compara 2 strings letra por letra y si obtiene 0 al realizar la resta, es que son iguales. La función strcasecmp() ignora mayúsculas y minúsculas.

      if (endCall == 0) {
        break;
      }

    }
    
  }

  close(commChannel);
  return 0;
}