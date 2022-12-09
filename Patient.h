#include <string>

typedef struct patient{
    std::string recordID;
    std::string patientFirstName;
    std::string patientLastName;
    std::string disease;
    std::string country;
    int age;
    time_t entryDate;
    time_t exitDate;    //0 if exitDate doesn't exist
} Patient;

