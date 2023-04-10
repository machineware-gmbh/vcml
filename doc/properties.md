# VCML Properties
Properties serve as a way to provide components with modeling parameters that
are specified during simulation elaboration (as opposed to using C/C++ constants
which are fixed at compile time). This enables you to use the same simulator
binary for different scenarios without recompilation by specifying different
set of parameter values when the simulation starts. Examples for this include
the `size` property of memories or the `backends` property of peripherals. Both
of them can be adapted during simulation start to suit different needs, e.g.
testing out larger or smaller memories. To find out which models expose which
properties check out the corresponding section from their documentation.
Properties can be provided with values using *Property Brokers*:

* `vcml::broker_file`: property values are retrieved from a text file.

* `vcml::broker_arg`: property values are taken from the command line

* `vcml::broker_env`: property values are retrieved from environment
variables.

* `vcml::broker_lua`: property values are defined by a LUA scriptfile.

----
## Scalar Properties
Scalar properties refer to properties wrapping a scalar data type, such as
`uint32_t` or `vcml::u32` etc. They are declared as members of a `sc_module` or
any of its subclasses using:

```
class my_module: public sc_core::sc_module
{
public:
    vcml::property<uint32_t> my_property;
    // ... other class members
};
```

During the constructor of your module, you need to provide the property with
its name, as well as a default value that it will use when no configuration
value is given for it:

```
my_module::my_module(const sc_core::sc_module_name& module_name):
    sc_core::sc_module(module_name),
    my_property("my_property", 0x11223344) {
    // ... other initialization code
}
```

As a general convention, properties should receive the same name as they have
within your class. It is furthermore possible to omit the default value, in
which case the property will receive the default value for that data type, e.g.
`uint32_t() = 0` in the example above.

After construction the full name of `my_property` will be
`module_name + "." + "my_property"`. Its base name will be `"my_property"`.

You can use a property `vcml::property<T>` as if it was an instance of `T`. For
the above example, `my_property` should behave just as any `uint32_t`, e.g.

```
uint32_t added_value = my_property + 1;
uint32_t masked_value = my_property & 0x0000FFFF;
uint32_t shifted_value = my_property >> 16;
```
Assignment and comparison:

```
my_property = 17; // permanently assigns 17, overwriting configuration
if (my_property == 17) ...;
```

Output:

```
std::cout << my_property << std::endl; // prints value converted to string
printf("value = %u\n", (uint32_t)my_property); // cast to POD needed
printf("value = %u\n", my_property.get()); // read below!
```

However, if you need to explicitly convert a property to its base type `T`
(e.g. for using it with `printf`), you can call `vcml::property<T>::get()`.

Properties receive their configuration values in string form. The system uses
`stringstreams` to convert this to `T`, e.g. the following must be possible for
your datatype:

```
T val;
std::stringstream ss;
ss.str(config_string); // config_string received from property provider
ss >> val;
```

The `std::stringstream` class provides conversions for most basic data types.
However, you can also provide custom operators to construct your data type from
a string:

```
std::istream& operator >> (std::istream& is, my_data_type& val) {
    std::string strval;
    is >> strval;
    // convert strval to val
    return is;
}
```

If you do not want to use property brokers, you can still initialize your
property directly from a string using `vcml::property<T>::str(string s)`, e.g.
`my_property.str("17")` assigns the integer value `17` to `my_property`. Note
that commas have a special meaning within initialization strings (see Array
Properties), so they must be escaped, e.g. `my,string` needs to become
`my\,string` in order to retain the comma. Note that backslashes must also be
escaped in C/C++, so initialization using a string constant within your code
must use: `"my\\,string"`.

----
## Array Properties
Properties can also be used to store array values. To create an array property
that holds four `uint32_t` values, you can use the following:

```
class my_module: public sc_core::sc_module
{
public:
    vcml::property<uint32_t, 4> my_property;
    // ... other class members
};
```

Array properties differ in some points from scalar properties:

* Initialization: you can only specify one default value for all array items.
* To read and write individual array items, you can use `operator []`.
* When cast into their base type, array properties return the first item.

Additionally, the string conversion routines behave differently. When
converting an array property to its string representation, the individual item
values are first converted to string and then returned as a comma separated
list, e.g.:

```
my_property[0] = 3;
my_property[1] = 2;
my_property[2] = 1;
my_property[3] = 0;

std::cout << my_property.str() << std::endl; // output: "3,2,1,0"
```

Similarly, initialization of array properties also requires comma separated
value lists. Note that if you want to use array properties with strings, you
need to escape commas or otherwise they will be treated as a delimiter, e.g.
`my,string,with,commas` needs to become `my\,string\,with\,commas`. Since `\`
has a special meaning in C/C++, you need to escape it as well for the compiler
if you are initializing your property by yourself:

```
my_property.str("my\\,string\\,with\\,commas");
```

----
## Property Brokers
Property Brokers are used to assign values to properties. Fundamentally, a
Property Broker is a key value store, where the keys are the names of the
properties and the values are their corresponding values in string form. This
functionality is provided within the `vcml::broker` base class that is used by
all property brokers.

All property brokers are kept in a list which is used by all properties while
looking for an initialization value. New providers are added in descending
priority order of the brokers: **higher priority brokers override property
initializations by lower priority brokers**. Newly created properties iterate
over this list front to back and ask each encountered provider for an
initialization value by passing its full name. This process stops once all
of the providers have been asked, with the first provided value being reported
back. If no provider had a suitable initialization value, the property uses its
default value specified in its constructor.

VCML includes a set of default property brokers that you can use to assign
values to properties. They are presented in the following.

----
### Configuration via Files
`vcml::broker_file` opens a text file it is given via its constructor and opens
it for reading. While reading, it skips over empty lines and lines starting
with '#' (comments), looking for the following syntax:

```
full.name.of.property = value_of_property   # this comment is ignored
# this line is ignored
```

Long lines can be split using `\`:
```
full.name.of.property = \
        value_of_property
```

Within configuration files, you can use a few variables which will be replaced
by the broker with its runtime values:

| Variable | Description                           |
| -------- | ------------------------------------- |
| `$dir`   | Directory of the configuration file   |
| `$cfg`   | Configuration file name (without ext) |
| `$app`   | Full path of simulation binary        |
| `$pwd`   | Current working directory             |
| `$tmp`   | Directory for temporary files         |
| `$usr`   | User that started the simulation      |
| `$pid`   | PID of the simulation process         |

----
### Configuration via the Command Line
`vcml::broker_arg` takes `argc` and `argv` from `sc_main` and looks for
configuration arguments. A configuration argument is introduced using `-c`
followed by a key value pair `name=value`. Example:

```
./simulator -c my_module.int_prop=17 -c my_module.str_prop="hello world"
```

This would initialize the property `int_prop` of module `my_module` to `17` and
the property `str_prop` of the same module with `hello world`.

----
### Configuration via the Environment
`vcml::broker_env` looks for environment variables that are suitable for
initializing properties. To be eligible, the environment variable must be
named according to the full name of the property it initializes, having dots `.`
replaced with underscores `_`:

```
# initializes my_module.int_prop to 17
export my_module_int_prop=17
```

Note that this assigment is ambiguous: property `my.module.int_prop`
would also receive that value.

----
### Configuration via LUA Scripting
If VCML is built with LUA support, the `vcml::broker_lua` class is available.
It receives a path to a LUA script file (typically ending with `.lua`) and
executes it. Inside the script, a new global variable called `vp` is
available to query and set simulation properties as well as for providing
general information about the simulation. The following table lists the
methods provided by `vp` to the LUA script:

| Method                     | Description                           |
| -------------------------- | ------------------------------------- |
| `vp.define(string, string)`| Defines the value of a property       |
| `vp.lookup(string)`        | Returnes the value of a property      |
| `vp.debug(string)`         | Prints a debug log message            |
| `vp.info(string)`          | Prints a information log message      |
| `vp.warn(string)`          | Prints a warning log message          |
| `vp.error(string)`         | Prints a error log message            |

Furthermore, `vp` provides the following data fields:

| Field                       | Type     | Description                      |
| --------------------------- | -------- | -------------------------------- |
| `vp.vcml_version_string`    | `string` | String version of VCML           |
| `vp.vcml_version`           | `int`    | Integer version of VCML          |
| `vp.systemc_version_string` | `string` | String version of SystemC        |
| `vp.systemc_version`        | `int`    | Integer version of SystemC       |
| `vp.username`               | `string` | User that started the simulation |
| `vp.pid`                    | `int`    | Process ID of the simulation     |
| `vp.curdir`                 | `string` | Current working directory        |
| `vp.cfgdir`                 | `string` | Directory of the LUA config file |
| `vp.simdir`                 | `string` | Directory of the vp binary       |
| `vp.config`                 | `string` | Name of the LUA config file      |

Below is a simple example of a LUA config file that computes an address and an
images to load for a `vcml::generic::memory` component:

```
addr = 0x8000000 + 0x1000
image = vp.cfgdir .. '../roms/kernel.bin@' .. addr
vp.debug('using image ' .. image)
vp.define('system.memory.images', image)
vp['system.memory.images'] = image -- alternative way to define image
```

----
### Configuration using Custom Providers
All custom property brokers should inherit from `vcml::broker`.
Afterwards, two options exist for providing initialization values:

* via the key value store: using the `define(string key, string val)` method
new entries can be added to the key value store that will be assigned to
properties automatically.

* via overriding the method `bool broker::lookup(const string key, string& val)`,
which will be called for every property requesting an initialization value.
This method should return `true` when a suitable value has been found and
`false` otherwise.

----
Documentation updated April 2023
