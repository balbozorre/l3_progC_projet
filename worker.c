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
    int fdIn;
    int fdToMaster;
    int fd_Next;
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
    data->fdIn = atoi(argv[2]);
    data->fdToMaster = atoi(argv[3]);
}

/************************************************************************
 * Boucle principale de traitement
 ************************************************************************/

void loop(workerData *data)
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

    bool running = true;
    int ret;

    while(running) {

        // attente de l'arrivée d'un nombre à tester
        ret = read(data->fdIn, &data->numberToCompute, sizeof(int));
        myassert(ret == sizeof(int), "Worker: Mauvaise communication entre le précédent worker (ou le master) et le worker actuel.");

        printf("%d , Worker %d: Nombre reçu à tester : %d\n", getpid(),data->workerNumber, data->numberToCompute);

        if(data->numberToCompute == WORKER_STOP) {
            // Si il y a un worker suivant, transmettre l'ordre et attendre sa fin

            // attente des processus enfant s'ils existent

            if(data->hasChild) {
                /*
                    transmettre l'ordre au fils
                */
                ret = write(data->fd_Next, &data->numberToCompute, sizeof(int));
                myassert(ret == sizeof(int), "Worker: erreur lors de la transmission de l'ordre d'arrêt au worker suivant.");
                
                ret = wait(NULL);
                myassert(ret >= 0, "Worker: erreur lors de l'attente de la fin d'un enfant d'un fork");
            }

            // Une fois le fils terminé, on peut terminer aussi
            running = false;

        }
        else {
            // N est soit divisible par P, soit égal P, sinon on le passe au worker suivant
            bool isPrime;
            int ret;
            if(data->numberToCompute % data->workerNumber == 0) {
                printf("%d , Worker %d : %d est divisible par %d, ce n'est pas un nombre premier\n",getpid(), data->workerNumber, data->numberToCompute, data->workerNumber);
                isPrime = false;
                ret = write(data->fdToMaster, &isPrime, sizeof(bool));
                myassert(ret == sizeof(bool), "Worker: erreur lors de l'envoi au master de l'information que le nombre n'est pas premier.");
            }
            else if(data->numberToCompute == data->workerNumber) {
                printf("%d , Worker %d : %d est égal à %d, c'est un nombre premier\n", getpid(),data->workerNumber, data->numberToCompute, data->workerNumber);
                isPrime = true;
                ret = write(data->fdToMaster, &isPrime, sizeof(bool));
                myassert(ret == sizeof(bool), "Worker: erreur lors de l'envoi au master de l'information que le nombre est premier.");
            }
            else {
                if(!data->hasChild) {
                    printf("%d , Worker %d : Creation d\'un nouveau worker no %d\n", getpid(), data->workerNumber, data->workerNumber + 1);
                    /*
                        creation du nouveau worker
                        la transmission à ce nouveau worker se fait en dehors de ce if
                    */
                    
                    int fd_ToNext[2];
                    ret = pipe(fd_ToNext);
                    myassert(ret == 0, "création du tube worker -> worker suivant a échoué");

                    pid_t pid_child = fork();

                    if (pid_child != 0) {
                        close(fd_ToNext[0]);

                        data->hasChild = true;
                        printf("%d : has child %d\n",getpid(),(int) data->hasChild);
                        
                        printf("%d : Pere PID est %d\n", getpid(),getpid()); 

                    } else {

                        printf("%d : has child %d\n",getpid(),(int) data->hasChild);
            
                        
                        printf("%d : Je suis le worker %d\n",getpid(),data->workerNumber);

                        close(fd_ToNext[1]);

                        data->fd_Next = fd_ToNext[0];
                        printf("%d : Fils PID est %d\n",getpid(),getpid()); 

                        char str_fd_toNext[20]; // faudrait faire un malloc
                        char str_fd_toMaster[20];

                        sprintf(str_fd_toNext,"%d",data->fd_Next);
                        sprintf(str_fd_toMaster,"%d",data->fdToMaster);

                        ret = read(data->fdIn, &data->numberToCompute, sizeof(int));
                        myassert(ret == sizeof(int), "Worker: erreur lors de la reception du nombre à tester au worker suivant.");

                        execl("./worker", "./worker", data->numberToCompute, str_fd_toNext, str_fd_toMaster, NULL);
                        EXIT_FAILURE;
                    }
                                   
                }
                
                printf("%d Worker %d : Transmission de %d au worker suivant.\n",getpid(), data->workerNumber, data->numberToCompute);
                /*
                    TODO : partie communication au worker N+1
                */
               
                ret = write(data->fd_Next, &data->numberToCompute, sizeof(int));
                //ERREUR - A CORRIGER
                //Partie fork pose probleme
                myassert(ret == sizeof(int), "Worker: erreur lors de la transmission du nombre à tester au worker suivant.");
                //printf("Worker %d : Transmission de %d au worker suivant.\n", data->workerNumber, data->numberToCompute);
                /*
                    TODO : partie communication au worker N+1
                *
                ret = write(data->fd_Next[1], &data->numberToCompute, sizeof(int));
                //ERREUR - A CORRIGER
                //Partie fork pose probleme
                myassert(ret == sizeof(int), "Worker: erreur lors de la transmission du nombre à tester au worker suivant.");
                */
            }
        }
    }

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

    bool isPrime = true;
    printf("%d , Je viens d'être créer, donc %d est premier\n",getpid(), data->numberToCompute);
    data->workerNumber = data->numberToCompute;
    printf("%d , worker %d : number %d\n",getpid(),data->workerNumber,data->numberToCompute);
    int ret = write(data->fdToMaster, &isPrime, sizeof(bool));
    myassert(ret == sizeof(bool), "Worker: erreur lors de l'envoi au master de l'information que le nombre est premier.");

    printf("%d , Le boolean vient d'etre envoyé, %d est premier\n",getpid(), data->numberToCompute);

    loop(data);


    // libérer les ressources : fermeture des files descriptors par exemple
    close(data->fdIn);
    close(data->fdToMaster);

    free(data);

    return EXIT_SUCCESS;
}
