/**
 * Subor: proj2.c
 * Datum: 2017/04/28
 * Autor: František Matečný, xmatec00
 * Projekt: 2. projekt - shell
 **/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>


#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED 1 /* XPG 4.2 - needed for WCOREDUMP() */


bool not_end = true;
char buffer[513];
char* args[64];
bool background = false;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t condition_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  condition_cond  = PTHREAD_COND_INITIALIZER;

/* pid procesu na pozadi */
pid_t  back_pid = -1;
/* pid procesu na popredi */
pid_t  front_pid = -1;

/**
 * Ukoncenie procesu na popredi
 **/
void end_process(){
	
	
	if (front_pid == -1 && back_pid == -1)
	{
		printf ("\n$ ");
		fflush(stdout);
	}
	
	if (front_pid > 0 ){
		kill(front_pid, SIGKILL);
		printf ("Foreground child: %d.\n", front_pid);
		fflush(stdout);
		front_pid = -1;
	}
	
}


/**
 * Ukoncenie procesu na pozadi
 **/
void proc_exit()
{
	int wstat;
	pid_t	pid;

	while (1) 
	{
		pid = wait3 (&wstat, WNOHANG, (struct rusage *)NULL );
		if (pid == 0)
			return;
		else if (pid == -1)
			return;
		else{
			printf ("Background child: %d.\n", pid);
			fflush(stdout);
			if (back_pid == pid){
				back_pid = -1;}
			if (front_pid == -1){
				printf("$ ");
				fflush(stdout);}
			}
	}

}

/**
 * Parsovanie a vykonanie vstupu
 **/
void parse(char *input){
	
	char *pch;
	int filedesc = -1;
	int in_filedesc = -1;
	
	/* presmerovanie do suboru */
	pch = strchr(input, '>');
	if (pch != NULL)
	{
		input[pch-input-1] = '\0';
		pch  +=2;
	
		/* otvorenie vystupneho suboru */
		filedesc = open(pch, O_WRONLY | O_CREAT | O_TRUNC, 0666);
		if(filedesc < 0){
			return;
			}
			
		/* presmerovanie zo stdout do suboru */
		close(1);
		if (dup(filedesc) < 0){
			fprintf(stderr, "Chyba vytvorenia filedesc\n");
			return;
			}	
	}
	
	/* presmerovanie vstupneho zo suboru */
	pch = strchr(input, '<');
	if (pch != NULL)
	{
		input[pch-input] = ' ';	
		pch  +=2;

		/* otvorenie suboru */
		in_filedesc = open(pch, O_RDONLY, 0666);
		if(in_filedesc < 0){
			return;
			}
			
		/* presmerovanie zo stdout do suboru */
		close(0);
		if (dup(in_filedesc) < 0){
			fprintf(stderr, "Chyba vytvorenia filedesc\n");
			return;
			}	
	}
	
	pch = strtok(input, " \t");
	/* nazov programu */
	args[0] = pch;
	
	/* spracovanie parametrov programu */
	if (pch != NULL)
	{		
		int i;
		for (i = 0; pch != NULL; i++)
		{			
			args[i] = pch;
			pch = strtok (NULL, " \t");
		}
		args[i] = NULL;
	}
	
	/* vykonanie prikazu */
	if (execvp(args[0], args) < 0) {
		fprintf(stderr, "Prikaz \"%s\" neznamy\n", args[0]);
		if (filedesc >= 0)	
			close(filedesc);
		exit(-1);
	}
	
	/* zatvorenie filedescriptoru */
	if (filedesc >= 0)	
		close(filedesc);
}


/**
 * Funkcia na nacitanie vstupu
 **/
void *read_function()
{
	int count = 0;
	char *pch;
	
	while(not_end){
		pthread_mutex_lock( &mutex );
		
		count = 0;
		
		while (count < 1){
			/* vypis promt */
			printf("$ ");
			fflush(stdout);
			
			/* citanie vstupu */
			count = read(0, buffer, 513);
			if (count > 512){
				fprintf(stderr, "Prilis velky vstup\n");
				continue;
				}
					
			/* osetrenie ctrl + d */
			pch = strchr(buffer, '\n');
			if(pch == NULL){
				printf("\n");
				fflush(stdout);
				count++;
				}	
		}
		buffer[count-1] = '\0';
		
		/* exit - nastavenie premennej na ukoncenie */
		if (strcmp(buffer, "exit") == 0){
			not_end = false;
			}

		/* uvolenie vlakna na spracovanie vstupu */
		pthread_cond_signal(&condition_cond);
		pthread_mutex_unlock( &condition_mutex );		
	}
	
	return NULL;
}


/**
 * Funkcia na sparsovanie a vykonanie prikazu
 **/
void *run_function()
{
	pthread_mutex_lock( &condition_mutex );
	while(not_end)
	{
		pthread_cond_wait( &condition_cond, &condition_mutex );
		
		/* ukoncenie */
		if(!not_end)
			break;

		/* vykonavanie na pozadi */
		char* pch = strchr(buffer, '&');
		if (pch != NULL){
			buffer[pch-buffer-1] = '\0';		/* vymazanie & */
			background = true;
		}	
			
		int    status;
		pid_t  pid;

		if ((pid = fork()) < 0) {     /* fork() */
		  printf("Chyba fork()\n");
		  exit(1);
		}
		else if (pid == 0) {          /* child */
			/* parsovanie vstupu */
			parse(buffer);
		}
		else /* parent */
		{ 						
			/* cakam ak sa nevykonava na pozadi */
			if (!background){
				front_pid = pid;	
				while (wait(&status) != pid) ;
				front_pid = -1;
			}
			else{ /* na pozadi */
				back_pid = pid;
				}
				
			background = false;  
		}

		/* uvolnenie citania vstupu */
		pthread_mutex_unlock( &mutex );
	}
		
	return NULL;
}


/**
 * Hlavný program
 **/
int main()
{
	
	// nastavenie signalu
	signal(SIGINT, end_process);
	signal(SIGCHLD, proc_exit);
	
	/* pole vlakien */
	pthread_t thread[2]; 
	void *result;
	int res;

	/* vytvorenie vlakien */
	res = pthread_create( &thread[0], NULL, run_function, NULL);
	if (res) {
		printf("pthread_create() error %d\n", res);
		return 1;
		} 
	
	res = pthread_create( &thread[1], NULL, read_function, NULL);
	if (res) {
		printf("pthread_create() error %d\n", res);
		return 1;
		} 


	/* čakanie na dokončenie */
	for (int i = 0; i < 2; i++)
	{
		if ((res = pthread_join(thread[i], &result)) != 0) {
			printf("pthread_attr_init() err %d\n",res);
			return 1;
			} 
	}

	return 0;	
}