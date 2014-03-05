#include "tm_scope.h"

#include "asem.h"
#include <cstdio>


void Mutex::lock() {
  while(cas(&state, Free, Busy) != Free) {   
    do { PAUSE(); } while(state == Busy);
  }   
}

void Mutex::unlock() {
  set_mb(&state, Free);
}

bool Mutex::isLocked() const { 
  return state == Busy; 
}

void Transaction::TransactionStart() { 
  int nretries = 0;

  while(1) {
    ++nretries;
    unsigned int status = _xbegin();

    if(status == _XBEGIN_STARTED) {
      if(!(fallBackLock.isLocked())) return;  
      _xabort(0xff);                         
    }

    // handle _xabort(0xff) from above
    if((status & _XABORT_EXPLICIT) && _XABORT_CODE(status)==0xff && !(status & _XABORT_NESTED)) {
      while(fallBackLock.isLocked()) PAUSE(); // to eliminate lemming effect, wait for fallback lock to be free
    } else if(!(status & _XABORT_RETRY)) break; // check hardware register for suggestion, grab the fallback lock if it is free

    if(nretries - 1 >= max_retries) break; // go to fallback path, retried too many timest
  }

  fallBackLock.lock();
}

void Transaction::TransactionEnd() {
  if(fallBackLock.isLocked())
    fallBackLock.unlock();
  else
    _xend();
}

bool Transaction::isLocked() {
  return fallBackLock.isLocked();
}
