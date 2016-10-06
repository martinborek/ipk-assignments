/**
  * File:    ftpclient.cpp
  * Date:    2014/03/23
  * Author:  Martin Borek, xborek08@stud.fit.vutbr.cz
  * Project: Simple FTP client for IPP (project 1) 
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
  EPASV, // PASV
  ERECV, // RECV 
  ESEND, // SEND
  ECONNECTION,
  EHOST,
  ELOGING,
  EUNKNOWN // Unknown error
};

/** Error messages */
const char *ECODEMSG[] = {
  "Everything is OK.",
  "Wrong number of parameters",
  "Wrong parameter",
  "Received wrong ip format to connect to",
  "Failed to receive a message",
  "Failed to send a command",
  "Failed to connect to server",
  "Host is not available",
  "Not logged in",
  "Unknown error"
};

/** Structure for url address */
typedef struct url {
  string user;
  string password;
  string host;
  string port;
  string path;
  bool auth;
} TUrl;

/**
 * Prints error messages according to given error code and exits;
 * @param ecode Error code
 */
void printECode (int eCode) {
  if (eCode == EOK)
    return;

  if (eCode < EOK || eCode > EUNKNOWN)
    eCode = EUNKNOWN;

  cerr << ECODEMSG[eCode] << endl;
  exit(eCode);
}

/**
 * Processes given parameters
 * @param argc Number of program parameters
 * @param argv Parameters
 * @param url Structure for processed parameters of url
 * @return Returns error value
 */
int getParams(int argc, char *argv[], TUrl *url) {
 
  if (argc != 2)
    return EPARAMNUM;
  
  string urlString = argv[1];
  url->user = "anonymous";
  url->password = "xborek08@stud.fit.vutbr.cz";
  url->host = "";
  url->port = "ftp", // default
  url->path = ""; 
  url->auth = false;

  unsigned long atPos;
  unsigned long colonPos;
  unsigned long bslashPos;
  unsigned long minPos;

  // [ftp://[user:password@]]host[:port][/path][/]
  if (urlString.compare(0, 6, "ftp://") == 0) {
    urlString.erase(0, 6); // erase ftp://
    
    if (urlString.length() == 0)
      return EPARAM;
    
    if (urlString.compare(urlString.length() - 1, 1, "/") ==  0)
      urlString.erase(urlString.length() - 1, 1);
    
    if ((atPos = urlString.find("@")) != string::npos) { // user & password
      url->auth = true;
      if ((colonPos = urlString.find(":")) > atPos || colonPos == 0 || colonPos == (atPos - 1))
         return EPARAM; // incorrect user:password@ usage

      url->user = urlString.substr(0, colonPos);
      urlString.erase(0, colonPos + 1);
      atPos -= colonPos + 1;
      url->password = urlString.substr(0, atPos);
      urlString.erase(0, atPos + 1);
    }
  }

  if (urlString.length() == 0)
    return EPARAM;
  
  colonPos = urlString.find(":");
  if ((bslashPos = urlString.find("/")) == string::npos)
    bslashPos = urlString.length(); // points past the last character
  
  minPos = MIN(colonPos, bslashPos);
  url->host = urlString.substr(0, minPos);

  if (colonPos < bslashPos) {
    urlString.erase(0, colonPos + 1);
    bslashPos -= colonPos + 1;
    
    string::const_iterator i = urlString.begin();
    unsigned j = 0;
    while (i != urlString.end() && isdigit(*i)){
      i++;
      j++;
    }

    if (j == 0)
      return EPARAM;
    
    url->port = urlString.substr(0, bslashPos).c_str();
  }
  if (bslashPos + 1 < urlString.length())
    url->path = urlString.substr(bslashPos + 1);


  return EOK;
}

/**
 * @param message Received message is returned in this string
 * @param socketfd Id of socket to read from
 * @return Returns code returned by ftp server
 */
int readSocket(string *message, int socketfd) {

  string code;
  char buffer[BUFFSIZE];
  memset(&buffer, 0, sizeof buffer); // make the buffer empty
  *message = "";
  if (recv(socketfd, buffer, BUFFSIZE - 1, 0) == -1) {
    close(socketfd);
    printECode(ERECV);
  }

  *message += buffer;

  string::const_iterator i = message->begin();
  unsigned j = 0;
  while (i != message->end() && isdigit(*i) ){
    i++;
    j++;
  }

  if (j !=  3)
    return ERECV;
    
  code = message->substr(0, 3);
  if (message->substr(3, 1) != " " && message->find("\n" + code + " ") == string::npos) {
    // some lines are missing
    while (message->find("\n" + code + " ") == string::npos) {

      if (recv(socketfd, buffer, BUFFSIZE - 1, 0) == -1) {
        close(socketfd);
        printECode(ERECV);
      }

      *message += buffer;
    }
  }
   
  while (message->find("\r\n") == string::npos) {
    // end of line is missing
    if (recv(socketfd, buffer, BUFFSIZE - 1, 0) == -1) {
      close(socketfd);
      printECode(ERECV);
    }
    *message += buffer;
  }
    
  return atoi(code.c_str());
}

//////// MAIN PROGRAM ////////
int main (int argc, char *argv[]) {
  
  TUrl url;
  int eCode = EOK;
  if ((eCode = getParams(argc, argv, &url)) != EOK)
    printECode(eCode);
  
  int stat;
  struct addrinfo setting;
  struct addrinfo *list;
  struct addrinfo *ptr;

  memset(&setting, 0, sizeof setting); // make the struct empty
  setting.ai_family = AF_INET;
  setting.ai_socktype = SOCK_STREAM; 
  setting.ai_protocol = 0;
  if ((stat = getaddrinfo(url.host.c_str(), url.port.c_str(), &setting, &list)) != 0) {
    printECode(EHOST);
  }

  int socketfd;

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
    printECode(ECONNECTION);
  }
    
  freeaddrinfo(list);
  int code;
  string order; 
  string message = "";

  code = readSocket(&message, socketfd);
  if (code == 220 || url.auth) {
    order = "USER " + url.user + "\r\n";  
    if (send(socketfd, order.c_str(), order.length(), 0) == -1) {
      close(socketfd);
      printECode(ESEND);
    }
    code = readSocket(&message, socketfd);

    order = "PASS " + url.password + "\r\n";  
    if (send(socketfd, order.c_str(), order.length(), 0) == -1) {
      close(socketfd);
      printECode(ESEND);
    }

    if ((code = readSocket(&message, socketfd)) != 230) {
      close(socketfd);
      printECode(ELOGING);
    }
  }  

  order = "MODE S\r\n"; // set mode to stream 
  if (send(socketfd, order.c_str(), order.length(), 0) == -1) {
    close(socketfd);
    printECode(ESEND);
  }
  code = readSocket(&message, socketfd);

  order = "TYPE A\r\n"; // ASCII
  if (send(socketfd, order.c_str(), order.length(), 0) == -1) {
    close(socketfd);
    printECode(ESEND);
  }
  code = readSocket(&message, socketfd);

  order = "PASV\r\n"; // pasive conection
  if (send(socketfd, order.c_str(), order.length(), 0) == -1) {
    close(socketfd);
    printECode(ESEND);
  }
  code = readSocket(&message, socketfd);

  string ip = "";
  unsigned port = 0;
  unsigned long bracketPos;
  if ((bracketPos = message.find("(")) == string::npos) {
    close(socketfd);
    printECode(EPASV);
  }

  message.erase(0, bracketPos + 1);
  unsigned long commaPos;
  for (int i = 0; i < 4; i++) { // get IP address to connect to
    if ((commaPos = message.find(",")) == string::npos) {
      close(socketfd);
      printECode(EPASV);
    } 

    if (ip != "")
      ip = ip + ".";

    ip = ip + message.substr(0, commaPos);
    
    message.erase(0, commaPos + 1);
  }

  if ((commaPos = message.find(",")) == string::npos) {
    close(socketfd);
    printECode(EPASV);
  }

  port = atoi(message.substr(0, commaPos).c_str()) * 256;
  message.erase(0, commaPos + 1);
  
  if ((bracketPos = message.find(")")) == string::npos) {
    close(socketfd);
    printECode(EPASV);
  }

  port += atoi(message.substr(0, bracketPos).c_str());

  ostringstream convert;
  convert << port;
  const string portStr = convert.str();
  
  // connect to data socket:
  int stat2;
  struct addrinfo setting2;
  struct addrinfo *list2;
  struct addrinfo *ptr2;
  int socketfd2;
  memset(&setting2, 0, sizeof setting2); // make the struct empty
  setting2.ai_family = AF_INET;
  setting2.ai_socktype = SOCK_STREAM; 
  setting2.ai_protocol = 0;
  if ((stat2 = getaddrinfo(ip.c_str(), portStr.c_str(), &setting2, &list2)) != 0) {
    close(socketfd);
    printECode(EHOST);
  }


  for (ptr2 = list2; ptr2 != NULL; ptr2 = ptr2->ai_next) {
    if ((socketfd2 = socket(ptr2->ai_family, ptr2->ai_socktype, ptr2->ai_protocol)) == -1)
      continue;

    if (connect(socketfd2, ptr2->ai_addr, ptr2->ai_addrlen) == -1) {
      close(socketfd2);
      continue;
    }
    break;
  }

  if (ptr2 == NULL) {
    close(socketfd);
    printECode(ECONNECTION);
  }


  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;

  setsockopt(socketfd2, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));


  order = "LIST "+ url.path +"\r\n"; // get list of files
  if (send(socketfd, order.c_str(), order.length(), 0) == -1) {
    close(socketfd2);
    close(socketfd);
    printECode(ESEND);
  } 
  code = readSocket(&message, socketfd);

  int check;
  char buffer[BUFFSIZE];
  string text = "";
  memset(&buffer, 0, sizeof buffer); // make the buffer empty

  while ((check = recv(socketfd2, buffer, BUFFSIZE - 1, 0)) > 0) {
    text += buffer;
    memset(&buffer, 0, sizeof buffer); // make the buffer empty
  }

  if (check == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
    close(socketfd2);
    close(socketfd);
    printECode(ERECV);
  }

  cout<< text;


  order = "QUIT\r\n"; 
  if (send(socketfd, order.c_str(), order.length(), 0) == -1) {
    close(socketfd2);
    close(socketfd);
    printECode(ESEND);
  }
  code = readSocket(&message, socketfd);

  close(socketfd);
  close(socketfd2);

  return eCode;
}
