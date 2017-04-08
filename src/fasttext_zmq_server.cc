/**
 * Copyright (c) 2016-present, Apixio, Inc.
 * All rights reserved.
 *
 */

#include <iostream>
#include <sstream>
#include <string>
#include <math.h>

#include <zmq.hpp>

#include "fasttext.h"
#include "args.h"

using namespace std;
using namespace fasttext;

void printUsage() {
  cout
    << "usage: fasttext_zmq_server <ip:port> <model>\n\n"
    << "  <ip:port>   ip and port to listen on\n"
    << "  <model>     model filename\n" 
    << endl;
}

int main(int argc, char** argv) {

  const string SHUTDOWN("[CMD:SHUTDOWN]");

  if (argc < 3) {
    printUsage();
    exit(EXIT_FAILURE);
  }

  cerr << "creating zmq context..." << endl;
  zmq::context_t context(5);

  cerr << "creating zmq socket... " << endl;
  zmq::socket_t socket(context, ZMQ_REP);  

  cerr << "binding zmq socket to " << argv[1] << endl;
  socket.bind(argv[1]);


  cerr << "Loading model file..." << endl;  
  FastText fasttext;
  fasttext.loadModel(string(argv[2]));
  const int32_t k = 1;

  cerr << "Waiting for requests..." << endl;
  bool done = false;
  while (!done) {

    zmq::message_t req_m;
    socket.recv(&req_m);

    const string req(static_cast<char *>(req_m.data()), req_m.size());
    string reply;

    if (req == SHUTDOWN) {
      cerr << "Received " << SHUTDOWN << " cmd. Exiting.. " << endl;
      reply = "OK";
      done = true;
    } else {
      
      // execute fasttext
      istringstream iss(req);
      cerr << "Received text: " << req << endl;
      vector<pair<real, string>> vec;
      fasttext.predict(iss, k, vec);

      stringstream ss;

      if (vec.size()) {
	ss << '[';
	ss << '{';
	ss << "\"label\"" << ':' << '"' << vec[0].second << '"';
	ss << ',';
	ss << "\"prob\"" << ':' << exp(vec[0].first);
	ss << '}';
	ss << ']';
      } else {
	ss << "[]";
      }

      reply = ss.str();

    }

    zmq::message_t reply_m(reply.c_str(), reply.size());
    socket.send(reply_m);
  }

  return 0;
}
