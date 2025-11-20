#ifndef MASTER_WORKER_H
#define MASTER_WORKER_H

#include <sys/wait.h>

#include <sys/sem.h>
#include <sys/types.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

// On peut mettre ici des éléments propres au couple master/worker :
//    - des constantes pour rendre plus lisible les comunications
//    - des fonctions communes (écriture dans un tube, ...)

// valeur des tubes master <-> worker
#define TUBE_MW "master_worker"
#define TUBE_WM "worker_master"

//ordre du master aux worker
#define WORKER_STOP -1

#endif
