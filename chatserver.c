/*
 *
 Chat Server - @author - Apoorva Davu,Laxmi Deepthi Atreyapurapu,Manisha Savale
 *
 */
//Header files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <resolv.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
//Constants
#define SERVERIP "192.168.1.8"
#define PORT 8080
#define BACKLOG 100 //defining the maximum number of connections the listen system call can execute
#define CLIENTS 100//defining maximum number of clients who can connect to the server
#define BUFFSIZE 1024// defining the size of the buffer for used for exchage of message.
#define ALIASLEN 32
#define OPTLEN 16

int client;


//protocol defined packet structure
struct PACKET {
    char option[OPTLEN]; // instruction
    char alias[ALIASLEN]; // client's alias
    char buff[BUFFSIZE]; // payload (message)
};

//structure to store client information
struct THREADINFO {
    pthread_t thread_ID; // thread's pointer
    int sockfd; //  create a socket file descriptor for establishing socket connections
    char alias[ALIASLEN]; //initialising a variable for  client's alias
};

//struture to store thread information and linked list node
struct LLNODE {
    struct THREADINFO threadinfo;
    struct LLNODE *next;
};

// linked list node used to contain information about thread nodes and linklistnode .This is used to store client information
struct LLIST {
    struct LLNODE *head, *tail;
    int size;
};
// method for comparing two sockets
int compare(struct THREADINFO *a, struct THREADINFO *b) {
    return a->sockfd - b->sockfd;
}
//method using linked list to accept client connections
void list_init(struct LLIST *ll) {
    ll->head = ll->tail = NULL;
    ll->size = 0;
}
//method using linked list to accept  new client connections and store them in a list
int list_insert(struct LLIST *ll, struct THREADINFO *thr_info) {
    if(ll->size == CLIENTS) return -1;
    if(ll->head == NULL) {
        ll->head = (struct LLNODE *)malloc(sizeof(struct LLNODE));
        ll->head->threadinfo = *thr_info;
        ll->head->next = NULL;
        ll->tail = ll->head;
    }
    else {
        ll->tail->next = (struct LLNODE *)malloc(sizeof(struct LLNODE));
        ll->tail->next->threadinfo = *thr_info;
        ll->tail->next->next = NULL;
        ll->tail = ll->tail->next;
    }
    ll->size++;
    return 0;
}
// method used to delete the client entry from the linked list after clients logout
int list_delete(struct LLIST *ll, struct THREADINFO *thr_info) {
    struct LLNODE *curr, *temp;
    if(ll->head == NULL) return -1;
    if(compare(thr_info, &ll->head->threadinfo) == 0) {
        temp = ll->head;
        ll->head = ll->head->next;
        if(ll->head == NULL) ll->tail = ll->head;
        free(temp);
        ll->size--;
        return 0;
    }
    for(curr = ll->head; curr->next != NULL; curr = curr->next) {
        if(compare(thr_info, &curr->next->threadinfo) == 0) {
            temp = curr->next;
            if(temp == ll->tail) ll->tail = curr;
            curr->next = curr->next->next;
            free(temp);
            ll->size--;
            return 0;
        }
    }
    return -1;
}
// method used to keep the chatlog and connection count of the clients
void list_dump(struct LLIST *ll) {
    struct LLNODE *curr;
    struct THREADINFO *thr_info;
    
    printf("Connection count: %d\n", ll->size);
    for(curr = ll->head; curr != NULL; curr = curr->next) {
        thr_info = &curr->threadinfo;
        printf("[%d] %s\n", thr_info->sockfd, thr_info->alias);
    }
}

/************* MAIN METHOD  ***********************/

int sockfd, newfd=0;
struct THREADINFO thread_info[CLIENTS];
struct LLIST client_list;
pthread_mutex_t clientlist_mutex;
// methods declaration
void *io_handler(void *param);
void *client_handler(void *fd);
int inBuiltHeartBeat(int s);
int handleServer(int port);
void sig_handler(int signum);
void servlet(void);

int main(int argc, char **argv) {
    
    handleServer(PORT);
    return 0;
}


// method for handling server connections
void *io_handler(void *param) {
    char option[OPTLEN];
    while(scanf("%s", option)==1) {
        if(!strcmp(option, "exit")) { // comparing the command line options given by user,and if matches with exit the connection from server will be lost
            /* clean up */
            printf("Terminating server...\n");
            printf("\nIn io handler socket fd value is : %d",sockfd);
            inBuiltHeartBeat(sockfd);
           
        }
        else if(!strcmp(option, "list")) { //if command line option matches with the list it gives the number of clients coonected to the serevr
            pthread_mutex_lock(&clientlist_mutex);
            list_dump(&client_list);
            pthread_mutex_unlock(&clientlist_mutex);
        }
        else {
            fprintf(stderr, "Unknown command: %s...\n", option);// if any option other than list and exit is given the server cant recognise that
        }
    }
    return NULL;
}
// method for handling entire client connections
void *client_handler(void *fd) {
    struct THREADINFO threadinfo = *(struct THREADINFO *)fd;
    struct PACKET packet;
    struct LLNODE *curr;
    int bytes, sent;
    while(1) {
        bytes = recv(threadinfo.sockfd, (void *)&packet, sizeof(struct PACKET), 0);
        if(!bytes) {
            fprintf(stderr, "Connection lost from [%d] %s...\n", threadinfo.sockfd, threadinfo.alias);
            pthread_mutex_lock(&clientlist_mutex);// acquring locks foe individual clients to avoid race conditions among various clinets threads
            list_delete(&client_list, &threadinfo);
            pthread_mutex_unlock(&clientlist_mutex);
            break;
        }// checking for the availability of client
        printf("[%d] %s %s %s\n", threadinfo.sockfd, packet.option, packet.alias, packet.buff);
        if(!strcmp(packet.option, "alias")) {
            printf("Set alias to %s\n", packet.alias);
            pthread_mutex_lock(&clientlist_mutex);// acquring lock
            for(curr = client_list.head; curr != NULL; curr = curr->next) {
                if(compare(&curr->threadinfo, &threadinfo) == 0) {
                    strcpy(curr->threadinfo.alias, packet.alias);
                    strcpy(threadinfo.alias, packet.alias);
                    break;
                }
            }
            pthread_mutex_unlock(&clientlist_mutex);
        }// after checking for clients available if clients wishes to have a  private chat the following method is called
        else if(!strcmp(packet.option, "whisp")) {
            int i;
            char target[ALIASLEN];
            for(i = 0; packet.buff[i] != ' '; i++)
            {
                
            }
            packet.buff[i++] = 0;
            strcpy(target, packet.buff);
            pthread_mutex_lock(&clientlist_mutex);//acquring lock
            for(curr = client_list.head; curr != NULL; curr = curr->next) {
                if(strcmp(target, curr->threadinfo.alias) == 0) {
                    struct PACKET spacket;
                    memset(&spacket, 0, sizeof(struct PACKET));
                    if(!compare(&curr->threadinfo, &threadinfo)) continue;
                    strcpy(spacket.option, "msg");
                    strcpy(spacket.alias, packet.alias);
                    strcpy(spacket.buff, &packet.buff[i]);
                    sent = send(curr->threadinfo.sockfd, (void *)&spacket, sizeof(struct PACKET), 0);
                }
            }
            pthread_mutex_unlock(&clientlist_mutex);
        }// if the clients wishes to have a group chat the chet room service is established
        else if(!strcmp(packet.option, "send")) {
            pthread_mutex_lock(&clientlist_mutex);
            for(curr = client_list.head; curr != NULL; curr = curr->next) {
                struct PACKET spacket;
                memset(&spacket, 0, sizeof(struct PACKET));
                if(!compare(&curr->threadinfo, &threadinfo)) continue;
                strcpy(spacket.option, "msg");
                strcpy(spacket.alias, packet.alias);
                strcpy(spacket.buff, packet.buff);
                sent = send(curr->threadinfo.sockfd, (void *)&spacket, sizeof(struct PACKET), 0);
            }
            pthread_mutex_unlock(&clientlist_mutex);
        }
        else if(!strcmp(packet.option, "exit")) {// disconnecting client connection once the client chooses logout
            printf("[%d] %s has disconnected...\n", threadinfo.sockfd, threadinfo.alias);
            pthread_mutex_lock(&clientlist_mutex);
            list_delete(&client_list, &threadinfo);
            pthread_mutex_unlock(&clientlist_mutex);
            break;
        }
        else {
            fprintf(stderr, "Garbage data from [%d] %s...\n", threadinfo.sockfd, threadinfo.alias);
        }
    }
    
    /* clean up */
    close(threadinfo.sockfd);
    
    return NULL;
}
//Method that establishes server conenctions and accepts client requests and invokes appropriate service
int handleServer(int port){
    int err_ret, sin_size;
    struct sockaddr_in serv_addr, client_addr;
    pthread_t interrupt;
    
    // initializing the  linked list
    list_init(&client_list);
    
    // initiate  mutex variables to avoid race conditions
    pthread_mutex_init(&clientlist_mutex, NULL);
    
    
    //create   a socket for establishing communication
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        err_ret = errno;
        fprintf(stderr, "socket() failed...\n");
        return err_ret;
    }
    // set all the initial values
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(IP);
    memset(&(serv_addr.sin_zero), 0, 8);
    
    // bind address with socket
    if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) == -1) {
        err_ret = errno;
        fprintf(stderr, "bind() failed...\n");
        return err_ret;
    }
    
    
    //start listening for connection
    if(listen(sockfd, BACKLOG) == -1) {
        err_ret = errno;
        fprintf(stderr, "listen() failed...\n");
        return err_ret;
    }
    
    // initialising the interrupt handler for IO controlling
    printf("Starting admin interface...\n");
    if(pthread_create(&interrupt, NULL, io_handler, NULL) != 0) {
        err_ret = errno;
        fprintf(stderr, "pthread_create() failed...\n");
        return err_ret;
    }
    
    // keep accepting connections
    printf("Starting socket listener...\n");
    while(1) {
        sin_size = sizeof(struct sockaddr_in);
        if((newfd = accept(sockfd, (struct sockaddr *)&client_addr, (socklen_t*)&sin_size)) == -1) {
            err_ret = errno;
            fprintf(stderr, "accept() failed...\n");
            return err_ret;
        }
        else {
            if(client_list.size == CLIENTS) {
                fprintf(stderr, "Connection full, request rejected...\n");
                continue;
            }
            printf("Connection requested received...\n");
            struct THREADINFO threadinfo;
            threadinfo.sockfd = newfd;
            strcpy(threadinfo.alias, "Anonymous");//if login name is  not given  the client is connected as anonymous
            pthread_mutex_lock(&clientlist_mutex);//mutex for providing synchronization for client list and thread info
            list_insert(&client_list, &threadinfo);//accepting the client requests and storing them in linkedlist
            pthread_mutex_unlock(&clientlist_mutex);//releasing the lock
            
            pthread_create(&threadinfo.thread_ID, NULL, client_handler, (void *)&threadinfo);
        }
    }
}

// method used to check the availability of server.
// the keepalive socket options enables the server to be active and wait for the client coonections.

int inBuiltHeartBeat(int s){
    int optval;
    socklen_t optlen = sizeof(optval);
    
    
    printf("\nUsing Heartbeat mechanism to backup server....");
     printf("Before setting sock option socket : %d",s);
    /* Set the option active*/
    optval = 1;
    optlen = sizeof(optval);
    if(setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0) {
        perror("setsockopt()");
        close(s);
        exit(EXIT_FAILURE);
    }
    printf("\nSO_KEEPALIVE set on server socket..Server cannot be shut untill 2 hours\n");
    
    printf("socket : %d",s);
    handleServer(8023);
    return 0;
}

