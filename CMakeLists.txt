project(sftp_server)
cmake_minimum_required(VERSION 2.8)

if(WIN32)
include_directories("C:/Users/zhengjie4/source/devtools/git/onecli/Modularization/extlib/include/")
include_directories("C:/Users/zhengjie4/source/devtools/libssh-0.7.5/libssh-0.7.5/include/")
link_directories("C:/Users/zhengjie4/source/devtools/git/onecli/Modularization/extlib/Win64/lib/")
link_directories("C:/Users/zhengjie4/source/devtools/libssh-0.7.5/build-libssh/src/")
else(WIN32)
link_directories("/root/zhengjie/libssh-0.7.5/build-linux/src/")
include_directories("/root/zhengjie/libssh-0.7.5/libssh-0.7.5/include/")
include_directories("/root/zhengjie/include/")
link_directories("/root/zhengjie/RHEL6_x64/lib/")
endif(WIN32)

if(WIN32)
  add_executable(sftp_server SftpHandler.cpp SftpServer.cpp test_sftp.cpp)
  target_link_libraries(sftp_server ssh)
else(WIN32)
  add_executable(sftp_server SftpHandler.cpp SftpServer.cpp test_sftp.cpp)
  target_link_libraries(sftp_server ssh boost_system boost_thread pthread)
endif(WIN32)
