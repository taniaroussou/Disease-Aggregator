# Disease-Aggregator
A distributed process system that records and responds to disease queries over TCP.

This project is part of an assignment under the subject Systems Programming.

# Introduction
This projects covers the concepts of multi-threaded programming, network communication using the TCP/IP protocol, creating processes, using system calls such as fork/exec, process communication through pipes, low level I/O and bash scripting.

# Program structure

![Screenshot 2022-12-09 202813](https://user-images.githubusercontent.com/57332815/206768508-d10014b5-7a0e-4466-b28a-681f8c908895.png)

# Data structure

![hw1-spring-2020](https://user-images.githubusercontent.com/57332815/206790449-6ea09016-9713-4b1c-975c-d91a61a58298.jpg)

## Master

The master program (parent process) creates a series of *Worker* processes, each one responsible for reading data from the input files given, filling up the respective data structures as well as answering to queries forwarded by the server (whoServer).
The master begins by creating *numWorkers* Workers child processes and distributes equally all the subdirectories inside *input_dir* to the Workers. Each Worker is informed about the name of the subdirectories through a named pipe. The parent process also sends the IP address and port number of the server. Once the creation of the Worker processes is done, the parent process keeps running in order to fork a new Worker process in case an existing Worker exits.

A Worker process connects to the server and sends back the following information:
- a port number that a Worker process will be listening for queries forwarded by the server
- summary statistics

Once a Worker is done passing information to the server, it listens on the chosen port number and waits for connections made from the server regarding the assigned countries.

### Usage
```
./master –w numWorkers -b bufferSize –s serverIP –p serverPort -i input_dir
```
Parameters
- numWorkers: Number of Worker processes created  
- bufferSize: Buffer size for read operations over pipes
- serverIP: IP address of server to which the Workers are connected in order to send summary statistics
- serverPort: Server's listening port
- input_dir: Directory that contains subdirectories named after a country name. Each subdirectory contains files that hold patients records


## Server

As soon as the whoServer starts running, the main thread creates *numThreads* threads. The main process listens to ports *queryPortNum* and *statisticsPortNum*, accepts connections through system call accept() and places file/socket descriptors that belong to these connections in a circular buffer of size equal to *bufferSize*. The main thread does not read from the incoming connections, it just places the file descriptors returned from accept() to the buffer and keeps on listening for future connections. The main task of the *numThreads* threads is to serve the incoming connections whose fds exists in the buffer. Each of these threads wakes up when there is at least one fd inside the buffer.

The main thread listens to *statisticsPortNum* for connections coming from Worker processes in order to receive the summary statistics and the port number each Worker is listening on. Also, it listens on *queryPortNum* for connections coming from whoClient in order to receive disease queries that have been recorded in the distributed process system.

### Usage
```
./whoServer –q queryPortNum -s statisticsPortNum –w numThreads –b bufferSize
```
Parameters
- queryPortNum: Server's listening port for connections coming from whoClient (query related)
- statisticsPortNum: Server's listening port for connections coming from Worker processes (summary statistics related)
- numThreads: Number of threads created from whoServer that will serve network incoming connections. Threads are created only once at the start of the server
- bufferSize: The size of a circular buffer that is being shared between the threades created by the whoServer process. The *bufferSize* represents the number of file/socket descriptors that can be stored inside the buffer.

The server accepts and serves the following requests made by the whoClient process:
- ```/diseaseFrequency virusName date1 date2 [country]```
If no *country* argument is given, the server finds the number of cases for the disease *virusName* that have been recorded in the system for the interval *[date1...date2]*. If the *country* argument is given, the server finds the number of cases for the disease *virusName* that have been recorded for the country *country* for the interval *[date1...date2]*. The arguments *date 1* and *date2* have the format DD-MM-YYYY. The *country* argument is optional.
- ```/topk-AgeRanges k country disease date1 date2```
The server finds the top k age groups of country *country* and disease *disease* that have appeared to record  cases. The arguments *date 1* and *date2* have the format DD-MM-YYYY.
- ```/searchPatientRecord recordID```
The server forwards the request to all Worker processes and waits for a response from the Worker with record *recordID*.
- ```/numPatientAdmissions disease date1 date2 [country]```
If the *country* argument is given, then the server has to forward the request to all the Workers in order to find the total number of patients admitted to the hospital with disease *disease* in country *country* through the period of *[date1...date2]*. If no *country* argument is given, then the server finds the total number of patients admitted to the hospital with disease *disease* through the period of *[date1...date2]*. The arguments *date 1* and *date2* have the format DD-MM-YYYY.
- ```/numPatientDischarges disease date1 date2 [country]```
If the *country* argument is given, then the server finds the total number of patients with disease *disease* that have been discharged in country *country* through the period of *[date1...date2]*. If no *country* argument is given, then the server finds the total number of patients with disease *disease* that have been discharged through the period of *[date1...date2]*. The arguments *date 1* and *date2* have the format DD-MM-YYYY.

When the server accepts a request, it forwards it to the respective Worker processes through a socket and awaits for a response. This request along with the responses the server receives from a Worker are printed to stdout. Lastly, the server also forwards the response to the client thread that made the request.

## Client

The client is a multithreaded program that is responsible for opening the *queryFile* and reading it line by line. Each line has a request that the server can accept. For each request the client creates a thread that sends the request to the server. The thread does not connect immidiately to the server after its creation. Once all the threads are created, they all try to connect the server at the same time and send their request. Once the request is sent, each thread prints the response it received from the server to stdout and then can exit. Once all the threads have exited the client can exit too.

### Usage
In whoClient folder:
```
./whoClient –q queryFile -numThreads –sp servPort –sip servIP
```
Parameters
- queryFile: The file containing the queries to be sent to the server
- numThreads: The number of threads created by the client which will be responsible of sending the requests to the server
- servPort: The port number the server listens on, to which the client is connected
- servIP: The IP address of the server to which the client is connected

## Script

``` ./create_infiles.sh diseases.txt countries.txt INPUT 5 5 ```

## Compile
In Disease-Aggregator folder:

```make```

## Clean
In Disease-Aggregator, whoCLient and whoServer folder:

```make clean```
