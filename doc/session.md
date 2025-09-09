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
The version command queries the SystemC as VCML versions used in the target
simulator. It does not receive any arguments:
- Command  `$version#**`
- Response `$OK,systemc-version-string,vcml-version-string#**`

#### Status
The status command queries the current time-stamp, delta-cycle and runstate.
The runstate can either be `running` or `stopped:<reason>`. Stop reason is a
string indicating what caused the simulation to stop.
* Command: `$status#**`
* Response: `$OK,runstate,time-stamp-ns,delta-cycle#**`

Valid stop reasons include (but are not limited to):
* `target:<name>:<t>`: target `<name>` completed its requested single-step at
  time stamp `<t>` ns
* `breakpoint:<id>:<t>`: one processor in the simulation hits breakpoint
  `<id>` at time stamp `<t>` ns
* `rwatchpoint:<id>:<addr>:<size>:<t>`: watchpoint `<id>` is being read
  starting at `<addr>` with cpu access size `<size>` at time stamp `<t>` ns
* `wwatchpoint:<id>:<addr>:<data>:<t>`: watchpoint `<id>` is being written at
  address `<addr>` with value `<data>` at time stamp `<t>` ns
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
desired format in which the hierarchy should be reported. The default (and
currently only supported format) is `xml`. This command may only be issued when
the simulation is stopped, otherwise, an error response will be returned.
* Command: `$list[,format]#**`
* Response: `$OK,<hierarchy>...</hierarchy>#**`

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
The step command performs a single step of a given target. Simulation will
automatically be stopped once the specified target has completed its step.
During this time, all other targets will be free-running.
* Command: `$step,<target-name>#**`
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
be stopped and indicates its `id` in the stop reason. The first argument must
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
Returns a list of names of CPU registers of a given target.
* Command: `$lreg,<target-name>#**`
* Response: `OK,reg_a,reg_b,reg_c#**`

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

----
Documentation updated September 2025
