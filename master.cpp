#include <iostream>
#include <cstring>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <signal.h>
#include "SymbolTable/SymbolTable.h"
#include "Vector/Vector.h"

#define PIPE_DIR "PIPES/"

using namespace std;

SymTable_T countries;
Vector<char *> countries_strs;
Vector<pid_t> pids;
enum writeRead { READ, WRITE};
int *pipefds = NULL, bufferSize = -1;
string input_dir;
bool signalCatch = false;

struct worker{
    pid_t proccessID;
    int workerNum;
};

static void writeToChild(int , const char * );
static string readFromChild(int );
static void freePipes(int );

//================================================================================
//================================== SIGNALS =======================================
static void killExit(){
    int pidsLen = pids.getSize();
    for(int i=0; i<pidsLen; i++){
        if (kill(pids.getAt(i),SIGKILL) == -1)
            perror("Parent couldn't kill child");
    }
    int countriesLen = countries_strs.getSize();

    for(int i=0; i<countriesLen; i++){
        char* tmp =  countries_strs.getAt(i);
        free(SymTable_get(countries, tmp));
        free(tmp);
    }

    SymTable_free(countries);
    freePipes(pidsLen);
    cout << "\nAll workers exited\nmaster exited" << endl;
}

static void replaceWorker(pid_t id){
    input_dir.pop_back();
    int countriesLen = countries_strs.getSize(), Workernum;
    //get workerNum
    for(int i=0; i<countriesLen; i++){
        char* country = countries_strs.getAt(i);
        struct worker* w = (struct worker*)SymTable_get(countries, country);
        if (id == w->proccessID)
            Workernum = w->workerNum;
    }

    string strPipePath1, strPipePath2, tmp;
    const char *pipePath1, *pipePath2, *str_bufferSize;
    tmp = to_string(bufferSize);
    str_bufferSize = tmp.c_str();

    strPipePath1 = PIPE_DIR + ("p2c." + to_string(Workernum));
    strPipePath2 = PIPE_DIR + ("c2p." + to_string(Workernum));
    pipePath1 = strPipePath1.c_str();
    pipePath2 = strPipePath2.c_str();
    close(pipefds[2*Workernum]);
    pid_t newID = fork();
    if (newID == 0){
        const char * const argV[] = {"worker/worker", pipePath1, pipePath2, str_bufferSize, &input_dir[0], NULL};
        execv("worker/worker", (char * const *)argV);
        cout << "execv failed" << endl;
        exit(1);
    }

    int pidsLen = pids.getSize();
    //update pids Vector
    for(int i=0; i<pidsLen; i++){
        if (pids.getAt(i) == id)
            pids.setAt(i, newID);
    }

    pipefds[2*Workernum] = open(pipePath1, O_WRONLY);       //wait for worker to open his read
    //distribute countries
    for(int i=0; i<countriesLen; i++){
        char* country = countries_strs.getAt(i);
        struct worker* w = (struct worker*)SymTable_get(countries, country);
        if (id == w->proccessID){
            writeToChild(w->workerNum, country);
        }
    }
    writeToChild(Workernum, "C_DONE");
    //update SymTable
    for(int i=0; i<countriesLen; i++){
        char* country = countries_strs.getAt(i);
        struct worker* w = (struct worker*)SymTable_get(countries, country);
        if (id == w->proccessID)
            w->proccessID = newID;
    }

    //get Statistics
    string msg;
    while(1){
        msg = readFromChild(Workernum);
        if (strcmp(&msg[0], "S_DONE") == 0) // DONE WITH STATS FROM THIS WORKER
            break;
        cout << msg << endl;
    }
}

static void signal_handler(int signum){
    if (signum==SIGQUIT || signum==SIGINT){
        signalCatch = true;
        killExit();
        exit(0);
    }
    if (signum == SIGCHLD){
        int status;
        pid_t childID = wait(&status);

        if (WTERMSIG(status) == 9)      //don't replace worker if he died from SIGKILL
            return;

        replaceWorker(childID);
    }
}


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

static bool tryParseArgString(char *str, const char *name){
    if (!input_dir.empty()){
        cout << name << " passed more than once" << endl;
        return false;
    }
    input_dir = str;
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

static bool parseArgIP(char *str, string &serverIP){
    if (!serverIP.empty()){
        cout << "-s passed more than once" << endl;
        return false;
    }
    serverIP = str;
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

static bool parseArgs(int argc, char **argv, int& numWorkers, int& serverPort, string &serverIP){
    bool validArgs = true;
    int i;
    for(i=1; i<argc-1; i++){
        if(!strcmp(argv[i], "-w")){
            if(tryParseArgInt(numWorkers, argv[++i], "-w") == false){
                validArgs = false;
                i--;
            }
        }
        else if(!strcmp(argv[i], "-b")){
            if(tryParseArgInt(bufferSize, argv[++i], "-b") == false){
                validArgs = false;
                i--;
            }
        }
        else if(!strcmp(argv[i], "-i")){
            if(tryParseArgString(argv[++i], "-i") == false){
                validArgs = false;
                i--;
            }
        }
        else if(!strcmp(argv[i], "-s")){
            if(parseArgIP(argv[++i], serverIP) == false){
                validArgs = false;
                i--;
            }
        }
        else if(!strcmp(argv[i], "-p")){
            if(tryParseArgInt(serverPort, argv[++i], "-p") == false){
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

    if (numWorkers == -1){
        cout << "Missing numWorkers argumnent" << endl;
        validArgs = false;
    }
    if (bufferSize == -1){
        cout << "Missing bufferSize argumnent" << endl;
        validArgs = false;
    }
    if (serverIP.empty()){
        cout << "Missing serverIP argumnent" << endl;
        validArgs = false;
    }
    if (serverPort == -1){
        cout << "Missing serverPort argumnent" << endl;
        validArgs = false;
    }
    if (input_dir.empty()){
        cout << "Missing input_dir argumnent" << endl;
        validArgs = false;
    }
    return validArgs;
}

//================================================================================
//================================== PIPES =======================================
static void createPipes(int numWorkers){
    string strPipePath1, strPipePath2;
    const char * pipePath1, *pipePath2;
    for(int i=0; i<numWorkers; i++){
        strPipePath1 = PIPE_DIR + ("p2c." + to_string(i));
        strPipePath2 = PIPE_DIR + ("c2p." + to_string(i));
        pipePath1 = strPipePath1.c_str();
        pipePath2 = strPipePath2.c_str();
        if(mkfifo(pipePath1, 0666)<0 || mkfifo(pipePath2, 0666)<0){
            cout << "pipe " << i << ": mkfifo fail" << endl << "unlinking previous pipes..." << endl;
            for(int j=0; j<=i; j++){
                strPipePath1 = PIPE_DIR + ("p2c." + to_string(j));
                strPipePath2 = PIPE_DIR + ("c2p." + to_string(j));
                pipePath1 = strPipePath1.c_str();
                pipePath2 = strPipePath2.c_str();
                if(unlink(pipePath1)<0)
                    cout << "pipe p2c." << j << ": unlink fail" << endl;
                if(unlink(pipePath2)<0)
                    cout << "pipe c2p." << j << ": unlink fail" << endl;
            }
            exit(1);
        }
    }
    pipefds = new int[numWorkers*2];
}

static void freePipes(int numWorkers){
    string strPipePath1, strPipePath2;
    const char * pipePath1, *pipePath2;
    for(int i=0; i<numWorkers; i++){
        strPipePath1 = PIPE_DIR + ("p2c." + to_string(i));
        strPipePath2 = PIPE_DIR + ("c2p." + to_string(i));
        pipePath1 = strPipePath1.c_str();
        pipePath2 = strPipePath2.c_str();
        if(unlink(pipePath1)<0)
            cout << "pipe p2c." << i << ": unlink fail" << endl;
        if(unlink(pipePath2)<0)
            cout << "pipe c2p." << i << ": unlink fail" << endl;
    }
    delete[] pipefds;
}

static int getChildFd(enum writeRead wr, int childNum){
    if(wr == WRITE)
        return pipefds[2*childNum];
    return pipefds[2*childNum + 1];
}

static void writeHeaderToChild(int childNum, int header){
    if (bufferSize >= (int)sizeof(header) ){
        if (write(getChildFd(WRITE, childNum), &header, sizeof(header)) == -1){
            perror("ERROR : write");
            exit(1);
        }
        return;
    }
    char *buffer = (char *)&header;
    int bufferIndex = 0;
    while(bufferIndex + bufferSize <= (int)sizeof(header)){
        if (write(getChildFd(WRITE, childNum), &buffer[bufferIndex], bufferSize) == -1){ // full bufferSize chunk
            perror("ERROR : write");
            exit(1);
        }
        bufferIndex += bufferSize;
    }
    if(bufferIndex != (int)sizeof(header)){
        if (write(getChildFd(WRITE, childNum), &buffer[bufferIndex], sizeof(header) - bufferIndex) == -1){ // less than bufferSize chunk
            perror("ERROR : write");
            exit(1);
        }
    }
}

static void writeToChild(int childNum, const char *buffer){
    int len = strlen(buffer) + 1;
    writeHeaderToChild(childNum,len);
    if(bufferSize >= len){
        if (write(getChildFd(WRITE, childNum), buffer, len) == -1){
            perror("ERROR : write");
            exit(1);
        }
        return;
    }
    int bufferIndex = 0;
    while(bufferIndex + bufferSize <= len){
        if (write(getChildFd(WRITE, childNum), &buffer[bufferIndex], bufferSize) == -1){ // full bufferSize chunk
            perror("ERROR : write");
            exit(1);
        }
        bufferIndex += bufferSize;
    }
    if(bufferIndex != len){
        if (write(getChildFd(WRITE, childNum), &buffer[bufferIndex], len - bufferIndex) == -1){ // less than bufferSize chunk
            perror("ERROR : write");
            exit(1);
        }
    }
}

static int getHeaderFromChild(int childNum){
    int header;
    if (bufferSize >= (int)sizeof(header) ){
        read(getChildFd(READ, childNum), &header, sizeof(header));
        return header;
    }
    char buffer[sizeof(header)];
    int bufferIndex = 0;
    while(bufferIndex + bufferSize <= (int)sizeof(header)){
        read(getChildFd(READ, childNum), &buffer[bufferIndex], bufferSize); // full bufferSize chunk
        bufferIndex += bufferSize;
    }
    if(bufferIndex != (int)sizeof(header)){
        read(getChildFd(READ, childNum), &buffer[bufferIndex], sizeof(header) - bufferIndex); // less than bufferSize chunk
    }
    header = *(int*)(buffer);
    return header;
}

static string readFromChild(int childNum){
    string msg;
    int messageSize = getHeaderFromChild(childNum);
    char *buffer = (char*)malloc(sizeof(char) * messageSize);
    if(bufferSize >= messageSize){
        read(getChildFd(READ, childNum), buffer, messageSize);
        msg = buffer;
        free(buffer);
        return msg;
    }
    int bufferIndex = 0;
    while(bufferIndex + bufferSize <= messageSize){
        read(getChildFd(READ, childNum), &buffer[bufferIndex], bufferSize); // full bufferSize chunk
        bufferIndex += bufferSize;
    }
    if(bufferIndex != messageSize){
        read(getChildFd(READ, childNum), &buffer[bufferIndex], messageSize - bufferIndex); // less than bufferSize chunk
    }
    msg = buffer;
    free(buffer);
    return msg;
}

//================================================================================
//================================================================================

static bool distributeCountriesToWorkers(int numWorkers){
    input_dir += '/';
    DIR *dir;
    struct dirent *entry;
    if ((dir = opendir (&input_dir[0])) == NULL){
        perror("ERROR");
        return false;
    }

    for(int i=0; i<numWorkers; i++){
        writeToChild(i, "COUNTRIES");
    }
    int workerRoundRobin = 0;
    while ((entry = readdir(dir)) != NULL) {
        if(strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")){
            char *country = (char*)malloc(strlen(entry->d_name) + 1);
            strcpy(country, entry->d_name);
            countries_strs.pushBack(country);
            writeToChild(workerRoundRobin, entry->d_name);
            struct worker *w = (struct worker*)malloc(sizeof(struct worker));
            w->proccessID = pids.getAt(workerRoundRobin);
            w->workerNum = workerRoundRobin;
            SymTable_put(countries,entry->d_name,(const void*)w);

            workerRoundRobin++;
            workerRoundRobin %= numWorkers;
        }
    }

    for(int i=0; i<numWorkers; i++){
        writeToChild(i, "C_DONE");
        string tmp = to_string(numWorkers);
        writeToChild(i, "EXTRA_INFO");
        writeToChild(i, &tmp[0]);       //send numWorkers to worker

    }
    closedir(dir);
    return true;
}


// ./master –w numWorkers -b bufferSize –s serverIP –p serverPort -i input_dir
int master(int argc, char **argv){
    signal(SIGINT,signal_handler);
    signal(SIGQUIT,signal_handler);
    int numWorkers = -1, serverPort = -1;
    string serverIP;
    if(!parseArgs(argc, argv, numWorkers, serverPort, serverIP))
        return -1;

    createPipes(numWorkers);

    int retVal;
    pid_t id;
    string strPipePath1, strPipePath2, tmp, workerPortStr, workerNumberStr;
    const char *pipePath1, *pipePath2, *str_bufferSize;
    tmp = to_string(bufferSize);
    str_bufferSize = tmp.c_str();
    for(int i=0; i<numWorkers; i++){
        strPipePath1 = PIPE_DIR + ("p2c." + to_string(i));
        strPipePath2 = PIPE_DIR + ("c2p." + to_string(i));
        pipePath1 = strPipePath1.c_str();
        pipePath2 = strPipePath2.c_str();
        id = fork();
        if (id == 0){
            workerPortStr = to_string(i+2000+1);
            const char *workerPort = workerPortStr.c_str();
            workerNumberStr = to_string(i);
            const char *workerNumber = workerNumberStr.c_str();
            const char * const argV[] = {"worker/worker", pipePath1, pipePath2, str_bufferSize, &input_dir[0], workerPort,  workerNumber, NULL};
            execv("worker/worker", (char * const *)argV);
            cout << "execv failed" << endl;
            exit(1);
        }
        pids.pushBack(id);
        if ( (pipefds[2*i] = open(pipePath1, O_WRONLY)) < 0)
            perror("parent: can't open write fifo");
        if ( (pipefds[2*i + 1] = open(pipePath2, O_RDONLY)) < 0)
            perror("parent: can't open write fifo");
    }

    countries = SymTable_new();

    if(!distributeCountriesToWorkers(numWorkers)){
        cout << "Could not distribute countries to workers." << endl;
        freePipes(numWorkers);
        return -1;
    }

    //Send whoServer IP address & port number to workers
    for(int i=0; i<numWorkers; i++){
        writeToChild(i, "IP");
        writeToChild(i, &serverIP[0]);
        writeToChild(i, "PORT");
        tmp = to_string(serverPort);
        writeToChild(i, &tmp[0]);
    }

    for(int i=0; i<numWorkers; i++){
        wait(&retVal);
    }

    return 0;
}
