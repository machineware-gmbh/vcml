# VCML Peripherals
----
`vcml::peripheral` serves as the base for developing custom I/O peripheral
models. Extending `vcml::component`, it adds essential convenience functions
for tasks like endianness conversion and access callbacks. Most importantly,
it supports `vcml::reg`—an extension of [Properties](properties.md) designed
to model memory-mapped I/O registers.
Being [Components](components.md), peripherals also inherit features like
[Logging and Tracing](logging.md), command handling, and support for TLM,
GPIO, and CLK sockets.

The following skeleton peripheral demonstrates a basic implementation:

```cpp
#include <vcml.h>

class my_peripheral: public vcml::peripheral {
public:
    vcml::tlm_target_socket tlm_in;

    my_peripheral(const sc_core::sc_module_name& nm):
        vcml::peripheral(nm),
        tlm_in("tlm_in") {
    }

    virtual tlm_response_status read(const vcml::range& addr, void* ptr,
                                     const tlm_sbi& info, address_space as) {
        // handle read from addr and store result to *ptr
        return tlm::TLM_OK_RESPONSE;
    }

    virtual tlm_response_status write(const vcml::range& addr, const void* ptr,
                                      const tlm_sbi& info, address_space as) {
        // handle write from *ptr to addr
        return tlm::TLM_OK_RESPONSE;
    }
};
```

----
## Registers
Registers represent memory mapped I/O registers that need to react to read and
write events to model the behaviour of the corresponding hardware. Fundamentally,
they are [Properties](properties.md) and can be initialized similarly.
Their size is determined from their data type, e.g. to model a 32 bit register,
you can use `vcml::reg<uint32_t>`. You can control allowed access modes via one
of the following:

* `reg::allow_read_only()`: read-only access
* `reg::allow_write_only()`: write-only access
* `reg::allow_read_write()`: allow read and write access

If an access attempt is performed that is not permitted (e.g. writing a
read-only register), the peripheral automatically returns
`tlm::TLM_COMMAND_ERROR_RESPONSE` to the caller to indicate the error.

### Access Configuration
Registers allow configuring custom behavior to enforce access constraints:

* `reg::aligned_accesses_only()`: Reject if `addr % size != 0`.
* `reg::natural_accesses_only()`: Reject if `size != reg.get_size()`.
* `reg::set_access_size(min, max)`: Reject if size is outside `[min, max]`.
* `reg::set_privilege(priv)`: Reject if `tlm_sbi.privilege < priv`.
* `reg::set_secure()`: Reject non-secure accesses.

### Synchronization
Registers can break the TLM quantum to synchronize with the SystemC kernel:

* `reg::sync_always()`: Sync before any access.
* `reg::sync_on_read()`: Sync before read access.
* `reg::sync_on_write()`: Sync before write access.

### Other
* `reg::no_writeback()`: Prevents read-callbacks from updating internal storage.

### Construction
A register has the following constructor: `reg(name, address, [initial_value=0])`.

To instantiate a register in a peripheral model:

```cpp
class my_peripheral: public vcml::peripheral {
public:
    vcml::tlm_target_socket tlm_in;
    vcml::reg<vcml::u32> my_reg;

    my_peripheral(const sc_core::sc_module_name& nm):
        vcml::peripheral(nm),
        tlm_in("tlm_in"),
        my_reg("my_reg", 0x100, 0) { // name, address, value
        my_reg.sync_always();        // synchronize with SystemC time
        my_reg.allow_read_write();   // grant access
    }
};
```

In order to react to read and write requests, you need to assign a callback
function that can be called whenever an access is detected. The signatures of
read and write callbacks for a register of type `vcml::reg<T>` are `T read_reg()`
and `T write_reg(T val)`, respectively. Extended callbacks holding extra
information about the access are also available:
`T read_reg(size_t tag, bool debug)` and
`T write_reg(T val, size_t tag, bool debug)`.
If the extended callbacks are used, `vcml::reg<T>::tag` will be passed as `tag`,
and `debug` will be `true` if the access originated from a `transport_dbg` call.
You can assign these during the constructor of your peripheral:

```cpp
class my_peripheral: public vcml::peripheral {
public:
    vcml::tlm_target_socket tlm_in;
    vcml::reg<vcml::u32> my_reg;

    u32 read_my_reg() {
        return my_reg; // here you can modify my_reg before returning its value
    }

    u32 write_my_reg(u32 val) {
        return val; // value returned here will be written to the register
    }

    my_peripheral(const sc_core::sc_module_name& nm):
        vcml::peripheral(nm),
        tlm_in("tlm_in"),
        my_reg("my_reg", 0x100, 0){
        my_reg.allow_read_write();
        my_reg.on_read(&my_peripheral::read_my_reg);
        my_reg.on_write(&my_peripheral::write_my_reg);

        // you can also use c++ lambdas:
        my_reg.on_read([&]() -> vcml::u32 {
            return 1234;
        });
    }
};
```

When working with registers, please note the following implementation details:

*   **Optional Callbacks**: Assigning read/write callbacks is not required.
    Default callbacks provide basic storage behavior: updating the value on
    writes and returning the stored value on reads.
*   **Permissions**: Callbacks are not invoked if access permissions are
    violated (e.g., writing to a Read-Only register). However, debug
    transactions (`transport_dbg`) bypass permission checks.
*   **Byte Access**: Partial writes are handled automatically. If a write
    updates only specific bytes, the write callback receives the fully merged
    value (new data for written bytes + existing data for unwritten bytes).
*   **Spanning Accesses**: Transactions spanning multiple registers trigger
    the callbacks for each involved register individually.
*   **Endianness**: Internal register storage uses host endianness. For
    targets with different byte ordering (e.g., Big Endian), invoke
    `peripheral::set_big_endian()` to handle conversion automatically.
*   **Precedence**: The peripheral's generic `read` and `write` virtual
    methods are not called if a defined register handles the address. To
    intercept all traffic, override `vcml::peripheral::receive`.

----
## Array Registers
Array registers hold multiple values, useful for modeling sets of registers that
share behavior and differ only by address offset. Use `vcml::reg<TYPE, N, STRIDE>`
to define an array of `N` elements of `TYPE`, spaced by `STRIDE` bytes.
`STRIDE` defaults to `sizeof(TYPE)` if omitted.

Access privileges apply to the entire array; e.g. `allow_read_only()` affects
all elements. Access individual elements using the `[]` operator (index type
`size_t`). Initialization supports comma-separated lists, similar to array
properties.

Addresses are computed automatically based on the array index and base data
type. The following example illustrates this for an array register of type
`vcml::reg<uint32_t, 4>` mapped to address `0x1000`:

| Index | Offset | Address            | Accessible via    |
| ----- | -------| ------------------ | ----------------- |
|   0   | `+0x0` | `0x1000 .. 0x1003` | `my_reg[0] = ...` |
|   1   | `+0x4` | `0x1004 .. 0x1007` | `my_reg[1] = ...` |
|   2   | `+0x8` | `0x1008 .. 0x100b` | `my_reg[2] = ...` |
|   3   | `+0xc` | `0x100c .. 0x100f` | `my_reg[3] = ...` |

The following example illustrates the addresses for a strided array register
`vcml::reg<uint32_t, 4, 8>` mapped to address `0x2000`:

| Index | Offset  | Address            | Accessible via    |
| ----- | --------| ------------------ | ----------------- |
|   0   | `+0x00` | `0x2000 .. 0x2003` | `my_reg[0] = ...` |
|   1   | `+0x08` | `0x2008 .. 0x200b` | `my_reg[1] = ...` |
|   2   | `+0x10` | `0x2010 .. 0x2013` | `my_reg[2] = ...` |
|   3   | `+0x18` | `0x2018 .. 0x201b` | `my_reg[3] = ...` |

Array registers support extended read/write callbacks that include the access
index. If no indexed callback is provided, non-indexed callbacks are used as a
fallback. Example:

```cpp
class my_peripheral: public vcml::peripheral {
public:
    vcml::tlm_target_socket tlm_in;
    vcml::reg<vcml::u32, 4> my_reg;

    u32 read_my_reg(size_t idx, bool debug) {
        return my_reg[idx]; // here you can modify reg before returning its value
    }

    u32 write_my_reg(u32 val, size_t idx, bool debug) {
        return val; // value returned here will be assigned to my_reg[idx]
    }

    my_peripheral(const sc_core::sc_module_name& nm):
        vcml::peripheral(nm),
        tlm_in("tlm_in"),
        my_reg("my_reg", 0x100, { 1, 2, 3, 4 }) {
        my_reg.allow_read_write();
        my_reg.on_read(&my_peripheral::read_my_reg);
        my_reg.on_write(&my_peripheral::write_my_reg);
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

1. Tell the `vcml::tlm_initiator_socket` of the processor model to expose its `cpuid`:

```cpp
my_processor.data.set_cpuid(cpuid);
my_processor.insn.set_cpuid(cpuid);
```

2. Switch your `vcml::reg` into banked mode:

```cpp
my_peripheral.my_reg.set_banked();
```

Afterwards, you can access the individual banks using
`vcml::reg::bank(int bank)`:

```cpp
std::cout << my_peripheral.my_reg.bank(1) << std::endl;
my_peripheral.my_reg.bank(1) = 0;
```

To access elements of banked array registers, you can use
`vcml::reg::bank(int bank, size_t index)`.

During read and write callbacks, bank switching is performed automatically.
Therefore, modifying the register during a callback will only affect the bank of the
accessing CPU. You can retrieve the bank id of the processor currently doing an
access using `int vcml::reg::current_bank()`. Accesses that do not expose a bank
id will use bank id `-1` when accessing banked registers.

----
Documentation update February 2026
