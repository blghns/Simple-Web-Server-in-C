/*===========================================================================
 * File Name: server.c
 * Instructor: Nguyen Thai
 * Student: Bilgehan Saglik
 * Date: 8/1/2016
 * Description: Simple Web Server
 *
 *
 *==========================================================================*/
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <netinet/in.h>

#define ERROR_FILE    0
#define REG_FILE      1
#define DIRECTORY     2

int typeOfFile(char *fullPathToFile);

void sendDataBin(char *fileToSend, int sock, char *webDir);

void extractFileRequest(char *req, char *buff);


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +
 +  Function: main()
 +  Description: Program entry point.
 + 
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
int main(int argc, char **argv) {
    pid_t pid;            /* pid of child */
    int sockFD;           /* our initial socket */

    int newSockFD;
    int port;             /* Port number, used by 'bind' */
    char webDir[128];    /* Your environment that contains your web webDir*/

    socklen_t clientAddressLength;


    /*
     * structs used for bind, accept..
     */
    struct sockaddr_in serverAddress, clientAddress;

    char fileRequest[256];    /* where we store the requested file name */

    /*

     * Check for correct program inputs.
     */
    if (argc != 3) {
        fprintf(stderr, "USAGE: %s <port number> <website directory>\n", argv[0]);
        exit(-1);
    }

    port = atoi(argv[1]);       /* Get the port number */
    strcpy(webDir, argv[2]);    /* Get user specified web content directory */


    if ((pid = fork()) < 0) {
        perror("Cannot fork (for deamon)");
        exit(0);
    }
    else if (pid != 0) {
        /*
         * I am the parent
         */
        char t[128];
        sprintf(t, "echo %d > %s.pid\n", pid, argv[0]);
        system(t);
        exit(0);
    }

    /*
     * Create our socket, bind it, listen.
     * This socket will be used for communication
     * between the client and this server
     */

    //Create
    if((sockFD = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("something went wrong at creating our socket.");
        exit(0);
    }
    //Bind
    serverAddress.sin_family = AF_INET; /* Internet domain */
    serverAddress.sin_addr.s_addr = INADDR_ANY; /* local machine IP address */
    serverAddress.sin_port = htons(port);
    if((bind(sockFD, (struct sockaddr *) &serverAddress, sizeof(serverAddress))) == -1){
        perror("something went wrong at binding socket.");
        exit(0);
    }
    //Listen
    int backlog = 5;
    if((listen(sockFD,backlog)) == -1){
        perror("something went wrong at listening.");
        exit(0);
    }

    signal(SIGCHLD, SIG_IGN);  /* Prevent zombie processes */

    /*

     * Get the size of the clientAddress structure, could pass NULL if we
     * don't care about who the client is.
     */
    clientAddressLength = sizeof(clientAddress);

    /*
     * - accept a new connection and fork.
     * - If you are the child process,  process the request and exit.
     * - If you are the parent close the socket and come back to
     *   accept another connection
     */

    while (1) {

        /*
         * Accept a connection from a client (a web browser)
         * accept the new connection. newSockFD will be used for the
         * child to communicate to the client (browser)
         */

        if((newSockFD = accept(sockFD, (struct sockaddr *) &clientAddress, &clientAddressLength)) == -1){
            perror("something went wrong at accepting a connection");
            exit(0);
        }

        /* Forking off a new process to handle a new connection */
        if ((pid = fork()) < 0) {
            perror("Failed to fork a child process");
            exit(0);
        }
        else if (pid == 0) {
            /*
             * I am the child
             */
            int bytes;
            char buff[1024];
            char ref[1024];

            close(sockFD);
            memset(buff, 0, 1024);
            memset(ref, 0, 1024);

            /*
             * Read client request into buff
             * 'use a while loop'
             */

            if((read(newSockFD, ref, 1024)) == -1){
                perror("something went wrong reading client request");
            }
            bytes = 0;
            while(ref[bytes] != '\0'){
                buff[bytes] = ref[bytes];
                bytes++;
            }
/*
  What you may get from the client:
			GET / HTTP/1.0
			Connection: Keep-Alive
			User-Agent: Mozilla/3.0 (X11; I; SunOS 5.5 sun4m)
			Host: spiff:6789
			Accept: image/gif, image/x-xbitmap, image/jpeg, image/pjpeg


  Write to client

			You should write to the client an HTTP response and then the
			requested file, if appropriate. A response may look like:

			HTTP/1.0 200 OK
			Content-length: 2032
			Content-type: text/html
			[single blank line necessary here]
			[document follows]
*/
            /* Extract user requested file name */
            printf("client request:\n %s\n", buff);
            extractFileRequest(fileRequest, buff);

            printf("File Requested: |%s|\n", fileRequest);
            fflush(stdout);

            /* Read file and send it to client */
            sendDataBin(fileRequest, newSockFD, webDir);
            shutdown(newSockFD, 1);
            close(newSockFD);
            exit(0);
        } // pid = 0
        else {
            /*
             * I am the Parent
             */
            close(newSockFD);    /* Parent handed off this connection to its child,
			                        doesn't care about this socket */
        }
    }
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +
 +  Function: typeOfFile()
 +  Description: Tells us if the request is a environment or a regular file
 + 
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
int typeOfFile(char *fullPathToFile) {
    struct stat buf;    /* to stat the file/dir */

    if (stat(fullPathToFile, &buf) != 0) {
        perror("Failed to identify specified file");
        fprintf(stderr, "[ERROR] stat() on file: |%s|\n", fullPathToFile);
        fflush(stderr);
        return (ERROR_FILE);
    }

    printf("File full path: %s, file length is %li bytes\n", fullPathToFile, buf.st_size);

    if (S_ISREG(buf.st_mode))
        return (REG_FILE);
    else if (S_ISDIR(buf.st_mode))
        return (DIRECTORY);

    return (ERROR_FILE);
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +
 + Function: extractFileRequest()
 + Description:  Extract the file request from the request lines the client
 +               sent to us.  Make sure you NULL terminate your result.
 + 
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void extractFileRequest(char *req, char *buff) {
    int i = 0;
    int j = 5;
    while(buff[j] != 32){
        req[i] = buff[j];
        i++;
        j++;
    }
    req[i] = '\0';
    return;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 +
 +  Function: sendDataBin()
 +  Description: Send the requested file.
 + 
 +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
void sendDataBin(char *fileToSend, int sock, char *webDir) {
    int fileHandle;
    char fullPathToFile[256];
    char Header[1024];
    ssize_t s;
    char buffer[1024];
    int file_size;
    char *fileType;
    char *extension;


    /*
     * Build the full path to the file
     */
    sprintf(fullPathToFile, "%s/%s", webDir, fileToSend);

    /*
     * - If the requested file is a environment (directory), append 'index.html'
     *   file to the end of the fullPathToFile
     *   (Use typeOfFile(fullPathToFile))
     * - If the requested file is a regular file, do nothing and proceed
     * - else your client requested something other than a environment
     *   or a reqular file
     */

    fileType = typeOfFile(fullPathToFile);

    if(fileType == DIRECTORY){
        sprintf(fullPathToFile, "%s%s", fullPathToFile, "index.html");
    }
    else if(fileType == REG_FILE){
        //doing nothing
    }
    else{
        //client requested something other than a environment
    }
    /*
     * 1. Send the header (use write())
     * 2. open the requested file (use open())
     * 3. now send the requested file (use write())
     * 4. close the file (use close())
     */
    
    int fileToSendLength = strlen(fileToSend);
    if(fileToSendLength > 3){
        extension = &fileToSend[strlen(fileToSend) - 3];
    }
    else
        extension = "html";


    //open requested file
    if( (fileHandle = open(fullPathToFile, O_RDONLY)) == -1) {
        fprintf(stderr, "File does not exist: %s\n", fullPathToFile);
        //close(fileHandle);
        //return;
        //exit(-1);
    }
    file_size = lseek(fileHandle, (off_t)0, SEEK_END);
    lseek(fileHandle, (off_t)0, SEEK_SET);

    //sprintf(Header, "HTTP/1.0 200 OK\n\n");
    if(fileType == DIRECTORY || fileType == REG_FILE){
        if(!strncmp(extension, "pdf", 3))
            sprintf(Header, "HTTP/1.1 200 OK "
                            "Content-length: %d "
                            "Content-type: application/pdf\n\n", file_size);
        else
            sprintf(Header, "HTTP/1.0 200 OK\r\n"
                            "Content-type: text/html\r\n"
                            "Content-length: %d\r\n\r\n", file_size);

    }
    else{
        sprintf(Header, "HTTP/1.1 400 Bad Request\r\n"
                        "Content-length: %d\r\n"
                        "Content-type: text/html\r\n\r\n", file_size);

    }


    //send header
    if( write(sock, Header, strlen(Header)) == -1){
        perror("Something went wrong writing header.");
        exit(-1);
    }


    int bufferToBeSend = 0;
    do{
        bufferToBeSend = bufferToBeSend + 1024;
        //read file
        if((s = read(fileHandle, buffer, 1024)) == -1){
            perror("Something went wrong reading to buffer.");
            exit(-1);
        }
        //send file
        if((write(sock, buffer, s)) == -1){
            perror("Something went wrong writing file.");
            exit(-1);
        }
    }while(bufferToBeSend<=file_size);

    close(fileHandle);

}


