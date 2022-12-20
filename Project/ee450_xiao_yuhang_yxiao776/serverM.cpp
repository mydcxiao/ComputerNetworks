/*
** serverM.cpp
** Author: Yuhang Xiao
** USC ID: 6913860906
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
#include <stdbool.h>
#include <ctype.h>
#include <iostream>
#include <string>
#include <unordered_set>
#include <bits/stdc++.h>

#define PORT_TCP "25906" // the port for TCP

#define PORT_UDP "24906" // the port for UDP

#define PORT_C "21906" // the port for serverC

#define PORT_CS "22906" // the port for serverCS

#define PORT_EE "23906"	// the port for serverEE

#define IP_ADDRESS "localhost" // hardcoded host address (127.0.0.1)

#define BACKLOG 10 // how many pending connections queue will hold

#define MAXBUFLEN 10000 // buffer size

using namespace std;

// child process handler, code from beej
void sigchld_handler(int s){
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;
    
    while(waitpid(-1, NULL, WNOHANG) > 0);
    
    errno = saved_errno;
}

// function to get port number, either IPv4 or IPv6
unsigned short get_port_num(struct sockaddr *sa){
    if(sa->sa_family == AF_INET6) {
        return ntohs(((struct sockaddr_in6*)sa)->sin6_port);
    }
    return ntohs(((struct sockaddr_in*)sa)->sin_port);
}

// funtion to encrypt authentication information, offset each digit/letter by 4 
void encrypt(char* s) {
    for(size_t i = 0; i < strlen(s); ++i) {
        if('a' <= s[i] && s[i] <= 'z') {
            s[i] = (s[i] - 'a' + 4) % 26 + 'a';
        } else if('A' <= s[i] && s[i] <= 'Z') {
            s[i] = (s[i] - 'A' + 4) % 26 + 'A';
        } else if('0' <= s[i] && s[i] <= '9') {
            s[i] = (s[i] - '0' + 4) % 10 + '0';
        }
    }
}

/*
** function to split multiquery messages, filter out the duplicate course codes and store them in 'toks[]'.
** (Keep the first occurrence of duplicate courses)
** @Params char *toks[] -- Array of pointers to store course codes
**         char *msg -- message received from client using TCP
**         const char* dlm -- delimiters to split messages
**         size_t* cs -- array of course indices that will be sent to serverCS
**         size_t* ee -- array of course indices that will be sent to serverEE
**         size_t* lc -- length of 'cs' array
**         size_t* le -- length of 'ee' array
** @Return size_t -- length of 'toks[]', which is the # of unique course codes.
*/
size_t split(char *toks[], char *msg, const char *dlm, size_t *cs, size_t *ee, size_t *lc, size_t *le) {
    char *p = strtok(msg, dlm);
    size_t i = 0;
    *lc = 0;
    *le = 0;
    unordered_set<string> s; //set to filter out duplicate course codes
    while (p != NULL) {
        string cur(p);
        if(s.find(cur) == s.end()) {
            toks[i] = (char *)malloc(strlen(p) + 1);
            strcpy(toks[i], p);
            if(*p == 'C' && *(p + 1) == 'S') {
                cs[*lc] = i;
                ++*lc;
            } else if(*p == 'E' && *(p + 1) == 'E') {
                ee[*le] = i;
                ++*le;
            }
            ++i;
            s.insert(cur);
        }
        p = strtok(NULL, " ");
    }
    return i;
}

/*
** function to fill the buffer with course codes that will be used to query backend server.
** @Params char *courses[] -- Array of Pointer storing the course codes
**         size_t* idx -- Array storing indices of course codes that will be used in query
**         size_t len -- Length of 'idx'
**         char* buf -- buffer to be filled
*/
void fillBuff(char *courses[], size_t *idx, size_t len, char *buf) {
    *buf = '\0';
    for(size_t i = 0; i < len; ++i) {
        strcat(buf, " ");
        strcat(buf, courses[idx[i]]);
    }
}

/*
** function to merge two query results from CS and EE department and keep the original multiquery order.
** @Params char *buf -- buffer to store the merged result
**         char *data_cs -- query results from CS department
**         char *data_ee -- query results from EE department
**         char *courses[] -- Array of pointers storing the course codes
**         size_t* cs -- array of course indices that were sent to serverCS
**         size_t* ee -- array of course indices that were sent to serverEE
**         size_t numqueries -- total # of queries in multiquery
*/
void merge(char *buf, char* data_cs, char* data_ee, char* courses[], size_t* cs, size_t* ee, size_t numqueries) {
    char *save1, *save2;
    char *p = strtok_r(data_cs, "\n", &save1);
    char *q = strtok_r(data_ee, "\n", &save2);
    buf[0] = '\0';
    size_t i = 0;
    size_t j = 0;
    size_t idx = 0;

    while(idx < numqueries) {
        if(p != NULL && cs[i] == idx) {
            strcat(buf, p);
            strcat(buf, "\n");
            p = strtok_r(NULL, "\n", &save1);
            ++i;
        } else if(q != NULL && ee[j] == idx) {
            strcat(buf, q);
            strcat(buf, "\n");
            q = strtok_r(NULL, "\n", &save2);
            ++j;
        } else {
            strcat(buf, "Didn’t find the course: ");
            strcat(buf, courses[idx]);
            strcat(buf, "\n");
        }
        ++idx;
    }
}

/* set up TCP parent socket, from beej
** @Params const char * port -- port to be bound
** @Return int -- socket file descriptor, otherwise negative number representing errors
*/
int getParentSock(const char * port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    struct sigaction sa;
    int yes = 1;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if((rv = getaddrinfo(IP_ADDRESS, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
            perror("setsockopt");
            exit(1);
        }
        if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }

    freeaddrinfo(servinfo);

    if(p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if(listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
    
    sa.sa_handler = sigchld_handler; // reap all dead processes 
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1); 
    }

    return sockfd;
}

/* set up UDP socket, from beej
** @Params const char * port -- port to be bound
** @Return int -- socket file descriptor, otherwise negative number representing errors
*/
int getUDPSock(const char * port) {
    int sockfd;
    int rv;
    struct addrinfo hints, *servinfo, *p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(IP_ADDRESS, port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("talker: socket");
			continue;
		}

        if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("talker: bind");
            continue;
        }

		break;
	}  

    if (p == NULL) {
		fprintf(stderr, "talker: failed to create socket\n");
		return -2;
	}  

    freeaddrinfo(servinfo);

    return sockfd;

}


/* use UDP socket to query backend servers, from beej
** @Params int sockfd -- socket file descriptor, which is used in UDP communications
**         char *req -- request message
**         const char *remote_port -- server port number
**         char *data -- string to store the query result
** @Return int -- indicator for if it runs successfully ('0'--success, '-1'--fail) 
*/
int query(int sockfd, char *req, const char* remote_port, char *data) {
    int numbytes;
    int rv;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
	socklen_t addr_len;
    unsigned short port_i;
    char recv_port[6];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    
    if ((rv = getaddrinfo(IP_ADDRESS, remote_port, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

    p = servinfo;

	if ((numbytes = sendto(sockfd, req, strlen(req), 0,
			 p->ai_addr, p->ai_addrlen)) == -1) {
		perror("talker: sendto");
		exit(1);
	}
 
    freeaddrinfo(servinfo);

	if (strcmp(remote_port, PORT_C)==0) {
		printf("The main server sent an authentication request to serverC.\n");
	} else if (strcmp(remote_port, PORT_CS)==0) {
		printf("The main server sent a request to serverCS.\n");
    } else if (strcmp(remote_port, PORT_EE)==0) {
        printf("The main server sent a request to serverEE.\n");
    }

    addr_len = sizeof their_addr;

    if((numbytes = recvfrom(sockfd, data, MAXBUFLEN - 1, 0, 
      (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }
    data[numbytes] = '\0';

    port_i = get_port_num((struct sockaddr *)&their_addr);
    sprintf(recv_port, "%d", port_i);

    if (strcmp(recv_port, PORT_C)==0) {
		printf("The main server received the result of the authentication request from ServerC using UDP over port %s.\n", PORT_UDP);
	} else if (strcmp(recv_port, PORT_CS)==0) {
		printf("The main server received the response from serverCS using UDP over port %s.\n", PORT_UDP);
    } else if (strcmp(recv_port, PORT_EE)==0) {
        printf("The main server received the response from serverEE using UDP over port %s.\n", PORT_UDP);
    }
    return 0;
}


int main(void) {
    int sockfd, new_fd, udp_fd; // parent socket, child socket, udp socket, from beej
	struct sockaddr_storage their_addr; // struct to store sender address info, from beej
	socklen_t sin_size;  // address length, from beej
	int numbytes; // # of bytes send/recv, from beej
    bool isValid = false; // flag indicating if authentication is successful
    char buf[MAXBUFLEN]; // buffer
    char data_cs[MAXBUFLEN]; // buffer to store multiquery data from CS department
    char data_ee[MAXBUFLEN]; // buffer to store multiquery data from EE department
    char *courses[10]; // store unique course codes in multiquery
    size_t numqueries; // # of course codes in multiquery
    size_t cs[10]; // array of indices for cs courses
    size_t len_cs; // length of cs array
    size_t ee[10]; // array of indices for ee courses
    size_t len_ee; // length of ee array
    char usr[51]; // store the username from client

    // create parent and udp socket
    sockfd = getParentSock(PORT_TCP);
	if(sockfd == -1){
        return 1;
    }

    udp_fd = getUDPSock(PORT_UDP);
    if(udp_fd == -1) {
        return 1;
    }
    if(udp_fd == -2) {
        return 2;
    }
	
	printf("The main server is up and running.\n");

    // TCP accept() and child process, from beej
    while(1) {
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if(new_fd == -1) {
            perror("accept");
            continue;
        }

        if(!fork()) {
            close(sockfd);
            // loop for authentication request
            while(!isValid) {
                // recv() from beej
                if((numbytes = recv(new_fd, buf, MAXBUFLEN-1, 0)) == -1) {
                    perror("recv");
                    exit(1);
                }
                
                // close socket if client is down
                if(numbytes == 0) {
                    // printf("client is down, child socket will close.\n");
                    close(udp_fd);
                    close(new_fd);
                    exit(0);
                }
                
                buf[numbytes] = '\0';

                size_t q;
                for(q = 0; q < numbytes; ++q) {
                    if(buf[q] == ',') break;
                }
                buf[q] = '\0';
                strcpy(usr, buf); // copy username out from buffer
                printf("The main server received the authentication for %s using TCP over port %s.\n", buf, PORT_TCP);
                buf[q] = ','; // restore original buffer

                encrypt(buf); // encrypt authentication info

                if(query(udp_fd, buf, PORT_C, buf) == -1) {
                    exit(1);
                }

                // if authentication succeed, change flag to true
                if(buf[0] == '0') {
                    isValid = true;
                }
                
                // send() from beej
                if ((numbytes = send(new_fd, buf, strlen(buf), 0)) == -1) {
                    perror("send");
                    exit(1);
                }

                printf("The main server sent the authentication result to the client.\n");
            }
            
            // loop for course queries
            while(isValid) {
                // recv() from beej
                if((numbytes = recv(new_fd, buf, MAXBUFLEN-1, 0)) == -1) {
                    perror("recv");
                    exit(1);
                }
                
                // close socket if client is down
                if(numbytes == 0) {
                    // printf("client is down, child socket will close.\n");
                    close(udp_fd);
                    close(new_fd);
                    exit(0);
                }
                
                buf[numbytes] = '\0';
                
                // single query if first byte of message is '0'
                if(buf[0] == '0') {
                    char *p = strtok(buf + 2, " ");
                    char *course = (char *) malloc(strlen(p) + 1);
                    strcpy(course, p); // copy course code out of buffer
                    string category(strtok(NULL, " ")); // get the category
                    transform(category.begin(), category.end(), category.begin(), ::tolower);
                    cout << "The main server received from " << usr << 
                    " to query course " << course << " about " << category << " using TCP over port " << PORT_TCP << ".\n";
                    *(buf + 2 + strlen(course)) = ' '; // restore original buffer
                    
                    // query different department according to code, if no match, directly not found
                    if(buf[2] == 'E' && buf[3] == 'E') {
                        if(query(udp_fd, buf, PORT_EE, buf) == -1) {
                            exit(1);
                        }
                    } else if(buf[2] == 'C' && buf[3] == 'S') {
                        if(query(udp_fd, buf, PORT_CS, buf) == -1) {
                            exit(1);
                        }
                    } else {
                        strcpy(buf, "Didn’t find the course: ");
                        strcat(buf, course);
                        strcat(buf, ".");
                    }

                } 
                // multiquery
                else if(buf[0] == '1') { 
                    printf("The main server received from %s to multiquery using TCP over port %s.\n", usr, PORT_TCP);
                    numqueries = split(courses, buf + 2, " ", cs, ee, &len_cs, &len_ee);
                    data_cs[0] = '\0';
                    data_ee[0] = '\0';
                    // if len <= 0, no need to do query
                    if(len_cs > 0) {
                        fillBuff(courses, cs, len_cs, buf + 1);
                        if(query(udp_fd, buf, PORT_CS, data_cs) == -1) {
                            exit(1);
                        }
                    }
                    if(len_ee > 0) {
                        fillBuff(courses, ee, len_ee, buf + 1);
                        if(query(udp_fd, buf, PORT_EE, data_ee) == -1) {
                            exit(1);
                        }
                    }

                    merge(buf, data_cs, data_ee, courses, cs, ee, numqueries);
                }

                // send() from beej
                if ((numbytes = send(new_fd, buf, strlen(buf), 0)) == -1) {
                    perror("send");
                    exit(1);
                }
                
                printf("The main server sent the query information to the client.\n");
            }
            close(udp_fd);
            close(new_fd);
            exit(0);
        }
        close(new_fd);
    }
    close(sockfd);
    close(udp_fd);
    return 0;
}