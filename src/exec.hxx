#ifndef _EXEC_H
#define _EXEC_H

#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

namespace pluma {
    class execbuf : public std::streambuf {
    protected:
        std::string output;

        int_type underflow(int_type character) {
            if (gptr() < egptr()) {
                return traits_type::to_int_type(*gptr());
            }
            return traits_type::eof();
        }

    public:
        execbuf(const char *command) {
            std::array<char, 128> buffer;
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command, "r"), pclose);

            if (!pipe) {
                throw std::runtime_error("Failed to open pipe");
            }

            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
                this->output += buffer.data();
            }

            setg((char *)this->output.data(), (char *)this->output.data(), (char *)(this->output.data() + this->output.size()));
        }
    };
}
#endif
