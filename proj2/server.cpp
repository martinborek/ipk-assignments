/**
  * File:    server.cpp
  * Date:    2014/05/04
  * Author:  Martin Borek, xborek08@stud.fit.vutbr.cz
  * Project: Simple server providing files with limited bandwidth.
  *          IPP project 2, FIT VUTBR
  */

#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fstream>
#include <cstdlib>
#include <sys/types.h>
#include <sys/socket.h>
#include <cerrno>
#include <signal.h>
#include <sys/wait.h>
#include <cstdio>
#include <sys/time.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define BUFFSIZE 1000

using namespace std;

/** Error values */
enum {
  EOK = 0, // No error detected
  EPARAMNUM, // Wrong number of parameters 
  EPARAM, // Wrong parameter
  ERECV, // RECV 
  ESEND, // SEND
  ECONNECTION,
  EHOST,
  ESIGACTION,
  EFORK,
  EFILE,
  EREAD,
  EPROTOCOL,
  EUNKNOWN // Unknown error
};

/** Error messages */
const char *ECODEMSG[] = {
  "Everything is OK.",
  "Wrong number of parameters",
  "Wrong parameter",
  "Failed to receive a message",
  "Failed to send a message",
  "Failed to connect to server",
  "Host is not available",
  "Sigaction error",
  "Fork error",
  "Requested file could not be opened",
  "Received message does not match the protocol",
  "Expecting different protocol code",
  "Unknown error"
};

/**
 * Prints error messages according to given error code and exits;
 * @param ecode Error code
 */
void error_exit(int eCode){
  if (eCode == EOK)
    return;

  if (eCode < EOK || eCode > EUNKNOWN)
    eCode = EUNKNOWN;

  cerr << ECODEMSG[eCode] << endl;
  exit(eCode);
}


/**
 * Class for holding data from given parameters
 */
class Params{
  public:
    Params(int argc, char *argv[]);
    string port;
    //time for sending 1 packet = 1000B (~1kB) in microseconds
    unsigned long sending_time; 
  private:
    int get_positive_number(const string &str);
};

/** Converts string to unsigned int */
int Params::get_positive_number(const string &str) {
  if (str.empty())
    return 0;

  int number = 0;
  string::const_iterator i;
  for (i = str.begin(); i != str.end(); i++){
    if (!isdigit(*i))
      return false;
    number *= 10;
    number += *i - '0';
    if (number == 0)
      return 0;
  }
  return number;
}

/**
 * Processes given parameters.
 * @param argc Number of program parameters
 * @param argv Parameters
 */
Params::Params(int argc, char *argv[]){
  unsigned long bandwidth;
  if (argc != 5) // -p "port" -d "bandwidth"
    error_exit(EPARAMNUM);
  
  if (strcmp(argv[1], "-p") == 0) {
    port = argv[2];
    if (get_positive_number(port) != 0 && strcmp(argv[3], "-d") == 0) {
      if ((bandwidth = get_positive_number(argv[4])) != 0)
        sending_time = 1000000.0 / bandwidth;
        return;
    }
  }else if (strcmp(argv[1], "-d") == 0){
    bandwidth = get_positive_number(argv[2]);
    if (bandwidth != 0 && strcmp(argv[3], "-p") == 0) {
      port = argv[4];
      if (get_positive_number(port) != 0)
        sending_time = 1000000.0 / bandwidth; 
        return;
    }
  }
  error_exit(EPARAM);
}

void sigcatcher(int n){
   wait3(NULL, WNOHANG, NULL);
}

/** Send file to a client.*/
int send_file(int newfd, unsigned long sending_time){

  char buffer[BUFFSIZE + 1];

  string recv_msg = "";
  while (recv_msg.length() < 2 ||
          strcmp(recv_msg.substr(recv_msg.length() - 2).c_str(), ";\n")){
    memset(&buffer, 0, sizeof(buffer)); // make the buffer empty
    if (recv(newfd, buffer, BUFFSIZE, 0) == -1)
      return ERECV;
    recv_msg += buffer;
  } 
  
  FILE * file;
  if ((file = fopen(recv_msg.substr(0, recv_msg.length() - 2).c_str(), "rb")) == NULL){
    // Could not open requested file
    string send_msg = "9"; 
    if (send(newfd, send_msg.c_str(), send_msg.length(), 0) == -1){
      return ESEND;
    }
    return EFILE;

  }else{
    fseek(file, 0, SEEK_END);
    long file_len = ftell(file);
    rewind(file);
    long read_total = 0;
    int read = 0;
    char file_buffer[BUFFSIZE+1];
    bool last = false;
    string send_msg;
    string recv_msg;
    double start;
    double end;
    double sec;
    double usec;
    struct timeval tp;
    bool time_set = false;
    int usleep_for;
    while (!last){
      memset(&file_buffer, 0, sizeof(file_buffer)); // make the send buffer empty

      read = fread(file_buffer, sizeof(char), BUFFSIZE - 1, file);
      read_total += read;
      if (read_total == file_len && read < BUFFSIZE - 4){ // last packet
      //Last packet contains 4 additional characters at the beginning.
      //!! must be changed if BUFFSIZE is different from 1000
      //First character - 7, followed by number of data(file) bytes in the last block.
      //That number cannot be more than 996. (e.g code for 52 bytes: 7052)
        send_msg = "7";
        if (read < 100){
          send_msg += "0";
          if (read < 10){
            send_msg += "0";
          }
        }
        // Convert integer to char.
        char read_str[4] = "";
        snprintf(read_str, 4, "%d", read);
        send_msg += read_str; // IMPORTANT - gets only 3 chars from read_str,
                              // because last is \0
        last = true;

      }else if (read == BUFFSIZE - 1){ // regular packet, is not last
        send_msg = "8";
      }else{
        fclose(file);
        return EREAD;
      }
      string a (file_buffer, read);

      send_msg += a;
      
      // Setting bandwidth
      if (time_set){
        gettimeofday(&tp, NULL);
        sec = static_cast<double>(tp.tv_sec);
        usec = static_cast<double>(tp.tv_usec)/1E6;
        end = sec + usec;
        if ((usleep_for = sending_time - ((end-start)*1000000)) > 0)
          usleep(usleep_for);
      }
      gettimeofday(&tp, NULL);
      sec = static_cast<double>(tp.tv_sec);
      usec = static_cast<double>(tp.tv_usec)/1E6;
      start = sec + usec;
      time_set = true;

      if (send(newfd, send_msg.c_str(), send_msg.length(), 0) == -1){
        fclose(file);
        return ESEND;
      }
      recv_msg = "";
      memset(&buffer, 0, sizeof(buffer)); // make the receive buffer empty
      if (recv(newfd, buffer, BUFFSIZE, 0) == -1){
        fclose(file);
        return ERECV;
      }
      recv_msg += buffer;
      if (last){
        if (strcmp(recv_msg.substr().c_str(), "2")){
        // client has not received all the file
          fclose(file);
          return EPROTOCOL;
        }
        break;
      }
      if (strcmp(recv_msg.substr().c_str(), "1")){
        // expecting 1, but code is different
        fclose(file);
        return EPROTOCOL;
      }
    }
  }
  fclose(file);
  return EOK;
}

/** Connects to client, forks and calls file sending function (send_file()) */
int connect(Params params){
  int socketfd;
  struct addrinfo setting;
  struct addrinfo *list;
  struct addrinfo *ptr;

  memset(&setting, 0, sizeof setting); // make the struct empty
  setting.ai_family = AF_INET;
  setting.ai_socktype = SOCK_STREAM; setting.ai_flags = AI_PASSIVE;
  if (getaddrinfo(NULL, params.port.c_str(), &setting, &list) != 0)
    return EHOST;

  for (ptr = list; ptr != NULL; ptr = ptr->ai_next){
    if ((socketfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) == -1)
      continue;

    if (bind(socketfd, ptr->ai_addr, ptr->ai_addrlen) == -1){
      close(socketfd);
      continue;
    }
    break;
  }

  if (ptr == NULL) {
    close(socketfd);
    return ECONNECTION;
  }

  freeaddrinfo(list);

  if (listen(socketfd, 10) == -1){ // 10 - max number of pending connections
    close(socketfd);
    return ECONNECTION;
  }

  int newfd;
  struct sockaddr_storage cl_addr;
  socklen_t cl_addr_size;
  signal(SIGCHLD, sigcatcher);
  while (1){
    cl_addr_size = sizeof(cl_addr);
    if ((newfd = accept(socketfd, (struct sockaddr*)&cl_addr, &cl_addr_size)) == -1){
      continue; 
    }

    int pid = fork(); // for each accepted connection create a child
    if (pid < 0){
      close(newfd);
      //kill(0, SIGTERM);
      return EFORK;
    }

    if (pid == 0){ // child
      close(socketfd); // no need to have for a child

      int stat;
      if ((stat = send_file(newfd, params.sending_time)) != EOK){
        close(newfd);
        return stat;
      }else{
        close(newfd);
        exit(EXIT_SUCCESS);
      }

    } else{
      close(newfd); // parent doesn't need it
    }
  }

  return EOK;
}
//////// MAIN PROGRAM ////////
int main (int argc, char *argv[]) {
  
  int stat = EOK;
  Params params(argc, argv);

  if ((stat = connect(params)) != EOK)
    error_exit(stat);

  return EXIT_SUCCESS;
}
