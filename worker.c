#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "myassert.h"

#include "master_worker.h"

/************************************************************************
 * Données persistantes d'un worker
 ************************************************************************/

// on peut ici définir une structure stockant tout ce dont le worker
// a besoin : le nombre premier dont il a la charge, ...

typedef struct {
    int workerNumber;
    bool hasChild;//pour gérer le wait(null) en cas d'ordre stop
    int numberToCompute;
    //file descriptor
    int fd_previous;
    int fd_Next;
    int fd_master;
} workerData;


/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message)
{
    fprintf(stderr, "usage : %s <n> <fdIn> <fdToMaster>\n", exeName);
    fprintf(stderr, "   <n> : nombre premier géré par le worker\n");
    fprintf(stderr, "   <fdIn> : canal d'entrée pour tester un nombre\n");
    fprintf(stderr, "   <fdToMaster> : canal de sortie pour indiquer si un nombre est premier ou non\n");
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}

static void parseArgs(int argc, char * argv[], workerData *data)
{
    if (argc != 4) {
        usage(argv[0], "Nombre d'arguments incorrect");
    }

    data->numberToCompute = atoi(argv[1]);
    data->fd_previous = atoi(argv[2]);
    data->fd_master = atoi(argv[3]);
}

/************************************************************************
 * Boucle principale de traitement
 ************************************************************************/

void loop(int fd_previous, int fd_Next, int fd_master, workerData *data)
{
    // boucle infinie :
    //    attendre l'arrivée d'un nombre à tester (> 1 donc)
    //    si ordre d'arrêt (un nombre <= 1 ?)
    //       si il y a un worker suivant, transmettre l'ordre et attendre sa fin
    //       sortir de la boucle
    //    sinon c'est un nombre à tester, 4 possibilités :
    //           - le nombre est premier
    //           - le nombre n'est pas premier
    //           - s'il y a un worker suivant lui transmettre le nombre
    //           - s'il n'y a pas de worker suivant, le créer

    bool running = false; // à initialiser à true une fois la suite écrite
    int order, ret;

    while(running) {

        toNext = -1; // à retirer une fois la suite écrite
        toMaster = -1; // à retirer une fois la suite écrite
        printf("%d, %d\n", toNext, toMaster); // pour éviter le warning, à retirer une fois la suite écrite

        // attente et lecture d'un ordre dans le tube lié au worker N-1
        ret = read(fromPrevious, &order, sizeof(int));
        myassert(ret == sizeof(int), "Worker: Mauvaise communication entre précédent worker (ou le master) et le worker actuel.");
        myassert(order > 1 || order == WORKER_STOP, "Worker: ordre incorrect"); // A voir, parce que si le master envoie -1 ça passe pas a cause du order == WORKER_STOP

        if(order == WORKER_STOP) {
            // Si il y a un worker suivant, transmettre l'ordre et attendre sa fin

            // Une fois le fils terminé, on peut terminer aussi
            running = false;
        }
        else {
            // N est soit divisible par P, soit égal P, sinon on le passe au worker suivant
            if(order % data->workerNumber == 0) {
                printf("%d est divisible par %d, ce n'est pas un nombre premier\n", order, data->workerNumber);
                /*
                    communication de l'information au master via tube
                */
            }
            else if(order == data->workerNumber) {
                printf("%d est égal à %d, c'est un nombre premier\n", order, data->workerNumber);
                /*
                    communication de l'information au master via tube
                */
            }
            else {
                if(!data->hasChild) {
                    printf("Creation d\'un nouveau worker no %d", data->workerNumber + 1);
                    /*
                        creation du nouveau worker
                        la transmission à ce nouveau worker se fait en dehors de ce if
                    */
                }
                printf("Transmission de %d au worker suivant.\n", order);
                running = false;// à retirer une fois la suite écrite
                /*
                    TODO : partie communication au worker N+1
                */
            }
        }
    }

    // attente des processus enfant s'ils existent
   /* 
   if(data->hasChild) {
        /*
            transmettre l'ordre au fils
        */
       /* ret = wait(NULL);
        myassert(ret > 0, "Worker: erreur lors de l'attente de la fin d'un enfant d'un fork");
        */
    //}
}

/************************************************************************
 * Programme principal
 ************************************************************************/

int main(int argc, char * argv[])
{
    workerData *data = (workerData *) malloc(sizeof(workerData));
    parseArgs(argc, argv, data);
    
    // Si on est créé c'est qu'on est un nombre premier
    // Envoyer au master un message positif pour dire
    // que le nombre testé est bien premier

    int fromPrevious, toNext = 0, toMaster; // meme qu'en dessous
    int ret;


    fromPrevious = open(TUBE_MW, O_RDONLY);
    myassert(fromPrevious != -1, "ouverture du tube master -> worker en lecture a échoué");
    printf("ouverture master -> worker lecture ok\n");
    
    toMaster = open(TUBE_WM, O_WRONLY);
    myassert(toMaster != -1, "ouverture du tube worker -> master en écriture a échoué");
    printf("ouverture worker -> master ecriture ok\n");

    loop(fromPrevious, toNext, toMaster, data);

    int v = 1000;
    ret = write(toMaster, &v, sizeof(int));
    myassert(ret == sizeof(int),"test");


    // libérer les ressources : fermeture des files descriptors par exemple
    ret = close(fromPrevious);
    myassert(ret == 0, "fermeture du tube master -> worker a échoué");
    printf("fermeture master -> worker ok\n");

    ret = close(toMaster);
    myassert(ret == 0, "fermeture du tube worker -> master a échoué");
    printf("fermeture worker -> master ok\n");

    free(data);

    return EXIT_SUCCESS;
}
