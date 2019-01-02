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

#include "celix/api.h"
#include "celix/IShellCommand.h"
#include "celix/IShell.h"

#include "commands.h"

namespace {

    class Shell : public celix::IShell {
    public:
        Shell(std::shared_ptr<celix::IBundleContext> _ctx) : ctx{std::move(_ctx)} {}

        bool executeCommandLine(const std::string &commandLine, std::ostream &out, std::ostream &err) noexcept override {
            std::string cmdName{};
            std::vector<std::string> cmdArgs{};

            char *savePtr = nullptr;
            char *cl = strndup(commandLine.c_str(), 1024*1024);
            char *token = strtok_r(cl, " ", &savePtr);
            while (token != nullptr) {
                if (cmdName.empty()) {
                    cmdName = std::string{token};
                } else {
                    cmdArgs.emplace_back(std::string{token});
                }
                token = strtok_r(nullptr, " ", &savePtr);
            }

            bool commandCalled = false;

            if (!cmdName.empty()) {
                std::string filter =
                        std::string{"("} + celix::IShellCommand::COMMAND_NAME + "=" + cmdName + ")";
                commandCalled = ctx->useService<celix::IShellCommand>([&](celix::IShellCommand &cmd) {
                    cmd.executeCommand(cmdName, cmdArgs, out, err);
                }, filter);
            }
            if (!cmdName.empty() && !commandCalled) {
                std::string filter =
                        std::string{"("} + celix::SHELL_COMMAND_FUNCTION_COMMAND_NAME + "=" + cmdName + ")";
                std::function<void(celix::ShellCommandFunction&)> use = [&](celix::ShellCommandFunction &cmd) -> void {
                    cmd(cmdName, cmdArgs, out, err);
                };
                commandCalled = ctx->useFunctionService(celix::SHELL_COMMAND_FUNCTION_SERVICE_FQN, use, filter);
            }

            //TODO C command service struct
            if (!cmdName.empty() && !commandCalled) {
                out << "Command '" << cmdName << "' not available. Type 'help' to see a list of available commands." << std::endl;
            }


            return commandCalled;
        }
    private:
        std::shared_ptr<celix::IBundleContext> ctx;
    };

    class ShellBundleActivator : public celix::IBundleActivator {
    public:
        ShellBundleActivator(std::shared_ptr<celix::IBundleContext> ctx) {
            //TODO ensure fixed framework thread that call ctor/dtor bundle activators
            registrations.push_back(impl::registerLb(ctx));
            registrations.push_back(impl::registerHelp(ctx));
            registrations.push_back(impl::registerStop(ctx));
            registrations.push_back(impl::registerStart(ctx));
            registrations.push_back(impl::registerInspect(ctx));

            registrations.push_back(ctx->registerService(std::shared_ptr<celix::IShell>{new Shell{ctx}}));
        }
    private:
        std::vector<celix::ServiceRegistration> registrations{};
    };

    __attribute__((constructor))
    static void registerShellBundle() {
        celix::Properties manifest{};
        manifest[celix::MANIFEST_BUNDLE_NAME] = "Shell";
        manifest[celix::MANIFEST_BUNDLE_GROUP] = "Celix";
        manifest[celix::MANIFEST_BUNDLE_VERSION] = "1.0.0";
        celix::registerStaticBundle<ShellBundleActivator>("celix::Shell", manifest);
    }
}
