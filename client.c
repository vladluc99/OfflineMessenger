#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>

/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server*/
int port;

int main(int argc, char *argv[])
{
  int sd;                    // descriptorul de socket
  struct sockaddr_in server; // structura folosita pentru conectare
                             // mesajul trimis
  char buf[1000];
  char buf1[1000];

  /* exista toate argumentele in linia de comanda? */
  if (argc != 3)
  {
    printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
    return -1;
  }

  /* stabilim portul */
  port = atoi(argv[2]);

  /* cream socketul */
  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("Eroare la socket().\n");
    return errno;
  }

  /* umplem structura folosita pentru realizarea conexiunii cu serverul */
  /* familia socket-ului */
  server.sin_family = AF_INET;
  /* adresa IP a serverului */
  server.sin_addr.s_addr = inet_addr(argv[1]);
  /* portul de conectare */
  server.sin_port = htons(port);

  /* ne conectam la server */
  if (connect(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
  {
    perror("[client]Eroare la connect().\n");
    return errno;
  }

  if (fork() == 0)
  {
    while (1)
    {
      /* citirea mesajului */
      printf("[client]Introduceti comanda: ");
      fflush(stdout);
      memset(buf, 0, sizeof(buf));
      read(0, buf, sizeof(buf));
      //nr=atoi(buf);
      //scanf("%d",&nr);
      printf("Lungimea: %d", strlen(buf));
      buf[strlen(buf) - 1] = 0;
      printf("[client] Am citit %s\n", buf);

      /* trimiterea mesajului la server */
      if (write(sd, buf, sizeof(buf)) <= 0)
      {
        perror("[client]Eroare la write() spre server.\n");
        return errno;
      }

      if (strncmp(buf, "quit", 4) == 0)
        return 0;
      /* citirea raspunsului dat de server 
     (apel blocant pina cind serverul raspunde) */

      if (read(sd, buf, sizeof(buf)) < 0)
      {
        perror("[client]Eroare la read() de la server.\n");
        return errno;
      }

      /* afisam mesajul primit */
      printf("[client]Mesajul primit este: %s\n", buf);
    }
  }
  else
  {
    while (1)
    {
      sleep(5);
      memset(buf1, 0, sizeof(buf1));

      if (write(sd, "checkmessages", 13) <= 0)
      {
        perror("[client]Eroare la write() spre server.\n");
        return errno;
      }

      if (read(sd, buf1, sizeof(buf1)) < 0)
      {
        perror("[client]Eroare la read() de la server.\n");
        return errno;
      }
      /* afisam mesajul primit */
      if (strncmp(buf1, "nimic", 5) != 0)
      {
        printf("%s \n [client]Introduceti comanda: ", buf1);
      }
    }
  }

  /* inchidem conexiunea, am terminat */
  close(sd);
}
