#!/bin/sh

INCLUDES="-I/usr/include/cairo -I./aimp_sdk -I./aimp_sdk/Helpers -I./amr -I./amrnb -I./amrwb"

INPUT=$(find ./ -name "*.cpp")

LIBRARIES="-lpthread -lm -ldl -lstdc++ -static-libgcc"

FLAGS="-fPIC -Wno-attributes -Wno-register"

clear
echo "Assembling..."
gcc -shared -o ./aimp_inputAMR/x64/aimp_inputAMR.so $INCLUDES $FLAGS $INPUT $LIBRARIES
strip --strip-unneeded ./aimp_inputAMR/x64/aimp_inputAMR.so
echo "Done!"
