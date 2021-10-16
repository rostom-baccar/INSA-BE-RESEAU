#include <mictcp.h>
#include <api/mictcp_core.h>
#define N 10 //taille du tableau

//variables globales
mic_tcp_sock sock;
mic_tcp_sock_addr addr_sock;
unsigned int PE = 0;                       //variable représentant le numéro de séquence de la trame à émettre
unsigned int PA = 0;                       //variable représentant le numéro de séquence de la trame attendue
int t[N] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1}; //t est un tableau qui contient l'état de réception d'un pdu donné
//ici on prend une taille égale à 10. Plus le tableau est grand plus on peut contrôler le taux de perte avec précision
//initialisation du tableau à des 1 (à l'initialisation on suppose que tout les pdu ont été reçu)
double perte_admissible = 0.3; //dans cet exemple on prend une perte admissible de 30%
int prochain_pdu = 0;          //cette variable servira à faire avancer les indices de chaque PDU dans le tableau, l'avancement se fait sous forme circulaire
//prochain_pdu est initialisée à l'indice du premier élément du tableau

/*
 * Permet de créer un socket entre l’application et MIC-TCP
 * Retourne le descripteur du socket ou bien -1 en cas d'erreur
 */
int mic_tcp_socket(start_mode sm)
{
    printf("[MIC-TCP] Appel de la fonction: ");
    printf(__FUNCTION__);
    printf("\n");
    if (initialize_components(sm) != -1)
    { /* Appel obligatoire */

        //init
        sock.fd = 0;
        set_loss_rate(30); //le loss rate est plus grand que la perte admissible, on est sur de renvoyer des PDU(s) pendant l'exécution (même dans le meilleur des cas)
        //on retourne la description du socket
        return sock.fd;
    }
    else
    {
        return -1;
    }
}

/*
 * Permet d’attribuer une adresse à un socket.
 * Retourne 0 si succès, et -1 en cas d’échec
 */
int mic_tcp_bind(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");
    printf(__FUNCTION__);
    printf("\n");
    if (sock.fd == socket)
    {
        sock.addr = addr; //commande plus simple
        return 0;
    }
    else
    {
        return -1;
    }
}

/*
 * Met le socket en état d'acceptation de connexions
 * Retourne 0 si succès, -1 si erreur
 */
int mic_tcp_accept(int socket, mic_tcp_sock_addr *addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");
    printf(__FUNCTION__);
    printf("\n");
    return 0;
}

/*
 * Permet de réclamer l’établissement d’une connexion
 * Retourne 0 si la connexion est établie, et -1 en cas d’échec
 */
int mic_tcp_connect(int socket, mic_tcp_sock_addr addr)
{
    printf("[MIC-TCP] Appel de la fonction: ");
    printf(__FUNCTION__);
    printf("\n");
    return 0;
}

/*
 * Permet de réclamer l’envoi d’une donnée applicative
 * Retourne la taille des données envoyées, et -1 en cas d'erreur
 */
int mic_tcp_send(int mic_sock, char *mesg, int mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: ");
    printf(__FUNCTION__);
    printf("\n");
    //déclarations
    mic_tcp_pdu pdu;
    addr_sock = sock.addr; //commande ajoutée, précedemment oubliée
    mic_tcp_pdu ack_pdu;   //pour éviter la confusion avec les .ack
    mic_tcp_pdu ack_pdu2;  //ACK DE L'ACK //BETA
    int taille_pdu;
    unsigned long timeout = 100; //100ms
    int ack_recv = 0;            //1 si ack reçu et 0 sinon
    int nbr_pdu_recus = 0;       //variable servant à parcourir le tableau de PDUs reçus (ou non) afin de calculer le pourcentage de perte
    double perte_calculee = 0;   //variable stockant la perte du PDUs calculée après parcours du tableau
    if (sock.fd == mic_sock)
    {
        //construction du pdu à envoyer
        //header
        pdu.header.dest_port = addr_sock.port;
        pdu.header.seq_num = PE; //association du num de séquence au sock
        //initialisation du champ ack
        pdu.header.ack = 0;
        //mise à jour de PE: 0 devient 1 et 1 devient 0 avec cette instruction
        PE = (PE + 1) % 2;
        //charge utile
        //pointeur sur la zone où se trouve la donnée à envoyer
        pdu.payload.data = mesg;
        //taille du message à envoyer
        pdu.payload.size = mesg_size;
        //envoi
        //renvoie la taille du message
        //la taille ne prend pas en compte le header!

        if ((taille_pdu = IP_send(pdu, addr_sock)) == -1)
        {
            printf("erreur envoi pdu\n");
            exit(-1);
        }
        printf("Envoi 1 effectué avec PE = %d\n", PE);
        //ack
        //après l'envoi, on attend un ack
        //charge utile
        ack_pdu.payload.size = 0;  //contenu vide
        ack_pdu2.payload.size = 0; //contenu vide //BETA
        while (ack_recv == 0)      //cette boucle tourne tant qu'on ne reçoit pas de ack
        {
            //on fait un double test: (on vérifie qu'on a bien reçu un ack, le timer n'ayant pa expiré) ET (on vérifie que c'est le bon numéro de séquence)
            if ((IP_recv(&(ack_pdu), &addr_sock, timeout) >= 0) && (ack_pdu.header.ack == 1) && (ack_pdu.header.ack_num == PE))
            {
                printf("Ack reçu avec PA = %d, PDU bien envoyé\n", PA);
                ack_recv = 1; //ack reçu
                //dans ce cas, on a bien reçu un ack, le PDU a bien été reçu. On met donc à jour son état de réception dans le tableau
                t[prochain_pdu] = 1;
                //on met à jour l'indice du prochain pdu pour faire avancer l'analyse dans la fenêtre. On lui attribue donc 1 dans le tableau
                prochain_pdu = (prochain_pdu + 1) % N;
                //BETA: ON ENVOIE ACK2 AU PUITS
                ack_pdu2.header.ack = 1;
                IP_send(ack_pdu2, addr_sock);
            }
            else if (IP_recv(&(ack_pdu), &addr_sock, timeout) < 0) //dans le cas ou le timer a expiré SEULEMENT
            {
                printf("Timer expiré\n");
                //dans ce cas, le PDU n'a pas été reçu car on n'a pas reçu de ack à temps
                //on lui attribue donc 0 dans le tableau
                t[prochain_pdu] = 0;

                //la fenêtre d'analyse est mise à jour à chaque pdu reçu mais le calcul de perte n'est fait que quand on perd un pdu

                //afin de calculer le pourcentage de perte, il nous faut d'abord savoir combien de PDU on a reçu sur N PDUs pour l'échantillon analysé
                //pour calculer le nombre de PDUs reçu on parcourt le tableau:
                nbr_pdu_recus = 0;
                for (int i = 0; i < N; i++)
                    nbr_pdu_recus += t[i]; //puisqu'il n'y a que des 0 et des 1 dans le tableau la somme les valeurs du tableau correspondra au nombre de PDUs reçus
                printf("nbr_pdu_recus: %d\n", nbr_pdu_recus);
                //on calcule maintenant le pourcentage de perte
                perte_calculee = (double)(N - nbr_pdu_recus) / (double)N;
                printf("Pourcentage de perte calculé: %f\n", perte_calculee * 100.0);
                printf("perte calculée: %f\n", perte_calculee);
                printf("perte admissible: %f\n", perte_admissible);

                //à ce stade il y a deux possibilités: soit la perte calculée respecte la perte admissible, soit elle ne la respecte pas
                //dans le cas ou la perte calculée respecte la perte admissible:
                if (perte_calculee <= perte_admissible)
                {
                    //dans ce cas on fait comme si le PDU a été reçu en recevant un ack
                    printf("La perte admissible est respectée\n");
                    ack_recv = 1; //ack reçu
                    //on met également à jour l'indice du prochain PDU dans le cas ou le PDU a été perdu
                    prochain_pdu = (prochain_pdu + 1) % N;
                    //il est important de mettre à jour le PE pour que le prochain PDU envoyé respecte PE=PA
                    PE = (PE + 1) % 2;
                    printf("Mise à jour du PE : PE = %d \n", PE);
                }
                else
                {
                    //dans le cas ou la perte calculee est plus importante que la perte admissible, on renvoie le PDU perdu
                    printf("La perte admissible n'est pas respectée, on renvoie le PDU perdu\n");
                    printf("Envoi 2 effectué avec PE = %d\n", PE);
                    if ((taille_pdu = IP_send(pdu, addr_sock)) == -1)
                    {
                        printf("erreur envoi pdu\n");
                        exit(-1);
                    }
                }
            }
            else
            { //dans le cas ou il s'agit du mauvais numéro de séquence : PA!=PE, on reste en attente d'une éventuelle répection du bon num de séquence (ou pas)
                printf("Réception du mauvais numéro de séquence, on reste en attente...\n");
            }
        }
    }
    return taille_pdu;
}

/*
 * Permet à l’application réceptrice de réclamer la récupération d’une donnée
 * stockée dans les buffers de réception du socket
 * Retourne le nombre d’octets lu ou bien -1 en cas d’erreur
 * NB : cette fonction fait appel à la fonction app_buffer_get()
 */
int mic_tcp_recv(int socket, char *mesg, int max_mesg_size)
{
    printf("[MIC-TCP] Appel de la fonction: ");
    printf(__FUNCTION__);
    printf("\n");

    //déclarations
    int taille_pdu_recu = -1; //taille de la donnée effectivement reçue
    mic_tcp_payload pdu;

    //charge utile
    pdu.data = mesg;
    pdu.size = max_mesg_size;

    if (sock.fd == socket)
    {
        //récupération du pdu dans les buffers de réception
        taille_pdu_recu = app_buffer_get(pdu);
    }
    //renvoie de la taille de la donnée effectivement reçue
    return taille_pdu_recu;
}

/*
 * Permet de réclamer la destruction d’un socket.
 * Engendre la fermeture de la connexion suivant le modèle de TCP.
 * Retourne 0 si tout se passe bien et -1 en cas d'erreur
 */
int mic_tcp_close(int socket)
{
    printf("[MIC-TCP] Appel de la fonction :  ");
    printf(__FUNCTION__);
    printf("\n");
    return 0;
}

/*
 * Traitement d’un PDU MIC-TCP reçu (mise à jour des numéros de séquence
 * et d'acquittement, etc.) puis insère les données utiles du PDU dans
 * le buffer de réception du socket. Cette fonction utilise la fonction
 * app_buffer_put().
 */
//fonction appelée automatiquement lorsqu'un paquet est reçu
void process_received_PDU(mic_tcp_pdu pdu, mic_tcp_sock_addr addr)
{
    mic_tcp_pdu ack_pdu;         //pour éviter les confusions avec .ack
    unsigned long timeout = 100; //100ms

    printf("[MIC-TCP] Appel de la fonction: ");
    printf(__FUNCTION__);
    printf("\n");

    //loader les buffers avec le pdu reçu S'IL S'AGIT DU BON NUM DE SEQUENCE
    if (pdu.header.seq_num == PA)
    {
        app_buffer_put(pdu.payload);
        //màj du PA
        PA = (PA + 1) % 2;
    }
    else
    {
        printf("mauvais numéro de séquence\n");
    }
    //remarque: en cas de mauvais numéro de séquence, PA ne change pas
    //ack
    // header
    ack_pdu.header.ack_num = PA;
    ack_pdu.header.ack = 1;
    //il faut dire à la machine que la taille est nulle pour qu'elle
    //sache qu'on n'envoie pas de pdu
    ack_pdu.payload.size = 0;

    //envoi de l'ack

    //PROBLEME:
    //Si le ack est perdu en cours de route, PA est quand-même mis à jour
    //ce qui crée un décalage de PE et PA pour le prochain PDU
    //le soucis c'est qu'une perte de ack ne peut pas être detectée coté source
    //problème à régler...

    printf("Envoi réceptionné avec PA = %d\n", ack_pdu.header.ack_num);
    if (IP_send(ack_pdu, addr) == -1)
    {
        printf("erreur envoi ack\n");
    }

    //DEBUT PARTIE BETA
    mic_tcp_pdu ack_pdu2; //ack de l'ack
    ack_pdu2.payload.size = 0;
    int ack_recv2 = 0;
    while (ack_recv2 == 0) //cette boucle tourne tant qu'on ne reçoit pas de ack de ack
    {
        //on fait un double test: (on vérifie qu'on a bien reçu un ack, le timer n'ayant pa expiré)
        if ((IP_recv(&(ack_pdu2), &addr_sock, timeout) >= 0) && (ack_pdu2.header.ack == 1))
        {
            ack_recv2 = 1; //num de ack reçu = 1
        }
        else
        { //dans le cas ou le timer a expiré OU s'il s'agit du mauvais numéro de séquence, on renvoie le pdu
            if (IP_send(ack_pdu, addr) == -1)
            {
                printf("erreur envoi ack\n");
            }
        }
    }
    //FIN PARTIE BETA
}
