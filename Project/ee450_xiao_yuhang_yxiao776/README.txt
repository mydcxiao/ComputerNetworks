***************************************
*      Full Name: Yuhang Xiao         *
*      Student ID: 6913860906         *
*      Net ID: YXiao776               *
***************************************

---------------------------------------------------------------------------
Have Done:

Have done all the phases of the project, including the Extra Credit.

---------------------------------------------------------------------------
Code Files:

* client.c: TCP client of the Main server, which could input the query
            and output the results.
* serverM.cpp: Main server connected to client with TCP and communicated
               with serverC, serverCS, serverEE via UDP. It processed the 
               request and queried the corresponding backend server.
* serverC.cpp: Credential server that loaded 'cred.txt' file's information 
               into its database and serve with lookup functionality.
* serverCS.cpp: CS Department server that loaded 'cs.txt' file's 
                information into its database and serve with lookup 
                functionality.
* serverEE.cpp: EE Department server that loaded 'ee.txt' file's 
                information into its database and serve with lookup 
                functionality.
                
---------------------------------------------------------------------------
Format of Message Exchanged:

* Phase 1
message format: 
"<username>,<password>"

It's a string of username and password separated by a comma.
The message is sent from client to serverM, then forwarded to serverC.

* Phase 2
message format: 
'0' or '1' or '2'

It's a single character. '0' means the authentication is successful. '1' means the username is not found. '2' means the password is not match.
The message is sent from serverC to serverM, the forwarded to client.

* Phase 3
message format: 
"0 <course code> <category>" or 
"1 <course code> <course code> ... <course code>"(for extra credit)

It's a string with tokens separated by a single space. The first character means the kind of query, where '0' means it's a single query (query with only one course code), '1' means it's a multiquery (query with multiple course codes).
The message is sent from client to serverM, then serverM will process it, exclude duplicate courses (for extra credit) and the courses that cannot be found on backend servers (not start with "EE" and "CS"), and send processed messages with the same format to serverEE and serverCS (no this transmission if all courses don't start with "EE" and "CS").

* Phase 4
message format:
"Didn’t find the course: <course code>.\n"
"The <category> of <course code> is <information>.\n"
"<course code>: <credits>, <professor>, <days>, <course name>\n<course code>: <credits>, <professor>, <days>, <course name>\nDidn’t find the course: <course code>\n...<course code>: <credits>, <professor>, <days>, <course name>\n" (for extra credit)

For single query, it's a string of the query result. For multiquery, it a string of multiple query results separated by '\n' character.
The message is sent from serverCS and serverEE to serverM (no this transmission if all courses don't start with "EE" and "CS") , then serverM will add courses' query results to the message, which cannot be found in backend servers (not start with "EE" and "CS", the query result is formed by serverM directly, which is "Didn't find the course: <course code>"), finally the message will be sent to client.

---------------------------------------------------------------------------
Idiosyncracies:

The project was found to fail to start when there are zombie processes. 
Please kill all zombie processes before starting the project in terminals.

---------------------------------------------------------------------------
Reused code:

* Codes to create and set up TCP and UDP socket are from "Beej's Guide to 
Network Programming -- Using Internet Sockets". (mainly from 'client.c', 
'server.c', 'listener.c' and 'talker.c')
* Codes to get locally-bound socket information are from the Project 
Description.

All of above are identified with comments in the program.

