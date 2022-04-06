#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>


#define TAILLE_COMMANDE 1024

extern int errno;

// -- fonction de comparaison entre deux valeurs utilisé dans qsort() --
int comp(const void * a, const void * b)
{
    int const *a2 = a;
    int const *b2 = b;
    return *a2 - *b2;
}


int main(int argc, char const *argv[])
{

    char commande[TAILLE_COMMANDE];

    int k = 0;
    int p = 0;
    int cr;
    int fd_poubelle;
    int tube_pere[2];
      
    struct timeval sec;
    struct timeval secTemp;
    
    float tFin = 0.0;
    float tMoy = 0.0; 
    float * tabTemp = NULL;

    pid_t pid = 0;

    if(argc < 4)
    {
        fprintf(stderr, "Usage : nomprog < K > < commande > <nb process> \n");
        exit(EXIT_FAILURE);
    }

    k = atoi(argv[1]);
    strcpy(commande, argv[2]);
    p = atoi(argv[3]);

    tabTemp = malloc(sizeof(float) * p);

    if((fd_poubelle = open("/dev/null", O_WRONLY, 0666)) == -1)
    {
        fprintf(stderr, "Probleme ouvertur /dev/null \n");
        exit(EXIT_FAILURE);
    }

    if(tabTemp == NULL)
    {
        fprintf(stderr, "Problème malloc \n");
        exit(EXIT_FAILURE);
    }
    

    printf("k : %d | commande : %s | p : %d \n", k, commande, p);

    pipe(tube_pere);
    

    for(int i = 0; i < p; i++)
    {
        switch (fork())
        {
            // -- Cas d'erreur -- 
            case -1:
                fprintf(stderr, "Problème dans le fork \n");
                exit(EXIT_FAILURE);
            break;
            
            // -- Execution du Fils --
            case 0:
                
                // -- on ferme le tube en lecture --
                close(tube_pere[0]);
                
                for(int y = 0; y < k; y++)
                {   
                    gettimeofday(&sec, NULL);


                    // printf("temps1 %ld \n", sec.tv_usec);

                    switch (pid = fork())
                    {
                        case -1: 
                            fprintf(stderr, "Problème fork du petit fils ... \n");
                            exit(-1);
                        
                        // -- dans le petit fils --
                        case 0:
                            
                            // -- ferme les entrées et sorties standard du fils --
                            // -- redirige vers /dev/null --
                            close(1);
                            dup(fd_poubelle);
                            close(2);
                            dup(fd_poubelle);
                            close(fd_poubelle);


                            if( execlp(commande, commande, NULL) == -1)
                            {
                                // -- retourne le code d'erreur placé dans errno par execlp --
                                fprintf(stderr, "Probleme execlp \n");
                                exit(errno);
                            }

                        default: waitpid(pid, &cr, 0);
                    }
                    
                    gettimeofday(&secTemp, NULL);

                    // -- Tests des codes erreurs en cas d'exit() -- 
                    if( WIFEXITED(cr))
                    {
                        if( WEXITSTATUS(cr) != 0)
                        {
                            // -- on fait un décalage de bit pour obtenir les 8 derniers bits du cr --
                            // -- c'est dans ces 8 derniers bit qu'est stocké l'errno -- 
                            
                            unsigned int low8bits = cr >> 8;
                            printf("Retour avec le code d'erreur errno : %u\n", low8bits);
                            
                            // -- traitements des cas d'erreur communs si non retourne la valeur de errno --
                            switch (low8bits)
                            {
                                case 1:
                                    fprintf(stderr,"Vous n'avez pas les droits d'exécuter cette commande... \n");
                                break;

                                case 22:
                                    fprintf(stderr,"La commande n'existe pas... \n");
                                break;

                                default:
                                    fprintf(stderr, "Erreur : %s\n", strerror(low8bits));
                                break;
                            }
                            
                            exit(EXIT_FAILURE);
                        }
                    }
                    
                    // -- Test si le fils à recu un signal --
                    if(WIFSIGNALED(cr)) 
                    {
                        fprintf(stderr, "Commande interrompue par un signal \n");
                        printf("Signal numéro : %d\n",WTERMSIG(cr));
                        
                        // -- on affiche la macro associée au signal reçu -- 
                        psignal(WTERMSIG(cr), "Signal reçu : ");
                        
                        exit(EXIT_FAILURE);
                    }
                    

                    // printf("SecTemp %ld \n", secTemp.tv_usec);
                    
                    tFin += secTemp.tv_sec*1000 - sec.tv_sec*1000 + secTemp.tv_usec/1000 - sec.tv_usec/1000;

                    // printf("tFin : %f \n", tFin);
                }

                tMoy = tFin / k;
                write(tube_pere[1], &tMoy, sizeof(float));
                close(tube_pere[1]);            
                exit(EXIT_SUCCESS);
            
            break;
            
            // -- processus père --
            default: 
                waitpid(pid, &cr, 0);
                if( WEXITSTATUS(cr) == EXIT_FAILURE) exit(EXIT_FAILURE);
            break;
        }
        
    }


    for(int i = 0; i < p; i++)
    {
        read(tube_pere[0],&tabTemp[i], sizeof(float));
        // printf("tube pere  %d : %.3f (s)\n",i, tabTemp[i]);
    }

    // -- trie le tableau par ordre croissant des résultats --
    qsort(tabTemp, p, sizeof(float), comp);
    
    for(int i = 0; i < p; i++)
    {
        printf("tube pere  %d : %.3f (s)\n",i, tabTemp[i]);
    }
    
    float tpMoy = tabTemp[p/2];

    printf("Temps médian : %.3f \n", tpMoy);
    
    return 0;
}