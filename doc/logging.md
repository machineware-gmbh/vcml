# VCML Logging
VCML uses a centralized logging system provided by [libmwr](https://github.com/machineware-gmbh/mwr) 
that allows its individual components and models to report on everything that happens during the simulation. 
Log messages are split into four categories, listed below in priority order:

* `mwr::LOG_ERROR`: highest priority, used for reporting on errors, e.g., when
an exception is caught.
* `mwr::LOG_WARNING`: used for reporting abnormal events, such as missing files
or erroneous configurations.
* `mwr::LOG_INFO`: used for generic info messages, e.g., for printing out the
simulation duration or bus memory maps at the beginning of the simulation.
* `mwr::LOG_DEBUG`: for printing debug messages

Log message output adheres the following format:

`[<LEVEL> <TIME>] <SOURCE>: <MESSAGE>`

In this format, `<LEVEL>` refers to the log level and is shortened to its first letter, 
i.e., `E`, `W`, `I`, or `D`. After the log level, 
the current SystemC time is printed with nanosecond resolution. 
The `<SOURCE>` of the log message is automatically determined, 
depending on the log function you called (see below).
A full log message could therefore look like this:

`[D 1.000000200] system.uart0: component disabled`

This tells us that there was a debug message at *1s + 200ns*, 
informing us that the component `system.uart0` has been disabled.

Log messages can be generated using the global top level logging function-like macros from the `mwr` namespace. 
They use a `printf` style for string formatting:

```
namespace mwr {
    log_error(...);
    log_warn(...);
    log_info(...);
    log_debug(...);
}

```

----
## Logging Output
In order to actually see log messages, you need a publisher backend that will receive the messages and display them in some form. 
[Libmwr](https://github.com/machineware-gmbh/mwr) currently provides three backends:

* `mwr::publishers::terminal`: write the log message to `stderr` using color if available.
* `mwr::publishers::file`: write log messages to a file specified during construction.
* `mwr::publishers::stream`: sends the log messages to any `std::ostream` instance.

Each publisher can furthermore also specify the log levels it is interested in.
This is done by calling the appropriate `set_level` methods as shown below:

```
mwr::publishers::file my_logger("log.txt");
my_logger.set_level(mwr::LOG_ERROR); // only receives error messages
my_logger.set_level(mwr::LOG_ERROR, mwr::LOG_INFO); // receives error, warning and info messages
```

Having multiple publishers for different purposes is possible and encouraged. 
You could have for example a logger that exclusively receives messages from the `mwr::LOG_DEBUG` level and writes them to a files. 
Then you could have a second logger to print all other messages to console:

```
#include <vcml.h>

int sc_main(int argc, char** argv) {
    mwr::publishers::file debug("debug.txt");
    debug.set_level(vcml::LOG_DEBUG);

    mwr::publishers::term terminal;
    terminal.set_level(vcml::LOG_ERROR, vcml::LOG_INFO);

    ...
}
```

Log printing is customizable to a certain extent, 
by setting the following static print control variables of `mwr::publisher::print_xxx` before or during simulation:

| Print control       | Description                               | Default |
|---------------------|-------------------------------------------|---------|
| `print_timestamp`   | Print the timestamp of the log message    | `true`  |
| `print_sender`      | Print sending process or module           | `true`  |
| `print_source`      | Print the source location of the log call | `false` |
| `print_backtrace`   | Print backtraces for logged reports       | `true`  |

It is also possible to create your own publisher if you need to process [libmwr](https://github.com/machineware-gmbh/mwr) log messages in a customizable way. 
To define a custom logger, derive from`mwr::publisher` and override `mwr::publisher::publish`:

```
class my_publisher: public mwr::publisher {
public:
    my_publisher() ...
    virtual ~my_publisher() ...
    virtual void publish(const logmsg& msg) override;
};
```

When you are not using a custom `main` function, a default one is provided for you that accepts the following logging related program arguments:

* `-l <file>` or `--log-file <file>`: creates a `mwr::publishers::file(<file>)`
* `-l` or `--log`: without an extra filename, creates a `vcml::log_term`
* `--log-stdout`: Send log output to stdout
* `--log-file`: Send log output to file
* `--log-debug`: elevates the log level of all loggers to `LOG_DEBUG`

----
## Exceptions
The logging system is typically also used for exception reporting. 
[Libmwr](https://github.com/machineware-gmbh/mwr) generally uses the class `mwr::report` for exceptions, 
so a basic approach to exception handling would be:

```
int sc_main(int argc, char** argv) {
    mwr::logger logger;
    try {
        // construct system
        sc_core::sc_start();
        return 0;
    } catch (mwr::report& r) {
        logger.error(r);
        return EXIT_FAILURE;
    } catch (std::exception& e) {
        mwr::log_error(e.what());
        return EXIT_FAILURE;
    }
}
```

This is also the default implementation used when not defining a custom `main` function in your program.

*Note*: when you are using `mwr::logger::error(const vcml::report&)` to log an exception, 
the logger will additionally print the call stack from when the exception was thrown for debug purposes. 
This behaviour can be disabled by setting `mwr::publisher::print_backtrace = false`.

----
Documentation January 2024
