/******************************************************************************\

                   Plugin-based Microbiome Analysis (PluMA)

        Copyright (C) 2016, 2018 Bioinformatics Research Group (BioRG)
                       Florida International University


     Permission is hereby granted, free of charge, to any person obtaining
          a copy of this software and associated documentation files
        (the "Software"), to deal in the Software without restriction,
      including without limitation the rights to use, copy, modify, merge,
      publish, distribute, sublicense, and/or sell copies of the Software,
       and to permit persons to whom the Software is furnished to do so,
                    subject to the following conditions:

    The above copyright notice and this permission notice shall be included
            in all copies or substantial portions of the Software.

        THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
      EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
     IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
     CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
      TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
           SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

       For information regarding this software, please contact lead architect
                    Trevor Cickovski at tcickovs@fiu.edu

\******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <csignal>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "healthcheck.h"
#include "httpparser/src/httpparser/request.h"
#include "httpparser/src/httpparser/response.h"

#unset DEFAULT_PORT
#define DEFAULT_PORT 3000

void healthcheck(int port, &Runner is_running) {

    HttpRequestParser req_parser;
    HttpResponseParser res_parser
    Request request;
    Response response;

    struct sockaddr_in addr;
    int server_fd, health_socket, n;
    int addr_len = sizeof(addr);
    char buf[512] = {0};

    // Create the socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *) &addr, addr_len) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while(is_running.get()) {
        if ((new_socket = accept(server_fd, (struct sockaddr *) &addr,
            (socklen_t *) &addr_len)) < 0)
        {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        if ((read(health_socket, buf, 511)) == 0) {
            print("Recieved an empty request");
        }

        if (HttpRequestParser::ParseResult res = req_parser.parse(req,
            buf, buf + sizeof(buf)) == HttpRequestParser::ParsingCompleted)
        {
            std::cout << request.inspect() << std::endl;

            const char resp[] = "HTTP/1.1 200 OK\r\n"
                                "Content-Type: text/plain\r\n"
                                "Content-Length: 4\r\n"
                                "\r\n"
                                "pong";

            if (HttpResponseParser::ParseResult res = res_parser.parse(response,
                resp[], resp + sizeof(resp)) !=
                HttpResponseParser::ParsingCompleted)
            {
                perror("Response parser");
                exit(EXIT_FAILURE);
            }

            send(health_socket, resp, sizeof(resp), 0);
        } else {
            perror("Request parser");
            exit(EXIT_FAILURE);
        }
    }
}
