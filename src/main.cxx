/********************************************************************************\

                   Plugin-based Microbiome Analysis (PluMA)

        Copyright (C) 2016, 2018, 2019-2021 Bioinformatics Research Group (BioRG)
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

\*********************************************************************************/

#include <argp.h>
#include <dlfcn.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "PluMA.hxx"
#include "utils.hxx"
#include "Plugin.h"
#include "PluginProxy.h"

using namespace std;
namespace filesystem = std::filesystem;

/**
 * Actions the PluMA application can take.
 *
 * @since v2.1.0
 */
enum PLUMA_MAIN_ACTIONS {
    ACTION_PIPELINE,
    ACTION_PRINT_PLUGINS,
    ACTION_INSTALL_DEPENDENCIES
};

enum PLUMA_ERROR {
    SUCCESS = 00U,
    ERR_INSTALL_PYTHON_DEP = 01U,
    ERR_INSTALL_LINUX_DEP = 02U,
    ERR_INSTALL_MACOS_DEP = 03U,
    ERR_INSTALL_WINDOWS_DEP = 04U
};


// Auto-generated program version
const char* argp_program_version = "PluMA v2.2.0";
// Bug address for submitting issues. Github URL instead of email address.
const char* argp_program_bug_address = "<https://github.com/FIUBioRG/PluMA/issues>";
// Top-level doc block
static char doc[] = "PluMA - Plugin-Based Microbiome Analysis";
// Arguments documentation
static char args_doc[] = "[CONFIGURATION FILE] [OPTIONAL: RESTART POINT]";

/**
 * Commandline options
 *
 * @since v2.1.0
 */
static struct argp_option options[] = {
    {"verbose", 'v', 0, 0, "Produce verbose output"},
    {"plugins", 'p', 0, 0, "Prints a list of currently installed plugins in PLUMA_PLUGINS_PATH"},
    { 0 }
};

/**
 * Commandline arguments structure
 *
 * @since v2.1.0
 */
struct arguments {
    int verbose;
    char* config_file;
    char* restart_point;
    bool do_restart;
    PLUMA_MAIN_ACTIONS action;
};

/**
 * Parse single options.
 *
 * @since v2.1.0
 *
 * @param key Key that is being parsed.
 * @param arg Arguments character pointer.
 * @param state State of the argp processing.
 * @return An error if there is any form of error in parsing our options.
 */
static error_t parse_opt(int key, char* arg, struct argp_state *state) {
    struct arguments* arguments = (struct arguments*) state->input;

    switch(key) {
        case 'v':
            arguments->verbose = 1;
            break;

        case 'p':
            arguments->action = PLUMA_MAIN_ACTIONS::ACTION_PRINT_PLUGINS;
            break;

        case ARGP_KEY_ARG:
            if(state->arg_num >= 2) {
                // Too many arguments
                cerr << "Too many arguments specified." << endl;
                argp_usage(state);
            }
            if (state->arg_num == 0) {
                arguments->config_file = arg;
            }
            if (state->arg_num == 1) {
                arguments->do_restart = true;
                arguments->restart_point = arg;
            }
            break;

        case ARGP_KEY_END:
            if(state->arg_num < 1 && arguments->action == ACTION_PIPELINE) {
                // Not enough arguments
                argp_usage(state);
            }
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

// Parse our arguments
static struct argp argp = { options, parse_opt, args_doc, doc };

/**
 * Print the standard PluMA banner
 *
 * @since v2.1.0
 */
void print_banner() {
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Display opening screen.
    cout << "***********************************************************************************" << endl;
    cout << "*                                                                                 *" << endl;
    cout << "*  PPPPPPPPPPPP    L               U           U   M           M         A        *" << endl;
    cout << "*  P           P   L               U           U   MM         MM        A A       *" << endl;
    cout << "*  P           P   L               U           U   M M       M M       A   A      *" << endl;
    cout << "*  P           P   L               U           U   M  M     M  M      A     A     *" << endl;
    cout << "*  P           P   L               U           U   M   M   M   M     A       A    *" << endl;
    cout << "*  PPPPPPPPPPPP    L               U           U   M    M M    M    A         A   *" << endl;
    cout << "*  P               L               U           U   M     M     M   AAAAAAAAAAAAA  *" << endl;
    cout << "*  P               L               U           U   M           M   A           A  *" << endl;
    cout << "*  P               L               U           U   M           M   A           A  *" << endl;
    cout << "*  P               LLLLLLLLLLLLL    UUUUUUUUUUU    M           M   A           A  *" << endl;
    cout << "*                                                                                 *" << endl;
    cout << "*            [Plu]gin-based [M]icrobiome [A]nalysis (formerly MiAMi)              *" << endl;
    cout << "*                          (C) 2016, 2018, 2019-2021                              *" << endl;
    cout << "*          Bioinformatics Research Group, Florida International University        *" << endl;
    cout << "*    Under MIT License From Open Source Initiative (OSI), All Rights Reserved.    *" << endl;
    cout << "*                                                                                 *" << endl;
    cout << "*    Any professionally published work using PluMA should cite the following:     *" << endl;
    cout << "*                        T. Cickovski and G. Narasimhan.                          *" << endl;
    cout << "*                Constructing Lightweight and Flexible Pipelines                  *" << endl;
    cout << "*                 Using Plugin-Based Microbiome Analysis (PluMA)                  *" << endl;
    cout << "*                     Bioinformatics 34(17):2881-2888, 2018                       *" << endl;
    cout << "*                                                                                 *" << endl;
    cout << "***********************************************************************************" << endl;
}

mutex mtx;

/**
 * Parse single options.
 *
 * @since v2.2.0
 */
void healthcheck(bool &do_run)
{
    int data_socket;

    string socket_name = "/tmp/pluma.socket";

    int health_socket = socket(AF_UNIX, SOCK_STREAM, 0);

    if (health_socket == -1)
    {
        throw runtime_error("Unable to create health socket");
    }

    struct sockaddr_un name;

    memset(&name, 0, sizeof(name));

    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, socket_name.c_str(), sizeof(name.sun_path) - 1);

    int ret = bind(health_socket, (struct sockaddr *) &name, sizeof(name));

    if (ret == -1)
    {
        throw runtime_error("Failed to bind health socket");
    }

    ret = listen(health_socket, 20);

    if (ret == -1)
    {
        throw runtime_error("Failed to open listening on health socket");
    }

    for (;;)
    {
        mtx.lock();
        if (do_run == false)
        {
            mtx.unlock();
            break;
        }
        mtx.unlock();
        char buffer[1024];
        data_socket = accept(health_socket, NULL, NULL);

        if (data_socket == -1)
        {
            throw runtime_error("Failed to accept connections on health socket");
        }

        while (true)
        {
            ret = read(data_socket, buffer, sizeof(buffer));

            if (ret == -1)
            {
                throw runtime_error("Unable to read from health socket");
            }

            if (!strncmp(buffer, "GET", sizeof(buffer)))
            {
                break;
            }
        }

        time_t now = time(0);

        char *time = ctime(&now);

        string res = "HTTP/1.1 200 Ok\nDate: " ;
        res += time;
        res += "\nContent-Length: 0\n";

        ret = write(data_socket, res.c_str(), sizeof(res.c_str()));

        if (ret == -1)
        {
            throw runtime_error("Failed to write to socket");
        }
    }

    close(health_socket);
    unlink(socket_name.c_str());
}

int main(int argc, char** argv) {
    struct arguments arguments;
    PluMA pluma;

    // Default argument configuration
    arguments.verbose = 0;
    arguments.action = PLUMA_MAIN_ACTIONS::ACTION_PIPELINE;
    arguments.do_restart = false;
    arguments.restart_point = (char*) "";
    arguments.config_file = (char*) "";

    // Parse commandline arguments using argp library
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    // Print the PluMA banner if argument parsing is successful
    print_banner();

    PluginManager::supportedLanguages(pluma.pluginpath, argc, argv);

    switch(arguments.action) {
        // Print a list of installed plugins
        case PLUMA_MAIN_ACTIONS::ACTION_PRINT_PLUGINS:
            for (auto const& path: utils::split(pluma.pluginpath, ":")) {
                cout << "Currently installed plugins in " << path << ":" << endl;

                vector<string> plugins;

                for (auto const& plugin: filesystem::directory_iterator(path)) {
                    if (plugin.is_directory()) {
                        string p = plugin.path();
                        plugins.push_back(p.substr(p.find_last_of("/") + 1, p.length()));
                    }
                }

                sort(plugins.begin(), plugins.end());

                for (auto const &plugin : plugins) {
                    cout << "    " << plugin << endl;
                }
            }
            exit(EXIT_SUCCESS);
            break;

        // Follow normal execution
        case PLUMA_MAIN_ACTIONS::ACTION_PIPELINE:
        default:
            try {
                bool do_run = true;
                glob_t globbuf;
                auto paths = utils::split(pluma.pluginpath, ":");

                thread health_thread(healthcheck, ref(do_run));

                health_thread.detach();

                for (auto const &path : paths)
                {
                    for (unsigned int i = 0; i < PluginManager::supported.size(); i++)
                    {
                        PluginManager::supported[i]->loadPlugin(path, &globbuf, &(PluginManager::getInstance().pluginLanguages));
                    }

                    for (auto const &itr : PluginManager::getInstance().pluginLanguages)
                    {
                        PluginManager::getInstance().add(itr.first.substr(0, itr.first.length() - 6));
                    }
                }

                time_t t = time(0);
                struct tm *now = localtime(&t);

                string currentTime = to_string(now->tm_year + 1900) + "-" +
                                     to_string(now->tm_mon + 1) + "-" + to_string(now->tm_mday) +
                                     "@" + to_string(now->tm_hour) + ":" + to_string(now->tm_min) +
                                     ":" + to_string(now->tm_sec);

                string logfile = "logs/" + currentTime + ".log.txt";

                PluginManager::getInstance().setLogFile(logfile);

                pluma.read_config(arguments.config_file, "", arguments.do_restart, arguments.restart_point);

                // Clean up
                for (unsigned int i = 0; i < PluginManager::supported.size(); i++)
                {
                    PluginManager::supported[i]->unload();
                }

                mtx.lock();
                do_run = false;
                mtx.unlock();

                // El Finito!
                exit(EXIT_SUCCESS);
            } catch (const exception& e) {
                cerr << e.what() << endl;
                unlink("/tmp/pluma.socket");
                exit(EXIT_FAILURE);
            }
    }
}
