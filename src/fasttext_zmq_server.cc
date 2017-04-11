/**
 * Copyright (c) 2016-present, Apixio, Inc.
 * All rights reserved.
 *
 */

#include <iostream>
#include <sstream>
#include <string>
#include <math.h>
#include <csignal>

#include <zmq.hpp>

#include "fasttext.h"
#include "args.h"

using namespace std;
using namespace fasttext;


static int s_interrupted = 0;
static void s_signal_handler(int signal_value) {
  s_interrupted = 1;
}

static void s_catch_signals(void) {
  struct sigaction action;
  action.sa_handler = s_signal_handler;
  action.sa_flags = 0;
  sigemptyset (&action.sa_mask);
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
}


void printUsage() {
  cout
    << "usage: fasttext_zmq_server <ip:port> <model>\n\n"
    << "  <ip:port>   ip and port to listen on\n"
    << "  <model>     model filename\n" 
    << endl;
}

int main(int argc, char** argv) {

  const string SHUTDOWN("[CMD:SHUTDOWN]");
  const string PING("[CMD:PING]");
  const string OK("OK");
  const string PONG("PONG");

  if (argc < 3) {
    printUsage();
    exit(EXIT_FAILURE);
  }

  cerr << "creating zmq context..." << endl;
  zmq::context_t context(2);

  s_catch_signals();

  cerr << "Loading model file..." << endl;  
  FastText fasttext;
  fasttext.loadModel(string(argv[2]));
  const int32_t k = 1;

  cerr << "creating zmq socket... " << endl;
  zmq::socket_t socket(context, ZMQ_REP);  

  // setup socket options
  int linger_ms = 5000;
  socket.setsockopt(ZMQ_LINGER, &linger_ms, sizeof(linger_ms));

  // 5 min receive timeout
  int receive_timeout_ms = 5 * 60 *1000;
  socket.setsockopt(ZMQ_RCVTIMEO, &receive_timeout_ms, sizeof(receive_timeout_ms));

  cerr << "binding zmq socket to " << argv[1] << endl;
  socket.bind(argv[1]);

  cerr << "Waiting for requests..." << endl;
  bool done = false;
  while (!done) {

    zmq::message_t req_m;
    try {

      bool rv = socket.recv(&req_m);
      if (rv == false) { // timed out
	cerr << "No messages received. Timed out. Exiting.." << endl;
	done = true;
	continue;
      }

    } catch(zmq::error_t &e) {

      if (e.num() == EINTR || s_interrupted) {
	cerr << "Received interrupt. Exiting.. " << endl;
      } else {
	cerr << "exception num: " << e.num() << " what: " << e.what() << endl;
      }

      done = true;
      continue;
    }

    const string req(static_cast<char *>(req_m.data()), req_m.size());
    string reply;

    if (req == SHUTDOWN) {
      cerr << "Received " << SHUTDOWN << " cmd. Exiting.. " << endl;
      reply = OK;
      done = true;
    } else if (req == PING) {
      reply = PONG;
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
