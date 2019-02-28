# Report for assignment 4
Members:
- Johan Johan Sjölén
- Nikhil Modak

## Project
Name: Apache Celix
URL: https://celix.apache.org/
The Apache Celix project implements the OSGi specification for the C
and C++ languages.
OSGi is a specification for modular and dynamic
components first implemented for Java.

## Selected issue(s)
Title: Refactor CppUTest tests to gtest
URL: https://issues.apache.org/jira/projects/CELIX/issues/CELIX-447?filter=allopenissues

The issue is about moving the project over from CppUTest to GTest+GMock as the chosen testing platform.
The reasons for why are outlined in the issue:

> 1) more easier to integrate as a external project
> 2) much more heavily used than CppUTest
> 3) integrated support in the CLion IDE. This means that the IDE supports directly running/debugging individually testcases/testsuites.

## Onboarding experience

The building of the project was fine. Installing dependencies and so on was all described in sufficient detail.
However it was very difficult to run the tests of the project.
To succeed in running the tests we had to read several CMake files to find the required flags and in the end
we had to actually change the CMake files ourselves to get the tests to run (we have an issue about this in our Github with more details, see issue #2).
Many links in the repo lead to 404s, which was also annoying. There was a certain amount of documentation however, which helped.
The main communication channel was through a mailing list. These are usually more well-suited for long-term discussion and since our team 
did not have a lot of experience using one we did not end up contacting the team for support.

Additionally, there was plenty of documentation on how to install the actual Celix application and use it to create more submodules, but there was very little, if any, documentation directing us on how to locate + run test suites for the entire project.

## Existing test cases relating to refactored code
All of the test cases that utilize CppUTest (which should be all) will be affected by our refactoring.

We knew that there was no possibility for us to refactor all of the tests, so instead we focused on the test file framework/private/test/filter_test.cpp.

## A test log
This is the run of the test log before the refactorings took place. Since the refactorings are about changing the tests this is really
not something useful, but it does prove that we ran the tests.

```
Running tests...
Test project /home/johan/src/celix/build
      Start  1: run_test_dfi
 1/19 Test  #1: run_test_dfi ......................   Passed    0.01 sec
      Start  2: attribute_test
 2/19 Test  #2: attribute_test ....................   Passed    0.00 sec
      Start  3: bundle_cache_test
 3/19 Test  #3: bundle_cache_test .................   Passed    0.00 sec
      Start  4: bundle_context_test
 4/19 Test  #4: bundle_context_test ...............   Passed    0.00 sec
      Start  5: bundle_revision_test
 5/19 Test  #5: bundle_revision_test ..............   Passed    0.00 sec
      Start  6: bundle_test
 6/19 Test  #6: bundle_test .......................   Passed    0.00 sec
      Start  7: capability_test
 7/19 Test  #7: capability_test ...................   Passed    0.00 sec
      Start  8: celix_errorcodes_test
 8/19 Test  #8: celix_errorcodes_test .............   Passed    0.00 sec
      Start  9: filter_test
 9/19 Test  #9: filter_test .......................   Passed    0.00 sec
      Start 10: framework_test
10/19 Test #10: framework_test ....................   Passed    0.00 sec
      Start 11: manifest_parser_test
11/19 Test #11: manifest_parser_test ..............   Passed    0.00 sec
      Start 12: manifest_test
12/19 Test #12: manifest_test .....................   Passed    0.00 sec
      Start 13: requirement_test
13/19 Test #13: requirement_test ..................   Passed    0.00 sec
      Start 14: service_reference_test
14/19 Test #14: service_reference_test ............   Passed    0.00 sec
      Start 15: service_registration_test
15/19 Test #15: service_registration_test .........   Passed    0.00 sec
      Start 16: service_registry_test
16/19 Test #16: service_registry_test .............   Passed    0.00 sec
      Start 17: service_tracker_customizer_test
17/19 Test #17: service_tracker_customizer_test ...   Passed    0.00 sec
      Start 18: wire_test
18/19 Test #18: wire_test .........................   Passed    0.00 sec
      Start 19: run_test_framework
19/19 Test #19: run_test_framework ................   Passed    0.01 sec

100% tests passed, 0 tests failed out of 19

Total Test time (real) =   0.07 sec
```


## UML of refactoring
Since the structure of the code and test cases largely remain the same (consider that we swap out test cases) we will
showcase the structure difference when mocking C-functions, since this is a major difference to how it's done with CppUTest.

Diagram1: This shows the change required for mocking a C function with GTest in one case.
## Requirements

Requirements related to the functionality targeted by the refactoring are identified and described in a systematic way. Each requirement has a name (ID), title, and description, and other optional attributes (in case of P+, see below). The description can be one paragraph per requirement.

1. gtest-import Import Gtest into the project

First of all GTest needs to be imported such that it can be utilized for the tests. This will include
installing it on your personal system (this is a slight amount of work because only the headers are available in the Ubuntu package manager).


2. gmock-import Import GMock into the project

Now install GMock and import it into the project, same issues are current here. As with GTest, there is a bit of an installation process necessary to make sure it works. Instructions can be found at https://github.com/google/googletest/tree/master/googlemock. Importing just GMock should also import GTest for you.


3. test-refactor Refactor at least one test file into using GTest+GMock

Pick out a suitable testfile which at least does mocking and re-implement it with Gtest+Gmock. Mocking done for C function via CppUTest will need some additional refactoring for it to be able to be mocked by GMock. GMock only supports object-oriented mocking, meaning C functions will need to be wrapped with an interface with virtual funcitons denoting the functions to be mocked.

4. make-test Set-up `make test`

Running the tests should be done using `make test` just like it is done now for CppUTest


## What we've done

We did not finish the task. The main road block was in the project structure.
We did however:

1. Succesfully import GTest into the project
2. Succesfully import GMock into the project
3. Start a conversion of a test file, along with attempting  mocking a C function in Google mock

Finding out how to run the tests took approx. 3-5 hours of time by itself. Importing GTest took about the same time, however importing GMock did not take as long (however GMock would not build
in the project for one of our members).

When we re-wrote the tests it was discovered that CppUTest is used when mocking C-function calls. Gmock on the other hand is not capable of doing this -- it's a C++ tool only.
To mock C functions workarounds[WA] are required. More about this is described in the overall experience. If you want to see our attempt at mocking see the branch `mocking`.

[WA] https://stackoverflow.com/questions/31989040/can-gmock-be-used-for-stubbing-c-functions

Since we did not finish the refactoring we will be submitting our findings to the bugtracker and describing the pros and cons of switching over and what potential issues might occur.
See the link in the Project section to read the comment.

## What our code affects
Our code replaces 4 test cases (really multiple tests are in each test) from filter_test.cpp,
and tests the code in filter\_private.h along with creating new files celix\_log\_gmock.h.
We also added CMake language code (the procedural programming language used to build things with CMake) for finding GMock and GTest,
along with the standard boilerplate for including the code in the project.

# P+ Efforts

## Our changes and overall software architecture
One of the reasons behind changing over to Gtest and Gmock was to use tools which are in greater use.
This sort of change is well-funded with regards to an open source project: You want to lower the requirements of entering a project
and using dependencies people are used to will increase the likelihood of that happening.
Thus our changes could help with regards to the community of Apache Celix.

Our changes lead to us having to see how the mocking procedure works in two different frameworks.
A mocking framework can be implemented using a creational patttern such as the Factory pattern, where mocks are created by what is essentially fake objects.
Or it can be done such as in GoogleTest where a mocking class is declared which inherits from the regular class and uses metaprogramming (macros and templates)
to provide the substitution in calling code.

## System Architecture + Purpose

Purpose: 

Apache Celix is a C/C++ implementation of the Java OSGi framework standard. It allows users to develop modular services and applications compliant with OSGi’s component and service-oriented programming.
In OSGi, bundles are defined to be a group of classes and resources with a MANIFEST.MF file that have additional behaviors that allow them to act as one component. In Java, these bundles are normally manifested as jars, however with Celix, bundles are represented as zip files. Within the bundle, Celix users can expect to find a bundle activator, responsible for managing and building the lifecycle of the bundle. 
C is, of course, not an object-oriented programming language so in development, a mapping is used to convert C functions to Java functions. According to the Apache Celix website, one of the reasons C was chosen was because C can act as a common denominator for interoperability between a variety of service-oriented programming languages.

Usage/Architecture:

To create a Celix OSGi service, your service must follow a specific outline. A service name, service provider version and service consumer range should be declared in the header file of your service as macros. According to Celix documentation, macros are used to prevent symbols so to that no linking dependencies are introduced. A struct must be explicitly defined for every service you want to create. Components of said service should follow the ADT (Abstract Data Type) convention in C. An example is shown below:

```
//example.h
#ifndef EXAMPLE_H_
#define EXAMPLE_H_

#define EXAMPLE_NAME            "org.example"
#define EXAMPLE_VERSION         "1.0.0"
#define EXAMPLE_CONSUMER_RANGE  "[1.0.0,2.0.0)"


struct example_struct {
    void *handle;
    int (*method)(void *handle, int arg1, double arg2, double *result);
} ;

typedef struct example_struct example_t;

#endif /* EXAMPLE_H_ */
``` 

The Apache Celix project uses CMake, an application for managing the build process of your software. A simple configuration file (CMakeLists.txt) is used to define and generate build files similar to a Makefile. The outermost CMakeLists.txt file is where scripts to find depndencies on your machine are located. This is also where you link .cpp/.c files to appropriate executable build targets. The respective `framework/CMakeLists.txt` is where we defined the build rules for our new filter_gtest.cpp tests.




## Effort spent
For each team member, how much time was spent in
1.  plenary discussions/meetings;
- Johan 1h
- Nikhil 1h
2.  discussions within parts of the group;
Discussions within Github and Messenger (time is an approximation of course)
- Johan 2h
- Nikhil 2h
3.  reading documentation;
- Johan 3h
- Nikhil 2h
4.  configuration;
- Johan 2,5h
- Nikhil 1h
5.  analyzing code/output;
- Johan 2h
- Nikhil 1h
6.  writing documentation;
- Johan 5h
- Nikhil 30 min
7.  writing code;
- Johan 4h
- Nikhil 6h
8.  running code?
- Johan 2h
- Nikhil 1h


## Overall experience

This was our first assignment whose project used C/C++ as its main development languages, that turned out to be a major pain point.
Because of the different tooling (CMake/make contra Gradle or Maven) we had some initial issues setting things up for development.
The assignment also reinforced our belief that the initial install/build/test instructions of a project needs to be there and they need to be as
simple and painless as possible, at least for hobbyists. It's probably less of an issue if a software engineer takes some time setting up a project,
however if a team needs to evaluate several projects and pick one to use then we can see issues with building one project will put it behind the others.

Overall this went OK, a lot of what we did are hacks because we struggled with the project because of a lack of documentation and lack of communication channels (none of us are very experienced with mailing lists).

From a technical standpoint, the main bottleneck issue was the conversion from CppUTest mocking to GMock. GMock is very much geared towards testing C++ code as many times, it requires object-oriented behavior to mock functions. For example, in order to mock a basic static logging function, we had to attempt to create an abstract base class, and a mock class that could inherit from aforementioned abstract class. This mock class would then call the static logging function and act as a wrapper. Now, we could create a mock object of the class and interface with the rest of the GMock + GTest API. 
We approximate that each member would need at least 3 hours more of work for us to finish the whole filter_test.cpp conversion, this is considering the amount of time it'd take to learn both CppUTest and Google Test+Mock
to such a degree that we would be able to convert between the two, and the natural time required to ensure that the conversion has been done correctly.
