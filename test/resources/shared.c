int global = 42;

int function(int arg) {
    return arg + global;
}

// gcc -fPIC -shared -Wl,-soname,shared.so -o shared.so shared.c
