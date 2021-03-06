#!/bin/bash
#
# Proyecto - Deployment
# By Recursantes 3.0
# Buscanos en redes sociales
#

clear
echo "     __             __                        "
echo " .--|  .-----.-----|  .-----.--.--.-----.----."
echo " |  _  |  -__|  _  |  |  _  |  |  |  -__|   _|"
echo " |_____|_____|   __|__|_____|___  |_____|__|  "
echo "             |__|           |_____|           "
echo "               Desplegando master"
echo " "


# Instalar cmake 

echo "Ingrese password para instalar las commons..."
sudo apt-get -y install cmake
cmake --version


# Installar las commons
git clone https://github.com/sisoputnfrba/so-commons-library.git

cd so-commons-library

echo "Ingrese password para instalar las commons..."

sudo make install

cd ..

# Installar commlib
cd commLib
mkdir build-dir
cd build-dir
cmake ..
sudo make install

cd ../..

rm -rf ./Tdos
cp -R Team/ ./Tdos
