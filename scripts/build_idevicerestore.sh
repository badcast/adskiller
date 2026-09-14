#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
DEST_DIR="${ROOT_DIR}/3rdParty/apple"
TMP_DIR="${ROOT_DIR}/build/idevicerestore_src"

echo "=== Сборка idevicerestore из официального репозитория libimobiledevice ==="
mkdir -p "${DEST_DIR}"
mkdir -p "${TMP_DIR}"
cd "${TMP_DIR}"

# 1. Проверка/сборка libimobiledevice-glue если не найдена
if ! pkg-config --exists libimobiledevice-glue-1.0; then
    echo "[1/3] Клонирование и сборка libimobiledevice-glue (официальный репозиторий)..."
    if [ ! -d "libimobiledevice-glue" ]; then
        git clone --depth 1 https://github.com/libimobiledevice/libimobiledevice-glue.git
    fi
    cd libimobiledevice-glue
    ./autogen.sh --prefix="${TMP_DIR}/install"
    make -j$(nproc 2>/dev/null || echo 2)
    make install
    export PKG_CONFIG_PATH="${TMP_DIR}/install/lib/pkgconfig:${TMP_DIR}/install/lib64/pkgconfig:${PKG_CONFIG_PATH}"
    export LD_LIBRARY_PATH="${TMP_DIR}/install/lib:${TMP_DIR}/install/lib64:${LD_LIBRARY_PATH}"
    cd "${TMP_DIR}"
fi

# 2. Проверка/сборка libirecovery если не установлена в системе
if ! pkg-config --exists libirecovery-1.0; then
    echo "[2/3] Клонирование и сборка libirecovery (официальный репозиторий)..."
    if [ ! -d "libirecovery" ]; then
        git clone --depth 1 https://github.com/libimobiledevice/libirecovery.git
    fi
    cd libirecovery
    ./autogen.sh --prefix="${TMP_DIR}/install"
    make -j$(nproc 2>/dev/null || echo 2)
    make install
    export PKG_CONFIG_PATH="${TMP_DIR}/install/lib/pkgconfig:${TMP_DIR}/install/lib64/pkgconfig:${PKG_CONFIG_PATH}"
    export LD_LIBRARY_PATH="${TMP_DIR}/install/lib:${TMP_DIR}/install/lib64:${LD_LIBRARY_PATH}"
    
    if [ -f "tools/irecovery" ]; then
        cp -fv tools/irecovery "${DEST_DIR}/irecovery"
        chmod +x "${DEST_DIR}/irecovery"
    fi
    cd "${TMP_DIR}"
fi

# 3. Клонирование и сборка idevicerestore
echo "[3/3] Клонирование и сборка idevicerestore..."
if [ ! -d "idevicerestore" ]; then
    git clone --depth 1 https://github.com/libimobiledevice/idevicerestore.git
fi
cd idevicerestore
export LDFLAGS="-L${TMP_DIR}/install/lib -L${TMP_DIR}/install/lib64 -Wl,-rpath,'\$\$ORIGIN' ${LDFLAGS}"
./autogen.sh --prefix="${TMP_DIR}/install"
make -j$(nproc 2>/dev/null || echo 2)

# 4. Копирование после успешной компиляции
if [ -f "src/idevicerestore" ]; then
    echo "=== Компиляция успешна! Копирование idevicerestore в ${DEST_DIR} ==="
    cp -fv src/idevicerestore "${DEST_DIR}/idevicerestore"
    chmod +x "${DEST_DIR}/idevicerestore"

    # Скопировать также библиотеки зависимостей из локального install, если они были собраны
    if [ -d "${TMP_DIR}/install/lib" ]; then
        cp -fv "${TMP_DIR}"/install/lib/lib*.so* "${DEST_DIR}/" 2>/dev/null || true
    fi
    if [ -d "${TMP_DIR}/install/lib64" ]; then
        cp -fv "${TMP_DIR}"/install/lib64/lib*.so* "${DEST_DIR}/" 2>/dev/null || true
    fi

    # Копирование в build/apple если каталог сборки существует
    if [ -d "${ROOT_DIR}/build" ]; then
        mkdir -p "${ROOT_DIR}/build/apple"
        cp -rf "${DEST_DIR}"/* "${ROOT_DIR}/build/apple/" 2>/dev/null || true
    fi
    echo "Готово: ${DEST_DIR}/idevicerestore успешно скомпилирован и скопирован."
else
    echo "Ошибка: исполняемый файл src/idevicerestore не найден после make!"
    exit 1
fi
