#include <boost/thread.hpp>

ThreadService::setup(){
  worker.set(server.run);
}


ThreadService::shutdown(){
  server.stop();
  worker.join();
}
