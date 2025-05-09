-- test that all fields got set
print(vp.vcml_version)
print(vp.systemc_version)
print(vp.vcml_version_string)
print(vp.systemc_version_string)
print(vp.config)
print(vp.cfgdir)
print(vp.simdir)
print(vp.curdir)
print(vp.username)
print(vp.pid)

-- test setting property values
data = vp.vcml_version_string
vp.define("test.property", data)
vp.define("test.property2", 123)

-- test setting not yet defined globals
vp.define_globals()

-- test that property lookup works
print(vp.lookup("test.property2"))
print(vp.lookup("nothing"))

-- test that the index operators work
vp["index.property"] = 456
print(vp["index.property"])

-- test that array properties work
vp["array.property"] = { "1", "2", 3, "white space", { 4, "5" } }
print(vp["array.property"])

-- objects can also be used as properties
local metatable = {
    __tostring = function(self)
        return tostring(self.value)
    end
}

local object = { value = 42 }
setmetatable(object, metatable)
vp.define("object.property", object);

-- test logging methods
vp.debug("this is a debug message from lua")
vp.info("this is a info message from lua")
vp.warn("this is a warning message from lua")
vp.error("this is a error message from lua")

-- globals that will be defined as properties
outer = {
    intprop = 55,
    inner = {
        strprop = "hello",
        floatprop = 6.4,
        boolprop = true,
    },

    __in = 0x1000,
}
