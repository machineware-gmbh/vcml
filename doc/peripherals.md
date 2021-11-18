# VCML Peripherals
----
VCML peripherals serve as the starting point for developing custom I/O
peripheral models. Themselves based on `vcml::component`, `vcml::peripheral` add
a set of convenience functions frequently needed, such as endianness conversion
and access (read and write) callback functions. However, their most important
asset is their capability to host `vcml::reg`, an extended version of regular
[Properties](properties.md) that can be used to model memory mapped I/O
registers. Since peripherals are also [Components](components.md), they also
inherit their capabilities, e.g., [Logging and Tracing](logging.md), command
facilities, and have access to master and slave sockets. Below you can find an
skeleton peripheral that can be used as a starting point for your own designs:

```
#include <vcml.h>

class my_peripheral: public vcml::peripheral {
public:
    vcml::slave_socket IN;

    my_peripheral(const sc_core::sc_module_name& nm):
        vcml::peripheral(nm),
        IN("IN") {
    }

    virtual tlm_response_status read(const vcml::range& addr, void* ptr, int flags) {
        // handle read from addr and store result to *ptr
        return tlm::TLM_OK_RESPONSE;
    }

    virtual tlm_response_status write (const vcml::range& addr, const void* ptr, int flags) {
        // handle write from *ptr to addr
        return tlm::TLM_OK_RESPONSE;
    }
};
```

----
## Registers
Registers represent memory mapped I/O registers that need to react to read and
write events to model the behaviour of their corresponding model. Fundamentally,
they are [Properties](properties.md) and can be initialized similarly.
Their size is determined from their data type, e.g. to model a 32 bit register,
you can use `vcml::reg<uint32_t>`. You can control allowed access modes via one
of the following:

* `reg::allow_read_only()`: read-only access
* `reg::allow_write_only()`: write-only access
* `reg::alow_read_write()`: allow read and write access

If an access attempt is performed that is not permitted (e.g. writing a
read-only register), the peripheral automatically returns
`tlm::TLM_COMMAND_ERROR_RESPONSE` to the caller to indicate the error.

The constructor of a register only requires its name and its address, but you
can optionally also specify its initial value (otherwise zero is assumed). To
add a register to your peripheral model, you can use the following example code:

```
class my_peripheral: public vcml::peripheral {
public:
    vcml::slave_socket IN;
    vcml::reg<vcml::u32> REG;

    my_peripheral(const sc_core::sc_module_name& nm):
        vcml::peripheral(nm),
        IN("IN")
        REG("REG", 0x100, 0){   // name, address, value
        REG.allow_read_write(); // grant access
    }
};
```

In order to react to read and write requests, you need to assign a callback
function that can be called whenever an access is detected. The signatures of
read and write callbacks for a register of type `vcml::reg<T>` are `T read_reg()`
and `T write_reg(T val)`, respectively. You can assign these during the
constructor of your peripheral:

```
class my_peripheral: public vcml::peripheral {
public:
    vcml::slave_socket IN;
    vcml::reg<vcml::u32> REG;

    u32 read_REG() {
        return REG; // here you can modify REG before returning its value
    }

    u32 write_REG(u32 val) {
        return val; // value returned here will be written to REG
    }

    my_peripheral(const sc_core::sc_module_name& nm):
        vcml::peripheral(nm),
        IN("IN"),
        REG("REG", 0x100, 0){
        REG.allow_read_write();
        REG.on_read(&my_peripheral::read_REG);
        REG.on_write(&my_peripheral::write_REG);

        // you can also use c++ lambdas:
        REG.on_read([&]() -> vcml::u32 {
            return 1234;
        });
    }
};
```

When working with registers, please keep the following in mind:

* Assigning read and write callbacks is optional, the register will use default
callbacks if none are provided. The default callbacks assign the new value and
return the current register value without modification.
* Callbacks will not be invoked if access permissions are not appropriate. For
example, the write callback for a read-only register will never be called.
However, requests received via the TLM `transport_dbg` interface will ignore
access permissions.
* Registers support access for individual register bytes. For example, if
only a single byte is written in a multi-byte register, your write callback
will be invoked with the parameter `val` having the appropriate byte updated and
the other bytes still at their current values.
* If an access spans over multiple registers, each register will have its
callback invoked.
* Registers always operate using host endianness. If data needs to be returned
using a different endianness (e.g. big endian for PowerPC or OpenRISC), you
should call `peripheral::set_big_endian()` to have the conversion done for you
automatically.
* The `read` and `write` callbacks of the host peripheral will not be invoked if
a register took the access. You can override `vcml::peripheral::receive` to
change this behaviour.

----
## Array Registers
Registers can also be used to hold an array of values. This is useful for
example when you need to model a set of registers that all behave in the same
way and only differ by their address offset. To declare an array register you
can use `vcml::reg<TYPE, N>` to add an array register that holds `N` elements of
`TYPE`. Note that access privileges can only be set for the entire array, i.e.
all registers will be marked read-only when you call `allow_read()`. Individual
register values can be accessed using the `[]` operator and the type used for
indexing is `size_t`. Initialization is done using comma separated lists,
similarly to how it is done with array properties.

Addresses are computed automatically based on the array index and base data
type. The following example illustrates this for an array register of type
`vcml::reg<uin32_t, 4>` mapped to address `0x1000`:

| Index | Offset | Address            | Accessible via |
| ----- | -------| ------------------ | -------------- |
|   0   | `+0x0` | `0x1000 .. 0x1003` | `REG[0] = ...` |
|   1   | `+0x4` | `0x1004 .. 0x1007` | `REG[1] = ...` |
|   2   | `+0x8` | `0x1008 .. 0x100B` | `REG[2] = ...` |
|   3   | `+0xC` | `0x100C .. 0x100F` | `REG[3] = ...` |

Array registers support an extended form of callback functions for reading and
writing that pass you additionally the index of the register within the array
that has received the access. However, if no indexed callback is provided, the
register will automatically fall back to using the non-indexed ones. See below
for an example of an indexed register:

```
class my_peripheral: public vcml::peripheral {
public:
    vcml::slave_socket IN;
    vcml::reg<vcml::u32, 4> REG;

    u32 read_REG(size_t idx) {
        return REG[idx]; // here you can modify reg before returning its value
    }

    u32 write_REG(u32 val, size_t idx) {
        return val; // value returned here will be assigned to REG[idx]
    }

    my_peripheral(const sc_core::sc_module_name& nm):
        vcml::peripheral(nm),
        IN("IN"),
        REG("REG", 0x100, 0){
        REG.allow_read_write();
        REG.on_read(&my_peripheral::read_REG);
        REG.on_write(&my_peripheral::write_REG);
    }
};
```

Note that you can also use the `size_t` indexed callback functions for scalar
registers. In this case, the value of `my_register.tag` will be passed as `idx`.

----
## Banked Registers
Banked registers hold values on a per-cpu basis, so that each processor in the
system will always only access its own copy. To enable register banking, you
need to:

1. Tell the `vcml::master_socket` of the processor model to expose its `cpuid`:

```
my_processor.DATA.set_bank(cpuid);
my_processor.INSN.set_bank(cpuid);
```

2. Switch your `vcml::register` into bank mode:

```
my_peripheral.REG.set_banked();
```

Afterwards, you can access the individual banks using
`vmcl::reg::bank(int bank)`:

```
std::cout << my_peripheral.REG.bank(1) << std::endl;
my_peripheral.REG.bank(1) = 0;
```

To access elements of banked array registers, you can use
`vcml::reg::bank(int bank, size_t index)`.

During read and write callbacks, bank switching is performed automatically.
Therefore, modifying `REG` during a callback will only affect the bank of the
accessing CPU. You can retrieve the bank id of the processor currently doing an
access using `int register::current_bank()`. Accesses that do not expose a bank
id will use bank id `-1` when accessing banked registers.

----
Documentation update November 2021
