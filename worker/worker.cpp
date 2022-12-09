#include <iostream>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dirent.h>
#include <netdb.h>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../Patient.h"
#include "../Vector/Vector.h"
#include "../AVLTree/AVL.h"
#include "../List/ListInterface.h"
#include "../SymbolTable/SymbolTable.h"
#include "../HashTable/HashTable.h"

#define SOCKET_BUFFERSIZE 1024

using namespace std;

int readfd, writefd, bufferSize;
bool signalCatch = false, newFiles = false;
SymTable_T countryFiles;
Vector<char*> countries;

struct file_date{
    char *file_str = NULL;
    time_t file_time = 0;
};

struct statsRanges{
    string disease;
    int ranges[4];
};

void signal_handler(int signum){
    if (signum==SIGINT || signum==SIGQUIT){
        close(readfd);      //if stuck in read
        signalCatch = true;
    }
    if (signum == SIGUSR1){
        close(readfd);
        newFiles = true;
    }
}

static void getCountries(HTHash* table,char* key, HTItem item, FILE *myfile){
    fprintf(myfile,"%s\n", key);
    cout << "Country = " << key << endl;
}

void terminate(HTHash** countryHashtable, int success, int fail){
    FILE *myfile;
    pid_t id = getpid();
    string tmpName = to_string(id);
    string name = "log_file." + tmpName;
    myfile = fopen(&name[0],"w");
    if (myfile==NULL)
        cout << "Couldn't create file " << name << endl;
    else{
        HTVisitFile(*countryHashtable, getCountries, myfile);
        fprintf(myfile,"%s %d\n","TOTAL",success+fail+1);
        fprintf(myfile,"%s %d\n","SUCCESS",success+1);
        fprintf(myfile,"%s %d\n","FAIL",fail);
        fclose(myfile);
        exit(0);
    }
}

static int getHeader(int bufferSize, int source){
    int header = -1;
    if (bufferSize >= (int)sizeof(header) ){
        read(source, &header, sizeof(header));
        return header;
    }
    char buffer[sizeof(header)];
    int bufferIndex = 0;
    while(bufferIndex + bufferSize <= (int)sizeof(header)){
        read(source, &buffer[bufferIndex], bufferSize); // full bufferSize chunk
        bufferIndex += bufferSize;
    }
    if(bufferIndex != (int)sizeof(header)){
        read(source, &buffer[bufferIndex], sizeof(header) - bufferIndex); // less than bufferSize chunk
    }
    header = *(int*)(buffer);
    return header;
}

static string getMessage(int bufferSize, int source){
    string msg;
    int messageSize = getHeader(bufferSize, source);
    if (messageSize == -1)
        return "";
    char *buffer = (char*)malloc(sizeof(char) * messageSize);
    if(bufferSize >= messageSize){
        read(source, buffer, messageSize);
        msg = buffer;
        free(buffer);
        return msg;
    }
    int bufferIndex = 0;
    while(bufferIndex + bufferSize <= messageSize){
        read(source, &buffer[bufferIndex], bufferSize); // full bufferSize chunk
        bufferIndex += bufferSize;
    }
    if(bufferIndex != messageSize){
        read(source, &buffer[bufferIndex], messageSize - bufferIndex); // less than bufferSize chunk
    }
    msg = buffer;
    free(buffer);
    return msg;
}

static void writeHeader(int header, int bufferSize, int destination){
    if (bufferSize >= (int)sizeof(header) ){
        if (write(destination, &header, sizeof(header)) == -1){
            perror("ERROR : write");
            exit(1);
        }
        return;
    }
    char *buffer = (char *)&header;
    int bufferIndex = 0;
    while(bufferIndex + bufferSize <= (int)sizeof(header)){
        if (write(destination, &buffer[bufferIndex], bufferSize) == -1){ // full bufferSize chunk
            perror("ERROR : write");
            exit(1);
        }
        bufferIndex += bufferSize;
    }
    if(bufferIndex != (int)sizeof(header)){
        if (write(destination, &buffer[bufferIndex], sizeof(header) - bufferIndex) == -1){ // less than bufferSize chunk
            perror("ERROR : write");
            exit(1);
        }
    }
}

static void writeMessage(const char *buffer, int bufferSize, int destination){
    int len = strlen(buffer) + 1;
    writeHeader(len, bufferSize, destination);
    if(bufferSize >= len){
        if (write(destination, buffer, len) == -1){
            perror("ERROR : write");
            exit(1);
        }
        return;
    }
    int bufferIndex = 0;
    while(bufferIndex + bufferSize <= len){
        if (write(destination, &buffer[bufferIndex], bufferSize) == -1){ // full bufferSize chunk
            perror("ERROR : write");
            exit(1);
        }
        bufferIndex += bufferSize;
    }
    if(bufferIndex != len){
        if (write(destination, &buffer[bufferIndex], len - bufferIndex) == -1){ // less than bufferSize chunk
            perror("ERROR : write");
            exit(1);
        }
    }
}

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

static char* getStringDate(time_t date){
    struct tm *timeinfo;
    timeinfo = localtime(&date);
    char *buffer = (char*)malloc(sizeof(char)*50);
    strftime(buffer,50,"%d-%m-%Y",timeinfo);
    return buffer;
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

static bool AgeCheck(string &word){
    int len = word.length();
    bool valid = true;
    for(int i=0; i<len; i++){
        if (!isNumber(word.at(i)) )
            valid = false;
    }
    if (valid){
        int age = stoi(word);
        if (age>0 && age<=120)
            valid = true;
        else
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

static bool RecordIDCheck(string &word){
    int len = word.length();
    bool valid = true;
    for(int i=0; i<len; i++){
        if (!isNumber(word.at(i)) && !isLetter(word.at(i)))
            valid = false;
    }
    return valid;
}

int compare(const void *a, const void *b){
    return  ((struct file_date*)a)->file_time - ((struct file_date*)b)->file_time;
}

bool parseLine(string line, time_t date, string country, HTHash **patientRecords, HTHash **diseaseHashtable, HTHash **countryHashtable){
    unsigned int i=0,argSum=0,len = line.length();
    string arg1, arg2, arg3, arg4, arg5, arg6;

    while (i<len){
        if (line[i]!=' '){
            if (argSum==0)
                arg1 += line[i];
            else if (argSum==1)
                arg2 += line[i];
            else if (argSum==2)
                arg3 += line[i];
            else if (argSum==3)
                arg4 += line[i];
            else if (argSum==4)
                arg5 += line[i];
            else if (argSum==5)
                arg6 += line[i];
            else if (argSum==6){
                errno = EINVAL;
                perror("ERROR"); //too many arguments in one line
                return false;
            }
        }
        else
            argSum++;
        i++;
    }

    if (arg1.empty() || arg2.empty() || arg3.empty() || arg4.empty() || arg5.empty() || arg6.empty()){
        errno = EINVAL;
        perror("ERROR"); //too few arguments in one line
        return false;
    }
    bool valid[6];
    for(int i=0; i<6; i++)  valid[i] = true;
    if (!RecordIDCheck(arg1))
        valid[0] = false;
    if (strcmp(&(arg2[0]),"ENTER") && strcmp(&(arg2[0]),"EXIT"))
        valid[1] = false;
    if (!isWord(arg3))
        valid[2] = false;
    if (!isWord(arg4))
        valid[3] = false;
    if (!DiseaseCheck(arg5))
        valid[4] = false;
    if (!AgeCheck(arg6))
        valid[5] = false;

    if (!valid[0] || !valid[1] || !valid[2] || !valid[3] || !valid[4] || !valid[5]){
        if (!valid[0]) cout << "Record ID not valid" << endl;
        if (!valid[1]) cout << "ENTER/EXIT field not valid" << endl;
        if (!valid[2]) cout << "Patient's first name not valid" << endl;
        if (!valid[3]) cout << "Patient's last name not valid" << endl;
        if (!valid[4]) cout << "Disease ID not valid" << endl;
        if (!valid[5]) cout << "Age not valid" << endl;
        return false;
    }
    char* recordid = &((arg1)[0]);
    Entry *patientRecord = HTSearch(*patientRecords,recordid);  //if NULL recordid is ok

    if (strcmp(&(arg2[0]),"ENTER")==0){
        if (patientRecord!=NULL){
            errno = EINVAL;
            perror("ERROR"); //found duplicate recordid
            return false;
        }
        Patient *tmpPatient = new Patient;
        if (tmpPatient==NULL){
            cout << "Error, not enough memory" << endl;
            return false;
        }
        tmpPatient->recordID = arg1;
        tmpPatient->patientFirstName = arg3;
        tmpPatient->patientLastName = arg4;
        tmpPatient->disease = arg5;
        tmpPatient->country = country;
        tmpPatient->age = stoi(arg6);
        tmpPatient->entryDate = date;
        tmpPatient->exitDate = 0;
        *patientRecords = HTInsert(*patientRecords,&(arg1[0]),tmpPatient);

        char* diseaseid = &((tmpPatient->disease)[0]);
        Entry *disease = HTSearch(*diseaseHashtable,diseaseid);
        if (disease==NULL){                             //disease doesn't exist in hash table
            AVLTree tree = AVLCreate();
            AVLTree root = AVLInsert(tree,tmpPatient,tmpPatient->entryDate);
            *diseaseHashtable = HTInsert(*diseaseHashtable,diseaseid,root);
        }
        else           //disease already exists, add tree node in tree
            disease->item = AVLInsert((AVLTree)disease->item,tmpPatient,tmpPatient->entryDate);
        //fill countryHashtable
        char* countryid = &((tmpPatient->country)[0]);
        Entry *country_ = HTSearch(*countryHashtable,countryid);
        if (country_==NULL){
            AVLTree tree = AVLCreate();
            AVLTree root = AVLInsert(tree,tmpPatient,tmpPatient->entryDate);
            *countryHashtable = HTInsert(*countryHashtable,countryid,root);
        }
        else
            country_->item = AVLInsert((AVLTree)country_->item,tmpPatient,tmpPatient->entryDate);
    }
    else{   //search for patient and add EXIT date
       if (patientRecord==NULL){
            errno = EINVAL;
            perror("ERROR"); //no recordid with ENTER, invalid EXIT
            return false;
        }
        Patient *tempPatient = (Patient*)patientRecord->item;
        if (tempPatient->entryDate<=date){
            tempPatient->exitDate = date;
            return true;
        }
        else{
            errno = EINVAL;
            perror("ERROR"); //invalid EXIT
            return false;
        }
    }
    return true;
}

static void diseaseStats(HTHash* table,char* key, HTItem item, int min, int max, time_t date, void *stats){
    struct tree *treePtr = (struct tree *)item;
    int count = AVLCountRange(treePtr,min,max,date);
    std::string *str = static_cast<std::string *>(stats);
    *str += key;
    *str += " ";
    *str += to_string(count);
    *str += ",";
}

bool parseFile(struct file_date file, string country, HTHash **patientRecords, HTHash **diseaseHashtable, HTHash **countryHashtable ){
    ifstream myfile(file.file_str);
    if(!myfile.good()){
        perror("ERROR");
        return false;
    }
    string line;
    while (getline (myfile, line)) {
        parseLine(line, file.file_time, country, patientRecords, diseaseHashtable, countryHashtable);
    }
    myfile.close();
    return true;
}

string getCountryFromDir(const char *dirName){
    assert(dirName);
    int len = strlen(dirName);
    string country;
    for(int i=0; i<len; i++){
        if(dirName[i] == '/')
            country = "";
        else
            country += dirName[i];
    }
    return country;
}

static void initializeStatRanges(Vector<struct statsRanges*> &vec, string &line){
    if(line.length() == 0)
        return;
    unsigned int i = 0;
    auto *st = new statsRanges;
    while(i < line.length()){
        if(line[i] != ' ')
            st->disease += line[i];
        else{
            vec.pushBack(st);
            st = new statsRanges;
            while(line[i] != ',')
                i++;
        }
        i++;
    }
    delete st;
}

static void addStatRanges(Vector<struct statsRanges*> &vec, string &line, unsigned int range){
    unsigned int i = 0, countryCounter = 0;
    string num;
    while(i < line.length()){
        if(line[i] == ' '){
            i++;
            while(line[i] != ',')
                num += line[i++];
            vec.getAt(countryCounter)->ranges[range] = stoi(num);
            num.clear();
            countryCounter++;
        }
        i++;
    }
}

bool readDir(const char *dirName, HTHash **patientRecords, HTHash **diseaseHashtable, HTHash **countryHashtable, HTHash** statisticsHashtable){
    assert(dirName);
    DIR *dir;
    struct dirent *entry;
    string country = getCountryFromDir(dirName);
    if ((dir = opendir (dirName)) == NULL){
        perror("ERROR");
        return false;
    }
    Vector<struct file_date> *pFiles = new Vector<struct file_date>;
    while ((entry = readdir (dir)) != NULL ) {
        if(strcmp(entry->d_name,".") && strcmp(entry->d_name,"..")){
            struct file_date file;
            file.file_time = getDate(entry->d_name);
            if (file.file_time < 0){
                errno = EINVAL;
                perror("ERROR"); // invalid date format, dont pushback dir.
                continue;
            }
            char *filePath = (char *)malloc(strlen(dirName) + strlen(entry->d_name) + 2); // dirName + '/' + fileName + '\0'.
            strcpy(filePath, dirName);
            strcat(filePath, "/");
            strcat(filePath, entry->d_name);
            file.file_str = filePath;
            pFiles->pushBack(file);
        }
    }

    pFiles->sortVector(compare);

    SymTable_put(countryFiles,&country[0],(const void*)pFiles);
    int size = pFiles->getSize();
    for(int i=0; i<size; i++){
        parseFile(pFiles->getAt(i), country, patientRecords, diseaseHashtable, countryHashtable);
        //SUMMARY STATISTICS
        string ageRanges[4];

        struct file_date tempFile = pFiles->getAt(i);
        time_t date = tempFile.file_time;

        string statistics;

        char *strDate = getStringDate(date);
        statistics.append(strDate);
        statistics.push_back('\n');
        statistics.append(country);
        statistics.push_back('\n');



        //0-20, 21-40 ,41-60, 60-120(60+)
        HTVisitRange(*diseaseHashtable,diseaseStats,0,20,date,&ageRanges[0]);
        HTVisitRange(*diseaseHashtable,diseaseStats,21,40,date,&ageRanges[1]);
        HTVisitRange(*diseaseHashtable,diseaseStats,41,60,date,&ageRanges[2]);
        HTVisitRange(*diseaseHashtable,diseaseStats,60,120,date,&ageRanges[3]);

        Vector<struct statsRanges*> st_rng;
        initializeStatRanges(st_rng, ageRanges[0]);
        for(unsigned int i=0; i<4; i++){
            addStatRanges(st_rng, ageRanges[i], i);
        }
        for(int i=0; i < st_rng.getSize(); i++){
            statistics += st_rng.getAt(i)->disease;
            statistics += '\n';

            statistics += "Age range 0-20 years: ";
            statistics += to_string(st_rng.getAt(i)->ranges[0]);
            statistics += " cases\n";

            statistics += "Age range 21-40 years: ";
            statistics += to_string(st_rng.getAt(i)->ranges[1]);
            statistics += " cases\n";

            statistics += "Age range 41-60 years: ";
            statistics += to_string(st_rng.getAt(i)->ranges[2]);
            statistics += " cases\n";

            statistics += "Age range 60+ years: ";
            statistics += to_string(st_rng.getAt(i)->ranges[3]);
            statistics += " cases\n\n";

        }
        string temp = strDate;
        temp += " ";
        temp += country; 
        const char *data = strdup(statistics.c_str());

        *statisticsHashtable = HTInsert(*statisticsHashtable, (char *)temp.c_str(), (char *)data);
        free(strDate);

        for(int i=0; i < st_rng.getSize(); i++){
            delete(st_rng.getAt(i));
        }
        free(pFiles->getAt(i).file_str);
    }
    closedir(dir);
    return true;
}

//searchPatientRecord
static string searchPatientRecord(string recordID, HTHash **patientRecords){
    char* recordid = &((recordID)[0]);
    Entry *patientRecord = HTSearch(*patientRecords,recordid);
    if (patientRecord==NULL)
        return "";
    else{
        string result;
        Patient *tmpPatient = (Patient*)patientRecord->item;

        result += tmpPatient->recordID;
        result += " ";
        result += tmpPatient->patientFirstName;
        result += " ";
        result += tmpPatient->patientLastName;
        result += " ";
        result += tmpPatient->disease;
        result += " ";

        string age = to_string(tmpPatient->age);
        result += age;
        result += " ";
        string entrydate = getStringDate(tmpPatient->entryDate);
        result += entrydate;
        if (tmpPatient->exitDate==0)
            result += " --";
        else{
            result += " ";
            string exitdate = getStringDate(tmpPatient->exitDate);
            result += exitdate;
        }
       return result;
    }
}

static int diseaseStats(HTHash* table,char* key, HTItem item, time_t date1, time_t date2){
    struct tree *treePtr = (struct tree *)item;
    return AVLCountRange2(treePtr,date1,date2);
}

static int diseaseStatsCountry(HTHash* table,char* key, HTItem item, time_t date1, time_t date2, char* country){
    struct tree *treePtr = (struct tree *)item;
    return AVLCountRangeKey(treePtr,date1,date2,country);
}

static int diseaseStatsCountry2(HTHash* table,char* key, HTItem item, time_t date1, time_t date2, char* country){
    struct tree *treePtr = (struct tree *)item;
    return AVLCountRangeKey2(treePtr,date1,date2,country);
}

//diseaseFrequency virusName date1 date2 [country]
static int diseaseFrequency(std::string arg, HTHash** diseaseHash, HTHash** countryHash){
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
        }
        else
            argSum++;
        i++;
    }

    char* diseaseid = &((arg1)[0]);
    Entry *disease = HTSearch(*diseaseHash,diseaseid);
    time_t date1 = getDate(arg2), date2 = getDate(arg3);

    if (disease==NULL)      //This worker does not have cases for this disease
        return 0;
    if (!arg4.empty()){
        char* countryid = &((arg4)[0]);
        Entry *country_ = HTSearch(*countryHash,countryid);
        if (country_==NULL){
            cout << "Country does not exist!" << endl;
            return -1;
        }
        return diseaseStatsCountry(*diseaseHash,diseaseid,disease->item,date1,date2,countryid);
    }
    return diseaseStats(*diseaseHash,diseaseid,disease->item,date1,date2);
}

static void countCountry2(HTHash* table,char* key, HTItem item, time_t date1, time_t date2,char *disease, void *result){
    struct tree *treePtr = (struct tree *)item;
    int count = AVLCountCountry2(treePtr,date1,date2,disease);
    std::string *str = static_cast<std::string *>(result);          //void* cast to string
    *str += key;
    *str += " ";
    *str += to_string(count);
    *str += '\n';
}

//numPatientAdmissions disease date1 date2 [country]
static string numPatientAdmissions(std::string arg, HTHash** diseaseHash, HTHash** countryHash){
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
        }
        else
            argSum++;
        i++;
    }

    char* diseaseid = &((arg1)[0]);
    Entry *disease = HTSearch(*diseaseHash,diseaseid);
    time_t date1 = getDate(arg2), date2 = getDate(arg3);

    if (disease==NULL)      //This country does not have entries for this disease
        return "NULL_VIRUS";

    if (!arg4.empty()){
        char* countryid = &((arg4)[0]);
        Entry *country_ = HTSearch(*countryHash,countryid);
        if (country_==NULL){
            cout << "Country does not exist!" << endl;
            return "";
        }
        return to_string(diseaseStatsCountry(*diseaseHash,diseaseid,disease->item,date1,date2,countryid));
    }
    string result;
    HTVisitRange2(*countryHash,countCountry2,date1,date2,&result,diseaseid);
    return result;
}

static void countCountry(HTHash* table,char* key, HTItem item, time_t date1, time_t date2,char *disease, void *result){
    struct tree *treePtr = (struct tree *)item;
    int count = AVLCountCountry(treePtr,date1,date2,disease);
    std::string *str = static_cast<std::string *>(result);
    *str += key;
    *str += " ";
    *str += to_string(count);
    *str += '\n';
}

//numPatientDischarges disease date1 date2 [country]
static string numPatientDischarges(std::string arg, HTHash** diseaseHash, HTHash** countryHash){
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
        }
        else
            argSum++;
        i++;
    }

    char* diseaseid = &((arg1)[0]);
    Entry *disease = HTSearch(*diseaseHash,diseaseid);
    time_t date1 = getDate(arg2), date2 = getDate(arg3);

    if (disease==NULL)      //This country does not have entries for this disease
        return "";

    if (!arg4.empty()){
        char* countryid = &((arg4)[0]);
        Entry *country_ = HTSearch(*countryHash,countryid);
        if (country_==NULL){
            cout << "Country does not exist!" << endl;
            return "";
        }
        return to_string(diseaseStatsCountry2(*diseaseHash,diseaseid,disease->item,date1,date2,countryid));
    }
    string result;
    HTVisitRange2(*countryHash,countCountry,date1,date2,&result,diseaseid);
    return result;
}

static int topk(HTItem item, int min, int max, time_t date1, time_t date2, char *disease){
    struct tree *treePtr = (struct tree *)item;
    return AVLCountRange3(treePtr,min,max,date1,date2,disease);
}

struct range{
    const char * name;
    int range;
};

int range_cmp(const void * a, const void * b){
    return ((struct range*)b)->range - ((struct range*)a)->range;
}

 //topk-AgeRanges k country disease date1 date2
static string topkAgeRanges(std::string arg, HTHash** diseaseHash, HTHash** countryHash){
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
        }
        else
            argSum++;
        i++;
    }

    Entry *country_ = HTSearch(*countryHash,&(arg2[0]));
    if (country_==NULL){
        cout << "Country does not exist!" << endl;
        return "";
    }
    Entry *disease = HTSearch(*diseaseHash,&(arg3[0]));
    if (disease==NULL){
        cout << "Disease does not exist!" << endl;
        return "";
    }
    time_t date1 = getDate(arg4), date2 = getDate(arg5);
    struct range ranges[4];
    int total = 0;
    ranges[0].name = "0-20";
    ranges[0].range = topk(country_->item,0,20,date1,date2,&(arg3[0]));
    total += ranges[0].range;
    ranges[1].name = "21-40";
    ranges[1].range = topk(country_->item,21,40,date1,date2,&(arg3[0]));
    total += ranges[1].range;
    ranges[2].name = "41-60";
    ranges[2].range = topk(country_->item,41,60,date1,date2,&(arg3[0]));
    total += ranges[2].range;
    ranges[3].name = "60+";
    ranges[3].range = topk(country_->item,61,120,date1,date2,&(arg3[0]));
    total += ranges[3].range;

    qsort(ranges, 4, sizeof(struct range), range_cmp);
    int k = stoi(arg1);
    string retStr;
    for(int i=0; i<4 && i<k; i++){
        retStr += ranges[i].name;
        retStr += ": ";
        retStr += to_string((ranges[i].range * 100) / total);
        retStr += "%\n";
    }
    return retStr;
}

static void destroyHTtree(HTHash* table, HTItem item){
    AVLDestroy((AVLTree)item);
}

static void deletePatients(HTHash* table, HTItem item){
    delete (Patient*)item;
}

void skoupa(HTHash** patientRecords, HTHash** diseaseHashtable, HTHash** countryHashtable, HTHash** statisticsHashtable){
    static HTHash** recordHash = patientRecords;
    static HTHash** disHash = diseaseHashtable;
    static HTHash** countryHash = countryHashtable;
    static HTHash** statsHash = statisticsHashtable;
    static bool alreadySet = false;
    if(alreadySet){
        HTEdit(*recordHash,deletePatients);
        HTDestroy(*recordHash);
        HTEdit(*disHash,destroyHTtree);
        HTDestroy(*disHash);
        HTEdit(*countryHash,destroyHTtree);
        HTDestroy(*countryHash);
        HTDestroy(*statsHash);
        int countriesLen = countries.getSize();
        for (int i=0; i<countriesLen; i++){
            char* tmp =  countries.getAt(i);
            delete (Vector<struct file_date> *)SymTable_get(countryFiles, tmp);
            free(countries.getAt(i));
        }
        close(readfd);
        close(writefd);
    }
    alreadySet = true;
}

void end(){
    skoupa(NULL, NULL, NULL, NULL);
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
        cout << "PORT WAS " << port << endl;
        exit(EXIT_FAILURE);
    }
    if (listen(sock, 10) < 0){
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
}


void sendStats(HTHash* table,char* key, HTItem item, int socket, int x, time_t y, void *z){
    const char *stats = (const char *)item;
    writeMessage(stats, SOCKET_BUFFERSIZE, socket);
}

int main(int argc, char **argv){
    signal(SIGINT, signal_handler);
    signal(SIGQUIT, signal_handler);
    signal(SIGUSR1, signal_handler);

    assert(argv[1]);
    assert(argv[2]);
    assert(argv[3]);
    assert(argv[4]);
    const char *fifo_read = argv[1], *fifo_write = argv[2], *input_dir = argv[4];
    bufferSize = atoi(argv[3]);
    int queryPort = atoi(argv[5]), workerNumber = atoi(argv[6]);
    if(bufferSize <= 0){
        perror("ERROR, bufferSize <= 0");
    }
    if ( (readfd = open(fifo_read, O_RDONLY)) < 0)
        perror("worker: can't open read fifo");
    if ( (writefd = open(fifo_write, O_WRONLY)) < 0)
        perror("worker: can't open write fifo");

    countryFiles = SymTable_new();
    HTHash *patientRecords = HTCreate(9,100);
    HTHash *diseaseHashtable = HTCreate(9,100);
    HTHash *countryHashtable = HTCreate(9,100);
    HTHash *statisticsHashtable = HTCreate(9,100);
    skoupa(&patientRecords, &diseaseHashtable, &countryHashtable, &statisticsHashtable);
    atexit(end);
//=============================================================================================================
    int counter = 0, serverPort;
    string country, serverip, message, countryPath;

    message = getMessage(bufferSize, readfd);
    if (strcmp(&message[0], "COUNTRIES") == 0){
        while(1){
            country = getMessage(bufferSize, readfd);
            if(strcmp(&country[0], "C_DONE") == 0) // DONE WITH COUNTRIES! SO BREAK LOOP.
                break;
            char *tmp = strdup(&country[0]);
            countries.pushBack(tmp);
            countryPath = input_dir;
            countryPath += '/';
            countryPath += country;
            if (!readDir(&countryPath[0], &patientRecords, &diseaseHashtable, &countryHashtable, &statisticsHashtable)){
                cout << "Could not read directory: " << countryPath << endl;
            }
        }
    }

    if (countries.getSize() == 0){      //worker has nothing to do - no countries - wait till SIGKILL from master
        pause();
    }

    int numWorkers = 0;
    while(1){
        message = getMessage(bufferSize, readfd);
        if(strcmp(&message[0], "IP") == 0){
            serverip = getMessage(bufferSize, readfd);
            counter++;
        }
        else if(strcmp(&message[0], "PORT") == 0){
            message = getMessage(bufferSize, readfd);
            serverPort = stoi(message);
            counter++;
        }
        else if (strcmp(&message[0], "EXTRA_INFO") == 0){       //number of workers
            message = getMessage(bufferSize, readfd);
            numWorkers = stoi(message);
        }
        if (counter == 2)
            break;
    }

    int sock;

    struct sockaddr_in server;
    struct sockaddr *serverPtr = (struct sockaddr *)&server;

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0){
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    server.sin_family = AF_INET;
    server.sin_port = htons(serverPort);
    //server.sin_addr.s_addr = inet_addr(&serverip[0]);
    struct hostent * rem ;
    rem = gethostbyname(&serverip[0]);

    memcpy(&server.sin_addr, rem->h_addr, rem->h_length);

    if (connect(sock, serverPtr, sizeof(server)) < 0){
        perror("Connect failed");
        exit(EXIT_FAILURE);
    }

    string tmp1 = to_string(numWorkers);
    const char *msg1 = strdup(tmp1.c_str());

    writeMessage("NUM_WORKERS", SOCKET_BUFFERSIZE, sock);
    writeMessage(msg1, SOCKET_BUFFERSIZE, sock);

    string tmp2 = to_string(workerNumber);
    const char *msg2 = strdup(tmp2.c_str());

    writeMessage("WORKER_NUM", SOCKET_BUFFERSIZE, sock);
    writeMessage(msg2, SOCKET_BUFFERSIZE, sock);

    int countriesLen = countries.getSize();
    string countriesStrs = "";
    for (int i=0; i<countriesLen; i++){
        char* tmp =  countries.getAt(i);
        countriesStrs += tmp;
        countriesStrs += " ";
    }

    writeMessage("COUNTRIES", SOCKET_BUFFERSIZE, sock);
    writeMessage(&countriesStrs[0], SOCKET_BUFFERSIZE, sock);
    writeMessage("STATISTICS", SOCKET_BUFFERSIZE, sock);
    HTVisitRange(statisticsHashtable, sendStats, sock, 0, 0, NULL);  //send statistics
    writeMessage("S_DONE", SOCKET_BUFFERSIZE, sock);


    writeMessage("PORT", SOCKET_BUFFERSIZE, sock);
    queryPort++;
    string tmp = to_string(queryPort);
    const char *msg = strdup(tmp.c_str());
    writeMessage(msg, SOCKET_BUFFERSIZE, sock);

    close(sock);

    int whoServerSock, whoServerNewSock;
    socklen_t whoServerLen;
    struct sockaddr_in whoServerServer, whoServerClient;
    struct sockaddr *whoServerServerPtr = (struct sockaddr *)&whoServerServer;
    struct sockaddr *whoServerClientPtr = (struct sockaddr *)&whoServerClient;
    whoServerLen = sizeof(whoServerClient);

    createPassiveEndpoint(whoServerSock, queryPort, &whoServerServer, &whoServerClient, whoServerServerPtr, whoServerClientPtr, whoServerLen);

    while(1){
        if ((whoServerNewSock = accept(whoServerSock, whoServerClientPtr, &whoServerLen)) < 0){
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        message = getMessage(SOCKET_BUFFERSIZE, whoServerNewSock);
        cout << "Worker with pid "<< getpid() << " got request: " << message << endl;

        unsigned int len = message.length(), i = 0;
        string args = "", com = "";
        while(i < len && message[i] != ' '){      //get command
            com += message[i];
            i++;
        }
        i++;                                    // skip space
        while(i < len){                         //get arguments
            args += message[i];
            i++;
        }
        string ans;


        if(strcmp(&com[0], "/diseaseFrequency") == 0){
            int count = diseaseFrequency(args, &diseaseHashtable, &countryHashtable);
            ans = to_string(count);
            if (count==-1)
                writeMessage("FAIL", SOCKET_BUFFERSIZE, whoServerNewSock);
            else
                writeMessage(&ans[0], SOCKET_BUFFERSIZE, whoServerNewSock);

        }
        else if(strcmp(&com[0], "/searchPatientRecord") == 0){
            string result = searchPatientRecord(args, &patientRecords);
            if (result.empty())
                writeMessage("FAIL", SOCKET_BUFFERSIZE, whoServerNewSock);
            else
                writeMessage(&result[0], SOCKET_BUFFERSIZE, whoServerNewSock);
        }
        else if(strcmp(&com[0], "/topkAgeRanges") == 0){
            string result = topkAgeRanges(args, &diseaseHashtable, &countryHashtable);
            if (result.empty())
                writeMessage("FAIL", SOCKET_BUFFERSIZE, whoServerNewSock);
            else
                writeMessage(&result[0], SOCKET_BUFFERSIZE, whoServerNewSock);
        }
        else if(strcmp(&com[0], "/numPatientAdmissions") == 0){
            string result = numPatientAdmissions(args, &diseaseHashtable, &countryHashtable);
            if (result.empty())
                writeMessage("FAIL", SOCKET_BUFFERSIZE, whoServerNewSock);

            else
                writeMessage(&result[0], SOCKET_BUFFERSIZE, whoServerNewSock);
        }
        else if(strcmp(&com[0], "/numPatientDischarges") == 0){
            string result = numPatientDischarges(args, &diseaseHashtable, &countryHashtable);
            if (result.empty())
                writeMessage("FAIL", SOCKET_BUFFERSIZE, whoServerNewSock);
            else
                writeMessage(&result[0], SOCKET_BUFFERSIZE, whoServerNewSock);
        }
    }

    exit(0); // dont ever return from main. only use exit(...). Cause can't free at skoupa from deleted pointer
}
