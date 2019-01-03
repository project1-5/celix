/**
 *Licensed to the Apache Software Foundation (ASF) under one
 *or more contributor license agreements.  See the NOTICE file
 *distributed with this work for additional information
 *regarding copyright ownership.  The ASF licenses this file
 *to you under the Apache License, Version 2.0 (the
 *"License"); you may not use this file except in compliance
 *with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing,
 *software distributed under the License is distributed on an
 *"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 *specific language governing permissions and limitations
 *under the License.
 */

#include <thread>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <mutex>

#include <glog/logging.h>

#include "celix/api.h"
#include "celix/IShellCommand.h"
#include "celix/IShell.h"

static constexpr int LINE_SIZE = 256;
static constexpr const char * const PROMPT = "-> ";

static constexpr int KEY_ENTER = '\n';

namespace {

    class ShellTui {
    public:
        ShellTui() {
            int fds[2];
            int rc  = pipe(fds);
            if (rc == 0) {
                readPipeFd = fds[0];
                writePipeFd = fds[1];
                if(fcntl(writePipeFd, F_SETFL, O_NONBLOCK) == 0) {
                    readThread = std::thread{&ShellTui::runnable, this};
                } else {
                    LOG(ERROR) << "fcntl on pipe failed" << std::endl;
                }
            } else {
                LOG(ERROR) << "fcntl on pipe failed" << std::endl;
            }
        }

        ~ShellTui() {
            write(writePipeFd, "\0", 1); //trigger select to stop
            readThread.join();
        }

        void runnable() {
            //setup file descriptors
            fd_set rfds;
            int nfds = writePipeFd > STDIN_FILENO ? (writePipeFd +1) : (STDIN_FILENO + 1);

            for (;;) {
                writePrompt();
                FD_ZERO(&rfds);
                FD_SET(STDIN_FILENO, &rfds);
                FD_SET(readPipeFd, &rfds);

                if (select(nfds, &rfds, NULL, NULL, NULL) > 0) {
                    if (FD_ISSET(readPipeFd, &rfds)) {
                        break; //something is written to the pipe -> exit thread
                    } else if (FD_ISSET(STDIN_FILENO, &rfds)) {
                       parseInput();
                    }
                }
            }
        }

        void writePrompt() {
            std::cout << PROMPT;
            std::flush(std::cout);
        }

        void parseInput() {
            char* line = NULL;
            int nr_chars = (int)read(STDIN_FILENO, buffer, LINE_SIZE-pos-1);
            for (int bufpos = 0; bufpos < nr_chars; bufpos++) {
                if (buffer[bufpos] == KEY_ENTER) { //end of line -> forward command
                    line = in; // todo trim string
                    std::lock_guard<std::mutex> lck{mutex};
                    if (shell) {
                        shell->executeCommandLine(line, std::cout, std::cerr);
                    } else {
                        std::cerr << "Shell service not available\n";
                    }
                    pos = 0;
                    in[pos] = '\0';
                } else { //text
                    in[pos] = buffer[bufpos];
                    in[pos + 1] = '\0';
                    pos++;
                    continue;
                }
            } //for
        }

        void setShell(std::shared_ptr<celix::IShell> _shell) {
           std::lock_guard<std::mutex> lck{mutex};
           shell = _shell;
        }
    private:
        std::mutex mutex{};
        std::shared_ptr<celix::IShell> shell{};

        std::thread readThread{};

        int readPipeFd{};
        int writePipeFd{};


        char in[LINE_SIZE+1]{};
        char buffer[LINE_SIZE+1]{};
        int pos{};
    };


    class ShellTuiBundleActivator : public celix::IBundleActivator {
    public:
        ShellTuiBundleActivator(std::shared_ptr<celix::BundleContext> ctx) {
            celix::ServiceTrackerOptions<celix::IShell> opts{};
            opts.set = std::bind(&ShellTui::setShell, &tui, std::placeholders::_1);
            trk = ctx->trackServices(opts);
        }
    private:
        ShellTui tui{};
        celix::ServiceTracker trk{};
    };

    __attribute__((constructor))
    static void registerShellBundle() {
        celix::Properties manifest{};
        manifest[celix::MANIFEST_BUNDLE_NAME] = "Shell Tui";
        manifest[celix::MANIFEST_BUNDLE_GROUP] = "Celix";
        manifest[celix::MANIFEST_BUNDLE_VERSION] = "1.0.0";
        celix::registerStaticBundle<ShellTuiBundleActivator>("celix::ShellTui", manifest);
    }
}