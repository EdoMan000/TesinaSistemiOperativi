// CLIENT 
//---------------------------------
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h> 
#include <errno.h>
#include <sys/types.h>         
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <signal.h>

//---------------------------------
#define MESS_SIZE_MAX  1024
int con_s;
int gid = -1;
int semds;
int condizione = 0;
int condizione2 = 1;

union semun {
   int              val;    /* Value for SETVAL */
   struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
   unsigned short  *array;  /* Array for GETALL, SETALL */
   struct seminfo  *__buf;  /* Buffer for IPC_INFO
                               (Linux-specific) */
};

//---------------------------------

//GESTORE SIGINT
void handler()
{
	puts(" ");
	if(write(con_s,"quit\n",strlen("quit\n")) == -1)
	{
		printf("CLIENT - errore nella comunicazione di chiusura socket.\n [stderr:%s]\n",strerror(errno));
		exit(-1);
	}
	close(con_s);
	exit(0);
}

//THREAD DI GESTIONE DELLA CONNESSIONE COL SERVER
void* ListenThread()
{
	int n;
	ssize_t ret;
    char    c, buffer[MESS_SIZE_MAX];
    struct sembuf op;
	while(1)
	{
WHILE2:
		//--------------------------------------
		for ( n = 0; n < MESS_SIZE_MAX-1; n++ )
		{
			ret = read(con_s, &c, 1);
			if (ret == 1)
			{
				buffer[n] = c;
				if (c == '\n') break;
			}
			else{
				if (ret == 0 && n!=0) break;
				else
				{
					if ( errno == EINTR ) continue;
					exit(-1);
				}
			}
		}
		if(n == (MESS_SIZE_MAX-1))
		{
			buffer[n] = '\0';
		}else{
			buffer[n+1] = '\0';
		}
		//GESTIONE RISPOSTE DA SERVER + SEMAFORO(SISTEMA DI ACKNOWLEDGEMENT ------------------
		if(strcmp(buffer,"ack\n")==0)
		{
			op.sem_num = 0;
			op.sem_op = 1;
			op.sem_flg = 0;
			
			if(semop(semds, &op, 1) == -1)
			{
				printf("CLIENT THREAD - Errore nella operazione su semaforo.\n");
				exit(-1);
			}
			goto WHILE2;
		}
		if(strcmp(buffer, "SERVER - Group ID non trovato.\n") == 0)
			condizione2 = 0;
		if(strcmp(buffer,"SERVER - Il proprietario ha eliminato il gruppo!\n")==0)
		{
			gid = -1;
		}
		if(strcmp(buffer,"SERVER - GroupID già esistente. Unirsi al gruppo o ritentare la creazione con un diverso ID.\n")==0)
		{
			condizione = 1;
			op.sem_num = 0;
			op.sem_op = 1;
			op.sem_flg = 0;
			
			if(semop(semds, &op, 1) == -1)
			{
				printf("CLIENT THREAD - Errore nella operazione su semaforo.\n");
				exit(-1);
			}
			goto WHILE2;
		}
		
		//STAMPA MESSAGGIO--------------------------
		printf("\n%s",buffer);
		fflush(stdout);
	}
}

int main()
{
	//////////////////////////////////// GESTIONE SEGNALE SIGINT
	struct sigaction act;
	sigset_t set;
	sigfillset(&set);
	act.sa_sigaction=handler;
	act.sa_mask=set;
	act.sa_flags=0;
	sigaction(SIGINT,&act,0);
	///////////////////////////////////
	//VARIABILI---------------------------------
	short int portNUM;
	int ret;
	int gNUM;
	pthread_t tid;
	char* identifier;
	char tempo[24];
	char flush_stdin[MESS_SIZE_MAX];
	char server_ip[16];
	char buffer[MESS_SIZE_MAX];
	struct sockaddr_in server_addr;
	struct sembuf op;
	union semun arg;
	
	//SEMAFORO--------------------------------
	if((semds = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666)) == -1)
	{
		printf("CLIENT - Errore nella creazione del semaforo.\n");
		exit(-1);
	}
	
	arg.val = 0;
	
	if(semctl(semds, 0, SETVAL, arg) == -1)
	{
		printf("CLIENT - Errore nella inizializzazione del semaforo.\n");
		exit(-1);
	}

	//START---------------------------------
RETRY:
	printf("CLIENT - Salve, inserire indirizzo IP server:");
	scanf("%[^\n]",server_ip);
	getchar();
	
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	/*  CONVERSIONE
		============
		inet_aton() converts the Internet host address cp from  the  IPv4  num‐
       bers-and-dots  notation  into  binary  form (in network byte order)
    */
	if(inet_aton(server_ip, &server_addr.sin_addr) == 0)
	{
		printf("CLIENT - indirizzo ip:'%s' non valido.\n",server_ip);
		goto RETRY;
	}
AGAIN:
	printf("CLIENT - Salve, inserire numero di porta:");
	scanf("%hd",&portNUM);
	getchar();
	int dummy=(int)portNUM;
	if(dummy<1024 || dummy>49151)
	{
		printf("CLIENT - port number '%d' non valido.\n",portNUM);
		goto AGAIN;
	}
	/*  CONVERSIONE
		============
		The htons() function converts the unsigned short integer hostshort from
       host byte order to network byte order.
	*/
	server_addr.sin_port = htons(portNUM);
	printf("CLIENT - tentativo di connessione al server [ip:%s|port:%hd] in corso...\n",server_ip,portNUM);
	//CREAZIONE SOCKET PER CONNESSIONE---------------------------------
	if((con_s = socket(AF_INET, SOCK_STREAM, 0))<0)
	{
		printf("CLIENT - Errore creazione socket per connessione.\n [stderr:%s]\n",strerror(errno));
		exit(-1);
	}
	if(connect(con_s, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
	{
		printf("CLIENT - tentativo di connessione fallito.\n");
		exit(-1);
	}
   	if((pthread_create(&tid,0,(void*)ListenThread,NULL)!=0))
   	{
    	printf("CLIENT - Errore nella creazione del thread di ascolto.\n \t[stderr:%s]\n",strerror(errno));
		raise(SIGINT);
	}
	
	
	printf("CLIENT - connessione stabilita server[ip:%s|port:%hd] con successo. Attendi l'accettazione.\n",server_ip,portNUM); 
	
	//Attesa accettazione della connessione---------------------------
	op.sem_num = 0;
	op.sem_op = -1;
	op.sem_flg = 0;
	
	if(semop(semds, &op, 1) == -1)
	{
		printf("CLIENT - Errore nella operazione su semaforo.\n");
		raise(SIGINT);
	}
	//Connessione accettata-------------------------------------------
	
	printf("CLIENT - connessione accettata dal server.\n");
	
	//IDENTIFICAZIONE-----------------------------------------------------------
IDENT:
	printf("CLIENT - Inserire un nome(max 16 char) con il quale essere identificato: ");
	scanf("%m[^\n]",&identifier);
	getchar();
	if((int)strlen(identifier) > 16 )
	{
		printf("CLIENT - nome troppo lungo!\n");
		free(identifier);
		goto IDENT;
	}
	char* temp;
	temp=malloc(strlen(identifier)+1);
	if(temp==NULL)
	{
		printf("CLIENT - Errore malloc.\n \t[stderr:%s]\n",strerror(errno));
		raise(SIGINT);
	}
	memset(temp,0,strlen(temp));
	strcat(temp,identifier);
	strcat(temp,"\n");
	//---------------------------------
	if(write(con_s,temp,strlen(temp))==-1)
	{
		printf("CLIENT - errore nella scrittura del nome '%s' sul socket.\n [stderr:%s]\n",temp,strerror(errno));
		raise(SIGINT);
	}
	//--------------------------------------------------------------------------
	while(1)
	{
WHILE:
		memset(buffer, 0, sizeof(char)*MESS_SIZE_MAX);
		printf("CLIENT - cosa vuoi fare? (digitare 'help' per istruzioni) ");
		fflush(stdout);
		scanf("%[^\n]",buffer);
		getchar();
		//gestione help-----------------------------------------------
		if(!strcmp("help",buffer))
		{
			puts("============================");
			printf("CLIENT - LISTA COMANDI:\n============================\n");
			printf("||'visualizza'|| -> stampa a schermo situazione gruppi corrente\n");
			printf("||'scrivi'|| -> inviare messaggio nella chat di gruppo corrente\n");
			printf("||'crea'|| -> creazione nuova chat di gruppo\n");
			printf("||'entra'|| -> entra in una chat di gruppo\n");
			printf("||'lascia'|| -> lascia la chat di gruppo corrente\n");
			printf("||'cancella'|| -> elimina la chat di gruppo corrente (solo proprietario)\n");
			printf("||'quit'|| -> chiusura programma\n");
			puts("----------------------------------------------------------");
			goto WHILE;
		}
		//stampa a schermo situazione gruppi-------------------
		if(!strcmp("visualizza",buffer))
		{
			strcat(buffer,"\n");
			if((ret=write(con_s, buffer, strlen(buffer)))==-1)
			{
				printf("CLIENT - errore nella comunicazione di visualizzazione.\n [stderr:%s]\n",strerror(errno));
				raise(SIGINT);
			}
			//attendo ack -----
			op.sem_num = 0;
			op.sem_op = -1;
			op.sem_flg = 0;
			
			if(semop(semds, &op, 1) == -1)
			{
				printf("CLIENT - Errore nella operazione su semaforo.\n");
				raise(SIGINT);
			}
			goto WHILE;
		}
		//scrivo----------------------------------------------
		if(!strcmp("scrivi",buffer))
		{
			if(gid==-1) //per evitare la print successiva se non si è in un gruppo
			{
				printf("CLIENT - prima di scrivere devi essere in un gruppo!\n");
				goto WHILE;
			}
			
			printf("CLIENT - adesso sei in modalità scrittura. Per uscire scrivi 'exit'.\n");
GOBACK:
			memset(buffer, 0, sizeof(char)*MESS_SIZE_MAX);
			fgets(buffer, MESS_SIZE_MAX, stdin);
			if(gid==-1) //necessaria se si viene cacciati dal gruppo quando viene chiuso
			{
				printf("CLIENT - prima di scrivere devi essere in un gruppo!\n");
				goto WHILE;
			}
			if(strcmp(buffer, "exit\n") == 0) goto WHILE;
			if((ret=write(con_s, buffer, strlen(buffer)))==-1)
			{
				if(errno == EINTR) goto GOBACK;
				printf("CLIENT - errore nella scrittura sul socket.\n [stderr:%s]\n",strerror(errno));
				raise(SIGINT);
			}
			goto GOBACK;
		}
		// creazione gruppo (impostare gid a 0) ----------------------------------
		if(!strcmp("crea",buffer))
		{
			if(gid != -1)
			{
				printf("CLIENT - creare un gruppo mentre si è già in un altro comporta l'uscita da quello corrente.\n Continuare?(Si = y; NO = n): ");
IF:
				fgets(flush_stdin,MESS_SIZE_MAX,stdin);
				if(strcmp(flush_stdin,"n\n")==0) goto WHILE;
				else{
					if(strcmp(flush_stdin,"y\n")!=0)
					{
						printf("Rispondi 'y' per dire si oppure 'n' per dire no: ");
						goto IF;
					}
					//risposta affermativa -> invio richiesta di lasciare il gruppo al server
					if((ret=write(con_s, "lascia\n", strlen("lascia\n")))==-1)
					{
						printf("CLIENT - errore nella comunicazione di creazione gruppo.\n [stderr:%s]\n",strerror(errno));
						raise(SIGINT);
					}
					//attendo ack -----
					op.sem_num = 0;
					op.sem_op = -1;
					op.sem_flg = 0;
					
					if(semop(semds, &op, 1) == -1)
					{
						printf("CLIENT - Errore nella operazione su semaforo.\n");
						raise(SIGINT);
					}
					gid=-1;
				}
			}
			//creazione gruppo effettiva -------------------------------------
			printf("CLIENT - specificare un GroupID per il nuovo gruppo: ");
			scanf("%d",&gNUM);
			getchar();
			sprintf(tempo," %d ",gNUM);
			strcat(buffer,tempo);
			strcat(buffer,"\n");
			if((ret=write(con_s, buffer, strlen(buffer)))==-1)
			{
				printf("CLIENT - errore nella comunicazione di creazione gruppo.\n [stderr:%s]\n",strerror(errno));
				raise(SIGINT);
			}
			//attendo ack -----
			op.sem_num = 0;
			op.sem_op = -1;
			op.sem_flg = 0;
			
			if(semop(semds, &op, 1) == -1)
			{
				printf("CLIENT - Errore nella operazione su semaforo.\n");
				raise(SIGINT);
			}
			
			if(condizione)
			{
				printf("SERVER - GroupID già esistente. Unirsi al gruppo o ritentare la creazione con un diverso ID.\n");
				condizione = 0;
				goto WHILE;
			}
			
			gid=0;
			goto WHILE;
		}
		//entra in un gruppo (impostare gid a 0)--------------------------------------
		if(!strcmp("entra",buffer))
		{
			if(gid != -1)
			{
				printf("CLIENT - sei già in un gruppo. Vuoi uscire da quello corrente?\n(Si = y; NO = n): ");
ELIF:
				fgets(flush_stdin,MESS_SIZE_MAX,stdin);
				if(strcmp(flush_stdin,"n\n")==0) goto WHILE;
				else{
					if(strcmp(flush_stdin,"y\n")!=0)
					{
						printf("Rispondi y per dire si oppure n per dire no.\n");
						goto ELIF;
					}
					//risposta affermativa -> invio richiesta di lasciare il gruppo al server
					if((ret=write(con_s, "lascia\n", strlen("lascia\n")))==-1)
					{
						printf("CLIENT - errore nella comunicazione di entrata in gruppo.\n [stderr:%s]\n",strerror(errno));
						raise(SIGINT);
					}
					//attendo ack -----
					op.sem_num = 0;
					op.sem_op = -1;
					op.sem_flg = 0;
					
					if(semop(semds, &op, 1) == -1)
					{
						printf("CLIENT - Errore nella operazione su semaforo.\n");
						raise(SIGINT);
					}
					gid = -1;
				}
			}	
			//entrata gruppo effettiva ------------------------
			printf("CLIENT - in quale chat di gruppo vuoi entrare?\n (specificare un GroupID, '-1' per visualizzare i Gruppi esistenti): ");
			scanf("%d",&gNUM);
			getchar();
			sprintf(tempo," %d ",gNUM);
			strcat(buffer,tempo);
			strcat(buffer,"\n");
			if((ret=write(con_s, buffer, strlen(buffer)))==-1)
			{
				printf("CLIENT - errore nella comunicazione di numero gruppo da creare.\n [stderr:%s]\n",strerror(errno));
				raise(SIGINT);
			}
			//attendo ack -----
			op.sem_num = 0;
			op.sem_op = -1;
			op.sem_flg = 0;
			
			if(semop(semds, &op, 1) == -1)
			{
				printf("CLIENT - Errore nella operazione su semaforo.\n");
				raise(SIGINT);
			}
			
			if(gNUM != -1 && condizione2 != 0) gid=0;
			condizione2 = 1;
			goto WHILE;
		}
		//lascio gruppo (impostare gid a -1)---------------------------------------------
		if(!strcmp("lascia",buffer))
		{
			strcat(buffer,"\n");
			if((ret=write(con_s, buffer, strlen(buffer)))==-1)
			{
				printf("CLIENT - errore nella comunicazione di uscita da gruppo.\n [stderr:%s]\n",strerror(errno));
				raise(SIGINT);
			}
			//attendo ack -----
			op.sem_num = 0;
			op.sem_op = -1;
			op.sem_flg = 0;
			
			if(semop(semds, &op, 1) == -1)
			{
				printf("CLIENT - Errore nella operazione su semaforo.\n");
				raise(SIGINT);
			}
			gid=-1;
			goto WHILE;
		}
		//cancello gruppo (impostare gid a -1, NB: solo se si è owner!)------------------------
		if(!strcmp("cancella",buffer))
		{
			if(gid==-1)
			{
				printf("CLIENT - non sei in alcun gruppo!\n");
				goto WHILE;
			}
			strcat(buffer,"\n");
			if((ret=write(con_s, buffer, strlen(buffer)))==-1)
			{
				printf("CLIENT - errore nella comunicazione di eliminazione gruppo.\n [stderr:%s]\n",strerror(errno));
				raise(SIGINT);
			}
			//attendo ack -----
			op.sem_num = 0;
			op.sem_op = -1;
			op.sem_flg = 0;
			
			if(semop(semds, &op, 1) == -1)
			{
				printf("CLIENT - Errore nella operazione su semaforo.\n");
				raise(SIGINT);
			}
			gid=-1;
			goto WHILE;
		}
		//chiusura ------------------------------------------------------------------------------
		if(!strcmp("quit",buffer))
		{
			raise(SIGINT);
		}
		//---------------------------
		printf("CLIENT - Non ho capito!\n");
	}
}
