#ifndef CLIENT_CRIBLE
#define CLIENT_CRIBLE

// On peut mettre ici des éléments propres au couple master/client :
//    - des constantes pour rendre plus lisible les comunications
//    - des fonctions communes (création tubes, écriture dans un tube,
//      manipulation de sémaphores, ...)

/*  (loic) Includes partagés entre le master et le client
    comme les semaphore et tubes*/
#include <sys/sem.h>
#include <sys/types.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include <semaphore.h>

typedef struct
{
    int thread_nbr;
    pthread_mutex_t *tmutex;

    bool *prime_tab;
    int tab_size;
} thread_args_t;

// valeurs necessaire à ftok()
#define FILENAME "Makefile"
#define MASTER_CLIENT 27
//valeur masquant les position des semaphore master <-> client
#define SEM_CLIENT 0
#define SEM_MASTER 1

//valeurs pour les tubes
#define TUBE_MC "master_client"
#define TUBE_CM "client_master"

//fichiers de log et de resultat pour compute local
#define FILE_RESULT "local_compute_result.txt"
#define FILE_LOG "local_compute_logs.txt"

// ordres possibles pour le master
#define ORDER_NONE                0
#define ORDER_STOP               -1
#define ORDER_COMPUTE_PRIME       1
#define ORDER_HOW_MANY_PRIME      2
#define ORDER_HIGHEST_PRIME       3
#define ORDER_COMPUTE_PRIME_LOCAL 4   // ne concerne pas le master

// bref n'hésitez à mettre nombre de fonctions avec des noms explicites
// pour masquer l'implémentation

/*
modifie un semaphore a la position sem_pos dans le tableau de semaphore
sem_id : id du tableau de semaphore
value : incrément du semaphore
sem_pos : position dans le tableau, de 0 à lenght - 1.
*/
void sem_edit(int sem_id, int value, int sem_pos);

#endif
