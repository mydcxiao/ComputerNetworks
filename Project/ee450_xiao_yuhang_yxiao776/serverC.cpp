/**
** serverC.cpp
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
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>

#define IP_ADDRESS "localhost" // hardcoded host address

#define LOCAL_PORT "21906" // port number to be bound with UDP socket

#define MAXBUFLEN 10000 // buffer size

using namespace std;

/* read cred.txt and set up a database using map
** @Params 'map' -- hashmap with string keys and string values
*/
void getDB(unordered_map<string, string>& map) {
    string line;
    ifstream input;
    input.open("./cred.txt");
    if(!input.is_open()) {
        perror("Error open");
        exit(EXIT_FAILURE);
    }
    while(getline(input, line)) {
        if(line.back() == '\r') {
            line.pop_back(); // remove CR from one line if OS uses CRLF for a line
        }
        size_t index = line.find(',');
        string usr = line.substr(0, index);
        string psd = line.substr(index + 1);
        map[usr] = psd; // username as key, password as value
    }
    return;
}

int main(void) {
    unordered_map<string, string> map; // map storing <username, password>
    getDB(map);

    //set up UDP socket, from beej
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    char buf[MAXBUFLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    // get local address information, from beej
    if((rv = getaddrinfo(IP_ADDRESS, LOCAL_PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the result and bind the first, from beej
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("listener: socket");
            continue;
        }

        if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("listener: bind");
            continue;
        }
        break;
    }
    
    // from beej
    if(p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);
    
    printf("The ServerC is up and running using UDP on port %s.\n", LOCAL_PORT);

    addr_len = sizeof their_addr;
    
    // the UDP keeps waiting for receiving auth informations
    while(1) {
        // recvfrom(), from beej
        if((numbytes = recvfrom(sockfd, buf, MAXBUFLEN-1, 0,
            (struct sockaddr *)&their_addr, &addr_len)) == -1) {
            perror("recvfrom");
            exit(1);
        }
        
        // no info, return to listening
        if(numbytes == 0) {
            continue;
        }

        buf[numbytes] = '\0';
        printf("The ServerC received an authentication request from the Main Server.\n");

        // get username and password from UDP message
        string auth = string(buf);
        size_t index = auth.find(',');
        string usr = auth.substr(0, index);
        string psd = auth.substr(index + 1);

        // check if the username and password in database and form response message
        buf[0] = '0'; // success
        if(map.find(usr) == map.end()) {
            buf[0] = '1'; // no username 
        } else if(map[usr] != psd) {
            buf[0] = '2'; // not correct password
        }
        
        buf[1] = '\0';

        // sendto(), from beej
        if((numbytes = sendto(sockfd, buf, strlen(buf), 0, 
           (struct sockaddr *)&their_addr, addr_len)) == -1) {
            perror("server: sendto");
            exit(1);
        }

        printf("The serverC finished sending the response to the Main Server.\n");
    }
    close(sockfd);
    return 0;
}