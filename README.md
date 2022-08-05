# fledge-south-c37118
## Install
### Install Open-c37.118 lib
* download [open-c37.118](https://github.com/marsolla/Open-C37.118)
* in your local open-c37.118 directory:
  * `$> autogen.sh`
  * `$> make`
  * `$> sudo make install`

on my computer, the libopenc37118-1.0.so is installed in `/usr/local/lib` and the include files are located in `/usr/local/include/openc37118-1.0`

Change your CMakeList.txt according to your own install result (or propose a improvements of the `CMakeLists.txt`).

### Build fledge-sout-c37.118
in your local fledge-south-c37118 directory :
* `mkdir build`
* `cd build`
* `cmake`
* `make`
* `sudo cp mobc37118.so /usr/local/fledge/plugins/south/c37118`

The plugin is configurable. As C37.118 enables to retrieve the configuration of the sending PMU, that's how it works now with the configuration attribute `REQUEST_CONFIG_TO_PMU : true`.
 Don't set it to `false` as it is still work in progress.

## Testing

The plugin was tested using `randomPMU.py` example of [pypmu](https://github.com/iicsys/pypmu).

## TODO
* finalize the **hard** configuration of the PMU
* actually only one PMU_station is retreived from one PMU. This would be easy to extend to multi station broadcasting.
* Filter to FledgePower pivot format is to be developed. Especially an analysis is ongoing to ensure the timestamp definition is compliant with IEC-61850.
* implement UDP, and TLS
* One bug is identified in the Open-c37.118 library in `PMU_Station *CONFIG_Frame::PMUSTATION_GETbyIDCODE(unsigned short idcode)` which will always get a PMU_station even if the given ID_CODE is not affected to any PMU_station.
