#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include "a2_helper.h"
#include <sys/ipc.h>
#include <sys/sem.h>

/*semaphores*/
int sem_id;

/*process 9 elements*/
volatile int thNo = 0;           //number of running threads
volatile int ranThreads = 0;     //number of already finished threads
int isRunning14 = 0;             //1 if thread 14 of p9 is running, 2 if terminated

/*synchronization elements*/
pthread_mutex_t lock;
pthread_cond_t cond;
pthread_cond_t cond14;
pthread_cond_t condEnd14;

/*Semaphore operations*/
void P(int semId, int semNr)      //decrements
{
    struct sembuf op = { semNr, -1, 0 };

    semop(semId, &op, 1);
}

void V(int semId, int semNr)     //increments
{
    struct sembuf op = { semNr, +1, 0 };

    semop(semId, &op, 1);
}

/*Thread functions*/
void* threadType6(void* id) {
    int idTh = *((int*)id);
    free(id);

    if (idTh == 4)
        P(sem_id, 3);

    info(BEGIN, 6, idTh);

    info(END, 6, idTh);

    if(idTh == 2)
        V(sem_id, 2);

    return NULL;
}


void* threadType7(void* id) {
    int idTh = *((int*)id);
    free(id);

    if (idTh == 2)
        P(sem_id, 0);

    if(idTh == 1)
        P(sem_id, 2);

    info(BEGIN, 7, idTh);

    if (idTh == 4)
        V(sem_id, 0);

    if(idTh == 4)
        P(sem_id, 1);

    info(END, 7, idTh);

    if(idTh == 2)
        V(sem_id, 1);

    if (idTh == 1)
        V(sem_id, 3);

    return NULL;
}

void* threadType9(void* id) {
    int idTh = *((int*)id);
    free(id);

    //critical zone
    if (pthread_mutex_lock(&lock) != 0) {
        perror("Cannot take the lock");
        exit(4);
    }

    while (thNo >= 5)
    if (pthread_cond_wait(&cond, &lock) != 0) {
        perror("Cannot wait for condition");
        exit(6);
    }


    if (idTh == 14)
        isRunning14 = 1;


    thNo++;

    if (pthread_cond_signal(&cond14) != 0) {
        perror("Cannot signal the condition waiters");
        exit(7);
    }

    if (pthread_mutex_unlock(&lock) != 0) {
        perror("Cannot release the lock");
        exit(5);
    }
    //critical zone end

    info(BEGIN, 9, idTh);

    //critical zone
    if (pthread_mutex_lock(&lock) != 0) {
        perror("Cannot take the lock");
        exit(4);
    }

    ranThreads++;

    if (idTh!=14 && isRunning14 != 2 && 42-ranThreads <= 5) {
        if (pthread_cond_wait(&condEnd14, &lock) != 0) {
            perror("Cannot wait for condition");
            exit(6);
        }
    }
    if (idTh!=14 && isRunning14 == 1) {
        while (isRunning14 != 2)
            if (pthread_cond_wait(&condEnd14, &lock) != 0) {
                perror("Cannot wait for condition");
                exit(6);
        }
    }

    if (idTh == 14 && thNo < 5)
        while (thNo<5){
           // printf("id = %dthNo = %d\n",idTh,thNo);
            if (pthread_cond_wait(&cond14, &lock) != 0) {
                perror("Cannot wait for condition");
                exit(6);
            }
        }
    info(END, 9, idTh);


    thNo--;

    if (pthread_cond_signal(&cond) != 0) {
        perror("Cannot signal the condition waiters");
        exit(7);
    }

    if (idTh == 14) {
        isRunning14 = 2;
        if (pthread_cond_broadcast(&condEnd14) != 0) {
            perror("Cannot signal the condition waiters");
            exit(7);
        }
    }

 /* if (isRunning14 == 1) {
        while (isRunning14 != 2)
            if (pthread_cond_wait(&condEnd14, &lock) != 0) {
                perror("Cannot wait for condition");
                exit(6);
        }
    }*/

    if (pthread_mutex_unlock(&lock) != 0) {
        perror("Cannot take the lock");
        exit(5);
    }
    //critical zone end


    return NULL;
}

int main(){
    init();

    /*Semaphore initializations*/
    sem_id = semget(IPC_PRIVATE, 4, IPC_CREAT | 0600);
    if (sem_id < 0) {
        perror("Error creating the semaphore set");
        exit(2);
    }
    semctl(sem_id, 0, SETVAL, 0);
    semctl(sem_id, 1, SETVAL, 0);
    semctl(sem_id, 2, SETVAL, 0);
    semctl(sem_id, 3, SETVAL, 0);

    /*Mutex initialisations*/
    if (pthread_mutex_init(&lock, NULL) != 0) {
        perror("Cannot initialize the lock");
        exit(2);
    }

    if (pthread_cond_init(&cond, NULL) != 0) {
        perror("Cannot initialize the condition variable");
        exit(3);
    }

    if (pthread_cond_init(&cond14, NULL) != 0) {
        perror("Cannot initialize the condition variable");
        exit(3);
    }

    if (pthread_cond_init(&condEnd14, NULL) != 0) {
        perror("Cannot initialize the condition variable");
        exit(3);
    }


    info(BEGIN, 1, 0);

    //Create process 2
    pid_t fork2 = fork();
    if (fork2 == 0) {
        info(BEGIN, 2, 0);

        //Create process 3
        pid_t fork3 = fork();
        if (fork3 == 0) {
            info(BEGIN, 3, 0);

            //Create process 6
            pid_t fork6 = fork();
            if (fork6 == 0) {
                info(BEGIN, 6, 0);

                //Create process 7
                pid_t fork7 = fork();
                if (fork7 == 0) {
                    info(BEGIN, 7, 0);

                    //Create process 9
                    pid_t fork9 = fork();
                    if (fork9 == 0) {
                        info(BEGIN, 9, 0);

                        //create 42 threads
                        int i;
                        pthread_t th[43];
                        int* id;
                        for (i = 1; i <= 42; i++) {
                            id = (int*)malloc(sizeof(int));
                            *id = i;
                            if (pthread_create(&th[i], NULL, threadType9, id) != 0) {
                                perror("Cannot create threads");
                                exit(1);
                            }
                        }
                        for (i = 1; i <= 42; i++)
                            pthread_join(th[i], NULL);

                        info(END, 9, 0);
                        exit(0);
                    }
                    //End of process 9

                    //create 4 threads
                    int i;
                    pthread_t th[5];
                    int* id;
                    for (i = 1; i<=4; i++) {
                        id = (int*)malloc(sizeof(int));
                        *id = i;
                        if (pthread_create(&th[i], NULL, threadType7, id) != 0) {
                            perror("Cannot create threads");
                            exit(1);
                        }
                    }
                    for (i = 1; i<=4; i++)
                        pthread_join(th[i], NULL);

                    waitpid(fork9, NULL, 0);

                    info(END, 7, 0);
                    exit(0);
                }
                //End of process 7

                //Create process 8
                pid_t fork8 = fork();
                if (fork8 == 0) {
                    info(BEGIN, 8, 0);
                    info(END, 8, 0);
                    exit(0);
                }
                //End of process 8

                //create 6 threads
                int i;
                pthread_t th[7];
                int* id;
                for (i = 1; i <= 6; i++) {
                    id = (int*)malloc(sizeof(int));
                    *id = i;
                    if (pthread_create(&th[i], NULL, threadType6, id) != 0) {
                        perror("Cannot create threads");
                        exit(1);
                    }
                }
                for (i = 1; i <= 6; i++)
                    pthread_join(th[i], NULL);

                waitpid(fork7, NULL, 0);
                waitpid(fork8, NULL, 0);

                info(END, 6, 0);
                exit(0);
            }
            //End of process 6

            waitpid(fork6, NULL, 0);

            info(END, 3, 0);
            exit(0);
        }
        //End of process 3

        //Create process 4
        pid_t fork4 = fork();
        if (fork4 == 0) {
            info(BEGIN, 4, 0);
            info(END, 4, 0);
            exit(0);
        }
        //End of process 4

        waitpid(fork3, NULL, 0);
        waitpid(fork4, NULL, 0);

        info(END, 2, 0);
        exit(0);
    }
    //End of process 2

    //Create process 5
    pid_t fork5 = fork();
    if (fork5 == 0) {
        info(BEGIN, 5, 0);
        info(END, 5, 0);
        exit(0);
    }
    //End of process 5

    waitpid(fork2, NULL, 0);
    waitpid(fork5, NULL, 0);

    info(END, 1, 0);

    /*Releasing semaphores*/
    semctl(sem_id, 0, IPC_RMID, 0);
    semctl(sem_id, 1, IPC_RMID, 0);
    semctl(sem_id, 2, IPC_RMID, 0);
    semctl(sem_id, 3, IPC_RMID, 0);

    /*Destroying mutexes*/
    if (pthread_mutex_destroy(&lock) != 0) {
        perror("Cannot destroy the lock");
        exit(8);
    }

    if (pthread_cond_destroy(&cond) != 0) {
        perror("Cannot destroy the condition variable");
        exit(9);
    }

    if (pthread_cond_destroy(&cond14) != 0) {
        perror("Cannot destroy the condition variable");
        exit(9);
    }

    if (pthread_cond_destroy(&condEnd14) != 0) {
        perror("Cannot destroy the condition variable");
        exit(9);
    }

    return 0;
}
