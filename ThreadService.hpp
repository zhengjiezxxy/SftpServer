#ifndef SERVICE_HPP
#define SERVICE_HPP
/**
 * a service that can be setup/shutdown
 */
class Service{
public:
  enum{
    SUCCESS = 0,
    FAIL = 1
  };

  Service(){}
  ~Service(){}

  /**
   * setup a service
   * usally spawn a thread,
   * and run a server here
   * return SUCCESS when
   */
  virtual int start() = 0;


  virtual int shutdown() = 0;
};

#endif
