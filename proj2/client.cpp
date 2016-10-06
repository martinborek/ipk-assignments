/**
  * File:    client.cpp
  * Date:    2014/05/04
  * Author:  Martin Borek, xborek08@stud.fit.vutbr.cz
  * Project: Simple server providing files with limited bandwidth.
  * IPP project 2, FIT VUTBR
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
  FOPEN,
  EFILE,
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
  "File for writing couldn't be opened",
  "Requested file could not be opened at server",
  "Received message does not match the protocol",
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


/** Converts string to unsigned int */
int get_positive_number(const string &str){
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
 * Class for holding data from given parameters
 */
class Params{ 
  public: 
    Params(int argc, char *argv[]); 
    void open_file();
    void write_file(string buffer);
    void close_file();
    string host, port, filename;
  private: 
    FILE * file;
};

/**
 * Processes given parameters.
 * @param argc Number of program parameters
 * @param argv Parameters
 */
Params::Params(int argc, char *argv[]){
  if (argc != 2) // host:port/soubor
    error_exit(EPARAMNUM);

  string param_str = argv[1];

  if (param_str.length() == 0)
    error_exit(EPARAM);

  unsigned long end_host = param_str.find(":");
  if (end_host != string::npos && end_host > 0){
    host = param_str.substr(0, end_host);
    unsigned long end_port = param_str.find("/");
    if (end_port != string::npos && end_port > end_host + 1){
      port = param_str.substr(end_host + 1, end_port - end_host - 1);
      if (get_positive_number(port) != 0 && param_str.length() > (end_port + 1)){
        filename = param_str.substr(end_port + 1);       
        open_file();
        return;
      }
    }
  }

 error_exit(EPARAM);
}

void Params::open_file(){
  if ((file = fopen(filename.c_str(), "wb")) == NULL)
    error_exit(FOPEN);
}

void Params::close_file(){
  fclose(file);
}

void Params::write_file(string buffer){
  fwrite(buffer.c_str(), sizeof(const char), buffer.length(),file);
}

/** Connects to server, returns descriptor */
int connect(Params params, int *fd){ 
  int socketfd;
  struct addrinfo setting;
  struct addrinfo *list;
  struct addrinfo *ptr;

  memset(&setting, 0, sizeof setting); // make the struct empty
  setting.ai_family = AF_INET;
  setting.ai_socktype = SOCK_STREAM; 
  if (getaddrinfo(params.host.c_str(), params.port.c_str(), &setting, &list) != 0) {
    return EHOST;
  }

  for (ptr = list; ptr != NULL; ptr = ptr->ai_next) {
    if ((socketfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol)) == -1)
      continue;

    if (connect(socketfd, ptr->ai_addr, ptr->ai_addrlen) == -1) {
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

  *fd = socketfd;
  return EOK;
}


/** Receives file by filename form params through socketfd */
int receive_file(Params params, int socketfd){

  char buffer[BUFFSIZE + 1];
  string send_msg;
  long int num_read;
  while (1){
    memset(&buffer, 0, sizeof buffer); // make the buffer empty
    if ((num_read = recv(socketfd, buffer, BUFFSIZE, 0)) == -1) {
       return ERECV;
    }
    string recv_msg(buffer, num_read);
    if (!strcmp(recv_msg.substr(0, 1).c_str(), "9")){
      return EFILE;
    }else if (!strcmp(recv_msg.substr(0, 1).c_str(), "8")){ // read file, not last
      while (recv_msg.length() < BUFFSIZE){
        memset(&buffer, 0, sizeof buffer); // make the buffer empty
        if ((num_read = recv(socketfd, buffer, BUFFSIZE, 0)) == -1)
          return ERECV;
        string a (buffer, num_read);
        recv_msg += a;
      }
      // obdrzen cely paket, zapis do souboru:
 
      params.write_file(recv_msg.substr(1)); //all apart from the first char "8"
      
      // Acknowledge.
      send_msg = "1"; // got it, expecting more 
      if (send(socketfd, send_msg.c_str(), send_msg.length(), 0) == -1)
        return ESEND;
    }else if (!strcmp(recv_msg.substr(0, 1).c_str(), "7")){ // read file, last
      while (recv_msg.length() < 4){ // If received less than 4 characters,
                                     // not very probable.
        memset(&buffer, 0, sizeof buffer); // make the buffer empty
        if ((num_read = recv(socketfd, buffer, BUFFSIZE, 0)) == -1)
          return ERECV;
        string a (buffer, num_read);
        recv_msg += a;
      }
      
      unsigned long to_read = strtol(recv_msg.substr(1, 3).c_str(), NULL, 10);
      if (to_read == 0 && strcmp(recv_msg.substr(1, 3).c_str(), "000"))
        return EPROTOCOL;

      while (recv_msg.length() < to_read + 4){
        memset(&buffer, 0, sizeof buffer); // make the buffer empty
        if ((num_read = recv(socketfd, buffer, BUFFSIZE, 0)) == -1)
          return ERECV;
        string a (buffer, num_read);
        recv_msg += a;
      }
      // received all packet, write to file
      params.write_file(recv_msg.substr(4)); //without protocol code (4 chars)

      // Acknowledge.
      send_msg = "2"; // got it, received all file 
      if (send(socketfd, send_msg.c_str(), send_msg.length(), 0) == -1)
        return ESEND;
      return EOK;
    }else{
      return EPROTOCOL;
    }
  }
  return EOK;  

}

//////// MAIN PROGRAM ////////
int main (int argc, char *argv[]) {
  int stat = EOK;
  Params params(argc, argv);

  int socketfd;
  if ((stat = connect(params, &socketfd)) != EOK){
    params.close_file();
    error_exit(stat);
  }

  string send_msg = params.filename + ";\n"; // send me file wih given filename
  if (send(socketfd, send_msg.c_str(), send_msg.length(), 0) == -1) {
    params.close_file();
    close(socketfd);
    error_exit(ESEND);
  }
  
  if ((stat = receive_file(params, socketfd)) != EOK){
    params.close_file();
    close(socketfd);
    error_exit(stat);
  }

  close(socketfd);
  params.close_file();
  return EXIT_SUCCESS;
}
