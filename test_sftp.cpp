#include <string>
#include <boost/filesystem.hpp>
#include "SftpServer.hpp"
#include <iostream>
#ifndef _WIN32
  #include <unistd.h>

#endif

int main(){
  std::string dsa_key = boost::filesystem::path("/etc/ssh/ssh_host_dsa_key").string();
  std::string rsa_key = boost::filesystem::path("/etc/ssh/ssh_host_rsa_key").string();

  SftpServer sftp_server(dsa_key, rsa_key, "test", "test", "6990", "10.104.196.56");
  sftp_server.start();

#ifndef _WIN32
   sleep(60*100);
#else
   Sleep(60*1000*100);
#endif
  //sftp_server.shutdown();
  std::cout << "sftp server close successfully" << std::endl;
}
