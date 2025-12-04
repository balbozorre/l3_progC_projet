#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#define _XOPEN_SOURCE

#include <stdlib.h>
#include <stdio.h>

#include "myassert.h"

#include "master_client.h"

// fonctions éventuelles internes au fichier

// fonctions éventuelles proposées dans le .h

/*
modifie un semaphore a la position sem_pos dans le tableau de semaphore
sem_id : id du tableau de semaphore
value : incrément du semaphore
sem_pos : position dans le tableau, de 0 à lenght - 1.
*/
void sem_edit(int sem_id, int value, int sem_pos) {
    struct sembuf operation;
    operation.sem_num = sem_pos;
    operation.sem_op = value;
    operation.sem_flg = 0;

    int err = semop(sem_id, &operation, 1);
    myassert(err != -1, "Echec de l'opération semop dans sem_edit()");
}
