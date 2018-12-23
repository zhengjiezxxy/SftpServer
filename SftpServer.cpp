//  #include "config.h"
#include <iostream>
#include <numeric>
#include <cassert>
#include <cstdio>

#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include "SftpHandler.h"
#include "SftpServer.hpp"

#ifdef _WIN32
  #define KEYS_FOLDER
#else
  #define KEYS_FOLDER "./"
#endif

int SftpServer::authPassword(const char *client_user, const char *client_password,
                        const char* server_user, const char* server_password){
  if(strcmp(client_user, server_user))
    return SftpServer::FAIL;
  if(strcmp(client_password, server_password))
    return SftpServer::FAIL;
  return SftpServer::SUCCESS; // authenticated
}

int SftpServer::authenticate(ssh_session& session, const char* server_user,
                             const char* server_password){
  int auth=0;
  ssh_message message;

  do{
    message=ssh_message_get(session);
    if(!message){
      break;
    }

    switch(ssh_message_type(message)){
    case SSH_REQUEST_AUTH:
      LOG(XM_DEBUG) << "auth";
      switch(ssh_message_subtype(message)){
      case SSH_AUTH_METHOD_PASSWORD:
        LOG(XM_INFO) << "User " << ssh_message_auth_user(message) <<  " wants to auth with pass " << ssh_message_auth_password(message);
        if(authPassword(ssh_message_auth_user(message),
                        ssh_message_auth_password(message),
                        server_user, server_password) == SftpServer::SUCCESS){
          auth=1;
          ssh_message_auth_reply_success(message,0);
          break;
        }
        // not authenticated, send default message
      case SSH_AUTH_METHOD_NONE:
      default:
        ssh_message_auth_set_methods(message,SSH_AUTH_METHOD_PASSWORD);
        ssh_message_reply_default(message);
        break;
      }
      break;
    default:
      ssh_message_reply_default(message);
    }
    ssh_message_free(message);
  } while (!auth);

  if(!auth){
    LOG(XM_ERR) << "auth error: " << ssh_get_error(session);
    ssh_disconnect(session);
    return FAIL;
  }

  return SUCCESS;
}

int SftpServer::openChannel(ssh_session& session, ssh_channel& chan){
  ssh_message message;
  do {
    message=ssh_message_get(session);
    if(message){
      switch(ssh_message_type(message)){
      case SSH_REQUEST_CHANNEL_OPEN:
        if(ssh_message_subtype(message)==SSH_CHANNEL_SESSION){
          chan=ssh_message_channel_request_open_reply_accept(message);
          break;
        }
      default:
        ssh_message_reply_default(message);
      }
      ssh_message_free(message);
    }
  } while(message && !chan);
  if(!chan){
    LOG(XM_ERR) << "error : " << ssh_get_error(session);
    ssh_finalize();
    return FAIL;
  }
  return SUCCESS;
}

int SftpServer::sftpChannelRequest(ssh_session& session){
  LOG(XM_DEBUG) << "sftp channel request";
  int sftp=0;
  ssh_message message;

  do {
    message=ssh_message_get(session);
    if(message && ssh_message_type(message) == SSH_REQUEST_CHANNEL){
      int sub_type = ssh_message_subtype(message);
      if(sub_type == SSH_CHANNEL_REQUEST_ENV){
        const char* env_name = ssh_message_channel_request_env_name(message);
        const char* env_value = ssh_message_channel_request_env_value(message);
        LOG(XM_DEBUG) << "request env name: " << env_name;
        LOG(XM_DEBUG) << "request env value: " << env_value;
      }else if(sub_type == SSH_CHANNEL_REQUEST_SUBSYSTEM){
        const char* subsystem = ssh_message_channel_request_subsystem(message);
        LOG(XM_DEBUG) << "request subsystem: " << subsystem;
        if(!strcmp(subsystem, "sftp")){
          sftp=1;
          ssh_message_channel_request_reply_success(message);
        }
      }
    }
    ssh_message_free(message);
  } while (message && !sftp);

  if(!sftp){
    LOG(XM_ERR) << "error : " << ssh_get_error(session);
    return FAIL;
  }

  return SUCCESS;
}

int SftpServer::start(){
  std::cout << "debug purpose" << std::endl;
  _sshbind=ssh_bind_new();

  ssh_bind_options_set(_sshbind, SSH_BIND_OPTIONS_BINDADDR, _listen_address.c_str());
  ssh_bind_options_set(_sshbind, SSH_BIND_OPTIONS_BINDPORT_STR, _port.c_str());
  ssh_bind_options_set(_sshbind, SSH_BIND_OPTIONS_DSAKEY, KEYS_FOLDER "ssh_host_dsa_key");
  //ssh_bind_options_set(_sshbind, SSH_BIND_OPTIONS_RSAKEY, KEYS_FOLDER "ssh_host_rsa_key");

  #ifdef WITH_PCAP
  SftpInternal::set_pcap(session);
  #endif
    
  if(ssh_bind_listen(_sshbind)<0){
    LOG(XM_ERR) << "Error listening to socket: " << ssh_get_error(_sshbind);
     std::cout << "debug purpose" << std::endl;
    return SftpServer::FAIL;
  }

  // boost::thread tr(SftpServer::mainLoop, _sshbind,
  //                     _user_name.c_str(), _password.c_str());
  SftpServer::mainLoop(_sshbind, _user_name.c_str(),
                       _password.c_str());

  // pthread kill will segmment fault
  // so let it go here.
  //tr.detach();
  return SUCCESS;
}

int SftpServer::shutdown(){
  // segment fault
// #ifdef _WIN32
//     TerminateThread(_tr.native_handle(), 0);
// #else
//     pthread_kill(_tr.native_handle(), 9);
// #endif
  ssh_bind_free(_sshbind);
#ifdef WITH_PCAP
  SftpInternal::cleanup_pcap();
#endif
  ssh_finalize();
  return SUCCESS;
}


void SftpServer::mainLoop(ssh_bind sshbind, const char* server_user,
                          const char* server_password){
  sftp_session sftp_ss;
  ssh_message message;
  ssh_channel chan=0;
  int r;
  ssh_session session;
  
  while(true){
    session=ssh_new();
    LOG(XM_DEBUG) << "before accept";
    r=ssh_bind_accept(sshbind, session);
    LOG(XM_DEBUG) << "after accept";
    if(r == SSH_ERROR){
      LOG(XM_ERR) << "error accepting a connection : " << ssh_get_error(sshbind);
    }

    if(ssh_handle_key_exchange(session)) {
      LOG(XM_ERR) << "ssh_handle_key_exchange: " << ssh_get_error(session);
    }

    if(authenticate(session, server_user, server_password) == SUCCESS &&
       openChannel(session, chan) == SUCCESS &&
       sftpChannelRequest(session) == SUCCESS){
      LOG(XM_DEBUG) << "ssh authenticate and initialize done";
      LOG(XM_DEBUG) << "begin to transfer file";

      sftp_ss = sftp_server_new(session, chan);
      sftp_server_init(sftp_ss);
      SftpInternal::sftpMessageLoop(sftp_ss);
      
      //ssh_channel_free(chan);
      //ssh_channel_close(chan);
      // sftp_free will free channel and close channel and free all handles
      sftp_free(sftp_ss);
    }
    ssh_disconnect(session);
  }
}
