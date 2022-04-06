#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <getopt.h>

#define NBARGUMENTS 5
#define TAILLE_COMMANDE 256 // Taille maximale de la commande
#define CORBEILLE "/dev/null"

int AFFICHER = 1;

/*
 * Usage
 */

void usage()
{
	fprintf(stdout, "Benchmark\n");
	fprintf(stdout, "Usage :\n");
	fprintf(stdout, ">> ./prog  <nbExecution> <commande> <nbProcessus> <trace>\n");
  fprintf(stdout, "  >> trace :      | 1 : Affichage\n");
  fprintf(stdout, "                  | 0 : Pas d'affichage\n");
}

int IntComparer(const void *a1, const void *b1)
{
	const int * a2 = a1;
	const int * b2 = b1;
	return *a2-*b2;
}

int main(int argc, char *argv[])
{
	// Variables des arguments
	int nbCommande; 					// Nombre de fois que la commande est executee par un processus
	char commande[TAILLE_COMMANDE];
	int nbProc; 				// Nombre de processus executant la commande
	// Variables
	struct timeval tpsDebut, tpsFin;      // Structure timeval, pour calculer le temps entre le début et la fin de l'exec
	int cr;                               // Compte-rendu
	int nbArg = argc;                     // Nombre d'arguments
	int trace;                            // Flag trace, si trace == 1, affiche le temps moyen pour chaque processus
	int cptProcessus;                      // Compteur de processus fils
  int cptExecutions;		                // Compteur de commandes à exécuter
	float tpsMoyen=0.0; 							    // Temps d'execution moyen d'une commande
	float tpsProc=0.0; 							      // Temps d'execution d'un processus
	int tube[2];
	int output;                           // Place dans le tableau où est la moyenne
  int fdCorbeille;


	if(nbArg != NBARGUMENTS)
	{
		usage();
		exit(-1);
	}

	printf("\n-- Debut du programme --\n\n");

	nbCommande = atoi(argv[1]);
	strcpy(commande, argv[2]);
	nbProc = atoi(argv[3]);
  trace = atoi(argv[4]);
	float tabMoy[nbProc];		// Tableau des temps moyen d'execution de chaque processus

	// Initialisation de tabMoy à 0
	for(cptProcessus=0; cptProcessus<nbProc; cptProcessus++)
  {
		tabMoy[cptProcessus] = 0;
	}

	pipe(tube);

	// Création des processus
	for(cptProcessus=0; cptProcessus < nbProc; cptProcessus++)
		switch(fork())
    {
			case -1: // Erreur fork
					perror("\nErreur fork\n");
					exit(-1);

			case 0:
      /*    FILS    */
					for (cptExecutions=0; cptExecutions < nbCommande; cptExecutions++)
          {
						// Ferme l'extremite de lecture du tube car le fils ne lit pas dedans
						close(tube[0]);

						// Acquisition de la date de debut
						gettimeofday(&tpsDebut, NULL);

						// Execution commande dans un petit-fils
						switch(fork())
            {
							case -1: // Erreur
                  perror("\nErreur fork\n");
                  exit(-1);

							case 0:
              /*    PETIT FILS    */
								// Redirection de la sortie des commandes
								if((fdCorbeille = open(CORBEILLE, O_WRONLY, 0666))==-1)
                {
									fprintf(stderr, "\nErreur open\n");
									exit(-1);
								}

								close(1);
								dup(fdCorbeille);
								close(2);
								dup(fdCorbeille);
								close(fdCorbeille);

								// Recouvrement du processus petit fils par la commande
								execlp(commande, commande, NULL);

							default:
              /*    FILS    */
              // Attend la fin de l'execution
								wait(&cr);
						}

          /*    SUITE FILS    */

						// Acquisition de la date de fin d'execution des commandes
						gettimeofday(&tpsFin, NULL);

						// Tests des erreurs
						if(WIFEXITED(cr)) // Renvoie vrai si le fils s'est terminé normalement (exit ou retour de main)
							if(WEXITSTATUS(cr) != 0) // Renvoie le code de sortie du fils
              {
								if(WEXITSTATUS(cr) == 127)
                {
									fprintf(stderr,"Commande \"%s\" inconnue\n", commande);
									exit(-1);
								}
								fprintf(stderr, "Commande \"%s\" interrompue par exit %d\n", commande, WEXITSTATUS(cr));
								exit(-1);
							}
						if(WIFSIGNALED(cr)) // Renvoie vrai si le fils s'est terminé à cause d'un signal
            {
							fprintf(stderr, "Commande \"%s\" interrompue par le signal %d\n", commande, WTERMSIG(cr));
							exit(-1);
						}
						// Calcul du temps ecoule
						tpsProc += tpsFin.tv_sec*1000 - tpsDebut.tv_sec*1000 + tpsFin.tv_usec/1000 - tpsDebut.tv_usec/1000;
						if (trace)
							printf("Temps d'execution : %s || %d fois || %f ms\n", commande, nbCommande, tpsProc);
					}
					tpsMoyen = tpsProc/nbCommande; // temps moyen d'execution / proc

					// Envoi au pere
					write(tube[1], &tpsMoyen, sizeof(float));
					close(tube[1]);
					exit(0);
		}

    /*      PERE     */

	// Le pere n'ecrit pas dans le tube
	close(tube[1]);

	// Acquisition du temps moyen total des commandes
	for(cptProcessus = 0; cptProcessus < nbProc; cptProcessus++)
  {
		read(tube[0], &tabMoy[cptProcessus], sizeof(float));
		if(tabMoy[cptProcessus] == 0.0)
    {
			AFFICHER = 0;
			break;
		}
	}
	close(tube[0]);

	if (AFFICHER) // Test de la validitée des temps reçu
  {
		// Tri du tableau tabMoy
		qsort(tabMoy, nbProc, sizeof(float), IntComparer);
			for(cptProcessus=0; cptProcessus<nbProc; cptProcessus++)
      {
				printf("Processus %2i || Temps moyen d'execution : %s || %d fois || %f ms\n", cptProcessus+1, commande, nbCommande, tabMoy[cptProcessus]);
			}
      printf("\n");


		// Affichage du resultat
    if(nbProc%2 == 0)
    {
      tpsMoyen = ( tabMoy[nbProc/2] + tabMoy[(nbProc/2)-1] ) / 2;
      printf("Temps moyen d'execution : %s || %i processus || %2d fois || %fs (%f ms) \n\n", commande, nbProc, nbCommande, tpsMoyen/1000, tpsMoyen);
      printf("-- Fin du programme --\n\n");
      exit(0);
    }
    else
    {
      output = nbProc/2;
      if(tabMoy[output] !=0)
      {
        printf("Temps moyen d'execution : %s || %i processus || %2d fois || %fs (%f ms) \n\n", commande, nbProc, nbCommande, tabMoy[output]/1000, tabMoy[output]);
        printf("-- Fin du programme --\n\n");
        exit(0);
      }
    }
	}
  else
  {
		printf("\nTemps d'exécution nul.\n\n");
	}
	return 0;
}
