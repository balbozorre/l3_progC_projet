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

void whichOrder(int order, int mc_fd, int cm_fd, int mw_fd, int wm_fd){
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
/*
    Au niveau des sémaphores :
        le client -1 celui bloquant les clients
        celui du master est déjà à 0
            (le master ne pourra pas lancer un nouveau tour de boucle avant que le client ne le libère)
        execution de l'ordre
        le client +1 le sémaphore du master -> le master attend un ordre
        le client +1 le sémaphore des clients -> un client peut donner un ordre
    Paramètres :
    -int mc_fd : file descriptor du tube master -> client
    -int cm_fd : file descriptor du tube client -> master
    -int sem_mc_states : tableau de sémaphores controllant la communication master <-> client
    ...
*/
void loop(int mc_fd, int cm_fd, int mw_fd, int wm_fd, int sem_mc_states)
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

    // Ouverture des tubes entre Master et Worker
    mw_fd = open(TUBE_MW, O_WRONLY);
    myassert(mc_fd != -1, "ouverture du tube master -> worker en écriture a échoué");
    printf("ouverture master -> client ecriture ok\n");
    wm_fd = open(TUBE_WM, O_RDONLY);
    myassert(cm_fd != -1, "ouverture du tube worker -> master en lecture a échoué");
    printf("ouverture client -> master lecture ok\n");

    // Lecture de l'ordre envoyé par Client
    ret = read(cm_fd, &order, sizeof(int));
    myassert(ret == sizeof(int), "lecture de l'ordre dans le tube client -> master a échoué");


    // Communication entre Master et Client (Matteo)
    whichOrder(order,mc_fd,cm_fd, mw_fd, wm_fd);


    // destruction des tubes nommés, des sémaphores, ...
    ret = close(mc_fd);
    myassert(ret == 0, "fermeture du tube master -> client a échoué");
    ret = close(cm_fd);
    myassert(ret == 0, "fermeture du tube client -> master a échoué");


    // TODO :
    // - attendre ordre du client avant de continuer (sémaphore : précédence)
    // - revenir en début de boucle
    sem_edit(sem_mc_states, -1, SEM_MASTER);
}


/************************************************************************
 * Fonction principale
 ************************************************************************/

int main(int argc, char * argv[])
{
    if (argc != 1) {
        usage(argv[0], NULL);
    }

    //valeur de retour
    int ret;

    //tube nommé entre master et client
    int mc_fd = 0, cm_fd = 0, mw_fd = 0, wm_fd = 0; // Initialisation à 0 pour éviter 2 warnings sur l'appel de la fonction Loop.

    // - création des sémaphores
    /*
        on crée 2 sémaphores liés à la variable sem_mc_states.
        en position 0 celui bloquant les clients
        en position 1 celui bloquant le master
        les positions sont masquées dans le .h
    */
    key_t key = ftok(FILENAME, MASTER_CLIENT);
    myassert(key != -1, "Erreur dans ftok(), la clé est incorrecte");
    int sem_mc_states = semget(key, 2, IPC_CREAT|IPC_EXCL|0644);
    myassert(sem_mc_states != -1, "Echec de la création des sémaphores client <-> master");

    //initialisation des semaphores
    ret = semctl(sem_mc_states, SEM_CLIENT, SETVAL, 1);
    myassert(ret != -1, "Erreur à l'initialisation du semaphor client");
    ret = semctl(sem_mc_states, SEM_MASTER, SETVAL, 1);
    myassert(ret != -1, "Erreur à l'initialisation du semaphor master");
    
    //semaphore pour les dialogues worker-master (loic)

    // - création des tubes nommés master <-> client
    ret = mkfifo(TUBE_MC, 0644);
    myassert(ret == 0, "création du tube master -> client a échoué");
    ret = mkfifo(TUBE_CM, 0644);
    myassert(ret == 0, "création du tube client -> master a échoué");

    // - création des tubes nommés master <-> worker
    ret = mkfifo(TUBE_MW, 0644);
    myassert(ret == 0, "création du tube master -> worker a échoué");
    ret = mkfifo(TUBE_WM, 0644);
    myassert(ret == 0, "création du tube worker -> master a échoué");

    // - création du premier worker
    // non testé !
    //char *args[]={"./worker",[arguments],NULL};
    //execv(args[0], args);
    // non testé !

    // boucle infinie
    //mise à 0 du sémaphore du master pour qu'il se bloque en fin de tour de boucle
    sem_edit(sem_mc_states, -1, SEM_MASTER);
    loop(mc_fd, cm_fd, mw_fd, wm_fd, sem_mc_states);

    //destruction des sémaphore entre le master et les clients
    printf("Destruction des semaphores\n");
    ret = semctl(sem_mc_states, -1, IPC_RMID);
    myassert(ret != -1, "Erreur lors de la destruction des semaphores client <-> master");

    printf("Suppression des tubes nommés\n");
    ret = unlink(TUBE_MC);
    myassert(ret == 0, "suppression du tube master -> client a échoué");
    ret = unlink(TUBE_CM);
    myassert(ret == 0, "suppression du tube client -> master a échoué");


    return EXIT_SUCCESS;
}

// N'hésitez pas à faire des fonctions annexes ; si les fonctions main
// et loop pouvaient être "courtes", ce serait bien
