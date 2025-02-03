cd build
cmake .. -DHORIZON_BUILD_EXAMPLES=On -DCMAKE_BUILD_TYPE=Release
cd ../debug_build
cmake .. -DHORIZON_BUILD_EXAMPLES=On -DCMAKE_BUILD_TYPE=Debug
cd ..
