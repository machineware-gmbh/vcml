# VCML Session
VCML Sessions convert the usually free-running SystemC simulations to be
controlled interactively and provide extensive introspection and debugging
facilities via an external user interface. To start a SystemC simulation in
interactive mode, you can either:

* use `vcml::system` and specify a valid port to host the session on (for
example via the command line: `-c system.session=4444`)
* modify your `sc_main` so that it uses `vcml:debugging::vspserver` instead of
`sc_start`:
```cxx
int sc_main(int argc, char** argv) {
    my_module top("top");
    vcml::debugging::vspserver session(4444);
    session.start(); // instead of sc_start()!
    return 0;
}
```
Once you have a running session, you can connect to it using GUI or CLI tools,
such as MachineWare's ViPER GUI or Python PyVP CI scripting framework, enabling you to:
* Pause, step and resume the simulation
* List simulation information, such as time, cycle, quantum, kernel- and modeling library version
* View and modify `vcml::properties` (including processor and peripheral registers)
* View processor disassembly, memory content, bus memory maps, terminal output
* List and execute `vcml::module` commands

### Session Discovery
While a session is running, it announces itself to local tools with a file
`vcml_session_<pid>` in the temporary directory (usually `/tmp`), where
`<pid>` is the process id of the simulator. The file is removed when the
session ends. It holds four lines:
1. the host the session listens on, as given via `session_host`; wildcard
   addresses such as `0.0.0.0` or `::` are reported as `localhost`
2. the port of the session
3. the name of the user running the simulator
4. the path of the simulator executable

If the simulator crashes, the file is left behind. Tools should check whether
a process with that id still exists before connecting.

----
## VCML Session Protocol (VSP)
Communication between UI tool and simulation is conducted according to the VCML
Session Protocol (VSP), which has been modeled after the
[GDB remote serial protocol](https://sourceware.org/gdb/current/onlinedocs/gdb/Remote-Protocol.html).
Communication packets have the following layout:


| `$` | `<payload>` | `#` | `checksum` |
| --- | ----------- | --- | ---------- |

Every RSP packet starts with `$`, followed by a payload `string`, followed by a
terminator symbol `#`, followed by a two-digit hexadecimal checksum. Correct
reception of a packet by the simulation will be acknowledged by a single `+`.
A `-` signals a request to resend the previous packet. Care must be taken to
escape characters that have special meaning: `$`, `#`, `*` and `}` characters
must be prefixed with a `}` character if they occur normally in the payload and
should not be interpreted according to their control character meaning.

Normally, the VSP payload is a string of comma-separated values representing
the command and its arguments. If a comma is not to be used as an argument
separator, it must be escaped using a backslash (in C strings, the backslash
must also be escaped, resulting in the character sequence `\\,`).
Response packets return their data in comma-separated lists as well, with the
first element always indicating response status: `OK` for success and `E` for
errors.

VSP commands can be divided into two groups currently, with more likely being
added in the future. General simulation commands control the global SystemC
state and simulation progress, while target commands interact with processors,
such as breakpoints and single-stepping.

### General Commands
The following general VSP commands have been defined with `[optional]` and
`<mandatory>` arguments:

#### Version
The version command queries the SystemC, VCML, and protocol versions used in
the target simulator. It does not receive any arguments:
- Command  `$version#**`
- Response `$OK,systemc-version-string,vcml-version-string,session-protover#**`

#### Status
The status command queries the current time-stamp, delta-cycle and runstate.
The runstate can either be `running` or `stopped:<reason>`. Stop reason is a
string indicating what caused the simulation to stop.
* Command: `$status#**`
* Response: `$OK,runstate,time-stamp-ns,delta-cycle[,events]#**`

If the client has subscribed to at least one event (see
[Event Commands](#event-commands)), the response carries an additional field
holding a JSON object with all events that occurred since the previous status
request. Like any other field, it is escaped: commas, backslashes and quotes
are prefixed with a backslash, which clients must remove before parsing the
JSON. The first response that reports
`stopped:<reason>` is guaranteed to contain all remaining events up to the
point where the simulation stopped.

Valid stop reasons include (but are not limited to):
* `target:<name>:<t>`: target `<name>` completed its requested single-step at
  time stamp `<t>` ns
* `breakpoint:<id>:<t>`: one processor in the simulation hits breakpoint
  `<id>` at time stamp `<t>` ns
* `rwatchpoint:<id>:<addr>:<size>:<t>`: watchpoint `<id>` is being read
  starting at `<addr>` with cpu access size `<size>` at time stamp `<t>` ns
* `wwatchpoint:<id>:<addr>:<data>:<t>`: watchpoint `<id>` is being written at
  address `<addr>` with bytes `<data>` at time stamp `<t>` ns
* `step`: requested simulation duration has elapsed
* `elaboration`: simulator has completed elaboration and is ready to simulate
* The stop command can define custom exit reason strings to be used

#### Resume
Resumes the simulation. If an optional `[duration<s|ms|us|ns>]` argument is
specified, simulation will automatically pause after `duration` has elapsed.
The stop reason returned by `status` will be `step` in this case.
* Command: `$resume[,duration<s|ms|us|ns>]#**`
* Response: `$OK#**` or `$E,errmsg#**` in case of an error

#### Stop
Interrupts a currently running simulation and brings it into the paused state.
An optional first argument can specify a custom stop reason, otherwise, the
stop reason `user` will be used. This command may also be issued to a currently
paused simulation in which case it is simply ignored.
* Command: `$stop[,reason]#**`
* Response: `$OK#**`

#### Set Stop Mode
Sends a request to change the current stop mode. The default mode is `hard`, meaning
the simulation can stop in the current quantum. Soft stop waits until all targets
complete the current quantum and has precedence over hard stop in multiclient scenarios.
* Command: `$setsm,<stop-mode>#**`
* Response: `$OK#**`

#### Quit
Sends a termination request to the simulation. The current delta-cycle will be
finished and `vspserver::start` will return normally, allowing all cleanup
routines to complete naturally. While this command will be acknowledged using a
`+`, no response will be transmitted and the simulator is expected to terminate
afterward.
* Command: `$quit#**`
* Response: `<none>`

#### List
The list command queries a listing of the entire object hierarchy of the
simulation. The command accepts an optional first argument, specifying the
desired format in which the hierarchy should be reported: `xml` (default) or
`json`. This command may only be issued when the simulation is stopped,
otherwise, an error response will be returned.
* Command: `$list[,format]#**`
* Response: `$OK,<hierarchy>...</hierarchy>#**`

Objects that publish events (see [Event Commands](#event-commands)) list the
names of these events: as a comma-separated `events` attribute of `<object>`
in `xml`, and as an `"events"` array in `json`. Only the events published by
the object itself are listed, not those of its children. Objects that do not
publish any events have no `events` entry.
```xml
<object name="leds" kind="vcml::gpio::leds" version="..." events="led">
```
```json
{"name":"leds","kind":"vcml::gpio::leds","version":"...","events":["led"],...}
```

#### Execute
The execute command sends a request to a `vcml::module` to perform a given
command. As a first parameter, it must receive the full hierarchical name of
the module that is supposed to execute the command. The second paramter is the
name of the module command to execute. The remaining arguments will be passed
in order to the module command handler. The response holds the command result
string or an error if something went wrong:
* Command: `$exec,<module>,<command>[,arg0][,arg1]...#**`
* Response: `$OK,<comand-response-string>#**`

#### Get Quantum
Retrieves the global quantum in nanoseconds.
* Command: `$getq#**`
* Response: `$OK,<quantum-ns>#**`

#### Set Quantum
Set the global quantum from a given value in nanoseconds.
* Command: `$setq,<quantum-ns>#**`
* Response: `$OK#**`

#### Get Attribute
Fetches the value of a given attribute via its full hierarchical name. This
includes all `vcml::properties`. In case of array properties, multiple values
are returned for each array element.
* Command: `$geta,<attribute-name>#**`
* Response: `$OK,<attribute-value>[,attribute-value1]...#**`

#### Set Attribute
Set the current value of the given attribute via its full hierarchical name.
This includes all `vcml::properties`. In case of array properties, new values
for all elements must be specified.
* Command: `$seta,<attribute-name>,<attribute-value>[,attribute-value1]...#**`
* Response: `$OK#**`

### Target Commands
The following target VSP commands have been defined to interact with processors
that implement `vcml::target`:

#### Step
The step command performs a single step for one or more specified targets.
The simulation will automatically stop when any of the specified targets
completes its step. During this time, all other targets will be free-running.
* Command: `$step,<target-name>[,target-name1]...#**`
* Response: `$OK#**`

#### Insert Breakpoint
The insert breakpoint command installs a new breakpoint on a given target. Once
the simulation is resumed next time, this breakpoint causes the simulation to
be stopped and indicates its `id` in the stop reason. The first argument must
be the full hierarchy name of a target. The second argument is the address or
the name of the symbol to place the breakpoint at. If successful, the response
reports the global `id` under which the breakpoint can be referenced.
* Command: `$mkbp,<target-name>,<address_or_symbol>#**`
* Response: `$OK,inserted breakpoint <id>#**`

#### Remove Breakpoint
Removes a breakpoint globally identified via its `<id>`.
* Command: `$rmbp,<id>#**`
* Response: `$OK#**`

#### Insert Watchpoint
The insert watchpoint command installs a new watchpoint on a given target. Once
the simulation is resumed next time, this watchpoint causes the simulation to
be stopped when the target performs a memory access that overlaps with the
watchpoint, and indicates its `id` in the stop reason. The first argument must
be the full hierarchy name of a target. The second argument is the base address or
the name of the symbol to place the watchpoint at. The third argument is the size in bytes
for the watchpoint region. The fourth argument is the type of the watchpoint;
which can be `r` (read), `w` (write), or `rw` (access). If successful, the response
reports the global `id` under which the watchpoint can be referenced.
* Command: `$mkwp,<target-name>,<address_or_symbol>,<number-of-bytes>,<type>#**`
* Response: `$OK,inserted watchpoint <id>#**`

#### Remove Watchpoint
Removes the specified access type from a watchpoint. The first argument is its `<id>`. The second
argument is the type of the access to remove, which can be `r` (read), `w` (write) or `rw` (access).
* Command: `$rmwp,<id>,<type>#**`
* Response: `$OK#**`

#### List CPU Registers
Returns a list of names and sizes (in byte) of CPU registers of a given target.
* Command: `$lreg,<target-name>#**`
* Response: `OK,reg_a:rega_a_size,reg_b:reg_b_size,reg_c:reg_c_size#**`

#### Read CPU Register
Returns the content of a CPU register.
* Command: `$getr,<target-name>,<reg-name>#**`
* Response: `OK,<byte0>,<byte1>,<byte2>,...#**`

#### Write CPU Register
Attempts to set the contents of the given CPU register to the given bytes.
It is implementation defined if partial writes are supported.
* Command: `setr,<target-name>,<reg-name>,<byte0>,<byte1>,...#**`
* Response: `OK,<n> bytes written#**`

#### Translate Virtual to Physical Address
Attempts to translate the given virtual address to a physical address using
the currently active translation regime.
* Command: `vapa,<target-name>,<virtual-address>#**`
* Response: `OK,<physical-address>#**`

#### Read Virtual Memory
Performs a debug read access using the provided virtual address and returns
the requested number of bytes.
* Command: `$vread,<target-name>,<virtual-address>,<number-of-bytes>#**`
* Response: `OK,<byte0>,<byte1>,<byte2>,...#**`

#### Write Virtual Memory
Performs a debug write access using the provided virtual address and stores
the given bytes to memory.
* Command: `$vwrite,<target-name>,<virtual-address>,<byte0>,<byte1>,..#**`
* Response: `OK,<n> bytes written#**`

#### Read Physical Memory
Performs a debug read access using the provided physical address and returns
the requested number of bytes.
* Command: `$pread,<target-name>,<physical-address>,<number-of-bytes>#**`
* Response: `OK,<byte0>,<byte1>,<byte2>,...#**`

#### Write Physical Memory
Performs a debug write access using the provided physical address and stores
the given bytes to memory.
* Command: `$pwrite,<target-name>,<physical-address>,<byte0>,<byte1>,..#**`
* Response: `OK,<n> bytes written#**`

#### Architecture
Retrieves the architecture string defined by the target. The response holds the queried
string or an error if something went wrong:
* Command: `$arch,<target-name>#**`
* Response: `OK,<architecture>#**`

### Event Commands
Clients can subscribe to events published by objects in the simulation, such
as LED changes or transactions on ports. The session buffers these events per
client and delivers them with the next `status` response, which then clears
the buffer of that client. Clients do not see the events of other clients, and
all subscriptions of a client are removed when it disconnects.

Both event commands can only be used while the simulation is stopped. While it
is running, they return `$E,simulation running#**`.

#### Subscribe
Subscribes to an event of one or more publishers, given by their full
hierarchical name. Subscriptions are not recursive: every object must publish
the event itself, as reported by `list`. Modules do not publish events of
their children, so to subscribe to all trace events below a module, a client
walks the hierarchy reported by `list` and subscribes every publisher it
finds. Socket arrays are not publishers either, only their sockets are. If
any object cannot be found or does not publish the event, nothing is
subscribed and an error is returned. Subscribing to a publisher that is
already subscribed has no effect, each event is reported only once.

The object hierarchy is fixed once the simulation has been elaborated, so the
publishers reported by `list` do not change. Publishers that models create
after the simulation has started are not supported: clients that listed the
hierarchy before will not know about them.
* Command: `$sub,<event>,<object>[,object1]...#**`
* Response: `$OK#**` or `$E,errmsg#**`

#### Unsubscribe
Removes subscriptions. Without arguments, all subscriptions of the client are
removed. With only an event, all subscriptions to that event are removed.
Otherwise, exactly the given publishers are unsubscribed from that event.
Unsubscribing an object that is not subscribed is not an error, but all
objects must exist.
* Command: `$unsub[,event][,object][,object1]...#**`
* Response: `$OK#**` or `$E,errmsg#**`

#### Set Event Buffer Size
Sets how many events the client buffers between two `status` requests. Once
the buffer is full, the oldest events are dropped and counted in `dropped`.
The default size is 65536 events.
* Command: `$sebs,<size>#**`
* Response: `$OK#**` or `$E,errmsg#**`

#### Events Object
The events object reported by `status` lists all events in the order they
were published. `dropped` is only present if events were lost because the
client did not poll `status` often enough: each client buffers at most 65536
events (see `sebs`), after that the oldest ones are dropped.
```json
{"events":[{"event":"led","sender":"top.leds","time":10000,"delta":4,"payload":{...}},...],"dropped":3}
```
* `event`: name of the event
* `sender`: full hierarchical name of the object that published the event
* `time`: simulation time of the event in picoseconds. For transactions this
  includes the local time offset of the sender, so due to temporal decoupling,
  timestamps of different senders are not guaranteed to be in ascending order.
* `delta`: SystemC delta cycle count when the event was published
* `payload`: event-specific contents, see below

#### Events
* `led`: published by `vcml::gpio::leds` whenever an LED changes:
  `{"led":<index>,"state":<true|false>}`
* `uart`: published by `vcml::serial::terminal` for every character it
  receives, i.e. every character the UART model transmits. The payload is a
  JSON string holding exactly one character: printable ASCII as is, all other
  bytes as `\u00XX`, where the code point equals the byte value (so `0xff` is
  sent as `"\u00ff"`, not as UTF-8). Concatenate the payloads of consecutive
  `uart` events of the same sender to get the output text. Note that this
  event is meant for displaying terminal output: interactive terminals should
  keep using a serial backend such as `tcp`, which also handles input.
* `trace`: published by ports, sockets and registers for every
  forward and backward transaction, independent of the `trace` and
  `trace_errors` properties and of any tracers given on the command line:
  ```json
  {"dir":"fw","protocol":"TLM","error":false,"tx":{...}}
  ```
  * `dir`: `fw` for requests, `bw` for responses. Protocols without responses
    (`CLK`, `SERIAL`, `SIGNAL`, `ETHERNET`, `CAN`) only report `fw`.
  * `protocol`: one of `TLM`, `GPIO`, `CLK`, `PCI`, `I2C`, `LIN`, `SPI`, `SD`,
    `SERIAL`, `SIGNAL`, `VIRTIO`, `ETHERNET`, `CAN`, `USB`
  * `error`: `true` if the transaction failed
  * `tx`: protocol-specific contents, as produced by `trace_payload_to_json`
    in `src/vcml/tracing/protocol.cpp`

Models publish events by deriving from `vcml::debugging::vsppublisher`
(`vcml/debugging/vspevents.h`), listing their event names in
`published_events()` and calling `publish_event()` with a JSON payload. Only
build the payload if `has_subscribers()` returns `true`, so that there is no
overhead while nobody is subscribed.

----
Documentation updated October 2026
