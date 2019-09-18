#!/bin/bash

SPDLOG_DIR="Y"   #Y for installing, path to installation folder if installed
OPENCV4_DIR="Y"  #Y for installing, path to installation folder if installed

function confirm() {
    # call with a prompt string or use a default
    read -r -p "${1:-Are you sure? [y/N]} " response
    case "$response" in
        [yY][eE][sS]|[yY]) 
            true
            ;;
        *)
            false
            ;;
    esac
}

# ask_sudo_permissions - 
# Sets global variables: SUDO_PREFIX, PIP_PREFIX
function ask_sudo_permissions()
{
    ### If the executing user is not root, ask for sudo priviledges
    SUDO_PREFIX=""
    PIP_PREFIX=""
    if [ $EUID != 0 ]; then
        SUDO_PREFIX="sudo -E"
        sudo -v
        # Keep-alive: update existing sudo time stamp if set, otherwise do nothing.
        while true; do sudo -n true; sleep 60; kill -0 "$$" || exit; done 2>/dev/null &
    fi    
}

ask_sudo_permissions

cd ..
THIS_DIR=`pwd`
cd pimovity

echo "Installing directory «${THIS_DIR}». Continue?"

confirm || exit


echo "Installing other..."
sudo apt install -y libpthread-stubs0-dev libsystemd-dev libboost-dev libusb-1.0-0-dev
#libturbojpeg0-dev
echo "installed."

echo "Checking SPDLOG..."
if [ $SPDLOG_DIR == "Y" ]; then
	cd $THIS_DIR
	echo "SPDLOG not installed. Installing in ./"

	git clone https://github.com/gabime/spdlog.git
	cd spdlog
	mkdir _build
	mkdir bin
	cmake -H. -B_build -DCMAKE_INSTALL_PREFIX=./bin -DCMAKE_BUILD_TYPE=Release
	cmake --build _build --target install
	
	SPDLOG_DIR=$THIS_DIR/spdlog
	cd $THIS_DIR/pimovity
fi
echo "SPDLOG installed."

spdlog_DIR=$SPDLOG_DIR/bin/lib/spdlog/cmake/

echo "Checking NCSDK..."

if [ `ldconfig -p | grep -n mvnc | wc -l` -lt 2 ]; then
	cd $THIS_DIR
	echo "NCSDK not installed. Installing in ./"

	git clone -b ncsdk2 http://github.com/Movidius/ncsdk
	cd ncsdk
	make api
	cd $THIS_DIR/pimovity
fi
echo "NCSDK installed."

echo $spdlog_DIR

mkdir -p release
cd release
cmake -Dspdlog_DIR=$spdlog_DIR ..
make
