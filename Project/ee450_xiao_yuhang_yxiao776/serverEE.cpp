/**
** serverEE.cpp
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
#include <sstream>
#include <array>
#include <vector>
#include <bits/stdc++.h>

#define IP_ADDRESS "localhost" // hardcoded host address

#define LOCAL_PORT "23906" // port number to be bound with UDP socket

#define MAXBUFLEN 10000 // buffer size

using namespace std;

/* read ee.txt and set up a database using map.
** @Params 'map' -- hashmap, course code as key, array of string as value storing 4 categories.
*/
void getDB(unordered_map<string, array<string,4>>& map) {
    string line, tok;
    ifstream input;
    input.open("./ee.txt");
    if(!input.is_open()) {
        perror("Error open");
        exit(EXIT_FAILURE);
    }
    while(getline(input, line)) {
        if(line.back() == '\r') {
            line.pop_back(); // remove CR from one line if OS uses CRLF for a line
        }
        array<string,4> cur;
        int i = -1;
        stringstream ss(line);
        string key;
        while(getline(ss, tok, ',')) {
            if(i == -1) {
                key = tok;
            } else{
                cur[i] = tok;
            }
            ++i;
        }
        map[key] = cur;
    }
    return;
}

int main(void) {
    unordered_map<string, array<string,4>> map;
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
    
    printf("The ServerEE is up and running using UDP on port %s.\n", LOCAL_PORT);

    addr_len = sizeof their_addr;
    
    // the UDP keeps waiting for receiving query informations
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

        string output;

        // single query, '0' -- single query, '1' -- multiquery
        if(buf[0] == '0') {
            string course(strtok(buf + 2, " "));
            string category(strtok(NULL, " ")); 
            transform(category.begin(), category.end(), category.begin(), ::tolower);
            cout <<"The ServerEE received a request from the Main Server about the " 
            << category << " of " << course << "." << endl;
            if(map.find(course) == map.end()) {
                output = "Didn’t find the course: " + course + ".\n";
                cout << output;
            } else {
                string info;
                if(category == "credit") info = map[course][0];
                else if(category == "professor") info = map[course][1];
                else if(category == "days") info = map[course][2];
                else info = map[course][3];
                output = "The " + category + " of " + course + " is " + info + ".\n";
                cout << "The course information has been found: " << output;
            }
        } 
        // multiquery
        else {
            cout <<"The ServerEE received a multiquery request from the Main Server."<<endl;
            vector<string> courses;
            char *p = strtok(buf + 2, " ");
            while(p != NULL) {
                courses.push_back(string(p));
                p = strtok(NULL, " ");
            }
            output = "";
            for(string course : courses) {
                if(map.find(course) == map.end()) {
                    output += "Didn’t find the course: " + course + "\n";
                } else {
                    output += course + ": " + map[course][0] + ", " + map[course][1] + ", " + map[course][2]+ ", " + map[course][3] + "\n";
                }
            }
            cout << "Multiquery result:" << endl;
            cout << output << endl;
        }
        
        const char * res = output.c_str(); // convert to c-string

        // sendto(), from beej
        if((numbytes = sendto(sockfd, res, strlen(res), 0, 
           (struct sockaddr *)&their_addr, addr_len)) == -1) {
            perror("server: sendto");
            exit(1);
        }
        
        printf("The ServerEE finished sending the response to the Main Server.\n");
    }
    close(sockfd);
    return 0;
}