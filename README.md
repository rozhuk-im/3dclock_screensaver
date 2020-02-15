# 3dclock_screensaver
This is the Linux port of my student OpenGL screensaver written in 2003.
The program was created in the development environment Netbeans 8.2. 

# To compile the program:
1. apt install clang libxrandr-dev  libfreetype6-dev libglew-dev
2. git clone https://github.com/Naezzhy/3dclock_screensaver.git
3. cd 3dclock_screensaver
4. clang++ -I/usr/include/freetype2 -lGL -lGLU -lXfixes -lX11 -lXrandr -lpthread -lfreetype -o screensaver_3dclock screensaver_3dclock.cpp
