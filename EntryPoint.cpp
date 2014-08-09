#include "PrecompiledHeaders.hpp"
#include "dk2test.hpp"

void error(std::string message) {
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error!", message.c_str(), NULL);

#ifdef _WIN32
  ExitProcess(0);
#else
  throw std::exception();
#endif
}

void notice(std::string message) {
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Fatal Error!", message.c_str(), NULL);
}


int main(int argc, char* argv[]) {
  auto path = boost::filesystem::current_path();
  boost::filesystem::current_path(path / "..");

  dk2test instance;
  instance.loop();
  return 0;
}
