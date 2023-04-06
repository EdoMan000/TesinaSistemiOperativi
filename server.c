// SERVER 
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
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <string.h>
#include <signal.h>
#include <sys/syscall.h>

//---------------------------------
#define MESS_SIZE_MAX  1024
#define BACKLOG 10

int semds;
int semds2;
int semds3;

union semun {
	int              val;    /* Value for SETVAL */
	struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
	unsigned short  *array;  /* Array for GETALL, SETALL */
	struct seminfo  *__buf;  /* Buffer for IPC_INFO
		                       (Linux-specific) */
};
//---------------------------------

//CATTURA SIGPIPE--------------------------------
void handler()
{
	printf("HANDLER - SIGPIPE catturato!\n");
	return;
}

//STRUTTURA LISTA SOCKET==========================================================================

struct socket_node{
	int descr;
	int index;
	struct socket_node *next;
};

struct socket_node *s_head;

void append_sock(struct socket_node *new)
{
	struct sembuf op;
	
	//SEZIONE CRITICA---------------------------------
	op.sem_num = 0;
	op.sem_op = -1;
	op.sem_flg = 0;
	
	if(semop(semds3, &op, 1) == -1)
	{
		printf("Thread - Errore nella operazione su semaforo 3.\n");
		exit(-1);
	}	
	//-------------------------------------------------
	new->next=s_head->next;
	s_head->next=new;
	//FINE SEZIONE CRITICA---------------------------------
	op.sem_num = 0;
	op.sem_op = 1;
	op.sem_flg = 0;
	
	if(semop(semds3, &op, 1) == -1)
	{
		printf("Thread - Errore nella operazione su semaforo 3.\n");
		exit(-1);
	}	
	//-------------------------------------------------
	return;
}

void delete_sock(int ds)
{
	struct sembuf op;
	
	//SEZIONE CRITICA---------------------------------
	op.sem_num = 0;
	op.sem_op = -1;
	op.sem_flg = 0;
	
	if(semop(semds3, &op, 1) == -1)
	{
		printf("Thread - Errore nella operazione su semaforo 3.\n");
		exit(-1);
	}	
	//-------------------------------------------------
	struct socket_node *curr=s_head->next;
	struct socket_node *prev=s_head;
	while(curr!=NULL){
		if(curr->descr==ds){
			prev->next=curr->next;
			free(curr);
			break;
		}	
		curr=curr->next;
		prev=prev->next;
	}
	//FINE SEZIONE CRITICA---------------------------------
	op.sem_num = 0;
	op.sem_op = 1;
	op.sem_flg = 0;
	
	if(semop(semds3, &op, 1) == -1)
	{
		printf("Thread - Errore nella operazione su semaforo 3.\n");
		exit(-1);
	}	
	//-------------------------------------------------
	return;
}

//STRUTTURA LISTA GESTIONE GRUPPI=================================================================
struct group_node{
	int gid;
	const struct membr_node *owner;
	struct group_node *next;
	struct membr_node *members;
};

struct membr_node{
	char name[16];
	int gid;
	pid_t tid;
	int socket;
	struct membr_node *next;
	struct group_node *group;
};

struct group_node *head;

void append_group(int gid,struct membr_node *owner)
{
	struct sembuf op;
	//SEZIONE CRITICA---------------------------------
	op.sem_num = 0;
	op.sem_op = -1;
	op.sem_flg = 0;
	
	if(semop(semds2, &op, 1) == -1)
	{
		printf("Thread - Errore nella operazione su semaforo 2.\n");
		exit(-1);
	}	
	//-------------------------------------------------
	char buff[1024];
	struct group_node *new = malloc(sizeof(struct group_node));
	new->gid = gid;
	new->owner = owner;
	new->members = owner;
	new->next = head->next;
	
	head->next = new;
	
	owner->gid = gid;
	owner->group = new;
	//FINE SEZIONE CRITICA--------------------------------
	op.sem_num = 0;
	op.sem_op = 1;
	op.sem_flg = 0;
	
	if(semop(semds2, &op, 1) == -1)
	{
		printf("Thread - Errore nella operazione su semaforo 2.\n");
		exit(-1);
	}	
	//-------------------------------------------------
	sprintf(buff, "SERVER - creazione gruppo[id:%d] eseguita con successo\n",gid);
			
	if(write(owner->socket, buff, strlen(buff))== -1)
	{
		printf("SERVER - Errore nella risposta al client %s.\n", owner->name);
		pthread_exit("Errore");
	}
	return;
}

void append_member(int gid, struct membr_node *member)
{
	//SEZIONE CRITICA----------------------------------------
	struct sembuf op;
	
	op.sem_num = 0;
	op.sem_op = -1;
	op.sem_flg = 0;
	
	if(semop(semds2, &op, 1) == -1)
	{
		printf("Thread - Errore nella operazione su semaforo 2.\n");
		exit(-1);
	}
	//-------------------------------------------------
	char buff[1024];
	struct group_node *curr = head->next;
	while(curr != NULL)
	{
		if(curr->gid == gid)
		{
			member->gid = gid;
			member->next = curr->members;
			curr->members = member;
			member->group = curr;
			
			printf("SERVER - unione a gruppo %d eseguita!\n",gid);
			sprintf(buff, "SERVER - sei stato aggiunto al gruppo[id:%d]\n",gid);
			
			if(write(member->socket, buff, strlen(buff))== -1)
			{
				printf("SERVER - Errore nella risposta al client %s.\n", member->name);
				pthread_exit("Errore");
			}
			//FINE SEZIONE CRITICA-------------------------------------------------
			op.sem_num = 0;
			op.sem_op = 1;
			op.sem_flg = 0;
			
			if(semop(semds2, &op, 1) == -1)
			{
				printf("Thread - Errore nella operazione su semaforo 2.\n");
				exit(-1);
			}
			//-------------------------------------------------
			return;
		}
		curr = curr->next;
	}
	//FINE SEZIONE CRITICA-------------------------------------------------
	op.sem_num = 0;
	op.sem_op = 1;
	op.sem_flg = 0;
	
	if(semop(semds2, &op, 1) == -1)
	{
		printf("Thread - Errore nella operazione su semaforo 2.\n");
		exit(-1);
	}
	//-------------------------------------------------
	printf("SERVER - Group ID non trovato.\n");
	sprintf(buff, "SERVER - Group ID non trovato.\n");
	
	if(write(member->socket, buff, strlen(buff))== -1)
	{
		printf("SERVER - Errore nella risposta al client %s.\n", member->name);
		pthread_exit("Errore");
	}
	return;
}

void delete_group(struct membr_node *member)
{
	struct sembuf op;
	
	//SEZIONE CRITICA--------------------------------
	
	op.sem_num = 0;
	op.sem_op = -1;
	op.sem_flg = 0;
	
	if(semop(semds2, &op, 1) == -1)
	{
		printf("Thread - Errore nella operazione su semaforo 2.\n");
		exit(-1);
	}
	//-------------------------------------------------
	char buff[1024];
	if(!(member == (member->group)->owner))
	{
		sprintf(buff, "SERVER - non puoi eliminare il gruppo, non sei il proprietario!\n");
		if(write(member->socket, buff, strlen(buff))== -1)
		{
			printf("SERVER - Errore nella risposta al client %s.\n", member->name);
			pthread_exit("Errore");
		}
		//FINE SEZIONE CRITICA-------------------------------------------------
		op.sem_num = 0;
		op.sem_op = 1;
		op.sem_flg = 0;
		
		if(semop(semds2, &op, 1) == -1)
		{
			printf("Thread - Errore nella operazione su semaforo 2.\n");
			exit(-1);
		}
		//-------------------------------------------------
		return;
	}
	
	struct membr_node *curr = (member->group)->members;
	struct membr_node *prev = (member->group)->members;
	struct group_node *group = member->group;
	curr = curr->next;
	
	while(prev != NULL)
	{
		prev->next = NULL;
		prev->group = NULL;
		prev->gid = -1;
		if(member!=prev)	
		{
			sprintf(buff, "SERVER - Il proprietario ha eliminato il gruppo!\n");
			if(write(prev->socket, buff, strlen(buff)) == -1)
			{
				printf("SERVER - Errore nella risposta al client %s.\n", prev->name);
				pthread_exit("Errore");
			}	
		}
		prev = curr;
		if(curr != NULL)
			curr = curr->next;
	}
	
	sprintf(buff, "SERVER - hai eliminato il gruppo!\n");
	if(write(member->socket, buff, strlen(buff)) == -1)
	{
		printf("SERVER - Errore nella risposta al client %s.\n", member->name);
		pthread_exit("Errore");
	}	
	group->members = NULL;
	struct group_node *Gcurr = head->next;
	struct group_node *Gprev = head;
	while(Gcurr != NULL)
	{
		if(Gcurr == group)
		{
			Gprev->next = Gcurr->next;
			break;
		}
		Gprev = Gprev->next;
		Gcurr = Gcurr->next;
	}
	member->gid = -1;
	member->next = NULL;
	member->group = NULL;
	printf("SERVER - eliminazione gruppo %d eseguita!\n",group->gid);
	fflush(stdout);
	free(group);
	//FINE SEZIONE CRITICA-------------------------------------------------
	op.sem_num = 0;
	op.sem_op = 1;
	op.sem_flg = 0;
	
	if(semop(semds2, &op, 1) == -1)
	{
		printf("Thread - Errore nella operazione su semaforo 2.\n");
		exit(-1);
	}
	
	return;
}
			
void delete_member(struct membr_node *member)
{
	struct sembuf op;
	
	if(member->gid == -1)
		return;
		
	struct membr_node *curr = (member->group)->members;
	struct membr_node *prev = (member->group)->members;
	curr = curr->next;
	if(prev==member)
	{
		(prev->group)->members = (prev->next);
		free(prev);
	}
	else
	{
		while(curr != NULL)
		{
			if(curr==member)
			{
				prev->next = curr->next;
				free(curr);
				break;
			}
			prev = prev->next;
			curr = curr->next;
		}
	}
	
	return;
}

void exit_member(struct membr_node *member)
{
	char buff[1024];
	if(member->gid == -1)
	{
		sprintf(buff, "SERVER - non puoi lasciare un gruppo, non sei ancora entrato!\n");
		if(write(member->socket, buff, strlen(buff))== -1)
		{
			printf("SERVER - Errore nella risposta al client %s.\n", member->name);
			pthread_exit("Errore");
		}
		return;
	}
	struct membr_node *curr = (member->group)->members;
	struct membr_node *prev = (member->group)->members;
	curr = curr->next;
	
	if(prev==member)
	{
		if(prev->next == NULL)
			(prev->group)->members = NULL;
		else
			(prev->group)->members = (prev->next);
		prev->gid=-1;
		prev->next=NULL;
		prev->group=NULL;
	}else{
		while(curr != NULL)
		{
			if(curr == member)
			{
				prev->next = curr->next;
				curr->gid=-1;
				curr->next=NULL;
				curr->group=NULL;
			}
			prev = prev->next;
			curr = curr->next;
		}
	}
	printf("SERVER - uscita da gruppo eseguita!\n");
	fflush(stdout);
	sprintf(buff, "SERVER - uscita da gruppo eseguita!\n");
	if(write(member->socket, buff, strlen(buff))== -1)
	{
		printf("SERVER - Errore nella risposta al client %s.\n", member->name);
		pthread_exit("Errore");
	}
	return;
}

int verify_gid(int gNUM)
{	
	struct sembuf op;
	
	//SEZIONE CRITICA---------------------------------
	
	op.sem_num = 0;
	op.sem_op = -1;
	op.sem_flg = 0;
	
	if(semop(semds2, &op, 1) == -1)
	{
		printf("Thread - Errore nella operazione su semaforo 2.\n");
		exit(-1);
	}
	//-------------------------------------------------
	if(head->next == NULL) goto VERIFY; //gid non utilizzato
	
	struct group_node *curr = head->next;
	
	while(curr != NULL)
	{
		if(gNUM == curr->gid) 
		{
			//FINE SEZIONE CRITICA-------------------------------------------------
			op.sem_num = 0;
			op.sem_op = 1;
			op.sem_flg = 0;
			
			if(semop(semds2, &op, 1) == -1)
			{
				printf("Thread - Errore nella operazione su semaforo 2.\n");
				exit(-1);
			}
			//-------------------------------------------------
			return 1; //gid già utilizzato
		}
		curr = curr->next;
	}
VERIFY:
	//FINE SEZIONE CRITICA-------------------------------------------------
	op.sem_num = 0;
	op.sem_op = 1;
	op.sem_flg = 0;
	
	if(semop(semds2, &op, 1) == -1)
	{
		printf("Thread - Errore nella operazione su semaforo 2.\n");
		exit(-1);
	}
	//-------------------------------------------------
	return 0; //gid non utilizzato
}

void notifica_uscita(struct membr_node *member)
{
	struct sembuf op;
	
	if(member->gid == -1)
		return;
	
	char str_gNUM[1024];
	struct membr_node *curr = (member->group)->members;
	memset(str_gNUM,0,1024);
	
	while(curr != NULL)
	{
		if(curr != member)
		{
			sprintf(str_gNUM, "SERVER - %s ha lasciato il gruppo!\n",member->name);
			if(write(curr->socket, str_gNUM, strlen(str_gNUM)) == -1)
			{
				printf("SERVER - Errore nella risposta al client %s.\n", curr->name);
				pthread_exit("Errore");
			}	
		}
		curr = curr->next;
	}
	
	return;
}
//=============================================================================================

void* ConnectionThread(struct socket_node *node)
{
	struct sembuf op;
	
	struct socket_node *myNode = node;
	//RILASCIO GETTONE-----------------------------
	op.sem_num = 0;
	op.sem_op = 1;
	op.sem_flg = 0;
	
	if(semop(semds, &op, 1) == -1)
	{
		printf("Thread %d - Errore nella operazione su semaforo.\n", myNode->index);
		exit(-1);
	}
		
	//---------------------------------------------
	int index = myNode->index;
	int sock_descr = myNode->descr;
	int n;
	int gNUM;
	int once=1;
	char* tmp;
	char* temp;
	ssize_t ret;
	char c, buffer[MESS_SIZE_MAX];
    //--------------------------------------------
    
    ret=write(sock_descr, "ack\n", strlen("ack\n"));
	if(ret==-1)
	{
		printf("Thread %d - ERRORE scrittura ack accettazione a sock->%d\n",index,sock_descr);
		delete_sock(sock_descr);
		pthread_exit("errore");
	}
    
	//Creazione nodo membro-------------
	struct membr_node *member = malloc(sizeof(struct membr_node));
	member->socket = sock_descr;
	member->gid = -1;
	member->tid = syscall(SYS_gettid);
	member->next = NULL;
	member->group = NULL;
	//-----------------------------------
	while(1)
	{
START:
		memset(buffer,0,strlen(buffer));
		//---------------------------------
		for ( n = 0; n < MESS_SIZE_MAX-1; n++ )
		{
			ret = read(sock_descr, &c, 1);
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
					pthread_exit("errore");
				}
			}
		}
		if(n == (MESS_SIZE_MAX-1))
		{
			buffer[n] = '\0';
		}else{
			buffer[n+1] = '\0';
		}
		//ricezione nome ------------------
		if(once)
		{
			memcpy(member->name,buffer,strlen(buffer)-1);
			tmp=malloc(MESS_SIZE_MAX);
			once=0;
			goto START;
		}
		//GESTIONE RICHIESTE---------------------------------
		char copy[MESS_SIZE_MAX];
		memcpy(copy,buffer,strlen(buffer));
		temp = strtok(copy," ");
		char str_gNUM[1024];
		//entrare in un gruppo ----------------------------------------
		if(!strcmp("entra",temp))
		{
			printf("SERVER - richiesta entrata in gruppo\n");
			temp = strtok(NULL," ");
			gNUM=atoi(temp);
			if(gNUM==-1)
			{
				goto LIST;
			}
			
			append_member(gNUM,member);
			
			ret=write(sock_descr, "ack\n", strlen("ack\n"));
			if(ret==-1)
			{
				printf("Thread %d - ERRORE scrittura ack entrata a sock->%d\n",index,sock_descr);
				delete_sock(sock_descr);
				pthread_exit("errore");
			}
			
			//SEZIONE CRITICA---------------------------------
			
			op.sem_num = 0;
			op.sem_op = -1;
			op.sem_flg = 0;
			
			if(semop(semds2, &op, 1) == -1)
			{
				printf("Thread - Errore nella operazione su semaforo 2.\n");
				exit(-1);
			}
			//-------------------------------------------------
			struct membr_node *curr = (member->group)->members;
			memset(str_gNUM,0,1024);
			while(curr != NULL)
			{
				if(curr != member)
				{
					sprintf(str_gNUM, "SERVER - %s e' entrato nel gruppo!\n",member->name);
					if(write(curr->socket, str_gNUM, strlen(str_gNUM)) == -1)
					{
						printf("SERVER - Errore nella risposta al client %s.\n", curr->name);
						pthread_exit("Errore");
					}	
				}
				curr = curr->next;
			}
			//FINE SEZIONE CRITICA---------------------------------
			
			op.sem_num = 0;
			op.sem_op = 1;
			op.sem_flg = 0;
			
			if(semop(semds2, &op, 1) == -1)
			{
				printf("Thread - Errore nella operazione su semaforo 2.\n");
				exit(-1);
			}
			//-------------------------------------------------
			goto START;
		}
		//creare un nuovo gruppo -------------------------------------------------------------
		if(!strcmp("crea",temp))
		{
			printf("SERVER - richiesta creazione gruppo\n");
			temp = strtok(NULL," ");
			gNUM=atoi(temp);
			if(verify_gid(gNUM))
			{
				ret=write(sock_descr, "SERVER - GroupID già esistente. Unirsi al gruppo o ritentare la creazione con un diverso ID.\n", strlen("SERVER - GroupID già esistente. Unirsi al gruppo o ritentare la creazione con un diverso ID.\n"));
				if(ret==-1)
				{
					printf("Thread %d - ERRORE scrittura risposta gid esistente a sock->%d\n",index,sock_descr);
					pthread_exit("errore");
				}
				goto START;
			}
			append_group(gNUM,member);
			ret=write(sock_descr, "ack\n", strlen("ack\n"));
			if(ret==-1)
			{
				printf("Thread %d - ERRORE scrittura ack creazione a sock->%d\n",index,sock_descr);
				pthread_exit("errore");
			}
			printf("SERVER - creazione gruppo %d eseguita!\n",gNUM);
			fflush(stdout);
			goto START;
		}
		//stampare lista gruppi correnti ------------------------------------
		if(!strcmp("visualizza\n",buffer))
		{
LIST:
			memset(copy,0,MESS_SIZE_MAX);
			sprintf(copy,"======================\nSERVER - LISTA GRUPPI:\n======================\n");
			//SEZIONE CRITICA---------------------------------
			
			op.sem_num = 0;
			op.sem_op = -1;
			op.sem_flg = 0;
			
			if(semop(semds2, &op, 1) == -1)
			{
				printf("Thread - Errore nella operazione su semaforo 2.\n");
				exit(-1);
			}
			//-------------------------------------------------
			
			struct group_node* curr = head->next;
			if(curr==NULL)
			{
				strcat(copy,"NESSUN GRUPPO ESISTENTE\n");
			}
			while(curr != NULL)
			{
				memset(str_gNUM,0,1024);
				sprintf(str_gNUM,"||GroupID:%d||",curr->gid);
				strcat(copy,str_gNUM);
				struct membr_node* mem = curr->members;
				while(mem != NULL)
				{
					memset(str_gNUM,0,1024);
					sprintf(str_gNUM,"->[%s]",mem->name);
					strcat(copy,str_gNUM);
					mem=mem->next;
				}
				strcat(copy,"\n");
				curr = curr->next;
			}
			//FINE SEZIONE CRITICA---------------------------------
			
			op.sem_num = 0;
			op.sem_op = 1;
			op.sem_flg = 0;
			
			if(semop(semds2, &op, 1) == -1)
			{
				printf("Thread - Errore nella operazione su semaforo 2.\n");
				exit(-1);
			}
			//-------------------------------------------------
			
			strcat(copy,"======================\n");
			ret=write(sock_descr,copy,strlen(copy));
			if(ret==-1)
			{
				printf("Thread %d - ERRORE scrittura risposta gid esistente a sock->%d\n",index,sock_descr);
				pthread_exit("errore");
			}
			ret=write(sock_descr, "ack\n", strlen("ack\n"));
			if(ret==-1)
			{
				printf("Thread %d - ERRORE scrittura ack entrata a sock->%d\n",index,sock_descr);
				pthread_exit("errore");
			}
			goto START;
		}
		//lasciare gruppo corrente ------------------------------------------
		if(!strcmp("lascia\n",buffer))
		{
			printf("SERVER - richiesta uscita da gruppo\n");
			//SEZIONE CRITICA---------------------------------
			
			op.sem_num = 0;
			op.sem_op = -1;
			op.sem_flg = 0;
			
			if(semop(semds2, &op, 1) == -1)
			{
				printf("Thread - Errore nella operazione su semaforo 2.\n");
				exit(-1);
			}
			//-------------------------------------------------
			notifica_uscita(member);
			exit_member(member);
			
			//FINE SEZIONE CRITICA-------------------------------------------------
			op.sem_num = 0;
			op.sem_op = 1;
			op.sem_flg = 0;

			if(semop(semds2, &op, 1) == -1)
			{
				printf("Thread - Errore nella operazione su semaforo 2.\n");
				exit(-1);
			}
			//-------------------------------------------------
			
			ret=write(sock_descr, "ack\n", strlen("ack\n"));
			if(ret==-1)
			{
				printf("Thread %d - ERRORE scrittura ack uscita a sock->%d\n",index,sock_descr);
				pthread_exit("errore");
			}
			goto START;
		}
		//eliminazione gruppo corrente --------------------------------------------
		if(!strcmp("cancella\n",buffer))
		{
			printf("SERVER - richiesta eliminazione gruppo\n");
			delete_group(member);
			ret=write(sock_descr, "ack\n", strlen("ack\n"));
			if(ret==-1)
			{
				printf("Thread %d - ERRORE scrittura ack uscita a sock->%d\n",index,sock_descr);
				pthread_exit("errore");
			}
			goto START;
		}
		//chiusura ------------------------------------
		if(!strcmp("quit\n",buffer))
		{
			//SEZIONE CRITICA---------------------------------
			
			op.sem_num = 0;
			op.sem_op = -1;
			op.sem_flg = 0;
			
			if(semop(semds2, &op, 1) == -1)
			{
				printf("Thread - Errore nella operazione su semaforo 2.\n");
				exit(-1);
			}
			//-------------------------------------------------
			printf("SERVER - richiesta chiusura connessione su sock->%d\n",sock_descr);
			delete_member(member);
			close(sock_descr);
			//FINE SEZIONE CRITICA---------------------------------
			
			op.sem_num = 0;
			op.sem_op = 1;
			op.sem_flg = 0;
			
			if(semop(semds2, &op, 1) == -1)
			{
				printf("Thread - Errore nella operazione su semaforo 2.\n");
				exit(-1);
			}
			//-------------------------------------------------
			printf("SERVER - chiusura sock->%d eseguita, chiudo thread %d\n",sock_descr,index);
			pthread_exit("uscita");
		}
		//---------------------------------
		memset(tmp,0,MESS_SIZE_MAX);
		memcpy(tmp,buffer,strlen(buffer));
		sprintf(buffer,"[%s]: %s",member->name,tmp);
		//scritture messaggi----------------------------------------
		//SEZIONE CRITICA---------------------------------
		
		op.sem_num = 0;
		op.sem_op = -1;
		op.sem_flg = 0;
		
		if(semop(semds2, &op, 1) == -1)
		{
			printf("Thread - Errore nella operazione su semaforo 2.\n");
			exit(-1);
		}
		//-------------------------------------------------
		
		struct membr_node *curr = (member->group)->members;
		while(curr != NULL)
		{
			memset(copy,0,MESS_SIZE_MAX);
			memcpy(copy,buffer,strlen(buffer)-1);
			if(curr != member)
			{
				printf("Thread %d - inoltro mess '%s' a sock->%d \n",index,copy,curr->socket);
				ret=write(curr->socket, buffer, strlen(buffer));
				if(ret==-1)
				{
					printf("Thread %d - ERRORE scrittura messaggio a sock->%d\n",index,curr->socket);
					if(errno==EPIPE)
					{
						printf("SERVER - chiusura connessione su sock->%d e relativo thread %d.\n ",curr->socket, curr->tid);
						close(curr->socket);
						notifica_uscita(curr);
						delete_member(curr);
						kill(curr->tid, SIGINT);
					}else{
						pthread_exit("errore");
					}
				}
			}
			curr = curr->next;
		}
		//FINE SEZIONE CRITICA---------------------------------
		
		op.sem_num = 0;
		op.sem_op = 1;
		op.sem_flg = 0;
		
		if(semop(semds2, &op, 1) == -1)
		{
			printf("Thread - Errore nella operazione su semaforo 2.\n");
			exit(-1);
		}
		//-------------------------------------------------
	}
}



int main(){
	//GESTIONE SEGNALE SIGPIPE IN CASO DI SOCKET ROTTO
	struct sigaction act;
	sigset_t set;
	sigfillset(&set);
	act.sa_sigaction=handler;
	act.sa_mask=set;
	act.sa_flags=0;
	sigaction(SIGPIPE,&act,0);
	///////////////////////////////////
	//VARIABILI---------------------------------
	short int portNUM;
	pthread_t tid;
	int ric_s;
	int index = 0;
	int addr_size;
	char flush_stdin[MESS_SIZE_MAX];
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	//---------------------------------
	head = malloc(sizeof(struct group_node));
	head->gid = 0;
	head->owner = NULL;
	head->next = NULL;
	head->members = NULL;
	
	s_head = malloc(sizeof(struct socket_node));
	s_head->descr = -1;
	s_head->index = -1;
	s_head->next = NULL;
RETRY:
	memset(&portNUM,0,sizeof(portNUM));
	printf("SERVER - Salve, inserire numero di porta:");
	scanf("%hd",&portNUM);
	getchar();
	int dummy=(int)portNUM;
	if(dummy<1024 || dummy>49151)
	{
		printf("SERVER: port number non valido.\n");
		fgets(flush_stdin,MESS_SIZE_MAX,stdin);
		goto RETRY;
	}
	//CREAZIONE SOCKET DI RICEZIONE ---------------------------
	
    if ((ric_s = socket(AF_INET, SOCK_STREAM, 0))<0)
	{
		printf("SERVER - Errore creazione socket di ricezione .\n [stderr:%s]\n",strerror(errno));
		exit(-1);
    }
    //Gestione errore 'Address already in use'------------------
    int True=1;
    if(setsockopt(ric_s,SOL_SOCKET,SO_REUSEADDR,&True,sizeof(int)) == -1)
    {
    	printf("SERVER - Errore setsockopt.\n");
    	exit(-1);
    }
    //---------------------------------------------
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port        = htons(portNUM);
    
     if ( bind(ric_s, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0 )
	{
		printf("SERVER - Errore nella bind dell'indirizzo al socket di ricezione.\n [stderr:%s]\n",strerror(errno));
		exit(-1);
    }

    if ( listen(ric_s, BACKLOG) < 0 )
	{
		printf("SERVER - Errore nella listen.\n \t[stderr:%s]\n",strerror(errno));
		exit(-1);
    }
    
    //Semaforo---------------------------------------------------
    if((semds = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666)) == -1)
	{
		printf("CLIENT - Errore nella creazione del semaforo 1.\n");
		exit(-1);
	}
    
    struct sembuf op;
	union semun arg;
    
    arg.val = 1;
    
    if(semctl(semds, 0, SETVAL, arg) == -1)
	{
		printf("SERVER - Errore nella inizializzazione del semaforo 1.\n");
		exit(-1);
	}
    
    //Semaforo 2-------------------------------------------------
    if((semds2 = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666)) == -1)
	{
		printf("CLIENT - Errore nella creazione del semaforo 2.\n");
		exit(-1);
	}	
	
    if(semctl(semds2, 0, SETVAL, arg) == -1)
	{
		printf("SERVER - Errore nella inizializzazione del semaforo 2.\n");
		exit(-1);
	}
    
    //Semaforo 3-------------------------------------------------
    if((semds3 = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666)) == -1)
	{
		printf("CLIENT - Errore nella creazione del semaforo 3.\n");
		exit(-1);
	}
    
    if(semctl(semds3, 0, SETVAL, arg) == -1)
	{
		printf("SERVER - Errore nella inizializzazione del semaforo 3.\n");
		exit(-1);
	}
    
    //-----------------------------------------------------------
    
    printf("SERVER - Sono in ascolto sulla porta %d .\n",portNUM);
    //-----------------------------------------------------------
    struct socket_node *new_sock;
    while(1)
    {
    	
		new_sock=malloc(sizeof(struct socket_node));
		new_sock->index=index;
    	
    	//Semop------------------------------------------------------
    	op.sem_num = 0;
		op.sem_op = -1;
		op.sem_flg = 0;
		
    	if(semop(semds, &op, 1) == -1)
		{
			printf("SERVER - Errore nella operazione su semaforo.\n");
			exit(-1);
		}
    	
    	//-----------------------------------------------------------
    	
    	if ((new_sock->descr = accept(ric_s, (struct sockaddr *) &client_addr, (socklen_t*)&addr_size)) < 0)
		{
		    printf("SERVER - Errore nella accept.\n \t[stderr:%s]\n",strerror(errno));
			exit(-1);
		}
		
		append_sock(new_sock);
    	//GESTIONE CONNESSIONE---------------------------------------
    	if((pthread_create(&tid,0,(void*)ConnectionThread,new_sock)!=0))
    	{
    		printf("SERVER - Errore nella creazione del thread %d.\n \t[stderr:%s]\n",index,strerror(errno));
			exit(-1);
    	}
    	printf("SERVER - Connessione a %s accettata sul thread %d tramite socket %d\n",inet_ntoa(client_addr.sin_addr),index,new_sock->descr);
    	index++;
    	//-----------------------------------------------------------
    }
}
