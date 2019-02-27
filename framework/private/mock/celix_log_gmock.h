#include <stdarg.h>

extern "C" {
#include "celix_errno.h"
#include "celix_log.h"
}

class CelixLogInterface {
    public:
        virtual ~CelixLogInterface(){}
        virtual void gmock_mock_framework_log(framework_logger_pt logger, framework_log_level_t level, const char *func, const char *file, int line, const char *fmsg) = 0;
};

class MockCelixLog : public CelixLogInterface {
    public:
        virtual ~MockCelixLog(){};
        //MOCK_METHOD6(gmock_mock_framework_log, void(framework_logger_pt logger, framework_log_level_t level, const char *func, const char *file, int line, const char *fmsg));
        virtual void gmock_mock_framework_log(framework_logger_pt logger, framework_log_level_t level, const char *func, const char *file, int line, const char *fmsg) {
            framework_log(logger, level, func, file, line, fmsg);
        }
};