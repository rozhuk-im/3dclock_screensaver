# 3dclock_screensaver

This is the Linux port of my student OpenGL screensaver written in 2003.


Naezzhy Petr(Наезжий Пётр) <petn@mail.ru> 2002-2020\
Rozhuk Ivan <rozhuk.im@gmail.com> 2020-2024


## Compilation

### Linux
```
sudo apt-get install build-essential git cmake clang libxrandr-dev libfreetype6-dev libglew-dev fakeroot
git clone https://github.com/rozhuk-im/3dclock_screensaver.git
cd 3dclock_screensaver
mkdir build
cd build
cmake ..
make -j 4
```

### FreeBSD/DragonFlyBSD
```
sudo pkg install git cmake x11/xrandr x11/libXfixes print/freetype2
git clone https://github.com/rozhuk-im/3dclock_screensaver.git
cd 3dclock_screensaver
mkdir build
cd build
cmake ..
make -j 4
```
