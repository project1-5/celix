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

#ifndef CXX_CELIX_UTILS_H
#define CXX_CELIX_UTILS_H


#include <string>
#include <iostream>

namespace {

    template<typename INTERFACE_TYPENAME>
    std::string typeName() {
        std::string result;

        const char *templateStr = "INTERFACE_TYPENAME = ";
        const size_t templateStrLen = strlen(templateStr);

        result = __PRETTY_FUNCTION__; //USING pretty function to retrieve the filled in template argument without using typeid()
        size_t bpos = result.find(templateStr) + templateStrLen; //find begin pos after INTERFACE_TYPENAME = entry
        size_t epos = bpos;
        while (isalnum(result[epos]) || result[epos] == '_' || result[epos] == ':' || result[epos] == ' ' || result[epos] == '*' || result[epos] == '&' || result[epos] == '<' || result[epos] == '>') {
            epos += 1;
        }
        size_t len = epos - bpos;
        result = result.substr(bpos, len);

        if (result.empty()) {
            std::cerr << "Cannot infer type name in function call '" << __PRETTY_FUNCTION__ << "'\n'";
        }

        return result;
    }

    template<typename Arg>
    std::string argName() {
        return typeName<Arg>(); //terminal;
    }

    template<typename Arg1, typename Arg2, typename... Args>
    std::string argName() {
        return typeName<Arg1>() + ", " + argName<Arg2, Args...>();
    }

    template<typename R>
    std::string functionName(const std::string &funcName) {
        return funcName + " [std::function<" + typeName<R>() + "()>]";
    }

    template<typename R, typename Arg1, typename... Args>
    std::string functionName(const std::string &funcName) {
        return funcName + " [std::function<" + typeName<R>() + "("  + argName<Arg1, Args...>() + ")>]";
    }
};

namespace celix {

    /* TODO
    template<typename I>
    typename std::enable_if<I::FQN, std::string>::type
    serviceName() {
        return I::FQN;
    }*/

    /**
    * Returns the service name for a type I
    */
    template<typename I>
    //NOTE C++17 typename std::enable_if<!std::is_callable<I>::value, std::string>::type
    std::string serviceName() {
        return typeName<I>();
    }

    /**
    * Returns the service name for a std::function I.
    * Note that for a std::function the additional function name is needed to get a fully qualified service name;
    */
    template<typename F>
    //NOTE C++17 typename std::enable_if<std::is_callable<I>::value, std::string>::type
    std::string functionServiceName(const std::string &fName) {
        return functionName<decltype(&F::operator())>(fName);
    }
}

#endif //CXX_CELIX_UTILS_H
