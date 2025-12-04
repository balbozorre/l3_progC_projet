#if defined HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "myassert.h"

#include "master_client.h"

// chaines possibles pour le premier paramètre de la ligne de commande
#define TK_STOP      "stop"
#define TK_COMPUTE   "compute"
#define TK_HOW_MANY  "howmany"
#define TK_HIGHEST   "highest"
#define TK_LOCAL     "local"

/************************************************************************
 * Usage et analyse des arguments passés en ligne de commande
 ************************************************************************/

static void usage(const char *exeName, const char *message)
{
    fprintf(stderr, "usage : %s <ordre> [<nombre>]\n", exeName);
    fprintf(stderr, "ordre \"" TK_STOP  "\" : arrêt master\n");
    fprintf(stderr, "ordre \"" TK_COMPUTE  "\" : calcul de nombre premier\n");
    fprintf(stderr, "<nombre> doit être fourni\n");
    fprintf(stderr, "ordre \"" TK_HOW_MANY "\" : combien de nombres premiers calculés\n");
    fprintf(stderr, "ordre \"" TK_HIGHEST "\" : quel est le plus grand nombre premier calculé\n");
    fprintf(stderr, "ordre \"" TK_LOCAL  "\" : calcul de nombres premiers en local\n");
    if (message != NULL)
        fprintf(stderr, "message : %s\n", message);
    exit(EXIT_FAILURE);
}

static int parseArgs(int argc, char * argv[], int *number)
{
    int order = ORDER_NONE;

    if ((argc != 2) && (argc != 3))
        usage(argv[0], "Nombre d'arguments incorrect");

    if (strcmp(argv[1], TK_STOP) == 0)
        order = ORDER_STOP;
    else if (strcmp(argv[1], TK_COMPUTE) == 0)
        order = ORDER_COMPUTE_PRIME;
    else if (strcmp(argv[1], TK_HOW_MANY) == 0)
        order = ORDER_HOW_MANY_PRIME;
    else if (strcmp(argv[1], TK_HIGHEST) == 0)
        order = ORDER_HIGHEST_PRIME;
    else if (strcmp(argv[1], TK_LOCAL) == 0)
        order = ORDER_COMPUTE_PRIME_LOCAL;
    
    if (order == ORDER_NONE)
        usage(argv[0], "ordre incorrect");
    if ((order == ORDER_STOP) && (argc != 2))
        usage(argv[0], TK_STOP" : il ne faut pas de second argument");
    if ((order == ORDER_COMPUTE_PRIME) && (argc != 3))
        usage(argv[0], TK_COMPUTE " : il faut le second argument");
    if ((order == ORDER_HOW_MANY_PRIME) && (argc != 2))
        usage(argv[0], TK_HOW_MANY" : il ne faut pas de second argument");
    if ((order == ORDER_HIGHEST_PRIME) && (argc != 2))
        usage(argv[0], TK_HIGHEST " : il ne faut pas de second argument");
    if ((order == ORDER_COMPUTE_PRIME_LOCAL) && (argc != 3))
        usage(argv[0], TK_LOCAL " : il faut le second argument");
    if ((order == ORDER_COMPUTE_PRIME) || (order == ORDER_COMPUTE_PRIME_LOCAL))
    {
        *number = strtol(argv[2], NULL, 10);
        if (*number < 2)
             usage(argv[0], "le nombre doit être >= 2");
    }       
    
    return order;
}


/************************************************************************
 * Gestion des ordres envoyé au Master
 ************************************************************************/

void whichOrder(int order, int number, int mc_fd, int cm_fd){
    int ret;

    if (order == ORDER_STOP) {
        bool ack;
        ret = write(cm_fd, &order, sizeof(int));
        myassert(ret == sizeof(int), "écriture de l'ordre \"stop\" dans le tube client -> master a échoué");

        ret = read(mc_fd, &ack, sizeof(bool));
        myassert(ret == sizeof(bool), "lecture de l'accusé de reception d'arret du master dans le tube master -> client a échoué");
        printf("Le master a bien reçu l'ordre d'arrêt et a bien supprimé tout les worker\n");
    }
    else if (order == ORDER_COMPUTE_PRIME) {
        bool isprime;
        ret = write(cm_fd, &order, sizeof(int));
        myassert(ret == sizeof(int), "écriture de l'ordre \"demande si un nombre est premier\" dans le tube client -> master a échoué");
        ret = write(cm_fd, &number, sizeof(int));
        myassert(ret == sizeof(int), "écriture du nombre a calculer dans le tube client -> master a échoué");
        
        ret = read(mc_fd, &isprime, sizeof(bool));
        myassert(ret == sizeof(bool), "lecture de la réponse si un nombre est premier dans le tube master -> client a échoué");
        printf("Le nombre %d %s premier\n", number, (isprime ? "est" : "n'est pas"));
    }
    else if (order == ORDER_HOW_MANY_PRIME) {
        int count;
        ret = write(cm_fd, &order, sizeof(int));
        myassert(ret == sizeof(int), "écriture de l'ordre \"combien de nombres premiers calculés\" dans le tube client -> master a échoué");
        
        ret = read(mc_fd, &count, sizeof(int));
        myassert(ret == sizeof(int), "lecture du nombre de nombre premier connu dans le tube master -> client a échoué");
        printf("Le master a calculé %d nombres premiers\n", count);
    }
    else if (order == ORDER_HIGHEST_PRIME) {
        int highest;
        ret = write(cm_fd, &order, sizeof(int));
        myassert(ret == sizeof(int), "écriture de l'ordre \"quel est le plus grand nombre premier calculé\" dans le tube client -> master a échoué");
        
        ret = read(mc_fd, &highest, sizeof(int));
        myassert(ret == sizeof(int), "lecture du plus grand nombre premier connu dans le tube master -> client a échoué");
        printf("Le plus grand nombre premier calculé par le master est %d\n", highest);
    }
}



/************************************************************************
 * Fonction principale
 ************************************************************************/

int main(int argc, char * argv[])
{
    int number = 0;
    int order = parseArgs(argc, argv, &number);
    //printf("%d\n", order); // pour éviter le warning

    key_t key = ftok(FILENAME, MASTER_CLIENT);
    myassert(key != -1, "Clé renvoyé par ftok incorrecte");
    int sem_mc_states = semget(key, 2, 0);
    myassert(sem_mc_states != -1, "Echec de la recupération des sémaphores client <-> master");

    // order peut valoir 5 valeurs (cf. master_client.h) :
    //      - ORDER_COMPUTE_PRIME_LOCAL
    //      - ORDER_STOP
    //      - ORDER_COMPUTE_PRIME
    //      - ORDER_HOW_MANY_PRIME
    //      - ORDER_HIGHEST_PRIME
    //
    // si c'est ORDER_COMPUTE_PRIME_LOCAL
    //    alors c'est un code complètement à part multi-thread
    // sinon
    //    - entrer en section critique :
    //           . pour empêcher que 2 clients communiquent simultanément
    //           . le mutex est déjà créé par le master
    //    - ouvrir les tubes nommés (ils sont déjà créés par le master)
    //           . les ouvertures sont bloquantes, il faut s'assurer que
    //             le master ouvre les tubes dans le même ordre
    //    - envoyer l'ordre et les données éventuelles au master
    //    - attendre la réponse sur le second tube
    //    - sortir de la section critique
    //    - libérer les ressources (fermeture des tubes, ...)
    //    - débloquer le master grâce à un second sémaphore (cf. ci-dessous)
    // 
    // Une fois que le master a envoyé la réponse au client, il se bloque
    // sur un sémaphore ; le dernier point permet donc au master de continuer
    //
    // N'hésitez pas à faire des fonctions annexes ; si la fonction main
    // ne dépassait pas une trentaine de lignes, ce serait bien.

    if (order == ORDER_COMPUTE_PRIME_LOCAL) {
        // TODO : 3.3 client (bis) -> code pour calculer les nombres premiers en local

        printf("Actuellement indisponible.\n");

    } else {
        // code pour communiquer avec le master
        // TODO : section critique avec sémaphore
        //entrée en section critique pour bloquer les clients
        sem_edit(sem_mc_states, -1, SEM_CLIENT);

        //valeur de retour pour les assert
        int ret;
        //tube nommé entre master et client
        int mc_fd, cm_fd;

        mc_fd = open(TUBE_MC, O_RDONLY);
        myassert(mc_fd != -1, "ouverture du tube master -> client en lecture a échoué");
        //TRACE("ouverture master -> client lecture ok\n");

        cm_fd = open(TUBE_CM, O_WRONLY);
        myassert(cm_fd != -1, "ouverture du tube client -> master en écriture a échoué");
        // TRACE("ouverture client -> master ecriture ok\n");

        // Communication entre Client et Master (Matteo)
        whichOrder(order, number, mc_fd, cm_fd);

        // TODO : sortir de la section critique
        sem_edit(sem_mc_states, +1, SEM_CLIENT);

        // fermeture des tubes nommés
        ret = close(mc_fd);
        myassert(ret == 0, "fermeture du tube master -> client a échoué");
        // TRACE("fermeture master -> client ok\n");

        ret = close(cm_fd);
        myassert(ret == 0, "fermeture du tube client -> master a échoué");
        // TRACE("fermeture client -> master ok\n");

        // TODO : libérer le master gràce au deuxième sémaphore
        sem_edit(sem_mc_states, +1, SEM_MASTER);
    }

    return EXIT_SUCCESS;
}
