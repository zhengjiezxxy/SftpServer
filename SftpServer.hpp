#ifndef SFTPSERVER_HPP
#define SFTPSERVER_HPP
#include <string>
#include <boost/thread.hpp>
#include <libssh/libssh.h>
#include <libssh/server.h>

#define WITH_SERVER 1
//include sftp server part
#include <libssh/sftp.h>
#include "Service.hpp"

class SftpServer : public Service{
public:
  SftpServer(const std::string& dsa_key, const std::string& rsa_key,
             const std::string& user_name = "internalsftp",
             const std::string& password = "internalpassword",
             const std::string& port = "6990",
             const std::string& listen_address = "127.0.0.1",
             int verbose = 0,
             const std::string& authorized_key = ""):
    _user_name(user_name), _password(password),
    _rsa_key(rsa_key),_dsa_key(dsa_key),
    _authorized_keys(authorized_key), _port(port),
    _listen_address(listen_address), _verbose(verbose),
    _running(false){}


  /**
   * run a server
   * call this acutall setup the server
   */
  int start();

  /**
   * close a server
   */
  int shutdown();

  /**
   * expand a path including user name like ~
   * make it expand_user function like python
   */
  //static std::string expandUser(std::string path);

private:
  /**
   * static function to verify password and username
   */
  static int authPassword(const char *client_user, const char *client_password,
                          const char* server_user, const char* server_password);

  /**
   * authenticate a ssh session
   * handle sorts of ssh message
   */
  static int authenticate(ssh_session& session, const char* server_user,
                          const char* server_password);

  /**
   * open a ssh channel that's the main transfer tunnel
   */
  static int openChannel(ssh_session& session, ssh_channel& chan);

  /**
   * request a sftp sub system
   */
  static int sftpChannelRequest(ssh_session& session);

  /**
   * thread loop
   * work around as there is no nonblocking api in libssh
   */
  static void mainLoop(ssh_bind sshbind, const char* server_user,
                       const char* server_password);
  std::string _user_name;
  std::string _password;
  std::string _rsa_key;
  std::string _dsa_key;
  std::string _authorized_keys;
  std::string _port;
  std::string _listen_address;
  int _verbose;
  int _running;

  // we need this member to free it when the server thread is killed
  ssh_bind _sshbind;
};

#endif
