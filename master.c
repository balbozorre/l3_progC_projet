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


#include "signal.h" // Nécessaire pour signal() et SIGINT

/* * Variable globale pour stocker l'ID du sémaphore.
 * Nécessaire car le gestionnaire de signal (handler) ne reçoit pas d'arguments
 * et ne voit pas les variables locales du main.
 */
int global_sem_id = -1;

/*
 * Gestionnaire de signal pour l'arrêt d'urgence (Ctrl+C).
 * Garantit que les ressources IPC (sémaphores) et fichiers (tubes nommés)
 * sont détruits proprement avant de quitter.
 */
void clean_exit(int signum) {
    fprintf(stderr, "\n[INTERRUPTION] Signal %d reçu (Ctrl+C). Nettoyage en cours...\n", signum);
    if (global_sem_id != -1) {
        // Suppression du tableau de sémaphores
        if (semctl(global_sem_id, 0, IPC_RMID) == -1) {
            perror("Erreur suppression sémaphore dans le handler");
        } else {
            fprintf(stderr, "-> Sémaphores supprimés.\n");
        }
    }
    // Suppression des tubes nommés
    unlink(TUBE_MC);
    unlink(TUBE_CM);
    fprintf(stderr, "-> Tubes nommés supprimés.\n");
    
    exit(EXIT_FAILURE);
}


/************************************************************************
 * Données persistantes d'un master
 ************************************************************************/

// on peut ici définir une structure stockant tout ce dont le master
// a besoin

typedef struct {
    int count;
    int highest;
} masterData;

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

bool whichOrder(int order, int mc_fd, int cm_fd, int fd_toWorker, int fd_toMaster, masterData *data, bool running){
    int ret;

    if (order == ORDER_STOP) {
        bool ack = true;

        // TODO : envoyer ordre de fin au premier worker et attendre sa fin
        // envoyer un accusé de réception au client
        ret = write(fd_toWorker, &order, sizeof(int));
        myassert(ret == sizeof(int), "écriture de l'ordre \"stop\" dans le tube master -> worker a échoué");

        // Une fois le premier worker terminé (celui ci se termine seulement quand les workers suivant sont terminé)
        ret = write(mc_fd, &ack, sizeof(bool));
        myassert(ret == sizeof(bool), "écriture de l'accusé de reception d'arrêt du master dans le tube master -> client a échoué");
        
        running = false;
    }
    else if (order == ORDER_COMPUTE_PRIME) {
        int number;

        ret = read(cm_fd, &number, sizeof(int));
        myassert(ret == sizeof(int), "lecture du nombre a calculer dans le tube client -> master a échoué");
        
        // TODO : pipeline workers
        for (int i = data->highest; i <= number; i++) {
            bool isprime;
            // TRACE("i: %d    highest: %d    max: %d\n",i,data->highest,number);

            ret = write(fd_toWorker, &i, sizeof(int));
            myassert(ret == sizeof(int), "écriture du nombre à tester dans le tube master -> worker a échoué");

            // TRACE("Les worker ont recu %d comme nombre\n",i);

            ret = read(fd_toMaster, &isprime, sizeof(bool));  // Reponse du worker
            myassert(ret == sizeof(bool), "lecture de la réponse si un nombre est premier dans le tube worker -> master a échoué");
            
            // TRACE("                                                            %c isPrime\n",isprime);
            if (isprime) {
                data->count += 1;
                data->highest = i;
                TRACE("                                                            %d premier\n",i);
            } else if(!isprime){
                TRACE("                                                            %d non premier\n",i);
            }
            else {
                // TRACE("Le retour du worker a planté\n");
            }
        }

        bool isprime;
        ret = write(fd_toWorker, &number, sizeof(int));
        myassert(ret == sizeof(int), "écriture du nombre à tester dans le tube master -> worker a échoué");

        ret = read(fd_toMaster, &isprime, sizeof(bool));  // Reponse du worker
        myassert(ret == sizeof(bool), "lecture de la réponse si un nombre est premier dans le tube worker -> master a échoué");
        

        ret = write(mc_fd, &isprime, sizeof(bool));
        myassert(ret == sizeof(bool), "écriture de la réponse si un nombre est premier dans le tube master -> client a échoué");
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
    return running;
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
void loop(int mc_fd, int cm_fd, int fd_toWorker, int fd_toMaster, int sem_mc_states, masterData *data)
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

    bool running = true;
    int order, ret;

    while (running) {
        // Ouverture des tubes entre Master et Client
        mc_fd = open(TUBE_MC, O_WRONLY);
        myassert(mc_fd != -1, "ouverture du tube master -> client en écriture a échoué");
        // TRACE("ouverture master -> client ecriture ok\n");

        cm_fd = open(TUBE_CM, O_RDONLY);
        myassert(cm_fd != -1, "ouverture du tube client -> master en lecture a échoué");
        // TRACE("ouverture client -> master lecture ok\n");

        // Lecture de l'ordre envoyé par Client
        ret = read(cm_fd, &order, sizeof(int));
        myassert(ret == sizeof(int), "lecture de l'ordre dans le tube client -> master a échoué");


        // Communication entre Master et Client (Matteo)
        running = whichOrder(order,mc_fd,cm_fd,fd_toWorker,fd_toMaster,data,running);


        // destruction des tubes nommés, des sémaphores, ...
        ret = close(mc_fd);
        myassert(ret == 0, "fermeture du tube master -> client a échoué");
        // TRACE("fermeture master -> client ok\n");

        ret = close(cm_fd);
        myassert(ret == 0, "fermeture du tube client -> master a échoué");
        // TRACE("fermeture client -> master ok\n");


        // TODO :
        // - attendre ordre du client avant de continuer (sémaphore : précédence)
        // - revenir en début de boucle
        sem_edit(sem_mc_states, -1, SEM_MASTER);
    
        }
}


/************************************************************************
 * Fonction principale
 ************************************************************************/

int main(int argc, char * argv[])
{
    /* * Installation du gestionnaire pour le signal SIGINT (Ctrl+C).
     * Si l'utilisateur interrompt le programme, 'clean_exit' sera exécutée.
     */
    signal(SIGINT, clean_exit);


    if (argc != 1) {
        usage(argv[0], NULL);
    }

    //valeur de retour
    int ret;

    //tube nommé entre master et client
    int mc_fd = 0, cm_fd = 0; // Initialisation à 0 pour éviter les warnings sur l'appel de la fonction Loop.

    int fd_toWorker[2], fd_toMaster[2];

    pid_t pid_child;

    masterData *data = (masterData *) malloc(sizeof(masterData));

    // - création des sémaphores
    /*
        on crée 2 sémaphores liés à la variable sem_mc_states.
        en position 0 celui bloquant les clients
        en position 1 celui bloquant le master
        les positions sont masquées dans le .h
    */
    key_t key = ftok(FILENAME, MASTER_CLIENT);
    myassert(key != -1, "Erreur dans ftok(), la clé est incorrecte");

    int sem_mc_states = semget(key, 2, IPC_CREAT|0641);
    myassert(sem_mc_states != -1, "Echec de la création des sémaphores client <-> master");

    // On sauvegarde l'ID dans la variable globale pour que le handler puisse le supprimer
    global_sem_id = sem_mc_states;

    //initialisation des semaphores
    ret = semctl(sem_mc_states, SEM_CLIENT, SETVAL, 1);
    myassert(ret != -1, "Erreur à l'initialisation du semaphor client");
    ret = semctl(sem_mc_states, SEM_MASTER, SETVAL, 1);
    myassert(ret != -1, "Erreur à l'initialisation du semaphor master");
    
    //semaphore pour les dialogues worker-master (loic)

    // - création des tubes nommés master <-> client
    ret = mkfifo(TUBE_MC, 0641);
    myassert(ret == 0, "création du tube master -> client a échoué");
    ret = mkfifo(TUBE_CM, 0641);
    myassert(ret == 0, "création du tube client -> master a échoué");

    // - création des tubes nommés master <-> worker
    ret = pipe(fd_toWorker);
    myassert(ret == 0, "création du tube master -> worker a échoué");

    ret = pipe(fd_toMaster);
    myassert(ret == 0, "création du tube worker -> master a échoué");

    pid_child = fork();

    if (pid_child != 0) {
        close(fd_toWorker[0]);
        close(fd_toMaster[1]);
    } else {
        close(fd_toWorker[1]);
        close(fd_toMaster[0]);

        char str_fd_toWorker[20]; // faudrait faire un malloc
        char str_fd_toMaster[20]; // faudrait faire un malloc

        sprintf(str_fd_toWorker,"%d",fd_toWorker[0]);
        sprintf(str_fd_toMaster,"%d",fd_toMaster[1]);

        execl("./worker", "./worker", "2", str_fd_toWorker, str_fd_toMaster, NULL);
        EXIT_FAILURE;
    }
    

    data->count += 1;
    data->highest = 2;

    // boucle infinie
    //mise à 0 du sémaphore du master pour qu'il se bloque en fin de tour de boucle
    sem_edit(sem_mc_states, -1, SEM_MASTER);
    loop(mc_fd, cm_fd, fd_toWorker[1], fd_toMaster[0], sem_mc_states, data);

    //destruction des sémaphore entre le master et les clients
    // TRACE("Destruction des semaphores\n");
    ret = semctl(sem_mc_states, -1, IPC_RMID);
    myassert(ret != -1, "Erreur lors de la destruction des semaphores client <-> master");

    // TRACE("Suppression des tubes nommés\n");
    ret = unlink(TUBE_MC);
    myassert(ret == 0, "suppression du tube master -> client a échoué");
    ret = unlink(TUBE_CM);
    myassert(ret == 0, "suppression du tube client -> master a échoué");

    // Fermeture des tubes entre master et worker coté master
    close(fd_toWorker[1]);
    close(fd_toMaster[0]);

    free(data);

    return EXIT_SUCCESS;
}

// N'hésitez pas à faire des fonctions annexes ; si les fonctions main
// et loop pouvaient être "courtes", ce serait bien
