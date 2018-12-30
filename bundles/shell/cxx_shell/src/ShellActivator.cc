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

namespace {

    class LbCommand : public celix::IShellCommand {
    public:
        LbCommand(std::shared_ptr<celix::IBundleContext> _ctx) : ctx{std::move(_ctx)} {}
        void executeCommand(const std::string &/*command line*/, std::ostream &out, std::ostream &) noexcept override {
            //TODO parse commandLine
            out << "Bundles: " << std::endl;
            ctx->useBundles([&out](const celix::IBundle &bnd) {
                out << "|- " << bnd.id() << ": " << bnd.name() << std::endl;
            }, true);
        }
    private:
        std::shared_ptr<celix::IBundleContext> ctx;
    };

    celix::ServiceRegistration registerLb(std::shared_ptr<celix::IBundleContext> ctx) {
        celix::Properties props{};
        props[celix::IShellCommand::COMMAND_NAME] = "lb";
        props[celix::IShellCommand::COMMAND_USAGE] = "list installed bundles";
        props[celix::IShellCommand::COMMAND_DESCRIPTION] = "TODO";
        return ctx->registerService(std::shared_ptr<celix::IShellCommand>{new LbCommand{ctx}}, std::move(props));
    }

    celix::ServiceRegistration registerHelp(std::shared_ptr<celix::IBundleContext> ctx) {

        celix::ShellCommandFunction help = [ctx](const std::string &, std::ostream &out, std::ostream &) {

            std::string hasCommandNameFilter = std::string{"("} + celix::IShellCommand::COMMAND_NAME + "=*)";
            //TODO parse command line to see if details is requested instead of overview
            std::vector<std::string> commands{};
            ctx->useServices<celix::IShellCommand>([&](celix::IShellCommand&, const celix::Properties &props) {
                commands.push_back(celix::getProperty(props, celix::IShellCommand::COMMAND_NAME, "!Error!"));
            }, hasCommandNameFilter);

            hasCommandNameFilter = std::string{"("} + celix::SHELL_COMMAND_FUNCTION_COMMAND_NAME + "=*)";

            std::function<void(celix::ShellCommandFunction&, const celix::Properties&)> use = [&](celix::ShellCommandFunction&, const celix::Properties &props) {
                commands.push_back(celix::getProperty(props, celix::IShellCommand::COMMAND_NAME, "!Error!"));
            };
            ctx->useFunctionServices(celix::SHELL_COMMAND_FUNCTION_SERVICE_FQN, use, hasCommandNameFilter);

            //TODO useCService with a shell command service struct

            out << "Available commands: " << std::endl;
            for (auto &name : commands) {
                out << "|- " << name << std::endl;
            }
        };

        celix::Properties props{};
        props[celix::SHELL_COMMAND_FUNCTION_COMMAND_NAME] = "help";
        props[celix::SHELL_COMMAND_FUNCTION_COMMAND_USAGE] = "help [command name]";
        props[celix::SHELL_COMMAND_FUNCTION_COMMAND_DESCRIPTION] = "TODO";
        return ctx->registerFunctionService(celix::SHELL_COMMAND_FUNCTION_SERVICE_FQN, std::move(help), std::move(props));
    }

    class Shell : public celix::IShell {
    public:
        Shell(std::shared_ptr<celix::IBundleContext> _ctx) : ctx{std::move(_ctx)} {
            celix::ServiceTrackerOptions<celix::IShellCommand> opts1{};
            opts1.updateWithProperties = [this](std::vector<std::tuple<celix::IShellCommand*,const celix::Properties*>> services) {
                std::lock_guard<std::mutex> lck(commands.mutex);
                commands.commands = std::move(services);
            };
            trk1 = ctx->trackServices(opts1);

            celix::ServiceTrackerOptions<celix::ShellCommandFunction> opts2{};
            opts2.updateWithProperties = [this](std::vector<std::tuple<celix::ShellCommandFunction*,const celix::Properties*>> services) {
                std::lock_guard<std::mutex> lck(commands.mutex);
                commands.commandFunctions = std::move(services);
            };
            trk2 = ctx->trackFunctionServices(celix::SHELL_COMMAND_FUNCTION_SERVICE_FQN, opts2);
        }

        bool executeCommand(const std::string &commandLine, std::ostream &out, std::ostream &) noexcept override {
            out << "TODO call command '" << commandLine << "'" << std::endl;
            return false;
        }
    private:
        std::shared_ptr<celix::IBundleContext> ctx;

        celix::ServiceTracker trk1{};
        celix::ServiceTracker trk2{};

        struct {
            mutable std::mutex mutex{};
            std::vector<std::tuple<celix::IShellCommand*, const celix::Properties*>> commands;
            std::vector<std::tuple<celix::ShellCommandFunction*, const celix::Properties*>> commandFunctions;
        } commands{};
    };

    class ShellBundleActivator : public celix::IBundleActivator {
    public:
        bool start(std::shared_ptr<celix::IBundleContext> ctx) noexcept override {
            //TODO ensure fixed framework thread that call bundle activators
            registrations.push_back(registerLb(ctx));
            registrations.push_back(registerHelp(ctx));

            registrations.push_back(ctx->registerService(std::shared_ptr<celix::IShell>{new Shell{ctx}}));

            return true;
        }

        bool stop(std::shared_ptr<celix::IBundleContext>) noexcept override {
            registrations.clear();
            return true;
        }
    private:
        std::vector<celix::ServiceRegistration> registrations;
    };
}

__attribute__((constructor))
static void registerShellBundle() {
    celix::StaticBundleOptions opts{};
    opts.bundleActivatorFactory = [](){
        return new ShellBundleActivator{};
    };
    opts.manifest[celix::MANIFEST_BUNDLE_VERSION] = "1.0.0";
    celix::registerStaticBundle("celix::Shell", opts);
}