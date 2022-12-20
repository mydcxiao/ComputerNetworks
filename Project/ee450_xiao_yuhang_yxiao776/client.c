/*
* client.c
* Author: Yuhang Xiao
* USC ID: 6913860906
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>
#include <stdbool.h>

#define PORT "25906"    // the TCP port of serverM which client connects to

#define IP_ADDRESS "localhost" // hardcoded host address (127.0.0.1)

#define MAXBUFLEN 10000   // the maximum buffer size of bytes

// function to get port number, either IPv4 or IPv6
unsigned short get_port_num(struct sockaddr *sa){
    if(sa->sa_family == AF_INET6) {
        return ntohs(((struct sockaddr_in6*)sa)->sin6_port);
    }
    return ntohs(((struct sockaddr_in*)sa)->sin_port);
}

// function to split original string and store tokens, return # of tokens
size_t split(char *toks[], char *msg, char *delimiters) {
    char *p = strtok (msg, delimiters);
    size_t i = 0;
    while (p != NULL) {
        toks[i] = malloc(strlen(p) + 1);
        strcpy(toks[i], p);
        ++i;
        p = strtok (NULL, " ");
    }
    return i;
}

int main(int argc, char *argv[]) {
    int attempt = 0; // # of client authentication attempt
    int sockfd, numbytes; // socket descriptor and count # of bytes, from beej
    char buf[MAXBUFLEN]; // buffer, from beej
    struct addrinfo hints, *servinfo, *p; // struct from beej
    int rv; // indicator of getaddrinfo(), from beej
    char usr[51], psd[51], data[102]; // username, password, combined message
    struct sockaddr_storage myaddr; // struct from beej
    socklen_t addrlen; // type from beej
    unsigned short local_port; // local port number
    bool isValid = false; // flag of valid username and password
    size_t numqueries; // # of input course queries
    char *courses[10]; // course codes for multiquery

    // client stream TCP socket, from beej
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(IP_ADDRESS, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("client: connect");
            close(sockfd);
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }
    
    addrlen = sizeof myaddr;

    //get locally-bound socket, from project description
    if(getsockname(sockfd, (struct sockaddr *)&myaddr, (socklen_t *)&addrlen) == -1) {
        perror("getsockname");
        exit(1);
    }

    local_port = get_port_num((struct sockaddr *)&myaddr);

    printf("The client is up and running.\n");

    freeaddrinfo(servinfo); // all done with this structure

    // loop for authentication phase
    while(attempt < 3 && !isValid) {
        printf("Please enter the username: ");
        scanf("%s", usr);
        printf("Please enter the password: ");
        scanf("%s", psd);
        // merge authentication info into a single string
        sprintf(data, "%s,%s", usr, psd);

        // send() through TCP, from beej
        if ((numbytes = send(sockfd, data, strlen(data), 0)) == -1) {
            perror("send");
            exit(1);
        }

        printf("%s sent an authentication request to the main server.\n", usr);
        
        // recv() from TCP, from beej
        if((numbytes = recv(sockfd, buf, MAXBUFLEN-1, 0)) == -1) {
            perror("recv");
            exit(1);
        }

        // connection is down, close client
        if(numbytes == 0) {
            close(sockfd);
            return 0;
        }

        buf[numbytes] = '\0';
        
        // check authentication flag, '0' for success, '1' for username error, '2' for password error
        if(buf[0] == '0') {
            isValid = true;
            printf("%s received the result of authentication using TCP over port %d. Authentication is successful\n", usr, local_port);
        } else if(buf[0] == '1') {
            ++attempt;
            printf("%s received the result of authentication using TCP over port %d. Authentication failed: Username Does not exist\n", usr, local_port);
            printf("\n");
            printf("Attempts remaining:%d\n", 3 - attempt);
        } else if(buf[0] == '2') {
            ++attempt;
            printf("%s received the result of authentication using TCP over port %d. Authentication failed: Password does not match\n", usr, local_port);
            printf("\n");
            printf("Attempts remaining:%d\n", 3 - attempt);
        }
    }

    // authentication fails after 3 attempts, close client
    if(!isValid) {
        close(sockfd);
        printf("Authentication Failed for 3 attempts. Client will shut down.\n");
        return 0;
    }

    // loop for course query
    while(1) {
        printf("Please enter the course code to query: ");
        scanf(" %[^\n]", buf); // read a line
        
        if((numqueries = split(courses, buf, " ")) == 1) {
            // loop until enter the right category
            while(1) {
                printf("Please enter the category (Credit / Professor / Days / CourseName): ");
                scanf("%s", data);
                if(strcmp(data, "Credit") == 0 || strcmp(data, "Professor") == 0 
                || strcmp(data, "Days") == 0 || strcmp(data, "CourseName") == 0) {
                    break;
                }
                printf("Invalid category!\n");
            }

            buf[0] = '0'; // first byte in msg is a flag, '0' for single query, '1' for multiquery 
            buf[1] = ' '; // space delimiter
            buf[2] = '\0';
            strcat(buf, courses[0]);
            strcat(buf, " ");
            strcat(buf, data);
        } else if(1 < numqueries && numqueries < 10) {
            buf[0] = '1'; // multiquery flag
            buf[1] = '\0';
            for(size_t i = 0; i < numqueries; ++i) {
                strcat(buf, " ");
                strcat(buf, courses[i]);
            }

        } else {
            printf("Invalid: more than 10 queries\n"); // enter more than or equal to 10 queries
            continue; 
        }
        
        // send() through TCP, from beej
        if ((numbytes = send(sockfd, buf, strlen(buf), 0)) == -1) {
            perror("send");
            exit(1);
        }
        
        if(buf[0] == '0') {
            printf("%s sent a request to the main server.\n", usr);
        } else {
            printf("%s sent a request with multiple CourseCode to the main server.\n", usr);
        }
        
        // recv() from TCP, from beej
        if((numbytes = recv(sockfd, buf, MAXBUFLEN-1, 0)) == -1) {
            perror("recv");
            exit(1);
        }

        // connection is down, client will close
        if(numbytes == 0) {
            close(sockfd);
            printf("serverM is down, client will close.\n");
            return 0;
        }

        buf[numbytes] = '\0';

        printf("The client received the response from the Main server using TCP over port %d.\n", local_port);

        // print query results
        if(numqueries == 1) {
            printf("%s\n", buf);
        } else {
            printf("CourseCode: Credits, Professor, Days, Course Name\n");
            printf("%s\n", buf);
        }

        printf("\n");
        printf("-----Start a new request-----\n");
    }
    close(sockfd);
    return 0;
}
