#include <iostream>
#include <string.h>
#include <fstream>
#include <pthread.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../Vector/Vector.h"

#define SOCKET_BUFFERSIZE 1024

using namespace std;

Vector<char *> restOfQueries;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condVar = PTHREAD_COND_INITIALIZER, condWait = PTHREAD_COND_INITIALIZER;
pthread_t *threadPool;
int threadsRunning = 0, threadsWaiting = 0, servPort = -1, numThreads = -1;
bool ready = false;
string servIP;

//================================================================================
//================================== PARSE ARGS =======================================
static bool tryParseArgInt(int &arg, char *str, const char *name){
    if(arg != -1){
        cout << name << " passed more than once" << endl;
        return false;
    }
    try{
        arg = stoi(str);
        if(arg < 0){
            arg = -2;
            cout << "Invalid value after " << name << endl;
        }
    }
    catch(invalid_argument& e){
        cout << "Expected int after " << name << endl;
        return false;
    }
    catch(out_of_range& e){
        cout << "Too large number after " << name << endl;
        return false;
    }
    return true;
}

static bool tryParseArgString(string &file, char *str, const char *name){
    if (!file.empty()){
        cout << name << " passed more than once" << endl;
        return false;
    }
    file = str;
    return true;
}

static bool validIPpart(char *str){
    unsigned int len = strlen(str);
    if (len > 3)
        return false;
    for (unsigned int i=0; i<len; i++){
        if (str[i]<'0' || str[i]>'9')
            return false;
    }
    string temp = str;
    if (len>1 && temp.find('0')==0)
        return false;
    int num = stoi(temp);
    if (num>=0 && num<=255)
        return true;
    else
        return false;
}

static bool parseArgIP(string &servIP, char *str){
    if (!servIP.empty()){
        cout << "-sip passed more than once" << endl;
        return false;
    }
    servIP = str;
    unsigned int len = strlen(str);
    int dots = 0;
    for(unsigned int i=0; i<len; i++){
        if (str[i] == '.')
            dots++;
    }
    if (dots != 3)
        return false;
    char *tmp = strtok(str, ".");
    if (tmp == NULL)    
        return false;
    dots = 0;
    while (tmp){
        if (validIPpart(tmp)){
            tmp = strtok(NULL, ".");
            if (tmp)
                dots++;
        }
        else
            return false;
    }
    if (dots != 3)
        return false;
    return true;
}

static bool parseArgs(int argc, char **argv, string &queryFile, int &servPort, string &servIP){
    bool validArgs = true;
    int i;
    for(i=1; i<argc-1; i++){

        if(!strcmp(argv[i], "-q")){
            if(tryParseArgString(queryFile, argv[++i], "-q") == false){
                validArgs = false;
                i--;
            }
        }
        else if(!strcmp(argv[i], "-w")){
            if(tryParseArgInt(numThreads, argv[++i], "-w") == false){
                validArgs = false;
                i--;
            }
        }
        else if(!strcmp(argv[i], "-sp")){
            if(tryParseArgInt(servPort, argv[++i], "-sp") == false){
                validArgs = false;
                i--;
            }
        }
        else if(!strcmp(argv[i], "-sip")){
            if(parseArgIP(servIP, argv[++i]) == false){
                validArgs = false;
                i--;
            }
        }
        else{
            cout << "Unexpected parameter " << argv[i] << endl;
            validArgs = false;
        }
    }
    if (i != argc){
        cout << "Unexpected parameter " << argv[i] << endl;
        validArgs = false;
    }
    if(!validArgs)
        return validArgs;

    if (queryFile.empty()){
        cout << "Missing queryFile argumnent" << endl;
        validArgs = false;
    }
    if (numThreads == -1){
        cout << "Missing numThreads argumnent" << endl;
        validArgs = false;
    }
    if (servPort == -1){
        cout << "Missing servPort argumnent" << endl;
        validArgs = false;
    }
    if (servIP.empty()){
        cout << "Missing servIP argumnent" << endl;
        validArgs = false;
    }
    return validArgs;
}

//================================================================================
//================================== COMMUNICATION =======================================
static int getHeader(int source){
    int header = -1;
    if (SOCKET_BUFFERSIZE >= (int)sizeof(header) ){
        read(source, &header, sizeof(header));
        return header;
    }
    char buffer[sizeof(header)];
    int bufferIndex = 0;
    while(bufferIndex + SOCKET_BUFFERSIZE <= (int)sizeof(header)){
        read(source, &buffer[bufferIndex], SOCKET_BUFFERSIZE); // full bufferSize chunk
        bufferIndex += SOCKET_BUFFERSIZE;
    }
    if(bufferIndex != (int)sizeof(header)){
        read(source, &buffer[bufferIndex], sizeof(header) - bufferIndex); // less than bufferSize chunk
    }
    header = *(int*)(buffer);
    return header;
}

static string getMessage(int source){
    string msg;
    int messageSize = getHeader(source);
    if (messageSize == -1)
        return "";
    char *buffer = (char*)malloc(sizeof(char) * messageSize);
    if(SOCKET_BUFFERSIZE >= messageSize){
        read(source, buffer, messageSize);
        msg = buffer;
        free(buffer);
        return msg;
    }
    int bufferIndex = 0;
    while(bufferIndex + SOCKET_BUFFERSIZE <= messageSize){
        read(source, &buffer[bufferIndex], SOCKET_BUFFERSIZE); // full bufferSize chunk
        bufferIndex += SOCKET_BUFFERSIZE;
    }
    if(bufferIndex != messageSize){
        read(source, &buffer[bufferIndex], messageSize - bufferIndex); // less than bufferSize chunk
    }
    msg = buffer;
    free(buffer);
    return msg;
}

static void writeHeader(int header, int destination){
    if (SOCKET_BUFFERSIZE >= (int)sizeof(header) ){
        if (write(destination, &header, sizeof(header)) == -1){
            perror("ERROR : write");
            exit(1);
        }
        return;
    }
    char *buffer = (char *)&header;
    int bufferIndex = 0;
    while(bufferIndex + SOCKET_BUFFERSIZE <= (int)sizeof(header)){
        if (write(destination, &buffer[bufferIndex], SOCKET_BUFFERSIZE) == -1){ // full bufferSize chunk
            perror("ERROR : write");
            exit(1);
        }
        bufferIndex += SOCKET_BUFFERSIZE;
    }
    if(bufferIndex != (int)sizeof(header)){
        if (write(destination, &buffer[bufferIndex], sizeof(header) - bufferIndex) == -1){ // less than bufferSize chunk
            perror("ERROR : write");
            exit(1);
        }
    }
}

static void writeMessage(const char *buffer, int destination){
    int len = strlen(buffer) + 1;
    writeHeader(len, destination);
    if(SOCKET_BUFFERSIZE >= len){
        if (write(destination, buffer, len) == -1){
            perror("ERROR : write");
            exit(1);
        }
        return;
    }
    int bufferIndex = 0;
    while(bufferIndex + SOCKET_BUFFERSIZE <= len){
        if (write(destination, &buffer[bufferIndex], SOCKET_BUFFERSIZE) == -1){ // full bufferSize chunk
            perror("ERROR : write");
            exit(1);
        }
        bufferIndex += SOCKET_BUFFERSIZE;
    }
    if(bufferIndex != len){
        if (write(destination, &buffer[bufferIndex], len - bufferIndex) == -1){ // less than bufferSize chunk
            perror("ERROR : write");
            exit(1);
        }
    }
}

static void connectToServer(string &serverIP, int serverPort, char *query){
    int sock;
    
    struct sockaddr_in server;
    struct sockaddr *serverPtr = (struct sockaddr *)&server;

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0){
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    server.sin_family = AF_INET;
    server.sin_port = htons(serverPort);
    struct hostent * rem ;
    rem = gethostbyname(&serverIP[0]);
    
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);

    if (connect(sock, serverPtr, sizeof(server)) < 0){
        perror("Connect failed");
        exit(EXIT_FAILURE);
    }

    writeMessage(query, sock);
    string answer = getMessage(sock);
    pthread_mutex_lock(&mtx);
    cout << "Client asked query: " << query << endl;
    cout << "Client got answer: " << answer << endl; 
    pthread_mutex_unlock(&mtx);
}

void *threadFunction(void *input){
    char *query = (char*)input;

    pthread_mutex_lock(&mtx);
    if (!ready){    
        threadsWaiting++;                       //so that main knows when to broadcast
        do {
            pthread_cond_signal(&condWait);     //so that main knows when a thread is waiting
            pthread_cond_wait(&condVar, &mtx);
        }while(!ready);                         //wait for ready flag to be true
        threadsWaiting--;
    }
    pthread_mutex_unlock(&mtx);

    //Connect to server - send query - get answer
    connectToServer(servIP, servPort, query);

    free(query);
    return NULL;
}

static bool readFile(string queryFile, int numThreads){
    char *fileName = &(queryFile)[0];
    ifstream myFile;
    myFile.open(fileName);
    if (!myFile.is_open()){
        cout << "Client: Failed to open queryFile" << endl;
        return false;
    }
    int lines = 0;
    string line;
    char *query;


    while(getline(myFile, line)){
        lines++;
        query = strdup(line.c_str());
        if (lines <= numThreads){
            pthread_create(&threadPool[lines-1], NULL, threadFunction, query);
            threadsRunning++;
        }
        else{
            restOfQueries.pushBack(query);      //save quries in Vector for later
        }
    }

    myFile.close();
    return true;
}

//Function that waits until all threads are waiting and then broadcasts
void threadHandle(int threadsCount){
    pthread_mutex_lock(&mtx);
    while(threadsWaiting < threadsRunning){ // wait using condWait until all running threads are waiting 
        pthread_cond_wait(&condWait,&mtx);
    }
    if (threadsWaiting != 0) {
        ready = true;
        pthread_cond_broadcast(&condVar);   //wake up all threads
    }
    pthread_mutex_unlock(&mtx);

    for(int i=0; i<threadsCount; i++){
        pthread_join(threadPool[i], NULL);
    }
    delete[] threadPool;
}

//./whoClient –q queryFile -w numThreads –sp servPort –sip servIP
int main(int argc, char **argv){
    string queryFile;
    if(!parseArgs(argc, argv, queryFile, servPort, servIP))
        return -1;

    threadPool = new pthread_t[numThreads];

    if(!readFile(queryFile, numThreads))
        return -1;
    
    threadHandle(numThreads);   //wait for all threads and then wake them up at the same time

    int length = restOfQueries.getSize();
    if (length != 0){                           //if there are leftover quries
        threadPool = new pthread_t[length];     //create as many threads as the leftover queries
        threadsRunning = 0;
        threadsWaiting = 0;
        ready = false;
        char *query;
        for(int i=0; i<length; i++){
            query = strdup(restOfQueries.getAt(i));     //get query
            pthread_create(&threadPool[i], NULL, threadFunction, query);
            threadsRunning++;
        }
        threadHandle(length);
    }

    for(int i=0; i<length; i++){
        free(restOfQueries.getAt(i));
    }

    cout << "Clients finished" << endl;
  
    return 0;
}
