#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <map>

#ifdef _WIN32
  #include <io.h>
  //#include "Win32-port.hpp"
  #define lstat stat
#else
  #include <unistd.h>
  #include <signal.h>
#endif

#define SUCCESS 0
#define FAIL 1
#include "SftpHandler.h"

namespace SftpInternal{
  // keep a record of all
  // directory objects property
  std::map<std::string, int> sftp_server_file_handles;

  // change all / except second to backforward slash character
  // except the second one.
  std::string changeToWindowsAddress(const std::string& path){
    LOG(XM_DEBUG) << "short file path: " << path;

    std::string res;
    int slash_cnt = 1;
    int n = path.size();
    for(int i=1;i<n;++i){
      if(path[i] == '/'){
        if(slash_cnt == 1){
          ++slash_cnt;
          res += ":\\";
        }else{
          res = '\\';
        }
      }else{
        res += path[i];
      }
    }

    if(slash_cnt == 1){
      LOG(XM_ERR) << "invalid address, windows sftp address shoule be  /harddisk drive/filepath  ";
      return path;
    }

    return res;
  }


  // in powershell ported version.
  // server only support file in c drive
  // we intententiolly change that.
  // change sftp address to windows address
  // becuase all is in our hand.

  // 1.  client side will make sure the address in following format:
  // /harddisk drive/filepath.
  // 2. client make sure windows package path be posix compliant.
  // 3. client should use full name.
  std::string convertPath(const std::string& path){
    std::string res = path;
#if defined(_WIN32)
#pragma message("windows")
    res = changeToWindowsAddress(path);
#endif
    return res;
  }


  /*
   * always return fails
   * to break mesage loop
   */
  int process_notimplemented(sftp_session sftp_ss, sftp_client_message client_message){
    int client_message_type = sftp_client_message_get_type(client_message);
    LOG(XM_ERR) << "client_message_type :" << client_message_type << " unimplmented";
    LOG(XM_ERR) << "client_message_type :" << client_message_type << " unimplmented";
    return 1;
  }

  int process_open(sftp_session sftp_ss, sftp_client_message client_message){
    LOG(XM_DEBUG) << "open";
    const char* file_name = sftp_client_message_get_filename(client_message);
    std::string long_file_name = convertPath(file_name);
    LOG(XM_DEBUG) << "long_file_name: " << long_file_name;

    struct sftp_client_message_struct client_message_data = *client_message;
    int message_flags = client_message_data.flags;
    int flags = 0;
    if(( (message_flags & SSH_FXF_READ) == SSH_FXF_READ) &&
       ( (message_flags & SSH_FXF_WRITE) == SSH_FXF_WRITE)){
      flags = O_RDWR;
    }else if((message_flags & SSH_FXF_READ) == SSH_FXF_READ){
      flags = O_RDONLY;
    }else if((message_flags & SSH_FXF_WRITE) == SSH_FXF_WRITE){
      flags = O_WRONLY;
    }

    if((message_flags & SSH_FXF_APPEND) == SSH_FXF_APPEND){
      flags |= O_APPEND;
    }

    if((message_flags & SSH_FXF_CREAT) == SSH_FXF_CREAT){
      flags |= O_CREAT;
    }

    if((message_flags & SSH_FXF_TRUNC) == SSH_FXF_TRUNC){
      flags |= O_TRUNC;
    }

    if((message_flags & SSH_FXF_EXCL) == SSH_FXF_EXCL){
      flags |= O_EXCL;
    }

    sftp_server_file_handles[long_file_name] = ::open(long_file_name.c_str(), flags);
    char* info = new char[long_file_name.size()+1];
    strcpy(info, long_file_name.c_str());
    ssh_string handle = sftp_handle_alloc(sftp_ss, info);
    sftp_reply_handle(client_message, handle);
    return 0;
  }

  int process_close(sftp_session sftp_ss, sftp_client_message client_message){
    LOG(XM_DEBUG) << "close";
    struct sftp_client_message_struct client_message_data = *client_message;
    const char* file_name = (char*)sftp_handle(sftp_ss, client_message_data.handle);
    std::string long_file_name = std::string(file_name);
    LOG(XM_DEBUG) << "long_file_name: " << long_file_name;

    int cnt = sftp_server_file_handles.count(long_file_name);
    if(cnt){
      int fd = sftp_server_file_handles[long_file_name];
      ::close(fd);
      sftp_server_file_handles.erase(long_file_name);
      sftp_reply_status(client_message, SSH_FX_OK, "Success");
    }else{
      LOG(XM_ERR)<< "fd is not found when closing file";
    }
    return 0;
  }

  // currently don't support link file in this server.
  // But need tell the client this file exists
  int process_lstat(sftp_session sftp_ss, sftp_client_message client_message){
    LOG(XM_DEBUG) << "lstat";
    const char* file_name = sftp_client_message_get_filename(client_message);
    std::string long_file_name = convertPath(file_name);
    LOG(XM_DEBUG) << "long_file_name: " << long_file_name;

    struct stat st;
    lstat(long_file_name.c_str(), &st);

    struct sftp_attributes_struct attr;
    attr.flags = 0;
    attr.flags |= SSH_FILEXFER_ATTR_SIZE;
    attr.size = st.st_size;
    attr.flags |= SSH_FILEXFER_ATTR_UIDGID;
    attr.uid = st.st_uid;
    attr.gid = st.st_gid;
    attr.flags |= SSH_FILEXFER_ATTR_PERMISSIONS;
    attr.permissions = st.st_mode;
    attr.flags |= SSH_FILEXFER_ATTR_ACMODTIME;
    attr.atime = st.st_atime;
    attr.mtime = st.st_mtime;
    sftp_reply_attr(client_message, &attr);
    return 0;
  }

  int process_stat(sftp_session sftp_ss, sftp_client_message client_message){
    LOG(XM_DEBUG) << "stat";
    const char* file_name = sftp_client_message_get_filename(client_message);
    std::string long_file_name = convertPath(file_name);
    LOG(XM_DEBUG) << "long_file_name: " << long_file_name;

    struct stat st;
    stat(long_file_name.c_str(), &st);

    struct sftp_attributes_struct attr;
    attr.flags = 0;
    attr.flags |= SSH_FILEXFER_ATTR_SIZE;
    attr.size = st.st_size;
    attr.flags |= SSH_FILEXFER_ATTR_UIDGID;
    attr.uid = st.st_uid;
    attr.gid = st.st_gid;
    attr.flags |= SSH_FILEXFER_ATTR_PERMISSIONS;
    attr.permissions = st.st_mode;
    attr.flags |= SSH_FILEXFER_ATTR_ACMODTIME;
    attr.atime = st.st_atime;
    attr.mtime = st.st_mtime;
    sftp_reply_attr(client_message, &attr);
    return 0;
  }

  int process_realpath(sftp_session sftp_ss, sftp_client_message client_message){
    std::string long_file_name;
    LOG(XM_DEBUG) << "realpath";
    const char* file_name = sftp_client_message_get_filename(client_message);
    long_file_name = convertPath(file_name);
    LOG(XM_DEBUG) << "long filename is: " << long_file_name;

    struct sftp_attributes_struct attr;
    sftp_reply_names_add(client_message, long_file_name.c_str(),
                         long_file_name.c_str(), &attr);
    sftp_reply_names(client_message);
    return 0;
  }

  int process_read(sftp_session sftp_ss, sftp_client_message client_message){
    LOG(XM_DEBUG) << "read";
    struct sftp_client_message_struct client_message_data = *client_message;
    const char* file_name = (char*)sftp_handle(sftp_ss, client_message_data.handle);
    std::string long_file_name = std::string(file_name);
    LOG(XM_DEBUG) << "long_file_name: " << long_file_name;
    LOG(XM_DEBUG) << "long_file_name: " << long_file_name;
    LOG(XM_DEBUG) << "long_file_name: " << long_file_name;

    int cnt = sftp_server_file_handles.count(long_file_name);
    if(cnt){
      int fd = sftp_server_file_handles[long_file_name];
      lseek(fd, client_message_data.offset, SEEK_SET);
      char* buffer = new char[client_message_data.len];
      int n = ::read(fd, buffer, client_message_data.len);
      if(n>0){
        LOG(XM_DEBUG) << "sftp server write " << n << " bytes";
        sftp_reply_data(client_message, buffer, n);
      }else{
        //eof
        LOG(XM_DEBUG) << "sftp server eof encountered";
        sftp_reply_status(client_message, SSH_FX_EOF, "End-of-file encountered");
      }
      delete[] buffer;
    }else{
      LOG(XM_ERR)<< "fd is not found when reading file";
    }
    return 0;
  }

  struct sftp_handler requests_handlers[] = {
    {"notimplemented", SFTP_SERVER_NOTIMPLEMENTED, process_notimplemented},
    {"init", SFTP_SERVER_INIT, process_notimplemented},
    {"version", SFTP_SERVER_VERSION, process_notimplemented},
    {"open", SFTP_SERVER_OPEN, process_open},
    {"close", SFTP_SERVER_CLOSE, process_close},
    {"read", SFTP_SERVER_READ, process_read},
    {"write", SFTP_SERVER_WRITE, process_notimplemented},
    {"lstat", SFTP_SERVER_LSTAT, process_lstat},
    {"fstat", SFTP_SERVER_FSTAT, process_notimplemented},
    {"setstat", SFTP_SERVER_SETSTAT, process_notimplemented},
    {"fsetstat", SFTP_SERVER_FSETSTAT, process_notimplemented},
    {"opendir", SFTP_SERVER_OPENDIR, process_notimplemented},
    {"readir", SFTP_SERVER_READDIR, process_notimplemented},
    {"remove", SFTP_SERVER_REMOVE, process_notimplemented},
    {"mkdir", SFTP_SERVER_MKDIR, process_notimplemented},
    {"rmdir", SFTP_SERVER_RMDIR, process_notimplemented},
    {"realpath", SFTP_SERVER_REALPATH, process_realpath},
    {"stat", SFTP_SERVER_STAT, process_stat},
    {"rename", SFTP_SERVER_RENAME, process_notimplemented},
    {"readlink", SFTP_SERVER_READLINK, process_notimplemented},
    {"symlink", SFTP_SERVER_SYMLINK, process_notimplemented},
  };

  /**
   * please see https://tools.ietf.org/html/draft-ietf-secsh-filexfer-13
   * for a complete command list

   * this message loop only implments minimal file transfter command,
   * so opendir and mkdir is not implemnted
   */
  int sftpMessageLoop(sftp_session& sftp_ss){
    sftp_client_message client_message;
    std::string long_file_name;

    while(true){
      LOG(XM_DEBUG) << "another new round of message";
      LOG(XM_DEBUG) << "another new round of message";

      client_message = sftp_get_client_message(sftp_ss);
      LOG(XM_DEBUG) << "client message: " << client_message;
      if(client_message == NULL){
        return FAIL;
      }

      int client_message_type = sftp_client_message_get_type(client_message);
      LOG(XM_DEBUG) << "client_message_type: " << client_message_type;

      if(client_message_type < SFTP_SERVER_MAX){
        sftp_command_handler handler = process_notimplemented;
        for(int i=0;i<SFTP_SERVER_MAX;++i){
          if(client_message_type == requests_handlers[i].command){
            LOG(XM_DEBUG) << requests_handlers[client_message_type].name;
            handler = requests_handlers[client_message_type].handler;
            break;
          }
        }
        //int ret = FAIL;
        int ret = handler(sftp_ss, client_message);
        if(ret == FAIL){
          sftp_client_message_free(client_message);
          return FAIL;
        }
      }else{
        sftp_client_message_free(client_message);
        return FAIL;
      }
    }
  }

#ifdef WITH_PCAP
  const char *pcap_file="debug.server.pcap";
  ssh_pcap_file pcap;

  void set_pcap(ssh_session session){
    if(!pcap_file)
      return;
    pcap=ssh_pcap_file_new();
    if(ssh_pcap_file_open(pcap,pcap_file) == SSH_ERROR){
      printf("Error opening pcap file\n");
      ssh_pcap_file_free(pcap);
      pcap=NULL;
      return;
    }
    ssh_set_pcap_file(session,pcap);
  }

  void cleanup_pcap(void) {
    ssh_pcap_file_free(pcap);
    pcap=NULL;
  }
#endif
} //namespace SftpInternal
