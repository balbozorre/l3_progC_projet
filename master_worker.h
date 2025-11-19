#ifndef MASTER_WORKER_H
#define MASTER_WORKER_H

#include <sys/wait.h>

// On peut mettre ici des éléments propres au couple master/worker :
//    - des constantes pour rendre plus lisible les comunications
//    - des fonctions communes (écriture dans un tube, ...)

// valeur des tubes master <-> worker
#define TUBE_MW "tube_mw"
#define TUBE_WM "tube_wm"

//ordre du master aux worker
#define WORKER_STOP -1

#endif
