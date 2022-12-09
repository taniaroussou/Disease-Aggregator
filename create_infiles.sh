#! /bin/bash

#./create_infiles.sh diseaseFile countriesFile input_dir numFilesPerDirectory
#numRecordsPerFile

#echo "diseaseFile is $1, countriesFile is $2, input_dir is $3, numFilesPerDirectory is $4, numRecordsPerFile is $5"
DISEASE=$1
COUNTRIES=$2
INPUT_DIR=$3
DIRFILES=$4
FILERECORDS=$5
FLAG=0
# -e file   True if the file exists (note that this is not particularly portable, thus -f is generally used)
# -f file   True if the provided string is a file
# -r file   True if the file is readable
# -s file   True if the file has a non-zero size

function parse(){
    if [ -e "$DISEASE" ] && [ -f "$DISEASE" ] && [ -r "$DISEASE" ] && [ -s "$DISEASE" ]
    then 
        ((FLAG++))
        DISEASECOUNTER=0
       
        while read -r CURRENTLINE
        do
            diseases[$DISEASECOUNTER]=$CURRENTLINE
            ((DISEASECOUNTER++))
           
        done < "./$DISEASE"
        ((DISEASECOUNTER--))
       
    else
        echo "diseaseFile not valid"
    fi
        if [ -e "$COUNTRIES" ] && [ -f "$COUNTRIES" ] && [ -r "$COUNTRIES" ] && [ -s "$COUNTRIES" ]
    then 
        ((FLAG++))
    else
        echo "countriesFile not valid"
    fi

    re='^[0-9]+$'
    if ! [[ $DIRFILES =~ $re ]] ; then
    echo "error: numFilesPerDirectory not a number" >&2; 
    else 
        ((FLAG++))
    fi

    re='^[0-9]+$'
    if ! [[ $FILERECORDS =~ $re ]] ; then
    echo "error: numRecordsPerFile not a number" >&2; 
    else 
        ((FLAG++))
    fi
    if [ $FLAG -ne 4 ]
    then
        exit 1
    fi

}

function createName() {
    MAX=12                     
    MIN=3 
    DIFF=$((MAX-MIN+1))
    LENGTH=$(($(($RANDOM%$DIFF))+MIN))

    chars=abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ
    NAME='';
    SURNAME='';
    for (( n=0; n<LENGTH; n++));
    do
        NAME+="${chars:RANDOM%${#chars}:1}"
        SURNAME+="${chars:RANDOM%${#chars}:1}"
    done
}



function randDate(){
    MAX=2020
    MIN=2000
    DIFF="$(($MAX-$MIN))"
    DIFF="$(($DIFF*365))"
    RRR="$(($DIFF/$DIRFILES))"
    if [ $(($DIFF%$DIRFILES)) -ne 0 ] 
    then
    ((RRR++))
    fi
    index=0
    for (( d=0; d<DIFF; d=d+RRR )) 
    do
    DAY=$(($(($RANDOM%$RRR))+$d));
    dates[$index]=$(date -d "2000-01-01 + $DAY days" +'%d-%m-%Y.txt')
    ((index++))
    done
}


functionRetVal=0

function findFileWithSpaceLeft(){
    i=$(($RANDOM % $DIRFILES));
    while [ ${spacesLeft[$i]} -eq 0 ]
    do
        
        ((i++))
        if [ $i -eq $DIRFILES ]
        then    
            i=0
        fi
    done
    functionRetVal=$i;
}

function printEntry(){ #arg1 = filepath, arg2 = 0 for enter / 1 for exit, arg3 = RECORDID, arg4 = NAME, arg5 = SURNAME, arg6 = disease, arg7 = age
    # createName
    # dis=$(($RANDOM % $DISEASECOUNTER))
    # age=$(($RANDOM % 120 +1))
    if [ $2 -eq 0 ]
    then
        echo "$3 ENTER $4 $5 $6 $7" >> $1
    else
        echo "$3 EXIT $4 $5 $6 $7" >> $1
    fi
}

parse


LINE=1
RECORDID=0
while read -r CURRENT_LINE
    do
        mkdir -p $INPUT_DIR/$CURRENT_LINE

        randDate
        for (( k=0; k<DIRFILES; k++ ))
        do
            touch $INPUT_DIR/$CURRENT_LINE/${dates[$k]}
        done
        
        amountOfEntries="$(($DIRFILES*$FILERECORDS))"
        
        for (( l=0; l<$DIRFILES; l++))
        do
            spacesLeft[$l]=$FILERECORDS;
        done
        
        for (( j=0; j<2*amountOfEntries/3; j = j+2))
        do
            findFileWithSpaceLeft
            entry1=$functionRetVal
            ((spacesLeft[$entry1]--))
            findFileWithSpaceLeft
            entry2=$functionRetVal
            ((spacesLeft[$entry2]--))
            if [ $entry1 -lt $entry2 ]
            then
                enter=$entry1
                exit=$entry2
            else
                enter=$entry2;
                exit=$entry1;
            fi
            createName
            dis=$(($RANDOM % $DISEASECOUNTER))
            age=$(($RANDOM % 120 +1))


            #arg0 = filepath, arg1 = 0 for enter / 1 for exit, arg2 = RECORDID
            printEntry "$INPUT_DIR/$CURRENT_LINE/${dates[$enter]}" "0" "$RECORDID" "$NAME" "$SURNAME" "${diseases[$dis]}" "$age"
            # write enter to file pathsToFiles[enter];
            printEntry "$INPUT_DIR/$CURRENT_LINE/${dates[$exit]}" "1" "$RECORDID" "$NAME" "$SURNAME" "${diseases[$dis]}" "$age"
            # write exit to file pathsToFiles[exit];
            ((RECORDID++))
        done

        for(( ; j<amountOfEntries; j++))
        do
            findFileWithSpaceLeft
            entry1=$functionRetVal
            ((spacesLeft[$entry1]--))
            createName
            dis=$(($RANDOM % $DISEASECOUNTER))
            age=$(($RANDOM % 120 +1))
            printEntry "$INPUT_DIR/$CURRENT_LINE/${dates[$entry1]}" "0" "$RECORDID" "$NAME" "$SURNAME" "${diseases[$dis]}" "$age"
            # write enter to file pathsToFiles[enter];
            ((RECORDID++))
        done

done < "./$COUNTRIES"


