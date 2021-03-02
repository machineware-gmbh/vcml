# VCML Session
VCML Sessions convert the usually free-running SystemC simulations to be
controlled interactively and provide extensive introspection and debugging
facilities via an external user interface. To start a SystemC simulation in
interactive mode, you can either:

* use `vcml::system` and specify a valid port to host the session on (for
example via the command line: `-c system.session=44444`)
* modify your `sc_main` so that it uses `vcml:debugging::vspserver` instead of
`sc_start`:
```
int sc_main(int argc,  const char** argv) {
    my_module top("top");
    vcml::debugging::vspserver session(44444);
    session.start(); // instead of sc_start()!
    return 0;
}
```
Once you have a running session, you can connect to it using VCML UI tools such
as [`viper`](https://github.com/janweinstock/viper/), which enables you to:
* Pause, step and resume the simulation
* List simulation information, such as time, cycle, quantum, kernel- and modelling version, etc.
* View and modify `vcml::properties` (including processor and peripheral registers)
* View processor disassembly, memory content, bus memory maps, UART output, etc.
* List and execute module commands

<a href="pictures/smp2.png"><img src="pictures/smp2.png" alt="viper smp cpu view, Ubuntu" width="400" /></a>

----
## VCML Session Protocol (VSP)
Communication between UI tool and simulation is conducted according to the VCML
Session Protocol (VSP), which has been modelled after the
[GDB remote serial protocol](https://sourceware.org/gdb/current/onlinedocs/gdb/Remote-Protocol.html).
Communication packets have the following layout:


| `$` | `<payload>` | `#` | `checksum` |
| --- | ----------- | --- | ---------- |

Every packet starts with `$`, followed by a payload `string`, followed by a
terminator symbol `#`, followed by a two-digit hexadecimal checksum. Correct
reception of a packet by the simulation will be acknowledged by a single `+`.
A `-` signals a request to resend the previous packet.

The payload is a `string` of comma-separated values representing the command
and its arguments and should have the characters `$`, `#`, `,` and `\` escaped
using `\$`, `\#`, `\,` and `\\`, respectively.
Response packets returns their data in comma-separated lists as well. However,
certain commands may encode their individual responses differently (e.g. the
object hierarchy is returned as XML).

The following VSP commands have been defined with `[optional]` and `<mandatory>`
arguments:

* `l,[m]`: lists the module hierarchy of m as XML or the entire hierarchy if `m` is omitted
* `e,<m>,<cmd>,[args,...]`: executes command `cmd` of module `m` passing `args` as parameters.
* `q`: returns the current quantum in seconds (as floating point value)
* `Q,<t>`: sets the current quantum from seconds (as floating point value)
* `a,<name>`: returns the property `name`, e.g. `a,system.cpu.pc`
* `A,<name>,<value>`: sets the property `name` to `value`, e.g. `A,system.cpu.pc,256`.
* `t`: returns the current time in nanoseconds and the current delta cycle as CSV
* `v`: returns the SystemC kernel and VCML version
* `s,[t]`: steps `t` seconds, or one cycle if `t` is omitted; e.g. `s,0.01` steps 10 milliseconds.
* `c`: resumes simulation. While simulation is running, you can send individual bytes as signals:
  * `u`: to query current time and cycle
  * `x`: to pause simulation again
* `x`: terminates the simulation

----
Documentation updated March 2021
