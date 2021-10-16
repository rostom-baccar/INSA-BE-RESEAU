//Changements par rapport à l'ancienne version:
//simplification de quelques commandes
//mise en place de quelques instructions de vérification supplémentaires
//suppression de la manipulation des states (facultatif)

#include <mictcp.h>
#include <api/mictcp_core.h>

//variables globales
mic_tcp_sock sock;
mic_tcp_sock_addr addr_sock;
unsigned int PE = 0; //variable représentant le numéro de séquence de la trame à émettre
unsigned int PA = 0; //variable représentant le numéro de séquence de la trame attendue

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
        set_loss_rate(10);
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
        sock.addr = addr; 
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
    int taille_pdu;
    unsigned long timeout = 100; //100ms
    int ack_recv = 0;            //1 si reçu et 0 sinon
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
        //ack
        //après l'envoi, on attend un ack
        //charge utile
        ack_pdu.payload.size = 0; //contenu vide
        while (ack_recv == 0)     //cette boucle tourne tant qu'on ne reçoit pas de ack
        {
            //on fait un double test: (on vérifie qu'on a bien reçu un ack, le timer n'ayant pa expiré) ET (on vérifie que c'est le bon numéro de séquence)
            if ((IP_recv(&(ack_pdu), &addr_sock, timeout) >= 0) && (ack_pdu.header.ack == 1) && (ack_pdu.header.ack_num == PE))
            {
                ack_recv = 1; //num de ack reçu = 1
            }
            else
            { //dans le cas ou le timer a expiré OU s'il s'agit du mauvais numéro de séquence, on renvoie le pdu
                taille_pdu = IP_send(pdu, addr_sock);
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
    mic_tcp_pdu ack_pdu; //pour éviter les confusions avec .ack
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
    //remarque: en cas de mauvais numéro de séquence, PA ne change pas
    //ack
    // header
    ack_pdu.header.ack_num = PA;
    ack_pdu.header.ack = 1;
    //il faut dire à la machine que la taille est nulle pour qu'elle
    //sache qu'on n'envoie pas de pdu
    ack_pdu.payload.size = 0;

    //envoi de l'ack
    if (IP_send(ack_pdu, addr) < 0)
    {
        printf("erreur envoi ack\n");
    }
}
