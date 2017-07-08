/*
 *
 Chat Client  - @author - Apoorva Davu,Laxmi Deepthi Atreyapurapu,Manisha Savale
 *
 *
 */
//Header files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <resolv.h>
#include <netinet/tcp.h>
//Constants
#define SERVERIP "192.168.1.8"
#define SERVERPORT 8080
#define BUFFSIZE 1024 // defining buffer size for sending messages
#define ALIASLEN 32 //defining aliaslen for storing the length of the connected clients name
#define OPTLEN 16 //used for defining service length like send -4
#define LINEBUFF 2048 //used for definign message length
#define DELAY	5 /*seconds*/

int serverfd, got_reply=1;

// protocol defined packet structure
struct PACKET {
    char option[OPTLEN];
    char alias[ALIASLEN]; // setting client's alias
    char buff[BUFFSIZE];  //buffer size for message
};
 
 // structure to store client information
struct USER {
        int sockfd; //  initialising  user's socket descriptor to create a socket
        char alias[ALIASLEN]; //  initalising the string alias to store the user's name
};
 
struct THREADINFO {
    pthread_t thread_ID; // thread's pointer
    int sockfd;
};

int isconnected, sockfd;
char option[LINEBUFF];
struct USER me;
//declarationn of all the methods used for establishing connection,chatroom,and private chat connections.
int connect_with_server();
void setalias(struct USER *me);
void logout(struct USER *me);
void login(struct USER *me);
void *receiver(void *param);
void sendtoall(struct USER *me, char *msg);
void sendtoalias(struct USER *me, char * target, char *msg);
void sig_handler(int signum);
 /*********************MAIN METHOD*******************/
int main(int argc, char **argv) {
    int sockfd, aliaslen;
    
    memset(&me, 0, sizeof(struct USER));
  // if the client tyes exit, the coonection is closed
    while(gets(option)) {
        if(!strncmp(option, "exit", 4)) {
            logout(&me);
            break;
        }
        if(!strncmp(option, "help", 4)) {
            FILE *fin = fopen("help.txt", "r");//help.text file is used to give information and services provided by the   chatserver for clients who newly joined.
            if(fin != NULL) {
                while(fgets(option, LINEBUFF-1, fin)) puts(option);
                fclose(fin);
            }
            else {
                fprintf(stderr, "Help file not found...\n");
            }  
        }// login command is used by the client to establish connection with the server

 
         // compare if client types login [alias]
        else if(!strncmp(option, "login", 5)) {
            char *ptr = strtok(option, " ");
            ptr = strtok(0, " ");
            memset(me.alias, 0, sizeof(char) * ALIASLEN);
            //copy the alias name given by client
			if(ptr != NULL) {
                aliaslen =  strlen(ptr);
                if(aliaslen > ALIASLEN) ptr[ALIASLEN] = 0;
                strcpy(me.alias, ptr);
            }
            else {// while login if the client does not gives any name he will be logged in as anonymous
                strcpy(me.alias, "Anonymous");//
            }
            login(&me);
        }// else he will be logged into the server with the given name
        else if(!strncmp(option, "alias", 5)) {
            char *ptr = strtok(option, " ");
            ptr = strtok(0, " ");
            memset(me.alias, 0, sizeof(char) * ALIASLEN);
            if(ptr != NULL) {
                aliaslen =  strlen(ptr);
                if(aliaslen > ALIASLEN) ptr[ALIASLEN] = 0;
                strcpy(me.alias, ptr);
                setalias(&me);
            }
        }// if client wants to establish a private chat whisp commnad must be given to start private chat
        else if(!strncmp(option, "whisp", 5)) {
            char *ptr = strtok(option, " ");
            char temp[ALIASLEN];
            ptr = strtok(0, " ");
            memset(temp, 0, sizeof(char) * ALIASLEN);
            if(ptr != NULL) {
                aliaslen =  strlen(ptr);
                if(aliaslen > ALIASLEN) ptr[ALIASLEN] = 0;
                strcpy(temp, ptr);
                while(*ptr) ptr++; ptr++;
                while(*ptr <= ' ') ptr++;
                sendtoalias(&me, temp, ptr);
            }
        }// for chat room service
        else if(!strncmp(option, "send", 4)) {
            sendtoall(&me, &option[5]);
        }// logout command for disconnecting from the server
        else if(!strncmp(option, "logout", 6)) {
            logout(&me);
        }// any option other than specified will give an error
        else fprintf(stderr, "Unknown option...\n");
    }
    return 0;
}
 // method used to help user to login with the specified name
void login(struct USER *me) {
    int recvd;
    if(isconnected) {
        fprintf(stderr, "You are already connected to server at %s:%d\n", SERVERIP, SERVERPORT);
        return;
    }
    sockfd = connect_with_server();
    if(sockfd >= 0) {
        isconnected = 1;
        me->sockfd = sockfd;
        if(strcmp(me->alias, "Anonymous")) setalias(me);
        printf("Logged in as %s\n", me->alias);
        printf("Receiver started [%d]...\n", sockfd);
        struct THREADINFO threadinfo;
        pthread_create(&threadinfo.thread_ID, NULL, receiver, (void *)&threadinfo);
 
    }
    else {
        fprintf(stderr, "Connection rejected...\n");
    }
}
 // method used to connect with server
int connect_with_server() {
    int newfd, err_ret;
    struct sockaddr_in serv_addr;
    struct hostent *to;
 
    // generate address
    if((to = gethostbyname(SERVERIP))==NULL) {
        err_ret = errno;
        fprintf(stderr, "gethostbyname() error...\n");
        return err_ret;
    }
 
    //  open a socket and establish the coonection
    if((newfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        err_ret = errno;
        fprintf(stderr, "socket() error...\n");
        return err_ret;
    }
 
    // set initial values
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVERPORT);
    serv_addr.sin_addr = *((struct in_addr *)to->h_addr);
    memset(&(serv_addr.sin_zero), 0, 8);
 
    // try to connect with server
    if(connect(newfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) == -1) {
        err_ret = errno;
        fprintf(stderr, "connect() error...\n");
        return err_ret;
    }
    else {
        printf("Connected to server at %s:%d\n", SERVERIP, SERVERPORT);
        return newfd;
    }
}
// method used to help user to logout from chat
void logout(struct USER *me) {
    int sent;
    struct PACKET packet;
    
    if(!isconnected) {
        fprintf(stderr, "You are not connected...\n");
        return;
    }
    
    memset(&packet, 0, sizeof(struct PACKET));
    strcpy(packet.option, "exit");
    strcpy(packet.alias, me->alias);
    
    /* send request to close this connetion */
    sent = send(sockfd, (void *)&packet, sizeof(struct PACKET), 0);
    isconnected = 0;
}
 //method used for client to change login name
void setalias(struct USER *me) {
    int sent;
    struct PACKET packet;
    
    if(!isconnected) {
        fprintf(stderr, "You are not connected...\n");
        return;
    }
    
    memset(&packet, 0, sizeof(struct PACKET));
    strcpy(packet.option, "alias");
    strcpy(packet.alias, me->alias);
    
    //send request to close this connetion
    sent = send(sockfd, (void *)&packet, sizeof(struct PACKET), 0);
}
 
 
 //methos used by client to read messages from server
void *receiver(void *param) {
    int recvd;
    struct PACKET packet;
    
    printf("Waiting here [%d]...\n", sockfd);
    while(isconnected) {
        
        recvd = recv(sockfd, (void *)&packet, sizeof(struct PACKET), 0);
        if(!recvd) {
            fprintf(stderr, "Connection lost from server...\n");
            isconnected = 0;
            close(sockfd);
            break;
        }
        if(recvd > 0) {
            printf("[%s]: %s\n", packet.alias, packet.buff);
        }
        memset(&packet, 0, sizeof(struct PACKET));
    }
    return NULL;
}
// method to send message to all the clients connected to the server(group chat)
void sendtoall(struct USER *me, char *msg) {
    int sent;
    struct PACKET packet;
    
    if(!isconnected) {
        fprintf(stderr, "You are not connected...\n");
        return;
    }
    
    msg[BUFFSIZE] = 0;
    
    memset(&packet, 0, sizeof(struct PACKET));
    strcpy(packet.option, "send");
    strcpy(packet.alias, me->alias);
    strcpy(packet.buff, msg);
    
    //send request to close this connetion 
    sent = send(sockfd, (void *)&packet, sizeof(struct PACKET), 0);
}




 // method to send message to the specific client with the help of login name
void sendtoalias(struct USER *me, char *target, char *msg) {
    int sent, targetlen;
    struct PACKET packet;
    
    if(target == NULL) {
        return;
    }
    
    if(msg == NULL) {
        return;
    }
    
    if(!isconnected) {
        fprintf(stderr, "You are not connected...\n");
        return;
    }
    msg[BUFFSIZE] = 0;
    targetlen = strlen(target);
    
    memset(&packet, 0, sizeof(struct PACKET));
    strcpy(packet.option, "whisp");
    strcpy(packet.alias, me->alias);
    strcpy(packet.buff, target);
    strcpy(&packet.buff[targetlen], " ");
    strcpy(&packet.buff[targetlen+1], msg);
    
    /* send request to close this connetion */
    sent = send(sockfd, (void *)&packet, sizeof(struct PACKET), 0);
}

