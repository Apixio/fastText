/**
 * Copyright (c) 2016-present, Apixio, Inc.
 * All rights reserved.
 *
 */

#include <iostream>
#include <fstream>
#include <string>

#include <zmq.hpp>

using namespace std;

void printUsage() {
  cout
    << "usage: fasttext_zmq_client <ip:port> <test-data> \n\n"
    << "  <ip:port>   ip and port to listen on\n"
    << "  <test-data> test data filename (if -, read from stdin)\n"
    << endl;
}

int main(int argc, char* argv[]) {

  const string SHUTDOWN("[CMD:SHUTDOWN]");
  const string PING("[CMD:PING]");

  if (argc < 3) {
    printUsage();
    exit(EXIT_FAILURE);
  }

  cerr << "Starting up..." << endl;

  cerr << "creating context..." << endl;
  zmq::context_t context(5);

  cerr << "creating socket... " << endl;
  zmq::socket_t socket(context, ZMQ_REQ);
  cerr << "connecting to " << argv[1] << "..." << endl;
  socket.connect(argv[1]);

  std::string infile(argv[2]);
  std::istream* isp;
  std::ifstream ifs; 
  if (infile == "-") {
    isp = &cin;
  }
  else {
    ifs.open(infile);
    if (!ifs.is_open()) {
      cerr << "Unable to open file: " << infile << endl;
      exit(EXIT_FAILURE);
    }
    isp = &ifs;
  }

  // send ping
  {
    zmq::message_t ping_m(PING.c_str(), PING.size());
    socket.send(ping_m);

    zmq::message_t ping_resp_m;
    socket.recv(&ping_resp_m);
    string PING_RESP(static_cast<char *>(ping_resp_m.data()), ping_resp_m.size());
    cerr << "PING response : " << PING_RESP << endl;
  }


  while (!isp->eof() && !isp->fail()) {
    
    char buf[8192];
    bzero(buf, 8192);
    isp->getline(buf, 8192);

    int len = strlen(buf);
    if (len == 0)
      continue;

    zmq::message_t request_m(buf, len);
    socket.send(request_m);

    zmq::message_t reply_m;
    socket.recv(&reply_m);

    string reply(static_cast<char *>(reply_m.data()), reply_m.size());
    cerr << "REPLY: " << reply << endl;
  }

  return 0;
}
