#ifndef SFTPHANDLER_H
#define SFTPHANDLER_H

#include <libssh/libssh.h>
#include <libssh/server.h>

#define WITH_SERVER 1
//include sftp server part
#include <libssh/sftp.h>
#include <iostream>
class dummy{
 public:
 dummy(): os(std::cout){

  }

  ~dummy(){
   os << std::endl;
  }
  std::ostream& os;
};


//#define LOG(A) dummy().os
#define LOG(A) std::cout << std::endl; std::cout

namespace SftpInternal{

 /**
   * main message loop in sftp server
   * handle different requests
   * mainly this server is restricted to minimal
   * file transfer server
   * please see https://tools.ietf.org/html/draft-ietf-secsh-filexfer-13
   * for a complete command list
   * this message loop only implments minimal file transfter command,
   * so opendir and mkdir is not implemnted
   */
  int sftpMessageLoop(sftp_session& sftp_ss);

  enum sftp_command{
    SFTP_SERVER_NOTIMPLEMENTED = 0,
    SFTP_SERVER_INIT = SSH_FXP_INIT,
    SFTP_SERVER_VERSION = SSH_FXP_VERSION,
    SFTP_SERVER_OPEN = SSH_FXP_OPEN,
    SFTP_SERVER_CLOSE = 4,
    SFTP_SERVER_READ = 5,
    SFTP_SERVER_WRITE = 6,
    SFTP_SERVER_LSTAT = SSH_FXP_LSTAT,
    SFTP_SERVER_FSTAT = SSH_FXP_FSTAT,
    SFTP_SERVER_SETSTAT = SSH_FXP_SETSTAT,
    SFTP_SERVER_FSETSTAT = SSH_FXP_FSETSTAT,
    SFTP_SERVER_OPENDIR = SSH_FXP_OPENDIR,
    SFTP_SERVER_READDIR = SSH_FXP_READDIR,
    SFTP_SERVER_REMOVE = SSH_FXP_REMOVE,
    SFTP_SERVER_MKDIR = SSH_FXP_MKDIR,
    SFTP_SERVER_RMDIR = SSH_FXP_RMDIR,
    SFTP_SERVER_REALPATH = SSH_FXP_REALPATH,
    SFTP_SERVER_STAT = SSH_FXP_STAT,
    SFTP_SERVER_RENAME = SSH_FXP_RENAME,
    SFTP_SERVER_READLINK = SSH_FXP_READLINK,
    SFTP_SERVER_SYMLINK = SSH_FXP_SYMLINK,
    SFTP_SERVER_MAX,
  };

  typedef int (*sftp_command_handler)(sftp_session, sftp_client_message);

  struct sftp_handler{
    const char* name;
    sftp_command command;
    sftp_command_handler handler;
  };

#ifdef WITH_PCAP
  void set_pcap(ssh_session session);
  void cleanup_pcap(void);
#endif
};

#endif
