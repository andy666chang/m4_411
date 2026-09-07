
OS=$(uname -s)

if [ "$OS" = "Linux" ]; then
    source .venv/bin/activate
elif [[ "$OS" =~ MINGW* ]] || [[ "$OS" =~ CYGWIN* ]]; then
    source .venv/Scripts/activate
else
    echo "未知的系統: $OS"
fi

export ZES_ENABLE_SYSMAN=1
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr

export WORKSPACE=$(pwd)
export ZEPHYR_SDK_INSTALL_DIR=/home/andy/MCHP/zephyr-sdk-0.16.8

echo ================ Zephyr ENV =======================
echo ZES_ENABLE_SYSMAN=$ZES_ENABLE_SYSMAN
echo ZEPHYR_TOOLCHAIN_VARIANT=$ZEPHYR_TOOLCHAIN_VARIANT
echo ZEPHYR_SDK_INSTALL_DIR=$ZEPHYR_SDK_INSTALL_DIR
