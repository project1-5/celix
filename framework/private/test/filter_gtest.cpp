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
/*
 * filter_test.cpp
 *
 *  \date       Feb 11, 2013
 *  \author     <a href="mailto:dev@celix.apache.org">Apache Celix Project Team</a>
 *  \copyright  Apache License, Version 2.0
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "gtest.h"
#include "gtest/gtest.h"
#include "gmock.h"
#include "gmock/gmock.h"
#include "../mock/celix_log_gmock.h"

extern "C" {
#include "filter_private.h"
#include "celix_log.h"

framework_logger_pt logger = (framework_logger_pt) 0x42;
}

MockCelixLog MCLObj;

int main(int argc, char** argv) {
	::testing::InitGoogleMock(&argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

static char* my_strdup(const char* s){
	if(s==NULL){
		return NULL;
	}

	size_t len = strlen(s);

	char *d = (char*) calloc (len + 1,sizeof(char));

	if (d == NULL){
		return NULL;
	}

	strncpy (d,s,len);
	return d;
}

//----------------FILTER TESTS----------------
TEST(filter, create_destroy){
	char * filter_str = my_strdup("(&(test_attr1=attr1)(|(test_attr2=attr2)(test_attr3=attr3)))");
	filter_pt get_filter;

	get_filter = filter_create(filter_str);
	ASSERT_NE(get_filter, NULL);
	filter_destroy(get_filter);

	//cleanup
	free(filter_str);
}

TEST(filter, match_operators){
	char * filter_str;
	filter_pt filter;
	properties_pt props = properties_create();
	char * key = my_strdup("test_attr1");
	char * val = my_strdup("attr1");
	char * key2 = my_strdup("test_attr2");
	char * val2 = my_strdup("attr2");
	properties_set(props, key, val);
	properties_set(props, key2, val2);

	//test EQUALS
	filter_str = my_strdup("(test_attr1=attr1)");
	filter = filter_create(filter_str);
	bool result = false;
	filter_match(filter, props, &result);
	ASSERT_TRUE(result);

	//test EQUALS false
	filter_destroy(filter);
	free(filter_str);
	filter_str = my_strdup("(test_attr1=falseString)");
	filter = filter_create(filter_str);
	result = true;
	filter_match(filter, props, &result);
	ASSERT_FALSE(result);

	//test APPROX TODO: update this test once APPROX is implemented
	filter_destroy(filter);
	free(filter_str);
	filter_str = my_strdup("(test_attr1~=attr1)");
	filter = filter_create(filter_str);
	result = false;
	filter_match(filter, props, &result);
	ASSERT_TRUE(result);

	//test APROX false TODO: update this test once APPROX is implemented
	filter_destroy(filter);
	free(filter_str);
	filter_str = my_strdup("(test_attr1~=ATTR1)");
	filter = filter_create(filter_str);
	result = true;
	filter_match(filter, props, &result);
	ASSERT_FALSE(result);

	//test PRESENT
	filter_destroy(filter);
	free(filter_str);
	filter_str = my_strdup("(test_attr1=*)");
	filter = filter_create(filter_str);
	result = false;
	filter_match(filter, props, &result);
	ASSERT_TRUE(result);

	//test PRESENT false
	filter_destroy(filter);
	free(filter_str);
	filter_str = my_strdup("(test_attr3=*)");
	filter = filter_create(filter_str);
	result = true;
	filter_match(filter, props, &result);
	ASSERT_FALSE(result);

	//test LESSEQUAL less
	filter_destroy(filter);
	free(filter_str);
	filter_str = my_strdup("(test_attr1<=attr5)");
	filter = filter_create(filter_str);
	result = false;
	filter_match(filter, props, &result);
	ASSERT_TRUE(result);

	//test LESSEQUAL equals
	filter_destroy(filter);
	free(filter_str);
	filter_str = my_strdup("(test_attr2<=attr2)");
	filter = filter_create(filter_str);
	result = false;
	filter_match(filter, props, &result);
	ASSERT_TRUE(result);

	//test LESSEQUAL false
	filter_destroy(filter);
	free(filter_str);
	filter_str = my_strdup("(test_attr2<=attr1)");
	filter = filter_create(filter_str);
	result = true;
	filter_match(filter, props, &result);
	ASSERT_FALSE(result);

	//test GREATEREQUAL greater
	filter_destroy(filter);
	free(filter_str);
	filter_str = my_strdup("(test_attr2>=attr1)");
	filter = filter_create(filter_str);
	result = false;
	filter_match(filter, props, &result);
	ASSERT_TRUE(result);

	//test GREATEREQUAL equals
	filter_destroy(filter);
	free(filter_str);
	filter_str = my_strdup("(test_attr2>=attr2)");
	filter = filter_create(filter_str);
	result = false;
	filter_match(filter, props, &result);
	ASSERT_TRUE(result);

	//test GREATEREQUAL false
	filter_destroy(filter);
	free(filter_str);
	filter_str = my_strdup("(test_attr1>=attr5)");
	filter = filter_create(filter_str);
	result = true;
	filter_match(filter, props, &result);
	ASSERT_FALSE(result);

	//test LESS less
	filter_destroy(filter);
	free(filter_str);
	filter_str = my_strdup("(test_attr1<attr5)");
	filter = filter_create(filter_str);
	result = false;
	filter_match(filter, props, &result);
	ASSERT_TRUE(result);

	//test LESS equals
	filter_destroy(filter);
	free(filter_str);
	filter_str = my_strdup("(test_attr2<attr2)");
	filter = filter_create(filter_str);
	result = true;
	filter_match(filter, props, &result);
	ASSERT_FALSE(result);

	//test LESS false
	filter_destroy(filter);
	free(filter_str);
	filter_str = my_strdup("(test_attr2<attr1)");
	filter = filter_create(filter_str);
	result = true;
	filter_match(filter, props, &result);
	ASSERT_FALSE(result);

	//test GREATER greater
	filter_destroy(filter);
	free(filter_str);
	filter_str = my_strdup("(test_attr2>attr1)");
	filter = filter_create(filter_str);
	result = false;
	filter_match(filter, props, &result);
	ASSERT_TRUE(result);

	//test GREATER equals
	filter_destroy(filter);
	free(filter_str);
	filter_str = my_strdup("(test_attr2>attr2)");
	filter = filter_create(filter_str);
	result = true;
	filter_match(filter, props, &result);
	ASSERT_FALSE(result);

	//test GREATER false
	filter_destroy(filter);
	free(filter_str);
	filter_str = my_strdup("(test_attr1>attr5)");
	filter = filter_create(filter_str);
	result = true;
	filter_match(filter, props, &result);
	ASSERT_FALSE(result);

	//test SUBSTRING equals
	filter_destroy(filter);
	free(filter_str);
	filter_str = my_strdup("(test_attr1=attr*)");
	filter = filter_create(filter_str);
	result = true;
	filter_match(filter, props, &result);
	ASSERT_TRUE(result);

	//test SUBSTRING false
	filter_destroy(filter);
	free(filter_str);
	filter_str = my_strdup("(test_attr1=attr*charsNotPresent)");
	filter = filter_create(filter_str);
	result = true;
	filter_match(filter, props, &result);
	ASSERT_FALSE(result);

	//cleanup
	properties_destroy(props);
	filter_destroy(filter);
	free(filter_str);
	free(key);
	free(key2);
	free(val);
	free(val2);

}