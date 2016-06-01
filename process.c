#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h> 
#include <sys/shm.h>
#include <sys/ipc.h>
#include <pthread.h>
#include <sys/sem.h>
#include <sys/msg.h>

pthread_t threads[2];
int numberOfSeq = 1;

struct msg{
	long type;
	char *msgtext;
}message;

typedef struct s_thread{
	int *t1;
	int *t2;
	int sem_Id;
	int idthread;
}s_thread;

void Child(int sharedmem[]){
	printf("\t\tSeg[1] = %d Seg[2] = %d\n", sharedmem[0], sharedmem[1]);
}
union semun{
	int val;	// value for SETVAL
	struct semid_ds* buf; 	// buffer for IPC_STAT, IPC_SET
	unsigned short* array; // array for GETALL, SETALL
	struct seminfo* __buf; // buffer for IPC_INFO
};

int sem_Create(int n, short *value){
	union semun args;
	int semId = semget(IPC_PRIVATE, n, SHM_R | SHM_W);
	args.array = value;
	semctl(semId, 0, SETALL, args);
	return semId;
}
void sem_Delete(int semId){
	if (semctl(semId, 0, IPC_RMID, 0) == -1)
       	perror("Erro na destruicao do semaforo\n");
   	else
   		printf("Semaforo %d destruido\n", semId);

}
void sem_Down(int semId, int iControl){
	struct sembuf psem;
	psem.sem_num = iControl;
	psem.sem_op = -1;
	psem.sem_flg = SEM_UNDO;
	semop(semId, &psem, 1);
}
void sem_Up(int semId, int iControl){
	struct sembuf vsem;
	vsem.sem_num = iControl;
	vsem.sem_op = 1;
	vsem.sem_flg = SEM_UNDO;
	semop(semId, &vsem, 1);
}
void *funcao(void *args){
    s_thread *id = (s_thread *) args;
    int i, varCpy;;
    for(i=0;i<100;i++){
    	sem_Down(id->sem_Id, 0);
    	varCpy = *(id->t1);
    	varCpy -= 1;
    	usleep((20+rand()%281)*1000);
    	*(id->t1) = varCpy;
    	sem_Up(id->sem_Id, 0);

    	sem_Down(id->sem_Id, 1);
    	*(id->t2) += 1;
    	sem_Up(id->sem_Id, 1);

		printf("Thread %d Série %d\n", id->idthread, numberOfSeq);
    	printf("\t\tSeg[1] = %d Seg[2] = %d\n", *(id->t1), *(id->t2) );
    	numberOfSeq+=1;
    }
}
int main(){
	int segId, *segPtr, status, i, msgId;
	pid_t pid;
	pid_t pid2;
	short value[2];
	
	value[0] = 1;
	value[1] = 1;

	segId = shmget (IPC_PRIVATE, 2*sizeof (int), IPC_CREAT |0666);
	segPtr = (int *) shmat(segId, NULL, 0);
	
	int semId = sem_Create(2, value);
	//atualiza primeiro valor = 300
	sem_Down(semId, 0);
	segPtr[0] = 300;
	sem_Up(semId, 0);
	//atualiza segundo valor = 0
	sem_Down(semId, 1);
	segPtr[1] = 0;
	sem_Up(semId, 1);
	
	//criar threads
	s_thread ptd1;
	
	sem_Down(semId, 0);
	ptd1.t1 = &segPtr[0];
	sem_Up(semId, 0);
	//atualiza segundo valor = 0
	sem_Down(semId, 1);
	ptd1.t2 = &segPtr[1];
	sem_Up(semId, 1);
	ptd1.sem_Id = semId;

	s_thread ptd2;
	
	sem_Down(semId, 0);
	ptd2.t1 = &segPtr[0];
	sem_Up(semId, 0);
	//atualiza segundo valor = 0
	sem_Down(semId, 1);
	ptd2.t2 = &segPtr[1];
	sem_Up(semId, 1);
	ptd2.sem_Id = semId;

	msgId = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0666);
	if(msgId == -1){
		printf("Erro na mensagem\n");
		exit(1);
	}
	message.type = 1;

	pid = fork();
	srand(time(NULL));

	if(pid == -1){
        printf("Error em PID");
        exit(2);
    }
	if(pid == 0){
	   int varCpy; //variável para receber a copia
	   for(i=0;i<100;i++){
			sem_Down(semId, 0);
			varCpy = segPtr[0];
			varCpy -= 1;
			usleep((20+rand()%281)*1000);  //DORME  5 ms
			segPtr[0] = varCpy;
			sem_Up(semId, 0);
			
			sem_Down(semId, 1);
		    segPtr[1] += 1;
		    sem_Up(semId, 1);

		   printf("filho 1 %d / Série: %d\n", getpid(), numberOfSeq);
		   printf("\t\tSeg[1] = %d Seg[2] = %d\n", segPtr[0], segPtr[1]);
		   numberOfSeq+=1;
		}	
		printf("Fim do FILHO1\n")       	  ;
	  
		//sprintf(message.msgtext, "Filho 1 finalizado\n");
		//msgsnd(msgId, &message, strlen(message.msgtext), IPC_NOWAIT);
		printf("Fim filho1\n");

	}
	else{
	   pid2 = fork();
	   
	   if(pid2 == 0){
	        int newTd1, newTd2;
	        newTd1 =  pthread_create(&threads[0], NULL, funcao, (void *)&ptd1);
	      	if(newTd1 != 0)
	      		printf("\nNao foi possivel criar a thread\n");
	      	else
	      		ptd1.idthread = 1;

      		newTd2 = pthread_create(&threads[1], NULL, funcao, (void *)&ptd2);
      		if(newTd2 != 0)
	      		printf("\nNao foi possivel criar a thread\n");
	      	else
	      		ptd2.idthread = 2;

      		pthread_join(threads[0], NULL);
      		printf("A thread 1 terminou\n");
      		pthread_join(threads[1], NULL);
      		printf("A thread 2 terminou\n");

      		sprintf(message.msgtext, "Filho 2 finalizado\n");
      		msgsnd(msgId, &message, strlen(message.msgtext), IPC_NOWAIT);
			printf("FILHO2>%s\n", message.msgtext);
	    }
	    else{
	    	int j, intText;
	    	printf("Fim do PAI\n");
	    	for(j=0;j<2;j++){
	    		intText = msgrcv(msgId, &message, 100, message.type, MSG_NOERROR);
	    		printf(" : %s\n", message.msgtext);
	    	}
	    } 
	     
	}
	printf("SAI\n");
	return 0;
}
