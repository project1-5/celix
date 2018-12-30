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

#ifndef CXX_CELIX_FILTER_H
#define CXX_CELIX_FILTER_H

#include <string>
#include <vector>

#include "celix/Properties.h"

namespace celix {

    enum class FilterOperator {
        EQUAL,
        APPROX,
        GREATER,
        GREATER_EQUAL,
        LESS,
        LESS_EQUAL,
        PRESENT,
        SUBSTRING,
        AND,
        OR,
        NOT
    };

    struct FilterCriteria; //forward declr
    struct FilterCriteria {
        const std::string attribute;
        const celix::FilterOperator op;
        const std::string value;
        const std::vector<std::shared_ptr<FilterCriteria>> subcriteria;
    };

    class Filter {
    public:
        Filter(const char* f) : filterStr{f}, criteria{parseFilter()} {}
        Filter(std::string f) : filterStr{std::move(f)}, criteria{parseFilter()} {}
        Filter(const Filter &rhs) = default;
        Filter(Filter &&rhs) = default;

        Filter& operator=(const Filter& rhs) = default;
        Filter& operator=(Filter&& rhs) = default;

        bool empty() const { return filterStr.empty(); }
        bool valid() const { return filterStr.empty() || criteria != nullptr; }
        bool match(const celix::Properties &props) const;

        const std::string filterStr;
        const std::shared_ptr<FilterCriteria> criteria;
    private:
        FilterCriteria* parseFilter();
    };
}

#endif //CXX_CELIX_FILTER_H
