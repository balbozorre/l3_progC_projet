#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
/*include des semaphore et tube peuvent aller dans "master_client.h" ? (loic)
le fichier master_client.h est partagé par le master et le client,
donc les deux auront l'include necessaire aux semaphore / tubes
*/

#include "myassert.h"

#include "master_client.h"
#include "master_worker.h"

/************************************************************************
 * Données persistantes d'un master
 ************************************************************************/

// on peut ici définir une structure stockant tout ce dont le master
// a besoin

// Idée :
//  Structure de données de type :
//        Count;
//        Highest;
//        ListOfPrime;


/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message)
{
    fprintf(stderr, "usage : %s\n", exeName);
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}


/************************************************************************
 * Gestion des ordres envoyé par le Client
 ************************************************************************/

void whichOrder(int order, int mc_fd, int cm_fd){
    int ret;

    if (order == ORDER_STOP) {
        int ack = 1; // équivalent a true

        // TODO : envoyer ordre de fin au premier worker et attendre sa fin
        // envoyer un accusé de réception au client

        // Une fois le premier worker terminé (celui ci se termine seulement quand les workers suivant sont terminé)
        ret = write(mc_fd, &ack, sizeof(int));
        myassert(ret == sizeof(int), "écriture de l'accusé de reception d'arrêt du master dans le tube master -> client a échoué");
    }
    else if (order == ORDER_COMPUTE_PRIME) {
        int number, isprime;

        ret = read(cm_fd, &number, sizeof(int));
        myassert(ret == sizeof(int), "lecture du nombre a calculer dans le tube client -> master a échoué");
        
        // TODO : pipeline workers

        /*
        ret = read(wm_fd, &isprime, sizeof(int));  // Reponse du worker
        myassert(ret == sizeof(int), "lecture de la réponse si un nombre est premier dans le tube worker -> master a échoué");
        */
       // Remplacement de la réponse du worker
        isprime = 1;  // Valeur arbitraire tant les workers n'ont pas été fait

        ret = write(mc_fd, &isprime, sizeof(int));
        myassert(ret == sizeof(int), "écriture de la réponse si un nombre est premier dans le tube master -> client a échoué");
        printf("Le nombre %d %s premier\n", number, (isprime ? "est" : "n'est pas"));
    }
    else if (order == ORDER_HOW_MANY_PRIME) {
        int count = 13;  // Valeur arbitraire tant les workers n'ont pas été fait

        ret = write(mc_fd, &count, sizeof(int));
        myassert(ret == sizeof(int), "écriture du nombre de nombre premier connu dans le tube master -> client a échoué");
    }
    else if (order == ORDER_HIGHEST_PRIME) {
        int highest = 1637;  // Valeur arbitraire tant les workers n'ont pas été fait
                
        ret = write(mc_fd, &highest, sizeof(int));
        myassert(ret == sizeof(int), "écriture du plus grand nombre premier connu dans le tube master -> client a échoué");
    }
}


/************************************************************************
 * boucle principale de communication avec le client
 ************************************************************************/
void loop(int mc_fd, int cm_fd)
{
    // boucle infinie :
    // - ouverture des tubes (cf. rq client.c)
    // - attente d'un ordre du client (via le tube nommé)
    // - si ORDER_STOP
    //       . envoyer ordre de fin au premier worker et attendre sa fin
    //       . envoyer un accusé de réception au client
    // - si ORDER_COMPUTE_PRIME
    //       . récupérer le nombre N à tester provenant du client
    //       . construire le pipeline jusqu'au nombre N-1 (si non encore fait) :
    //             il faut connaître le plus grand nombre (M) déjà enovoyé aux workers
    //             on leur envoie tous les nombres entre M+1 et N-1
    //             note : chaque envoie déclenche une réponse des workers
    //       . envoyer N dans le pipeline
    //       . récupérer la réponse
    //       . la transmettre au client
    // - si ORDER_HOW_MANY_PRIME
    //       . transmettre la réponse au client (le plus simple est que cette
    //         information soit stockée en local dans le master)
    // - si ORDER_HIGHEST_PRIME
    //       . transmettre la réponse au client (le plus simple est que cette
    //         information soit stockée en local dans le master)
    // - fermer les tubes nommés
    // - attendre ordre du client avant de continuer (sémaphore : précédence)
    // - revenir en début de boucle
    //
    // il est important d'ouvrir et fermer les tubes nommés à chaque itération
    // voyez-vous pourquoi ?
    
    int ret;

    int order;

    // Ouverture des tubes entre Master et Client
    mc_fd = open(TUBE_MC, O_WRONLY);
    myassert(mc_fd != -1, "ouverture du tube master -> client en écriture a échoué");
    printf("ouverture master -> client ecriture ok\n");

    cm_fd = open(TUBE_CM, O_RDONLY);
    myassert(cm_fd != -1, "ouverture du tube client -> master en lecture a échoué");
    printf("ouverture client -> master lecture ok\n");

    // Lecture de l'ordre envoyé par Client
    ret = read(cm_fd, &order, sizeof(int));
    myassert(ret == sizeof(int), "lecture de l'ordre dans le tube client -> master a échoué");


    // Communication entre Master et Client (Matteo)
    whichOrder(order,mc_fd,cm_fd);


    // destruction des tubes nommés, des sémaphores, ...
    ret = close(mc_fd);
    myassert(ret == 0, "fermeture du tube master -> client a échoué");
    ret = close(cm_fd);
    myassert(ret == 0, "fermeture du tube client -> master a échoué");


    // TODO :
    // - attendre ordre du client avant de continuer (sémaphore : précédence)
    // - revenir en début de boucle

    
}


/************************************************************************
 * Fonction principale
 ************************************************************************/

int main(int argc, char * argv[])
{
    if (argc != 1) {
        usage(argv[0], NULL);
    }

    //valeur de retour des fonctions ipc
    //int ret;
    //tube nommé entre master et client
    int mc_fd = 0, cm_fd = 0; // Initialisation à 0 pour éviter 2 warnings sur l'appel de la fonction Loop.

    // - création des sémaphores
    // semaphore indiquant si le master peut prendre des ordres des clients (loic)

    /*
    key_t key = ftok(FILENAME, MASTER_CLIENT);
    int sem_master_state = semget(key, 1, IPC_CREAT|IPC_EXCL|0644);
    ret = semctl(sem_master_state, 0, SETVAL, 1);
    */

        //un pour les dialogues worker-master (loic)
    // - création des tubes nommés master <-> client
    int ret = mkfifo(TUBE_MC, 0644);
    myassert(ret == 0, "création du tube master -> client a échoué");
    ret = mkfifo(TUBE_CM, 0644);
    myassert(ret == 0, "création du tube client -> master a échoué");

    // - création du premier worker

    // boucle infinie
    loop(mc_fd, cm_fd);


    //ret = semctl(sem_master_state, -1, IPC_RMID);
    ret = unlink(TUBE_MC);
    myassert(ret == 0, "suppression du tube master -> client a échoué");
    ret = unlink(TUBE_CM);
    myassert(ret == 0, "suppression du tube client -> master a échoué");


    return EXIT_SUCCESS;
}

// N'hésitez pas à faire des fonctions annexes ; si les fonctions main
// et loop pouvaient être "courtes", ce serait bien
