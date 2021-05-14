#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <sqlite3.h>

/* portul folosit */
#define PORT 2908

/* codul de eroare returnat de anumite apeluri */
extern int errno;


sqlite3 *db;
typedef struct thData
{
  int idThread; //id-ul thread-ului tinut in evidenta de acest program
  int cl;       //descriptorul intors de accept
  sqlite3 *db;  //handle-ul pentru baza de date deschisa
} thData;

static int callback_afisare(void *data, int argc, char **argv, char **azColName)
{
  short int i;
  for (i = 0; i < argc; i++)
  {
    strcat(data, azColName[i]);
    strcat(data, ": ");
    if (argv[i])
      strcat(data, argv[i]);
    strcat(data, "\n");
  }

  strcat(data, "\n");
  return 0;
}

static int callback(void *data, int argc, char **argv, char **azColName)
{
  strcat(data, argv[0]);
  return 0;
}

static void *treat(void *); /* functia executata de fiecare thread ce realizeaza comunicarea cu clientii */


int main()
{
  sqlite3_open("messenger.db", &db);
  struct sockaddr_in server; // structura folosita de server
  struct sockaddr_in from;
  int sd; //descriptorul de socket
  int pid;
  pthread_t th[100]; //Identificatorii thread-urilor care se vor crea
  int i = 0;

  /* crearea unui socket */
  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
  {
    perror("[server]Eroare la socket().\n");
    return errno;
  }
  /* utilizarea optiunii SO_REUSEADDR */
  int on = 1;
  setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

  /* pregatirea structurilor de date */
  bzero(&server, sizeof(server));
  bzero(&from, sizeof(from));

  /* umplem structura folosita de server */
  /* stabilirea familiei de socket-uri */
  server.sin_family = AF_INET;
  /* acceptam orice adresa */
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  /* utilizam un port utilizator */
  server.sin_port = htons(PORT);

  /* atasam socketul */
  if (bind(sd, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1)
  {
    perror("[server]Eroare la bind().\n");
    return errno;
  }

  /* punem serverul sa asculte daca vin clienti sa se conecteze */
  if (listen(sd, 2) == -1)
  {
    perror("[server]Eroare la listen().\n");
    return errno;
  }
  /* servim in mod concurent clientii...folosind thread-uri */
  while (1)
  {
    int client;
    thData *td; //parametru functia executata de thread
    int length = sizeof(from);

    printf("[server]Asteptam la portul %d...\n", PORT);
    fflush(stdout);

    // client= malloc(sizeof(int));
    /* acceptam un client (stare blocanta pina la realizarea conexiunii) */
    if ((client = accept(sd, (struct sockaddr *)&from, &length)) < 0)
    {
      perror("[server]Eroare la accept().\n");
      continue;
    }

    /* s-a realizat conexiunea, se astepta mesajul */

    // int idThread; //id-ul threadului
    // int cl; //descriptorul intors de accept

    td = (struct thData *)malloc(sizeof(struct thData));
    td->idThread = i++;
    td->cl = client;
    td->db = db;

    pthread_create(&th[i], NULL, &treat, td);

  } //while
};
static void *treat(void *arg)
{
  struct thData tdL;
  short int id_mesager;
  while (1)
  {
    tdL = *((struct thData *)arg);
    char fromClient[1000];
    char toClient[1000];
    char rezultat[2000];
    char argument[1000];
    char query[255];
    char *zErrMsg;
    printf("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
    fflush(stdout);
    pthread_detach(pthread_self());

    //////////////////////////////////////
    if (read(tdL.cl, fromClient, sizeof(fromClient)) <= 0)
    {
      printf("[Thread %d]\n", tdL.idThread);
      perror("Eroare la read() de la client.\n");
    }
    //printf ("[Thread %d]Mesajul a fost receptionat...%s\n",tdL.idThread, fromClient);

    /*pregatim mesajul de raspuns */

    //printf("[Thread %d]Trimitem mesajul inapoi...%s\n",tdL.idThread, fromClient);

    /* returnam mesajul clientului */
    if (strncmp(fromClient, "login ", 6) == 0)
    {
      bzero(rezultat, sizeof(rezultat));
      strcpy(argument, fromClient + 6);
      printf("argumentul: %s sfarsit\n", argument);
      sprintf(query, "SELECT id FROM users where name='%s';", argument);
      printf("query: %s\n", query);
      sqlite3_exec(tdL.db, query, callback, rezultat, &zErrMsg);
      printf("rez: %s\n", rezultat);
      id_mesager = atoi(rezultat);
      if (id_mesager == 0)
      {
        sprintf(toClient, "Contul nu exista!");
      }
      else
        sprintf(toClient, "ID ul tau este: %d", id_mesager);
    }

    else if (strncmp(fromClient, "register ", 9) == 0)
    {
      int maxid = 0;
      char id[100];
      id_mesager = 0;
      strcpy(argument, fromClient + 9);
      sprintf(query, "SELECT id FROM users where name='%s';", argument);
      bzero(rezultat, sizeof(rezultat));
      sqlite3_exec(tdL.db, query, callback, rezultat, &zErrMsg);
      id_mesager = atoi(rezultat);
      if (id_mesager == 0)
      {
        bzero(id, sizeof(id));
        sqlite3_exec(tdL.db, "select max(id) from users;", callback, id, &zErrMsg);
        maxid = atoi(id) + 1;
        printf("id nou: %d", maxid);
        sprintf(query, "INSERT INTO users values(%d,'%s');", maxid, argument);
        bzero(rezultat, sizeof(rezultat));
        sqlite3_exec(tdL.db, query, callback, rezultat, &zErrMsg);
        sprintf(toClient, "Utilizator %s creat cu succes!", argument);
        id_mesager = maxid;
      }
      else
        sprintf(toClient, "Userul %s exista deja!", argument);
    }

    else if (strncmp(fromClient, "namechange ", 11) == 0)
    {
      if (id_mesager == 0)
      {
        strcpy(toClient, "Trebuie sa fii logat!");
      }
      else
      {
        int id_nume = 0;
        strcpy(argument, fromClient + 11);
        sprintf(query, "SELECT id from users where name='%s';", argument);
        bzero(rezultat, sizeof(rezultat));
        sqlite3_exec(tdL.db, query, callback, rezultat, &zErrMsg);
        id_nume = atoi(rezultat);
        if (id_nume == 0)
        {
          sprintf(query, "UPDATE users set name='%s' where id=%d;", argument, id_mesager);
          sqlite3_exec(tdL.db, query, callback, rezultat, &zErrMsg);
          sprintf(toClient, "Nume schimbat cu succes in %s!", argument);
        }
        else
        {
          sprintf(toClient, "Numele %s deja exista!", argument);
        }
      }
    }

    else if (strncmp(fromClient, "message ", 8) == 0)
    {
      if (id_mesager == 0)
      {
        sprintf(toClient, "Trebuie sa fii logat!");
      }
      else
      {
        int id_persoana = 0;
        char persoana[100];
        char mesaj[1000];
        strcpy(argument, fromClient + 8);
        char *token;
        token = strtok(argument, "|");
        strcpy(persoana, token);
        token = strtok(NULL, "|");
        strcpy(mesaj, token);
        printf("Persoana: %s Mesajul: %s. \n", persoana, mesaj);
        sprintf(query, "SELECT id FROM users where name='%s';", persoana);
        bzero(rezultat, sizeof(rezultat));
        sqlite3_exec(tdL.db, query, callback, rezultat, &zErrMsg);
        id_persoana = atoi(rezultat);
        if (id_persoana == 0)
        {
          sprintf(toClient, "Utilizatorul %s nu exista", persoana);
        }
        else
        {
          int maxid;
          sprintf(query, "SELECT max(id_mesaj) from messages;");
          bzero(rezultat, sizeof(rezultat));
          sqlite3_exec(tdL.db, query, callback, rezultat, &zErrMsg);
          maxid = atoi(rezultat) + 1;
          sprintf(query, "INSERT INTO messages VALUES(%d,%d,%d,0,'%s',0);", maxid, id_mesager, id_persoana, mesaj);
          //printf("query mesaj: %s", query);
          sqlite3_exec(tdL.db, query, callback, rezultat, &zErrMsg);
          sprintf(toClient, "Mesaj trimis cu succes!");
        }
      }
    }

    else if (strncmp(fromClient, "reply ", 6) == 0)
    {
      if (id_mesager == 0)
      {
        sprintf(toClient, "Trebuie sa fii logat!");
      }
      else
      {
        int id_mesaj = 0;
        int id_destinatar = 0;
        char persoana[100];
        char mesaj[1000];
        strcpy(argument, fromClient + 6);
        char *token;
        token = strtok(argument, "|");
        strcpy(persoana, token);
        token = strtok(NULL, "|");
        strcpy(mesaj, token);
        id_mesaj = atoi(persoana);
        printf("check id %d", id_mesaj);
        sprintf(query, "select id_expeditor from messages where id_mesaj=%d and id_destinatar=%d;", id_mesaj, id_mesager);
        bzero(rezultat, sizeof(rezultat));
        sqlite3_exec(tdL.db, query, callback, rezultat, &zErrMsg);
        id_destinatar = atoi(rezultat);
        if (id_destinatar == 0)
        {
          sprintf(toClient, "Mesajul nu exista sau nu iti este adresat tie!");
        }
        else
        {
          int maxid;
          sprintf(query, "SELECT max(id_mesaj) from messages;");
          bzero(rezultat, sizeof(rezultat));
          sqlite3_exec(tdL.db, query, callback, rezultat, &zErrMsg);
          maxid = atoi(rezultat) + 1;
          sprintf(query, "INSERT INTO messages VALUES(%d,%d,%d,0,'%s',%d);", maxid, id_mesager, id_destinatar, mesaj, id_mesaj);
          printf("query mesaj: %s", query);
          sqlite3_exec(tdL.db, query, callback, rezultat, &zErrMsg);
          sprintf(toClient, "Mesaj trimis cu succes!");
        }
      }
    }

    else if (strncmp(fromClient, "see new messages", 16) == 0)
    {
      if (id_mesager == 0)
      {
        sprintf(toClient, "Trebuie sa fii logat!");
      }
      else
      {
        sprintf(query, "select id_mesaj,name,text_mesaj,isReplyTo from messages m join users u on m.id_expeditor=u.id where m.isSeen=0 and m.id_destinatar=%d;", id_mesager);
        bzero(rezultat, sizeof(rezultat));
        sqlite3_exec(tdL.db, query, callback_afisare, rezultat, &zErrMsg);
        printf("rezultatul: %s", rezultat);
        sprintf(toClient, "Mesaje noi:\n %s", rezultat);
        sprintf(query, "UPDATE messages set isSeen=1 where id_destinatar=%d;", id_mesager);
        sqlite3_exec(tdL.db, query, callback_afisare, rezultat, &zErrMsg);
      }
    }

    else if (strncmp(fromClient, "checkmessages", 13) == 0)
    {
      if (id_mesager != 0)
      {
        int check = 0;
        sprintf(query, "select count(*) from messages where id_destinatar=%d and isSeen=0;", id_mesager);
        bzero(rezultat, sizeof(rezultat));
        sqlite3_exec(tdL.db, query, callback, rezultat, &zErrMsg);
        check = atoi(rezultat);
        if (check == 0)
        {
          sprintf(toClient, "nimic");
        }
        else
        {
          sprintf(query, "select id_mesaj,name,text_mesaj,isReplyTo from messages m join users u on m.id_expeditor=u.id where m.isSeen=0 and m.id_destinatar=%d;", id_mesager);
          bzero(rezultat, sizeof(rezultat));
          sqlite3_exec(tdL.db, query, callback_afisare, rezultat, &zErrMsg);
          printf("rezultatul daca ajunge pana aici: %s", rezultat);
          sprintf(toClient, "Mesaje noi:\n %s \n[client]Introduceti comanda: ", rezultat);
          sprintf(query, "UPDATE messages set isSeen=1 where id_destinatar=%d;", id_mesager);
          sqlite3_exec(tdL.db, query, callback_afisare, rezultat, &zErrMsg);
        }
      }
      else
      {
        sprintf(toClient, "nimic");
      }
    }

    else if (strncmp(fromClient, "see conversation ", 17) == 0)
    {
      if (id_mesager == 0)
      {
        sprintf(toClient, "Trebuie sa fii logat!");
      }
      else
      {
        int id_destinatar = 0;
        strcpy(argument, fromClient + 17);
        sprintf(query, "select id from users where name='%s';", argument);
        bzero(rezultat, sizeof(rezultat));
        sqlite3_exec(tdL.db, query, callback, rezultat, &zErrMsg);
        id_destinatar = atoi(rezultat);
        if (id_destinatar == 0)
        {
          sprintf(toClient, "Utilizatorul nu exista!");
        }
        else
        {
          sprintf(query, "select id_mesaj,name,text_mesaj,isReplyTo from messages m join users u on m.id_expeditor=u.id where (m.id_expeditor=%d and m.id_destinatar=%d) or (m.id_expeditor=%d and m.id_destinatar=%d)  order by id_mesaj;", id_mesager, id_destinatar, id_destinatar, id_mesager);
          bzero(rezultat, sizeof(rezultat));
          sqlite3_exec(tdL.db, query, callback_afisare, rezultat, &zErrMsg);
          sprintf(toClient, "Conversatia:\n %s", rezultat);
        }
      }
    }

    else if (strncmp(fromClient, "quit", 4) == 0)
    {
      close(tdL.cl);
      break;
    }
    else
    {
      sprintf(toClient, "Comenzile disponibile sunt:\nlogin <Nume utilizator>\nregister <Nume Utilizator>\nmessage <Destinatar> | <Text Mesaj>\nsee new messages\nreply <ID mesaj>|<text mesaj>\nsee conversation <Nume Utilizator>\nqui\nnamechange <Nume nou> ");
    }

    if (strncmp(fromClient, "quit", 4) != 0)
    {

      if (write(tdL.cl, toClient, sizeof(toClient)) <= 0)
      {
        printf("[Thread %d] ", tdL.idThread);
        perror("[Thread]Eroare la write() catre client.\n");
      }
      else
        printf("[Thread %d]Mesajul a fost trasmis cu succes.\n", tdL.idThread);
      /////////////////////////////////////////////
    }
  }

  return (NULL);
};
