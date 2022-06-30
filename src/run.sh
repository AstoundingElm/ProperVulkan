cd ..
cd assets
cd shaders
./compile.sh 
cd ..
cd ..
cd src
g++ -std=c++17 main.cpp -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi -g 
./a.out
cd ..

