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

### Build fledge-south-c37.118
in your local fledge-south-c37118 directory :
* `mkdir build`
* `cd build`
* `cmake`
* `make`
* `sudo cp mobc37118.so /usr/local/fledge/plugins/south/c37118`

## Configuration
The plugin is configurable.

C37.118 enables to retrieve the configuration of the sender, that's the easiest and most obvious way of doing it by setting the attribute `REQUEST_CONFIG_TO_SENDER : true`. If you prefere a hard configuration you can set it to `false` and configure the fields under `SENDER_HARD_CONFIG` - which are ignored otherwise. Please not that in the case of hard configuration, if the configuration does not fit to the one of the sender, the plugin will crash.

`SPLIT_STATIONS`: From one stream source, C37.118 allow to have multiple data sources. You can decide wether 

* `true` the plugin desagregate the sources into individual readings
* `false` the plugin keeps the multiple sources into one reading. 


## Testing

The plugin was tested using `randomPMU.py` example of [pypmu](https://github.com/iicsys/pypmu).

## TODO
* Implement hard configuration for multiple sources
* Filter should be implemented in this plugin based on source IDCODE to choose
* Filter to FledgePower pivot format is to be developed. Especially an analysis is ongoing to ensure the timestamp definition is compliant with IEC-61850.
* Implement UDP, and TLS
* One bug is identified in the Open-c37.118 library in `PMU_Station *CONFIG_Frame::PMUSTATION_GETbyIDCODE(unsigned short idcode)` which will always get a PMU_station even if the given ID_CODE is not affected to any PMU_station.
