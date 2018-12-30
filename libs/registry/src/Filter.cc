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

#include "celix/Filter.h"

#include <glog/logging.h>

static celix::FilterCriteria* filter_parseFilter(const char *filterString, int *pos);
static void filter_skipWhiteSpace(const char *filterString, int *pos);
static celix::FilterCriteria* filter_parseAndOrOr(const char *filterString, celix::FilterOperator andOrOr, int *pos);
static celix::FilterCriteria* filter_parseFilterComp(const char *filterString, int *pos);
static celix::FilterCriteria* filter_parseItem(const char *filterString, int *pos);
static std::string filter_parseAttr(const char *filterString, int *pos);
static std::string filter_parseValue(const char *filterString, int *pos);

static void filter_skipWhiteSpace(const char *filterString, int *pos) {
    size_t length;
    for (length = strlen(filterString); (*pos < (int)(length)) && isspace(filterString[*pos]);) {
        (*pos)++;
    }
}

static celix::FilterCriteria* filter_parseAndOrOr(const char *filterString, celix::FilterOperator andOrOr, int *pos) {

    filter_skipWhiteSpace(filterString, pos);
    bool failure = false;

    if (filterString[*pos] != '(') {
        LOG(ERROR) << "Filter Error: Missing '('.\n";
        return nullptr;
    }

    std::vector<std::shared_ptr<celix::FilterCriteria>> subcriteria{};
    while (filterString[*pos] == '(') {
        celix::FilterCriteria *cr = filter_parseFilter(filterString, pos);
        if (cr == nullptr) {
            failure = true;
            break;
        }
        subcriteria.push_back(std::unique_ptr<celix::FilterCriteria>{cr});
    }


    celix::FilterCriteria* criteria = nullptr;
    if (!failure) {
        criteria = new celix::FilterCriteria{
            .attribute = "",
            .op = andOrOr,
            .value = "",
            .subcriteria = std::move(subcriteria)
        };
    }

    return criteria;
}

static celix::FilterCriteria* filter_parseNot(const char * filterString, int * pos) {
    celix::FilterCriteria *criteria = nullptr;

    filter_skipWhiteSpace(filterString, pos);
    if (filterString[*pos] != '(') {
        LOG(ERROR) << "Filter Error: Missing '('.\n";
        return nullptr;
    }

    std::vector<std::shared_ptr<celix::FilterCriteria>> subcriteria{};
    celix::FilterCriteria *cr = filter_parseFilter(filterString, pos);
    if (cr != nullptr) {
        subcriteria.push_back(std::unique_ptr<celix::FilterCriteria>(cr));
        criteria = new celix::FilterCriteria {
            .attribute = "",
            .op = celix::FilterOperator::NOT,
            .value = "",
            .subcriteria = std::move(subcriteria)
        };
    }

    return criteria;
}

static std::string filter_parseAttr(const char *filterString, int *pos) {
    std::string attr{};
    char c;
    int begin = *pos;
    int end = *pos;
    int length = 0;

    filter_skipWhiteSpace(filterString, pos);
    c = filterString[*pos];

    while (c != '~' && c != '<' && c != '>' && c != '=' && c != '(' && c != ')') {
        (*pos)++;

        if (!isspace(c)) {
            end = *pos;
        }

        c = filterString[*pos];
    }

    length = end - begin;

    if (length == 0) {
        LOG(ERROR) << "Filter Error: Missing attr.\n";
    } else {
        attr.assign(filterString+begin, (size_t)length);
    }

    return attr;
}

static std::string filter_parseValue(const char *filterString, int *pos) {
    std::string val{};
    int keepRunning = 1;

    while (keepRunning) {
        char c = filterString[*pos];

        switch (c) {
            case ')': {
                keepRunning = 0;
                break;
            }
            case '(': {
                LOG(ERROR) << "Filter Error: Invalid value.\n";
                val = "";
                keepRunning = 0;
                break;
            }
            case '\0':{
                LOG(ERROR) << "Filter Error: Unclosed bracket.\n";
                val = "";
                keepRunning = 0;
                break;
            }
            case '\\': {
                (*pos)++;
                c = filterString[*pos];
                char ch[2];
                ch[0] = c;
                ch[1] = '\0';
                val.append(ch);
                (*pos)++;
                break;
            }
            default: {
                char ch[2];
                ch[0] = c;
                ch[1] = '\0';
                val.append(ch);
                (*pos)++;
                break;
            }
        }
    }

    if (val.empty()) {
        LOG(ERROR) << "Filter Error: Missing value.\n";
    }
    return val;
}

static celix::FilterCriteria* filter_parseItem(const char * filterString, int * pos) {
    celix::FilterCriteria* criteria = nullptr;

    std::string attr = filter_parseAttr(filterString, pos);
    if (attr.empty()) {
        return nullptr;
    }

    filter_skipWhiteSpace(filterString, pos);
    switch(filterString[*pos]) {
        case '~': {
            if (filterString[*pos + 1] == '=') {
                *pos += 2; //skip ~=
                std::string val = filter_parseValue(filterString, pos);
                if (val.empty()) {
                    LOG(ERROR) << "Unexpected emtpy value after ~= operator in filter '" << filterString << "'\n";
                } else {
                    criteria = new celix::FilterCriteria{
                            .attribute = std::move(attr),
                            .op = celix::FilterOperator::APPROX,
                            .value = std::move(val),
                            .subcriteria = {}
                    };
                }
            } else {
                LOG(ERROR) << "Unexpected ~ char without the expected = in filter '" << filterString << "'\n";
            }
            break;
        }
        case '>': {
            auto op = celix::FilterOperator::GREATER;
            if (filterString[*pos + 1] == '=') {
                *pos += 2; //skip >=
                op = celix::FilterOperator::GREATER_EQUAL;
            } else {
                *pos += 1; //skip >
            }
            std::string val = filter_parseValue(filterString, pos);
            if (val.empty()) {
                LOG(ERROR) << "Unexpected empty value in > or >= operator for filter '" << filterString << "'\n";
            } else {
                criteria = new celix::FilterCriteria{
                        .attribute = std::move(attr),
                        .op = op,
                        .value = std::move(val),
                        .subcriteria = {}
                };
            }
            break;
        }
        case '<': {
            celix::FilterOperator op = celix::FilterOperator::LESS;
            if (filterString[*pos + 1] == '=') {
                *pos += 2; //skip <=
                op = celix::FilterOperator::LESS_EQUAL;
            } else {
                *pos += 1; //skip <
            }
            std::string val = filter_parseValue(filterString, pos);
            if (val.empty()) {
                LOG(ERROR) << "Unexpected emtpy value after < or <= operator in filter '" << filterString <<"'\n";
            } else {
                criteria = new celix::FilterCriteria {
                        .attribute = std::move(attr),
                        .op = op,
                        .value = std::move(val),
                        .subcriteria = {}
                };
            }
            break;
        }
        case '=': {
            if (filterString[*pos + 1] == '*') {
                int oldPos = *pos;
                *pos += 2; //skip =*
                filter_skipWhiteSpace(filterString, pos);
                if (filterString[*pos] == ')') {
                    criteria = new celix::FilterCriteria{
                            .attribute = attr,
                            .op = celix::FilterOperator::PRESENT,
                            .value = "",
                            .subcriteria = {}
                    };
                } else {
                    *pos = oldPos; //no present criteria -> reset
                }
            }
            if (criteria == nullptr) { //i.e. no present criteria set
                //note that the value can start and end with a wildchar *. this is not parsed specifically, but tested
                *pos += 1; //skip =
                filter_skipWhiteSpace(filterString, pos);
                std::string val = filter_parseValue(filterString, pos);
                bool substring = val.size() > 2 && (val.front() == '*' || val.back() == '*');
                if (val.empty()) {
                    LOG(ERROR) << "Unexpected emtpy value after = operator in filter '" << filterString << "'\n";
                } else {
                    criteria = new celix::FilterCriteria{
                            .attribute = std::move(attr),
                            .op = substring ? celix::FilterOperator::SUBSTRING : celix::FilterOperator::EQUAL,
                            .value = std::move(val),
                            .subcriteria = {}
                    };
                }
            }
            break;
        }
        default: {
            LOG(ERROR) << "Unexpected operator " << *pos << std::endl;
            break;
        }
    }

    if (criteria == nullptr) {
        LOG(ERROR) << "Filter Error: Invalid operator.\n";
    }

    return criteria;
}

static celix::FilterCriteria* filter_parseFilterComp(const char *filterString, int *pos) {
    celix::FilterCriteria* criteria = nullptr;
    char c;
    filter_skipWhiteSpace(filterString, pos);

    c = filterString[*pos];

    switch (c) {
        case '&': {
            (*pos)++;
            criteria = filter_parseAndOrOr(filterString, celix::FilterOperator::AND, pos);
            break;
        }
        case '|': {
            (*pos)++;
            criteria = filter_parseAndOrOr(filterString, celix::FilterOperator::OR, pos);
            break;
        }
        case '!': {
            (*pos)++;
            criteria = filter_parseNot(filterString, pos);
            break;
        }
        default : {
            criteria =filter_parseItem(filterString, pos);
            break;
        }
    }
    return criteria;
}


static celix::FilterCriteria* filter_parseFilter(const char *filterString, int *pos) {
    celix::FilterCriteria *criteria = nullptr;
    filter_skipWhiteSpace(filterString, pos);
    if (filterString[*pos] != '(') {
        LOG(ERROR) << "Filter Error: Missing '(' in filter string '" << filterString << "'.\n";
        return NULL;
    }
    (*pos)++; //consume (

    criteria = filter_parseFilterComp(filterString, pos);

    filter_skipWhiteSpace(filterString, pos);

    if (filterString[*pos] != ')') {
        LOG(ERROR) << "Filter Error: Missing ')' in filter string '" << filterString << "'.\n";
        if (criteria != NULL) {
            delete criteria;
        }
        return NULL;
    }
    (*pos)++;
    filter_skipWhiteSpace(filterString, pos);

    return criteria;
}

celix::FilterCriteria* celix::Filter::parseFilter() {
    celix::FilterCriteria* criteria = nullptr;
    if (!filterStr.empty()) {
        int pos = 0;
        criteria = filter_parseFilter(filterStr.c_str(), &pos);
        if (criteria != nullptr && pos != (int) filterStr.size()) {
            LOG(ERROR) << "Filter Error: Missing '(' in filter string '" << filterStr << "'.'n";
            delete criteria;
            criteria = nullptr;
        }
    }
    return criteria;
}

static bool match_criteria(const celix::Properties &props, const celix::FilterCriteria &criteria) {
    bool result;
    switch (criteria.op) {
        case celix::FilterOperator::AND: {
            result = true;
            for (const auto &sc : criteria.subcriteria) {
                if (!match_criteria(props, *sc)) {
                    result = false;
                    break;
                }
            }
            break;
        }
        case celix::FilterOperator::OR: {
            result = false;
            for (const auto &sc : criteria.subcriteria) {
                if (match_criteria(props, *sc)) {
                    result = true;
                    break;
                }
            }
            break;
        }
        case celix::FilterOperator::NOT: {
            result = !match_criteria(props, *criteria.subcriteria[0]);
            break;
        }
        case celix::FilterOperator::SUBSTRING: {
            const std::string &val = celix::getProperty(props, criteria.attribute, "");
            if (val.empty()) {
                result = false;
            } else {
                bool wildcharAtFront = criteria.value.front() == '*';
                bool wildcharAtBack = criteria.value.back() == '*';
                std::string needle = wildcharAtFront ? criteria.value.substr(1) : criteria.value;
                needle = wildcharAtBack ? needle.substr(0, needle.size() - 1) : needle;
                const char *ptr = strstr(val.c_str(), needle.c_str());
                if (wildcharAtFront && wildcharAtBack) {
                    result = ptr != nullptr;
                } else if (wildcharAtFront) { //-> end must match
                    result = ptr != nullptr && ptr + strlen(ptr) == val.c_str() + strlen(val.c_str());
                } else { //wildCharAtBack -> begin must match
                    result = ptr != nullptr && ptr == val.c_str();
                }
            }
            break;
        }
        case celix::FilterOperator::EQUAL: {
            const std::string &val = celix::getProperty(props, criteria.attribute, "");
            result = !val.empty() && val == criteria.value;
            break;
        }
        case celix::FilterOperator::GREATER_EQUAL: {
            const std::string &val = celix::getProperty(props, criteria.attribute, "");
            result = !val.empty() && val >= criteria.value;
            break;
        }
        case celix::FilterOperator::LESS: {
            const std::string &val = celix::getProperty(props, criteria.attribute, "");
            result = !val.empty() && val < criteria.value;
            break;
        }
        case celix::FilterOperator::LESS_EQUAL: {
            const std::string &val = celix::getProperty(props, criteria.attribute, "");
            result = !val.empty() && val <= criteria.value;
            break;
        }
        case celix::FilterOperator::PRESENT: {
            const std::string &val = celix::getProperty(props, criteria.attribute, "");
            result = !val.empty();
            break;
        }
        case celix::FilterOperator::APPROX: {
            const std::string &val = celix::getProperty(props, criteria.attribute, "");
            result = !val.empty() && strcasecmp(val.c_str(), criteria.value.c_str()) == 0;
            break;
        }
        default: {
            result = false;
            break;
        }
    }

    return result;
}

bool celix::Filter::match(const celix::Properties &props) const {
    if (!valid()) {
        return false;
    } else if (filterStr.empty()) {
        return true;
    }

    return match_criteria(props, *criteria);
}