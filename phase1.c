#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdbool.h>

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
struct sockaddr_in responseAddress; // Ubicación donde el servidor responde al cliente.

int main() {
  int commChannel = socket(AF_INET, SOCK_DGRAM, 0); // Crea un canal de comunicación donde se utiliza IPv4, se envían paquetes sueltos (UDP) y se utiliza el protocolo por defecto (0).

  bzero(&serverAddress, sizeof(serverAddress)); // Limpia la ficha.
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(69); // Convierte (en caso de que sea necesario) de Little Endian a Big Endian.
  serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

  uint16_t operationCode = htons(1); // En el Trivial File Transfer Protocol, el 1 indica petición de lectura (descargar un archivo).
  char form[512]; // El TFTP es un mensaje estructurado que pide la operación a realizar, el nombre del archivo y el modo en el que se va a enviar.
  int offset = 0; // Usamos el nombre offset cuando estamos creando algo de manera secuencial.

  memcpy(form, &operationCode, 2); // Por convención, el TFTP necesita sí o sí que el código de operación sea de 2 bytes.
  offset += 2; // Esto indica que los primeros 2 bytes ya están ocupados.

  char fileName[] = "prueba.txt";
  strcpy(form + offset, fileName);
  offset += strlen(fileName) + 1; // El nombre de nuestro archivo (en este caso) ocupa 10 bytes, pero por diseño, C necesita un byte más para saber dónde colocar el cáracter nulo, es decir, para saber dónde termina el string.
  
  char mode[] = "octet"; // Método de envío que envía los bytes tal cual están.
  strcpy(form + offset, mode);
  offset += strlen(mode) + 1;

  ssize_t sent = sendto(
    commChannel, // Canal donde se comunica el cliente y el servidor.
    form, // Formulario o solicitud que se enviará.
    offset, // El tamaño (en bytes) de dicho formulario.
    0, // Le indica al servidor que no haga ninguna operación adicional.
    (struct sockaddr *) &serverAddress, // La razón por la que se pasa un sockaddr es porque sendto (y otras funciones) también maneja los tipos de red IPv6 y Unix.
    sizeof(serverAddress) // El tamaño de un sockaddr_in es de 16 bytes.
  );

  if (sent == -1) {
    perror("Error al enviar.");
  }
  else {
    printf("Se enviaron %zd bytes\n", sent);
  }

  char serverResponse[512]; // Si el archivo que estamos solicitando existe, aquí caerá la información.
  socklen_t responseAddressLen = sizeof(responseAddress); 

  bool contentLeft = true;

  FILE *downloaded;
  downloaded = fopen("test.txt", "w");

  while (contentLeft) {
    ssize_t receive = recvfrom(
      commChannel,
      serverResponse,
      sizeof(serverResponse),
      0,
      (struct sockaddr *) &responseAddress,
      &responseAddressLen // Los punteros pueden modificar el contenido original de la variable. Como ya hemos dicho antes, funciones como sendto y recvfrom también aceptan IPv6 y Unix, que tienen diferente tamaño. En nuestro caso esto es redundante, pero es posible crear programas que acepten IPv4 e IPv6 al mismo tiempo.
    );

    if (receive == -1) {
      perror("Error al recibir");
      break;
    }
    else {

      // El servidor necesita una "firma de recibido" (ACK) para poder mandarnos el siguiente bloque. Sin eso, el while loop nunca terminaría.
      // Para mandar un ACK solo necesita 4 bytes de información: el operation code número 4 y el número de bloque (siempre parte de 1 ya que el 0 está reservado para cuando se hace una solicitud de upload/escritura). 
      // Un bloque es una "caja" de hasta 516 bytes de información que podemos recibir del servidor.

      char confirmation[4];
      uint16_t confirmationCode = htons(4); // El operation code para decirle al servidor que hemos recibido el bloque de manera éxitosa es el 4.
      int confirmationHeader = 0;

      memcpy(confirmation, &confirmationCode, 2); // 2 bytes para el operation code.
      confirmationHeader += 2;

      uint16_t blockNumber;
      memcpy(&blockNumber, serverResponse + 2, 2); // Lee y guarda el bloque actual (se encuentra después del byte 2, es decir, después del operation code).

      memcpy(confirmation + confirmationHeader, &blockNumber, 2); // 2 bytes para el número de bloque.
      confirmationHeader += 2;

      ssize_t clientConfirmation = sendto(
        commChannel,
        confirmation,
        sizeof(confirmation),
        0,
        (struct sockaddr *) &responseAddress,
        responseAddressLen
      );
    }

    char *fileContent = serverResponse + 4; // Apunta al contenido real (después del header). Los primeros 4 bytes están reservados para el header: 2 para el operaation code (00 03) y 2 para el número de bloque (00 01). Si intentasemos mostrar el header no podríamos porque C detectaría que el primer byte es 00, que es exactamente lo mismo a \0, que es el carácter nulo que indica que el string termina ahí.
    int dataSize = receive - 4; // Eliminamos el header para saber la cantidad real de información que tiene el bloque. 
    fwrite(fileContent, 1, dataSize, downloaded); 

    if (dataSize < 512) { // Si el bloque recibido tiene menos de 512 bytes es porque es el último bloque con contenido.
      contentLeft = false;
    }

  }

  fclose(downloaded);

  return 0;
}