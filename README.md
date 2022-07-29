# fledge-south-c37118
## Install
### Install Open-c37.118 lib
* download [open-c37.118](https://github.com/marsolla/Open-C37.118)
* in your local open-c37.118 directory:
  * `$> autogen.sh`
  * `$> make`
  * `$> sudo make install`

on my computer, the libopenc37118-1.0.so is installed in `/usr/local/lib` and the include files are located in `/usr/local/include/openc37118-1.0`

Change your CMakeList.txt according to your own install result.

### Build fledge-sout-c37.118
in your local fledge-south-c37118 directory :
* the plugin is not configurable in the actual state. Configure it directly in `plugin.cpp` code, especially, the IP address and port of the c37118 server : `#define INET_ADDR "127.0.0.1"` and
`#define TCP_PORT 1410`
* `mkdir build`
* `cd build`
* `cmake`
* `make`
* `sudo cp mobc37118.so /usr/local/fledge/plugins/south/c37118`

## Testing

The plugin was tested using [pypmu](https://github.com/iicsys/pypmu).

Note that `test-c37` is a lightweight executable to test the `fc37118.cpp` code that binds `open-c37.118` outside of Fledge (will be removed once the plugin is mature).