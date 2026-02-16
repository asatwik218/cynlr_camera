## Dependencies

* OpenCV
* Spinnaker SDK

cmake [...] -DOpenCV_DIR=<path-to-opencv>

### Example which works on Windows

`build/> conan install .. -s build_type=Release -s compiler="msvc" -s arch=x86_64 --build=missing --output-folder=.`

`build/> cmake .. -DCMAKE_TOOLCHAIN_FILE="build/conan_toolchain.cmake" -DSPINNAKER_DIR="C:\Program Files\Teledyne\Spinnaker"`

`build/> cmake --build .. --config Release`

**Important note** : do not forget to match debug and release configurations when installing through conan and then build through cmake.

**At the moment, try Release mode as I did not yet have time to make Debug mode work**