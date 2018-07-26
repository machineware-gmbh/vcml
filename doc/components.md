# VCML Components
Components form the basic building blocks for hardware models in VCML. In
combination with master- and slave-sockets, they can be used to swiftly
assemble a skeleton component that can receive and send TLM transactions. To
begin, first derive your class from `vcml::component` and add instances of
`vcml::slave_socket` for inbound transactions and `vcml::master_socket` for sending
transactions. Define your own constructor and call the `vcml::component`
constructor from there, passing a copy of `sc_core::sc_module_name` as shown
below:

```
#include <vcml.h>

class my_component: public vcml::component {
public:
    vcml::slave_socket IN;
    vcml::master_socket OUT;

    my_component(sc_core::sc_module_name& nm):
        vcml::component(nm), IN("IN"), OUT("OUT") {
    }
};
```

To be able to react to incoming transactions, you need to override a set of
methods from `vcml::component`. They will be called whenever the sockets receive
a transaction:

```
virtual void b_transport(slave_socket* origin, tlm_generic_payload& tx, sc_time& dt);
virtual unsigned int transport_dbg(slave_socket* origin, tlm_generic_payload& tx);
virtual bool get_direct_mem_ptr(slave_socket* origin, const tlm_generic_payload& tx, tlm_dmi& dmi);
virtual void invalidate_direct_mem_ptr(master_socket* origin, u64 start, u64 end);
```
This allows low level access to all supported TLM communication interface:
blocking, debug and direct memory. For convenience, the component merges
blocking and debug calls into a single transport function, using an additional
flags parameter to indicate the request type:

```
virtual unsigned int transport(tlm_generic_payload& tx, sc_time& dt, int flags);
```

These are the possible flags used for the `flags` parameter:

* `VCML_FLAG_DEBUG`: received via the debug interface, do **not** call `wait`
* `VCML_FLAG_NODMI`: indicates that the request should not be serviced via DMI
* `VCML_FLAG_SYNC`: synchronize before and after call with local time offset
* `VCML_FLAG_EXCL`: request should follow load-linked/store exclusive semantics

Flags can be checked using C-style binary operations or one of the following
utility functions from the `vcml` namespace:

```
bool is_set(int flags, int set);
bool is_debug(int flags);
bool is_nodmi(int flags);
bool is_sync(int flags);
bool is_excl(int flags);
```

----
## Direct Memory Interface
Instead of calling the interface methods within `vcml::component`, some request
may also be handled via the TLM Direct Memory Interface (DMI). DMI passes data
pointers from a TLM target back to the initiator, allowing it to directly
operate on the data via a plain C/C++ pointer, e.g. using `memcpy`. This
approach reduces simulation overhead when accessing memories and should be used
whenever possible to increase simulation speed. To make a data array available
for DMI, you can call the following methods:

```
void map_dmi(unsigned char* ptr, u64 start, u64 end, vcml_access access_type, const sc_time& read_latency, const sc_time& write_latency);
```

The following example makes 1KB of ROM memory available for DMI at address
`0x1000..0x13ff`:

```
unsigned char memory[1024]; // 1kB
...
map_dmi(memory, 0x1000, 0x1000 + sizeof(memory) - 1, vcml::VCML_ACCESS_READ);
```

You can optionally also specify read and write latencies to indicate to the
initiator how long it should `wait` to compensate for the access. The following
access privileges can be used for a DMI region:

* `VCML_ACCESS_READ`: initiator should use pointer only for reading
* `VCML_ACCESS_WRITE`: initiator should use pointer only for writing
* `VCML_ACCESS_READ_WRITE`: initiator may read and write using the pointer

To unmap a previously mapped DMI region, you can call:

```
void unmap_dmi(u64 start, u64 end);
```

This call returns once all initiators have confirmed invalidation. It is
possible to only invalidate subregions of a previously mapped region. E.g., you
can map an entire memory block of 1GB and then unmap individual 4kB pages.

----
## Sending Transactions
If a component has been equipped with a `vcml::master_socket`, you can use it to
send transactions. Fundamentally, you can use it to directly call TLM interface
methods, e.g.:

```
vcml::master_socket OUT;
...
OUT->b_transport(...); // !not! OUT.b_transport
```

However, there are some high level routines that abstract away transaction
management overhead and provide some convenience functions frequently needed
when handling `tlm::tlm_generic_payload` instances:

```
tlm_response_status master_socket::read(u64 addr, void* data, unsigned int size, int flags, unsigned int* nbytes);
tlm_response_status master_socket::write(u64 addr, const void* data, unsigned int size, int flags, unsigned int* nbytes);
```

* `read` reads `size` bytes from address `addr` and stores them to `data`.
* `write` writes `size` bytes from buffer `data` to address `addr`.

VCML flags may be passed to suppress DMI (`VCML_FLAG_NODMI`), to indicate a debug
call (`VCML_FLAG_DEBUG`) or to force time synchronization (`VCML_FLAG_SYNC`).
Multiple flags can be combined using the binary `|` operator.
Finally, you can provide an integer pointer `nbytes` that will be used to report
the number of bytes actually written (otherwise specify `NULL`).
Both methods return their success status using the standard TLM
`tlm_response_status` type. You can use the utility functions `vcml::success` and
`vcml::failed` to find out if your call was successful.

----
## Logging and Tracing
You can trace all incoming transactions and outgoing transaction responses for
transactions received via `vcml::slave_socket` automatically. To enable tracing
for individual components, set the following:

```
my_component.loglvl = vcml::LOG_TRACE;
```

*Note*: you need to make sure to have an appropriate [logger](logging.md)
active that listens for messages on the `LOG_TRACE` level.

For convenience, `vcml::component` also provides logging facilities that
automatically add the component name as the source of the log message. The
following logging functions are available in addition to the global ones
described [here](logging.md).

```
void vcml::component::log_error(const char* format, ...);
void vcml::component::log_warn(const char* format, ...);
void vcml::component::log_info(const char* format, ...);
void vcml::component::log_debug(const char* format, ...);
```

----
## Commands
Components can serve as the host for user-specifiable commands. These commands
are intended for debug purposes and look similar to UNIX command line commands.
They use the following syntax:

```
COMMAND ARG0 ARG1 ARG2 ...
```

Commands can be invoked via a remote session that is connected to the VP, or
directly via `vcml::component` command processor interface:

```
my_component.execute("my_command", {"arg0", "arg1", "arg2"}, std::cout);
```

New commands can be registered like shown in the following example:

```
bool my_component::say_hello(const std::vector<std::string> args, std::ostream& os) {
    os << "hello" << std::endl;
    return true;
}

void my_component::init() {
    register_command("hello", 0, this, &my_component::say_hello, "says hello");
}
```

Each component has a set of pre-registered default commands. These are:

* `clist`: returns a list of supported commands
* `cinfo <cmd>`: returns the description for command `<cmd>`
* `reset`: resets the component and calls `vcml::component::reset`

----
Documentation July 2018
