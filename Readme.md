## Build

```
mkdir build
cd build
cmake3 ..
make install -j
```

## Run Tests

```
cd build/bin
VE_LD_LIBRARY_PATH=$PWD/../lib LD_LIBRARY_PATH=$PWD/../lib ./ping_vh
```