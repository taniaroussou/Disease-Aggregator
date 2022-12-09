#include <iostream>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iomanip>
#include <sstream>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include "../RingBuffer/RingBuffer.h"
#include "../SymbolTable/SymbolTable.h"
#include "../Vector/Vector.h"

#define SOCKET_BUFFERSIZE 1024

using namespace std;

struct workerInfo{
    char ip[1024];
    int port;
};

pthread_t *threadPool;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condVarNonEmpty = PTHREAD_COND_INITIALIZER, condVarNonFull = PTHREAD_COND_INITIALIZER;
RingBuffer buffer;
int numWorkers = -1, numThreads = -1;
SymTable_T fdIPs, workers, countries;
Vector<int> workerfds;
Vector<char *> countriesStrs;
int (*f_ptr)(std::string, int);

static string connectToWorker(char *, int , std::string );

static void signal_handler(int signum){
    if (signum==SIGQUIT || signum==SIGINT){
        int fdsLen = workerfds.getSize();
        free(threadPool);
        RingBufferDestroy(buffer);
        for(int i=0; i<numWorkers; i++){
            string keyStr = to_string(i);
            const char *key = keyStr.c_str();
            delete (struct workerInfo *)SymTable_get(workers, key);
        }
        unsigned int vec = countriesStrs.getSize();
        for(unsigned int i=0; i<vec; i++){
            free(countriesStrs.getAt(i));
        }
        SymTable_free(workers);
        SymTable_free(fdIPs);
        SymTable_free(countries);
        cout << "\nwhoServer exited" << endl;
        exit(0);
    }
}

//================================================================================
//================================== DISEASE =====================================
static time_t getDate(string str) {
	struct tm tmp = { 0 };
	tmp.tm_isdst = -1;
	istringstream ss(str);
	ss >> std::get_time(&tmp, "%d-%m-%Y");
	if (ss.fail()) {
        return (time_t) - 1;        //invalid format, return -1
	}
	return mktime(&tmp);    //return date as seconds that have passed since 1 Jan 1970 00:00:00
}

static bool isNumber(char c){
    return (c>='0' && c<='9');  //check ASCII codes
}

static bool isLetter(char c){
    return ((c>='A' && c<='Z') || (c>='a' && c<='z'));   //check ASCII codes
}

static bool isWord(string &word){
    int len = word.length();
    bool valid = true;
    for(int i=0; i<len; i++){
        if (!isLetter(word.at(i)))
            valid = false;
    }
    return valid;
}

static bool isStringNumber(string &word){
    int len = word.length();
    bool valid = true;
    for(int i=0; i<len; i++){
        if (!isNumber(word.at(i)))
            valid = false;
    }
    return valid;
}

static bool RecordIDCheck(string &word){
    int len = word.length();
    bool valid = true;
    for(int i=0; i<len; i++){
        if (!isNumber(word.at(i)) && !isLetter(word.at(i)))
            valid = false;
    }
    return valid;
}

static bool DiseaseCheck(string &word){
    int len = word.length(), dash = 0;
    bool valid = true;
    for(int i=0; i<len; i++){
        if (!isNumber(word.at(i)) && !isLetter(word.at(i)) && word.at(i)!='-')
            valid = false;
        if (word.at(i)=='-' && dash!=0)
            valid = false;
    }
    return valid;
}

static string diseaseFrequency(std::string arg, int socket){
    unsigned int i=0,argSum=0,len = arg.length();
    string arg1,arg2,arg3,arg4;
    while (i<len){
        if (arg[i]!=' '){
            if (argSum==0)
                arg1 += arg[i];
            else if (argSum==1)
                arg2 += arg[i];
            else if (argSum==2)
                arg3 += arg[i];
            else if (argSum==3)
                arg4 += arg[i];
            else if (argSum==4){
                cout << "Too many arguments in /diseaseFrequency" << endl;
                return "";
            }
        }
        else
            argSum++;
        i++;
    }

    if (arg1.empty() || arg2.empty() || arg3.empty()){
        cout << "Not enough arguments in /diseaseFrequency" << endl;
        return "";
    }
    time_t date1 = getDate(arg2), date2 = getDate(arg3);
    bool valid[4];
    for (int i=0 ; i<4 ; i++) valid[i] = true;

    if (!DiseaseCheck(arg1))
        valid[0] = false;
    if (date1<0)
        valid[1] = false;
    if (date2<0)
        valid[2] = false;
    if (valid[1] && valid[2] && date1>date2)
        valid[3] = false;

    if (!valid[0] || !valid[1] || !valid[2] || !valid[3]){
        if (!valid[0]) cout << "VirusName not valid" << endl;
        if (!valid[1]) cout << "Date1 not valid" << endl;
        if (!valid[2]) cout << "Date2 not valid" << endl;
        if (!valid[3]) cout << "Dates don't make sense" << endl;
        return "";
    }

    arg.insert(0,"/diseaseFrequency ");
    if (!arg4.empty()){
        if (!isWord(arg4)){
            cout << "Country not valid" << endl;
            return "";
        }
        //call with country
        void *searchResult = SymTable_get(countries, &arg4[0]); //find worker with country = arg4
        if (searchResult == NULL){
            cout << "Country does not exist!" << endl;
            return "";
        }
        char *workerNumber = strdup((const char *)searchResult);
        struct workerInfo *info = (struct workerInfo*)SymTable_get(workers, workerNumber);
        free(workerNumber);
        char ip[1024];
        strcpy(ip, info->ip);
        int port = info->port;
        struct sockaddr_in addr;
        socklen_t addr_size = sizeof(addr);
        char *sIP = (char *)malloc(sizeof(char)*100);
        getpeername(socket, (struct sockaddr *)&addr, &addr_size);
        strcpy(sIP, inet_ntoa(addr.sin_addr));
        return connectToWorker(sIP, port, arg);
    }
    //call without country
    unsigned int workersLen = SymTable_getLength(workers);
    string answer = "";
    int ans = 0;
    for(unsigned int i=0; i<workersLen; i++){
        string tmp = to_string(i);
        char *workerNumber = (char *)malloc(sizeof(char)*15);
        strcpy(workerNumber, tmp.c_str());
        struct workerInfo *info = (struct workerInfo*)SymTable_get(workers, workerNumber);
        free(workerNumber);
        char ip[1024];
        strcpy(ip, info->ip);
        int port = info->port;
        struct sockaddr_in addr;
        socklen_t addr_size = sizeof(addr);
        char *sIP = (char *)malloc(sizeof(char)*100);
        getpeername(socket, (struct sockaddr *)&addr, &addr_size);
        strcpy(sIP, inet_ntoa(addr.sin_addr));

        answer = connectToWorker(sIP, port, arg);
        if (strcmp(&answer[0],"FAIL") == 0)
            return "";
        else
            ans += stoi(answer);

    }
    return to_string(ans);
}

static string topkAgeRanges(std::string arg, int socket){
    unsigned int i=0,argSum=0,len = arg.length();
    string arg1,arg2,arg3,arg4,arg5;
    while (i<len){
        if (arg[i]!=' '){
            if (argSum==0)
                arg1 += arg[i];
            else if (argSum==1)
                arg2 += arg[i];
            else if (argSum==2)
                arg3 += arg[i];
            else if (argSum==3)
                arg4 += arg[i];
            else if (argSum==4)
                arg5 += arg[i];
            else if (argSum==5){
                cout << "Too many arguments in /topk-AgeRanges" << endl;
                return "";
            }
        }
        else
            argSum++;
        i++;
    }

    if (arg1.empty() || arg2.empty() || arg3.empty() || arg4.empty() || arg5.empty()){
        cout << "Too few arguments in /topk-AgeRanges" << endl;
        return "";
    }
    time_t date1 = getDate(arg4), date2 = getDate(arg5);
    bool valid[6];
    for(int i=0; i<6; i++)  valid[i] = true;
    if (!isStringNumber(arg1))
        valid[0] = false;
    if (!isWord(arg2))
        valid[1] = false;
    if (!DiseaseCheck(arg3))
        valid[2] = false;
    if (date1<0)
        valid[3] = false;
    if (date2<0)
        valid[4] = false;
    if (valid[1] && valid[2] && date1>date2)
        valid[5] = false;

    if (!valid[0] || !valid[1] || !valid[2] || !valid[3] || !valid[4] || !valid[5]){
        if (!valid[0]) cout << "k not valid" << endl;
        if (!valid[1]) cout << "Country not valid!" << endl;
        if (!valid[2]) cout << "Disease not valid!" << endl;
        if (!valid[3]) cout << "Date 1 not valid" << endl;
        if (!valid[4]) cout << "Date 2 not valid" << endl;
        if (!valid[5]) cout << "Dates don't make sense" << endl;
        return "";
    }
    arg.insert(0,"/topkAgeRanges ");
    void *searchResult = SymTable_get(countries, &arg2[0]);
    if (searchResult == NULL){
        cout << "Country does not exist!" << endl;
        return "";
    }
    char *workerNumber = strdup((const char *)searchResult);

    struct workerInfo *info = (struct workerInfo*)SymTable_get(workers, workerNumber);
    free(workerNumber);
    char ip[1024];
    strcpy(ip, info->ip);
    int port = info->port;
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(addr);
    char *sIP = (char *)malloc(sizeof(char)*100);
    getpeername(socket, (struct sockaddr *)&addr, &addr_size);
    strcpy(sIP, inet_ntoa(addr.sin_addr));

    return connectToWorker(sIP, port, arg);
}

static string searchPatientRecord(std::string arg, int socket){
    unsigned int i=0,argSum=0,len = arg.length();
    string arg1;

    while(i<len){
        if (arg[i]!=' '){
            if (argSum==0)
                arg1 += arg[i];
            else if (argSum==1){
                cout << "Too many arguments in /searchPatientRecord" << endl;
                return "";
            }
        }
        else
            argSum++;
        i++;
    }

    if (arg1.empty()){
        cout << "Too few arguments in /searchPatientRecord" << endl;
        return "";
    }
    if (!RecordIDCheck(arg1)){
        cout << "RecordID invalid" << endl;
        return "";
    }

    arg.insert(0,"/searchPatientRecord ");

    unsigned int workersLen = SymTable_getLength(workers);
    string tmpanswer = "", answer = "";
    bool found = false;
    for(unsigned int i=0; i<workersLen; i++){
        string tmp = to_string(i);
        char *workerNumber = (char *)malloc(sizeof(char)*15);
        strcpy(workerNumber, tmp.c_str());
        struct workerInfo *info = (struct workerInfo*)SymTable_get(workers, workerNumber);
        free(workerNumber);
        char ip[1024];
        strcpy(ip, info->ip);
        int port = info->port;
        struct sockaddr_in addr;
        socklen_t addr_size = sizeof(addr);
        char *sIP = (char *)malloc(sizeof(char)*100);
        getpeername(socket, (struct sockaddr *)&addr, &addr_size);
        strcpy(sIP, inet_ntoa(addr.sin_addr));

        tmpanswer = connectToWorker(sIP, port, arg);
        if (strcmp(&tmpanswer[0],"FAIL") != 0){
            found = true;
            answer = tmpanswer;
        }
    }
    if (found)
        return answer;
    else
        return "RecordID not found";
}

static string numPatientAdmissions(std::string arg, int socket){
    unsigned int i=0,argSum=0,len = arg.length();
    string arg1,arg2,arg3,arg4;
    while (i<len){
        if (arg[i]!=' '){
            if (argSum==0)
                arg1 += arg[i];
            else if (argSum==1)
                arg2 += arg[i];
            else if (argSum==2)
                arg3 += arg[i];
            else if (argSum==3)
                arg4 += arg[i];
            else if (argSum==4){
                cout << "Too many arguments in /numPatientAdmissions" << endl;
                return "";
            }
        }
        else
            argSum++;
        i++;
    }

    if (arg1.empty() || arg2.empty() || arg3.empty()){
        cout << "Not enough arguments in /numPatientAdmissions" << endl;
        return "";
    }
    time_t date1 = getDate(arg2), date2 = getDate(arg3);
    bool valid[4];
    for (int i=0; i<4; i++) valid[i] = true;

    if (!DiseaseCheck(arg1))
        valid[0] = false;
    if (date1<0)
        valid[1] = false;
    if (date2<0)
        valid[2] = false;
    if (valid[1] && valid[2] && date1>date2)
        valid[3] = false;

    if (!valid[0] || !valid[1] || !valid[2] || !valid[3]){
        if (!valid[0]) cout << "VirusName not valid" << endl;
        if (!valid[1]) cout << "Date1 not valid" << endl;
        if (!valid[2]) cout << "Date2 not valid" << endl;
        if (!valid[3]) cout << "Dates don't make sense" << endl;
        return "";
    }

    arg.insert(0,"/numPatientAdmissions ");
    if (!arg4.empty()){
        if (!isWord(arg4)){
            cout << "Country not valid" << endl;
            return "";
        }
        //call with country
        string answer = "";
        void *searchResult = SymTable_get(countries, &arg4[0]);

        if (searchResult == NULL){
            cout << "Country does not exist!" << endl;
            return "";
        }
        char *workerNumber = strdup((const char *)searchResult);


        struct workerInfo *info = (struct workerInfo*)SymTable_get(workers, workerNumber);
        free(workerNumber);
        char ip[1024];
        strcpy(ip, info->ip);
        int port = info->port;
        struct sockaddr_in addr;
        socklen_t addr_size = sizeof(addr);
        char *sIP = (char *)malloc(sizeof(char)*100);
        getpeername(socket, (struct sockaddr *)&addr, &addr_size);
        strcpy(sIP, inet_ntoa(addr.sin_addr));

        answer = connectToWorker(sIP, port, arg);
        if (strcmp(&answer[0],"NULL_VIRUS")==0)
            return "0";
        return answer;
    }
    //call without country
    unsigned int workersLen = SymTable_getLength(workers);
    string answer = "", ans = "";
    for(unsigned int i=0; i<workersLen; i++){
        string tmp = to_string(i);
        char *workerNumber = (char *)malloc(sizeof(char)*15);
        strcpy(workerNumber, tmp.c_str());
        struct workerInfo *info = (struct workerInfo*)SymTable_get(workers, workerNumber);
        free(workerNumber);
        char ip[1024];
        strcpy(ip, info->ip);
        int port = info->port;
        struct sockaddr_in addr;
        socklen_t addr_size = sizeof(addr);
        char *sIP = (char *)malloc(sizeof(char)*100);
        getpeername(socket, (struct sockaddr *)&addr, &addr_size);
        strcpy(sIP, inet_ntoa(addr.sin_addr));

        answer = connectToWorker(sIP, port, arg);
        if (strcmp(&answer[0],"FAIL") == 0)
            return "";
        else if (strcmp(&answer[0],"NULL_VIRUS")==0){   //a worker doesn't have cases for virus -> 0 for every country of his
            int countriesLen = countriesStrs.getSize();
            for(int j=0; j<countriesLen; j++){
                char* country = (char *)malloc(sizeof(char)*30);
                strcpy(country, countriesStrs.getAt(j));
                char * w = (char *)malloc(sizeof(char)*24);
                strcpy(w, (char *)SymTable_get(countries, country));
                unsigned int ww = atoi(w);
                if (ww == i){
                    ans += country;
                    ans += " 0";
                }
                else
                    ans += answer;
            }
        }

    }
    return ans;

}

static string numPatientDischarges(std::string arg, int socket){
    unsigned int i=0,argSum=0,len = arg.length();
    string arg1,arg2,arg3,arg4;
    while (i<len){
        if (arg[i]!=' '){
            if (argSum==0)
                arg1 += arg[i];
            else if (argSum==1)
                arg2 += arg[i];
            else if (argSum==2)
                arg3 += arg[i];
            else if (argSum==3)
                arg4 += arg[i];
            else if (argSum==4){
                cout << "Too many arguments in /numPatientDischarges" << endl;
                return "";
            }
        }
        else
            argSum++;
        i++;
    }

    if (arg1.empty() || arg2.empty() || arg3.empty()){
        cout << "Not enough arguments in /numPatientDischarges" << endl;
        return "";
    }
    time_t date1 = getDate(arg2), date2 = getDate(arg3);
    bool valid[4];
    for (int i=0; i<4; i++) valid[i] = true;

    if (!DiseaseCheck(arg1))
        valid[0] = false;
    if (date1<0)
        valid[1] = false;
    if (date2<0)
        valid[2] = false;
    if (valid[1] && valid[2] && date1>date2)
        valid[3] = false;

    if (!valid[0] || !valid[1] || !valid[2] || !valid[3]){
        if (!valid[0]) cout << "VirusName not valid" << endl;
        if (!valid[1]) cout << "Date1 not valid" << endl;
        if (!valid[2]) cout << "Date2 not valid" << endl;
        if (!valid[3]) cout << "Dates don't make sense" << endl;
        return "";
    }

    arg.insert(0,"/numPatientDischarges ");
    if (!arg4.empty()){
        if (!isWord(arg4)){
            cout << "Country not valid" << endl;
            return "";
        }
        //call with country
        string answer = "";
        void *searchResult = SymTable_get(countries, &arg4[0]);
        if (searchResult == NULL){
            cout << "Country does not exist!" << endl;
            return "";
        }
        char *workerNumber = strdup((const char *)searchResult);

        struct workerInfo *info = (struct workerInfo*)SymTable_get(workers, workerNumber);
        free(workerNumber);
        char ip[1024];
        strcpy(ip, info->ip);
        int port = info->port;
        struct sockaddr_in addr;
        socklen_t addr_size = sizeof(addr);
        char *sIP = (char *)malloc(sizeof(char)*100);
        getpeername(socket, (struct sockaddr *)&addr, &addr_size);
        strcpy(sIP, inet_ntoa(addr.sin_addr));

        answer = connectToWorker(sIP, port, arg);
        if (strcmp(&answer[0],"FAIL")==0)
            return "0";
        return answer;
    }
    //call without country
    unsigned int workersLen = SymTable_getLength(workers);
    string answer = "", ans = "";
    for(unsigned int i=0; i<workersLen; i++){

        struct workerInfo *info = (struct workerInfo*)SymTable_get(workers, to_string(i).c_str());
        char ip[1024];
        strcpy(ip, info->ip);
        int port = info->port;
        struct sockaddr_in addr;
        socklen_t addr_size = sizeof(addr);
        char *sIP = (char *)malloc(sizeof(char)*100);
        getpeername(socket, (struct sockaddr *)&addr, &addr_size);
        strcpy(sIP, inet_ntoa(addr.sin_addr));

        answer = connectToWorker(sIP, port, arg);
        if (strcmp(&answer[0],"FAIL") == 0)
            return "";
        else
            ans += answer;

    }
    return ans;
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

static bool parseArgs(int argc, char **argv, int& queryPortNum, int& statisticsPortNum, int& numThreads, int& bufferSize){
    bool validArgs = true;
    int i;
    for(i=1; i<argc-1; i++){
        if(!strcmp(argv[i], "-q")){
            if(tryParseArgInt(queryPortNum, argv[++i], "-q") == false){
                validArgs = false;
                i--;
            }
        }
        else if(!strcmp(argv[i], "-s")){
            if(tryParseArgInt(statisticsPortNum, argv[++i], "-s") == false){
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
        else if(!strcmp(argv[i], "-b")){
            if(tryParseArgInt(bufferSize, argv[++i], "-b") == false){
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

    if (queryPortNum == -1){
        cout << "Missing queryPortNum argumnent" << endl;
        validArgs = false;
    }
    if (statisticsPortNum == -1){
        cout << "Missing statisticsPortNum argumnent" << endl;
        validArgs = false;
    }
    if (numThreads == -1){
        cout << "Missing numThreads argumnent" << endl;
        validArgs = false;
    }
    if (bufferSize == -1){
        cout << "Missing bufferSize argumnent" << endl;
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

void createPassiveEndpoint(int &sock, int &port, struct sockaddr_in *server, struct sockaddr_in *client, struct sockaddr *serverPtr, struct sockaddr *clientPtr, socklen_t &len){

    len = sizeof(*client);
    server->sin_family = AF_INET;
    server->sin_addr.s_addr = htonl(INADDR_ANY);
    server->sin_port = htons(port);

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0){
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    if (bind(sock, serverPtr, sizeof(*server)) < 0){
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(sock, 10) < 0){
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
}

static string connectToWorker(char *ip, int port, std::string message){
    int sock;

    struct sockaddr_in server;
    struct sockaddr *serverPtr = (struct sockaddr *)&server;

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0){
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    struct hostent * rem ;
    rem = gethostbyname(ip);

    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);

    if (connect(sock, serverPtr, sizeof(server)) < 0){
        perror("Connect failed");
        exit(EXIT_FAILURE);
    }
    writeMessage(&message[0], sock);
    free(ip);
    return getMessage(sock);
}

//================================================================================
//================================== THREADS =======================================
bool handleConnection(BufferData clientfd){

    int clientSock = clientfd->fd, workerNumber;
    JobType job = clientfd->type;
    string message;
    message = getMessage(clientSock);

    pthread_mutex_lock(&mtx);
    if (job == STATISTICS)
        cout << "Accepted Connection from worker" << endl;
    else{
        cout << "Accepted connection from client" << endl;
        cout << "Server got query: " << message << endl;
    }
    pthread_mutex_unlock(&mtx);

    if (job == STATISTICS){
        if (strcmp(&message[0], "NUM_WORKERS") == 0){
            message = getMessage(clientSock);
            numWorkers = stoi(message);
            message = getMessage(clientSock); //get next message
        }
        if (strcmp(&message[0], "WORKER_NUM") == 0){
            message = getMessage(clientSock);
            workerNumber = stoi(message);
            message = getMessage(clientSock); //get next message
        }
        string countriesMsg = "";
        if (strcmp(&message[0], "COUNTRIES") == 0){
            countriesMsg = getMessage(clientSock);
            unsigned int len = countriesMsg.length(), i = 0;
            string country;
            string valueStr = to_string(workerNumber);
            char *value = (char *)malloc(sizeof(char)*15);
            strcpy(value, valueStr.c_str());
            while(i < len){
                if (countriesMsg[i] != ' '){
                    country += countriesMsg[i];
                }
                else{
                    //insert country to symtable
                    char *countryChar = strdup((const char *)country.c_str());
                    SymTable_put(countries, countryChar, (const void *)value);      //match country to worker number
                    countriesStrs.pushBack(countryChar);
                    country.clear();
                }
                i++;
            }
            message = getMessage(clientSock); //get next message
        }
        string stats = "";
        if (strcmp(&message[0], "STATISTICS") == 0){
            message = getMessage(clientSock); //get stats
            while(strcmp(&message[0], "S_DONE") != 0){
                stats += message;
                message = getMessage(clientSock);
            }
            if (strcmp(&message[0], "S_DONE") == 0)
                message = getMessage(clientSock); //get next message
        }
        if (strcmp(&message[0], "PORT") == 0){
            message = getMessage(clientSock); //get port

            string temp = to_string(clientSock);
            char* ctemp = (char *)malloc(sizeof(char)*40);
            strcpy(ctemp, temp.c_str());
            char *ip = (char *)SymTable_get(fdIPs, ctemp);      //get worker's ip address from fdIPs

            string keyStr = to_string(workerNumber);
            char *key = (char *)malloc(sizeof(char)*40);
            strcpy(key, keyStr.c_str());

            struct workerInfo *info = new workerInfo;
            info->port = stoi(message);
            strcpy(info->ip, ip);

            SymTable_put(workers, key, (const void*)info);       //match worker number to ip address and queryport
        }
        close(clientSock);
    }
    else if (job == QUERY){
        //get command and args from message
        string com, args;
        unsigned int len = message.length();

        unsigned int j=0;
        while(j < len && message[j] != ' '){      //get command
            com += message[j];
            j++;
        }
        j++;                                    // skip space
        while(j < len){                         //get arguments
            args += message[j];
            j++;
        }
        char * command = &(com[0]);
        string answer = "";
        if (strcmp(command, "/diseaseFrequency") == 0)
            answer = diseaseFrequency(args, clientSock);
        else if (strcmp(command, "/topk-AgeRanges") == 0)
            answer = topkAgeRanges(args, clientSock);
        else if (strcmp(command, "/searchPatientRecord") == 0)
            answer = searchPatientRecord(args, clientSock);
        else if (strcmp(command, "/numPatientAdmissions") == 0)
            answer = numPatientAdmissions(args, clientSock);
        else if (strcmp(command, "/numPatientDisharges") == 0)
            answer = numPatientDischarges(args, clientSock);

        writeMessage(&answer[0], clientSock);
        pthread_mutex_lock(&mtx);
        close(clientSock);
        pthread_mutex_unlock(&mtx);
    }
    return true;
}

void *threadFunction(void *input){
    struct data * clientfd = new struct data;
    while(1){
        pthread_mutex_lock(&mtx);
        while(RingBufferisEmpty(buffer)){
            pthread_cond_wait(&condVarNonEmpty, &mtx);            //wait and release the lock for other threads
        }
        *clientfd = RingBufferRemove(buffer);
        pthread_mutex_unlock(&mtx);
        pthread_cond_signal(&condVarNonFull);
        handleConnection(clientfd);
    }
}

static void threadWork(int fd, JobType type){
    pthread_mutex_lock(&mtx);

    while(RingBufferisFull(buffer)){
        pthread_cond_wait(&condVarNonFull, &mtx);            //wait and release the lock for other threads
    }
    RingBufferInsert(buffer, fd, type);
    pthread_mutex_unlock(&mtx);
    pthread_cond_signal(&condVarNonEmpty);
}

static void addFdIPs(int sock, char adderss[]){
    workerfds.pushBack(sock);
    string tempKey = to_string(sock);
    char *value = (char *)malloc(1024);
    strcpy(value, adderss);
    SymTable_put(fdIPs, tempKey.c_str(), (const void*)value);       //match worker socket to ip address
}

//./whoServer –q queryPortNum -s statisticsPortNum –w numThreads –b bufferSize

int main(int argc, char **argv){
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    int queryPortNum = -1, statisticsPortNum = -1, bufferSize = -1;
    if(!parseArgs(argc, argv, queryPortNum, statisticsPortNum, numThreads, bufferSize))
        return -1;

    buffer = RingBufferCreate(bufferSize);

    threadPool = (pthread_t *)malloc(sizeof(pthread_t)*numThreads);
    for(int i=0; i<numThreads; i++){
        pthread_create(&threadPool[i], NULL, threadFunction, NULL);
    }

    int workerSock, workerNewSock, whoClientSock, whoClientNewSock;
    socklen_t workerClientLen, whoClientClientLen;
    struct sockaddr_in workerServer, workerClient, whoClientServer, whoClientClient;
    struct sockaddr *workerServerPtr = (struct sockaddr *)&workerServer, *whoClientServerPtr = (struct sockaddr *)&whoClientServer;
    struct sockaddr *workerClientPtr = (struct sockaddr *)&workerClient, *whoClientClientPtr = (struct sockaddr *)&whoClientClient;
    workerClientLen = sizeof(workerClient);
    whoClientClientLen = sizeof(whoClientClient);

    //create passive endpoints for workers and whoClient
    createPassiveEndpoint(workerSock, statisticsPortNum, &workerServer, &workerClient, workerServerPtr, workerClientPtr, workerClientLen);
    createPassiveEndpoint(whoClientSock, queryPortNum, &whoClientServer, &whoClientClient, whoClientServerPtr, whoClientClientPtr, whoClientClientLen);


//============================== CONNECTIONS =============================================
    fd_set readfds;
    int retVal, max_fd;
    string message;
    fdIPs = SymTable_new();
    workers = SymTable_new();
    countries = SymTable_new();
    while(1){

        FD_ZERO(&readfds);

        FD_SET(workerSock, &readfds);
        FD_SET(whoClientSock, &readfds);

        max_fd = workerSock;
        if (whoClientSock > max_fd)
            max_fd = whoClientSock;

        retVal = select(max_fd+1, &readfds, NULL, NULL, NULL);
        if ((retVal < 0) && (errno != EINTR))
            perror("Select");

        if (FD_ISSET(workerSock, &readfds)){    //statistics connection (worker)
            if ((workerNewSock = accept(workerSock, workerClientPtr, &workerClientLen)) < 0){
                perror("Accept failed");
                exit(EXIT_FAILURE);
            }

            struct sockaddr_in addr;
            socklen_t addr_size = sizeof(addr);
            char sIP[100];
            getpeername(workerNewSock, (struct sockaddr *)&addr, &addr_size);
            strcpy(sIP, inet_ntoa(addr.sin_addr));

            addFdIPs(workerNewSock, sIP);

            threadWork(workerNewSock, STATISTICS);

        }
        else if (FD_ISSET(whoClientSock, &readfds)){        //query connection (whoClient)
            if ((whoClientNewSock = accept(whoClientSock, whoClientClientPtr, &whoClientClientLen)) < 0){
                perror("Accept failed");
                exit(EXIT_FAILURE);
            }
            threadWork(whoClientNewSock, QUERY);
        }
    }

    return 0;
}
