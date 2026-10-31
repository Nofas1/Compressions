#pragma once
#include <iostream>
#include <ostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <complex>
#include <cassert>
#include <set>
#include <ctime>
#include <memory>
#include <mutex>
#include <optional>

#include "My_types.h"

class Logger
{

public:
    enum Level
    {
        Debug = 1,
        Info,
        Warning,
        Error, // не упало
        Fatal // упало
    };

    // 
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    static void set_level(Level level)
    {
        get_instance().minLevel = level;
    }

    static void set_output(const std::string& path = "")
    {
        if (!path.empty()) {
            get_instance().opt_file.emplace(std::ofstream(path));
        }

        get_instance().stream_ptr = &std::cout;
        if (!path.empty()) {
            get_instance().stream_ptr = &get_instance().opt_file.value();
        }
    }

    static void debug(const std::string& message)
    {
        get_instance().log(Debug, message);
    }

    static void info(const std::string& message)
    {
        get_instance().log(Info, message);
    }

    static void warning(const std::string& message)
    {
        get_instance().log(Warning, message);
    }

    static void error(const std::string& message)
    {
        get_instance().log(Error, message);
    }

    static void fatal(const std::string& message)
    {
        get_instance().log(Fatal, message);
    }

    static Logger& get_instance()
    {
        static Logger instance;
        return instance;
    }

private:

    Logger() : minLevel(Info), stream_ptr(nullptr) {}
    // Logger(Level level, const std::string& path = "")
    // {
    //     minLevel = level;
    //     if (!path.empty()) {
    //         opt_file.emplace(std::ofstream(path));
    //     }

    //     stream_ptr = &std::cout;
    //     if (!path.empty()) {
    //         stream_ptr = &opt_file.value();
    //     }   
    // }

    void log(Level level, const std::string& message) {
        
        if (level < minLevel) return;

        *stream_ptr << message;
    }

    std::optional<std::ofstream> opt_file;
    std::ostream* stream_ptr;
    Level minLevel;
};
