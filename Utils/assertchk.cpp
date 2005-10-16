
#include <iostream>
#include <cassert>

int main (int argc, char *argv[])

{

  std::ios::sync_with_stdio(true) ;

  // Extract the streambuf object from cout
  std::streambuf *coutBuf = std::cout.rdbuf() ;

  // Install it in cerr
  std::cerr.rdbuf(coutBuf) ;

  std::cout << "1 " ;
  std::cerr << "2 " ;
  std::cout << "3 " ;
  std::cout << "4 " ;
  std::cerr << "5 " ;
  std::cout << "6 " ;
  std::cout << "7 " ;
  std::cerr << "8 " ;
  std::cout << "9 " ;
  std::cout << "a " ;
  std::cerr << "b " ;
  std::cout << "c " ;
  // assert(false) ;

  return (0) ; }
